/**
 * ESP32-S3 Internet Radio with MAX98357A I2S Amplifier
 *
 * Pinout:
 *   MAX98357A:  DIN=GPIO6,  LRC=GPIO5,  BCLK=GPIO4
 *   Button:     GPIO21 → GND (active-LOW, pulled up)
 */

#include <Arduino.h>
#include <WiFi.h>
#include "secrets.h"

#include "AudioGeneratorMP3.h"
#include "AudioGeneratorAAC.h"
#include "AudioFileSourceHTTPStream.h"
#include "AudioOutputI2S.h"

/* ──────────────────────────── Config ───────────────────────────────── */

// NOTE: Wi-Fi credentials come from secrets.h - copy secrets.h.example
//       to secrets.h and fill it in before building.

#define I2S_DOUT  6
#define I2S_LRC   5
#define I2S_BCLK  4
#define BUTTON_PIN 21

/* ──────────────────────────── Stations ─────────────────────────────── */

static const char* stationUrls[] = {
    "http://icecast.vgtrk.cdnvideo.ru/vestifm",
    "http://nashe1.hostingradio.ru/nashe-128.mp3",
    "http://icecast.vgtrk.cdnvideo.ru/v1",
    "http://cast.rusradio.ru/rrr",
    "http://pub01.nonstop.ru/",
};
const size_t numStations = sizeof(stationUrls) / sizeof(stationUrls[0]);

/* ──────────────────────────── Globals ─────────────────────────────── */

// Allocated ONCE — no new/delete ever
static AudioOutputI2S            i2sOut;
static AudioFileSourceHTTPStream httpFile;
static AudioGeneratorMP3         mp3Gen;
static AudioGeneratorAAC         aacGen;

static AudioGeneratorMP3  *pMP3 = nullptr;   // currently active generator
static AudioGeneratorAAC  *pAAC = nullptr;
static int                 currentStation = 0;
static bool                isActive = false;
static unsigned long       lastBtnMillis = 0;
static bool                btnPrevState  = true;

/* ──────────────────────────── Helpers ─────────────────────────────── */

static void stopAudio(void) {
    if (pMP3) { pMP3->stop(); pMP3 = nullptr; }
    if (pAAC) { pAAC->stop(); pAAC = nullptr; }
    isActive = false;
}

static void playStation(int idx) {
    currentStation = idx;
    const char* url = stationUrls[idx];

    Serial.println("\n=====================================");
    Serial.printf("Station %d/%d: %s\n", idx + 1, numStations, url);
    Serial.println("=====================================");

    // 1) Stop whatever is playing
    stopAudio();

    // 2) Reset HTTP stream & connect to new URL
    httpFile.close();
    if (!httpFile.open(url)) {
        Serial.printf("[!] HTTP open failed for: %s\n", url);
        return;
    }

    // 3) Pick decoder by URL hint
    String urlStr = String(url);
    bool isAAC = (urlStr.indexOf("aac") >= 0);

    if (isAAC) {
        pAAC = &aacGen;
        pAAC->begin(&httpFile, &i2sOut);
        Serial.println("[Audio] AAC decoder started");
    } else {
        pMP3 = &mp3Gen;
        pMP3->begin(&httpFile, &i2sOut);
        Serial.println("[Audio] MP3 decoder started");
    }
    isActive = true;
}

static void changeStation(void) {
    int next = (currentStation + 1) % numStations;
    playStation(next);
}

/* ──────────────────────────── Setup ──────────────────────────────── */

void setup() {
    Serial.begin(115200);
    Serial.println("\n[Radio] Starting up ...");

    // I2S (MAX98357A — external I2S amp)
    i2sOut.SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    i2sOut.SetGain(1.0f);

    // Button
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    btnPrevState = digitalRead(BUTTON_PIN);

    // WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("[WiFi] Connecting");
    for (int i = 0; i < 30; i++) {
        delay(500);
        Serial.print(".");
        if (WiFi.status() == WL_CONNECTED) break;
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] FAILED – rebooting...");
        delay(3000);
        ESP.restart();
        return;
    }

    Serial.printf("[WiFi] OK  IP: %s\n", WiFi.localIP().toString().c_str());

    // Play first station
    playStation(0);
}

/* ──────────────────────────── Loop ──────────────────────────────── */

void loop() {
    // ── Feed the active generator ─────────────────────────────
    if (pMP3 && pMP3->isRunning()) {
        if (!pMP3->loop()) {
            Serial.println("[!] MP3 stream ended");
            pMP3 = nullptr;
        }
    }
    if (pAAC && pAAC->isRunning()) {
        if (!pAAC->loop()) {
            Serial.println("[!] AAC stream ended");
            pAAC = nullptr;
        }
    }

    // Generator stopped (stream ended, network drop, etc.) → retry
    if (isActive && !pMP3 && !pAAC) {
        Serial.println("[Auto] Restarting current station...");
        isActive = false;
        playStation(currentStation);
    }

    // ── WiFi watchdog ─────────────────────────────────────────
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Disconnected! Reconnecting...");
        WiFi.reconnect();
        int wait = 0;
        while (WiFi.status() != WL_CONNECTED && wait++ < 20) {
            delay(500);
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[WiFi] Reconnected  IP: %s\n", WiFi.localIP().toString().c_str());
            // Restart the station that was playing
            playStation(currentStation);
        }
    }

    // ── Button (polling, debounce 400 ms) ──────────────────────
    bool btn = digitalRead(BUTTON_PIN);
    if (btn != btnPrevState) {
        lastBtnMillis = millis();
        btnPrevState  = btn;
    }
    if (btn == LOW && (millis() - lastBtnMillis) > 400) {
        Serial.println("[Button] Switching station...");
        changeStation();
        lastBtnMillis = millis();
    }

    delay(10);   // gentle yield — prevents watchdog
}