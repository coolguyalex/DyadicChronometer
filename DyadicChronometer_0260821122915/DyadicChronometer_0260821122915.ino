// ============================================================
//  TimeDilator — ESP32 Edition (core build)
//  by Alexander Sousa, June 2026
//
//  Hardware:
//    NodeMCU ESP32-S
//    WS2811 RGB LED string (20 LEDs)
//    Passive buzzer
//    2x momentary push buttons
//
//  Wiring:
//    LED data        → GPIO13
//    Hour forward    → GPIO12  (other leg to GND)
//    Hour backward   → GPIO14  (other leg to GND)
//    Buzzer signal   → GPIO27  (other leg to GND)
//    LED VCC         → 5V
//    LED GND         → GND
//
//  WiFi credentials:
//    Set WIFI_SSID and WIFI_PASSWORD below.
//    On first successful NTP sync the onboard LED
//    flashes green three times.
//
//  LED Layout — 20 LED string (0-based indices, MSB left):
//
//    Col:               2^3  2^2  2^1  2^0
//    Row 1 (H1, 2 bits):  -,   -, 19,  18   <- top of clock
//    Row 2 (H2, 4 bits): 14,  15, 16,  17
//    Row 3 (M1, 3 bits):  -,  13, 12,  11
//    Row 4 (M2, 4 bits):  7,   8,  9,  10
//    Row 5 (S1, 3 bits):  -,   6,  5,   4
//    Row 6 (S2, 4 bits):  0,   1,  2,   3   <- bottom of clock
//
//  ON  = bit is 1  (ROYGBIV colour based on current hour)
//  OFF = bit is 0  (dim white)
// ============================================================

#include <WiFi.h>
#include <time.h>
#include <FastLED.h>

// ── WiFi credentials ─────────────────────────────────────────
const char* WIFI_SSID     = "Yggdrasil";
const char* WIFI_PASSWORD = "Odin1eye";

// ── Timezone ─────────────────────────────────────────────────
//  UTC offset in seconds. EDT = -4*3600, EST = -5*3600
//  dstOffset: 3600 if your region observes DST, else 0
// UTC_OFFSET_SEC should be STANDARD time offset (EST), not EDT
#define UTC_OFFSET_SEC  (-5 * 3600)   // EST = UTC-5
#define DST_OFFSET_SEC  (3600)        // adds 1hr automatically when DST is active

// ── Pin definitions ──────────────────────────────────────────
#define LED_PIN     13
#define BTN_FWD     12
#define BTN_BWD     14
#define BUZZER_PIN  27
#define BUZZER_CH    0    // ESP32 LEDC channel

// ── FastLED ──────────────────────────────────────────────────
#define NUM_LEDS    20
#define LED_TYPE    WS2811
#define COLOR_ORDER RGB
CRGB leds[NUM_LEDS];

// ── LED index map (0-based, MSB first per row) ───────────────
const uint8_t ROW1[] = {19, 18};           // H1 — 2 bits
const uint8_t ROW2[] = {14, 15, 16, 17};   // H2 — 4 bits
const uint8_t ROW3[] = {13, 12, 11};       // M1 — 3 bits
const uint8_t ROW4[] = { 7,  8,  9, 10};   // M2 — 4 bits
const uint8_t ROW5[] = { 6,  5,  4};       // S1 — 3 bits
const uint8_t ROW6[] = { 0,  1,  2,  3};   // S2 — 4 bits

// ── Colours ──────────────────────────────────────────────────
//  ROYGBIV across 24 hours via FastLED HSV wheel:
//    Midnight (0)  -> Red        HUE =   0
//    3:00 AM       -> Orange     HUE ~  21
//    6:00 AM       -> Yellow     HUE ~  43
//    9:00 AM       -> Green      HUE ~  85
//    Noon    (12)  -> Cyan/Teal  HUE = 128
//    3:00 PM       -> Blue       HUE ~ 171
//    6:00 PM       -> Indigo     HUE ~ 192
//    9:00 PM       -> Violet     HUE ~ 213
//    Midnight (24) -> Red again  (wraps)
const CRGB COLOR_OFF = CRGB(10, 10, 10);   // dim white for OFF bits

CRGB timeColor(uint8_t hour) {
  uint8_t hue = (uint8_t)((uint16_t)hour * 255 / 24);
  return CRGB(CHSV(hue, 255, 255));
}

// ── Chime frequencies ────────────────────────────────────────
//  Whole tone scale across 4 octaves (C3 to A#6)
//  Hour N plays notes 0 through N-1, ascending
const int FREQ_WHOLE_TONE[24] = {
   131, 147, 165, 185, 208, 233,
   262, 294, 330, 370, 415, 466,
   523, 587, 659, 740, 831, 932,
  1047,1175,1319,1480,1661,1865
};

//  Just intonation — pure integer ratios from C3
const int FREQ_JUST[24] = {
   131, 147, 164, 174, 196, 218,
   245, 262, 294, 327, 349, 392,
   436, 491, 523, 587, 654, 698,
   785, 871, 981,1047,1175,1308
};

// Active chime set — swap to FREQ_JUST to change mode
const int* CHIME_FREQS = FREQ_WHOLE_TONE;

// ── Button debounce ──────────────────────────────────────────
uint32_t lastPressFwd = 0;
uint32_t lastPressBwd = 0;
#define DEBOUNCE_MS 300

// ── Chime state ──────────────────────────────────────────────
uint8_t lastChimeHour = 255;

// ── Hour offset (adjusted by buttons) ───────────────────────
//  Buttons shift this value rather than rewriting NTP config.
//  Persists in RAM only — resets to 0 on power cycle.
int8_t hourOffset = 0;

// ── Buzzer helpers ───────────────────────────────────────────
void playNote(int freq, int durationMs) {
  ledcWriteTone(BUZZER_PIN, freq);
  delay(durationMs);
  ledcWriteTone(BUZZER_PIN, 0);   // silence between notes
}

void buzzerOff() {
  ledcWriteTone(BUZZER_PIN, 0);
}
// ── Chime: play N notes for hour N ───────────────────────────
void playChime(uint8_t hour) {
  uint8_t count = (hour == 0) ? 24 : hour;
  int noteDur = map(count, 1, 24, 180, 80);
  int gapDur  = map(count, 1, 24,  60, 30);
  for (uint8_t i = 0; i < count; i++) {
    playNote(CHIME_FREQS[i % 24], noteDur);
    delay(gapDur);
  }
  buzzerOff();
}

// ── Startup sequence ─────────────────────────────────────────
void startupSequence() {
  // Flash all LEDs white once
  fill_solid(leds, NUM_LEDS, CRGB::White);
  FastLED.show();
  delay(300);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  delay(200);

  // Seconds sweep 0-59
  for (uint8_t s = 0; s < 60; s++) {
    uint8_t s1 = s / 10, s2 = s % 10;
    writeRow(ROW5, 3, s1, CRGB::White);
    writeRow(ROW6, 4, s2, CRGB::White);
    FastLED.show();
    if (s % 3 == 0) {
      playNote(CHIME_FREQS[(s / 3) % 24], 35);
    }
    delay(25);
  }

  // Minutes sweep 0-59
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  for (uint8_t m = 0; m < 60; m++) {
    uint8_t m1 = m / 10, m2 = m % 10;
    writeRow(ROW3, 3, m1, CRGB::White);
    writeRow(ROW4, 4, m2, CRGB::White);
    FastLED.show();
    if (m % 3 == 0) {
      playNote(CHIME_FREQS[(m / 3) % 24], 35);
    }
    delay(25);
  }

  // Hours sweep 0-23
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  for (uint8_t h = 0; h < 24; h++) {
    writeRow(ROW1, 2, h / 10, timeColor(h));
    writeRow(ROW2, 4, h % 10, timeColor(h));
    FastLED.show();
    playNote(CHIME_FREQS[h], 55);
    delay(55);
  }

  buzzerOff();
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  delay(300);
}

// ── LED helpers ──────────────────────────────────────────────
void writeRow(const uint8_t* indices, uint8_t numBits,
              uint8_t value, CRGB colorOn) {
  for (uint8_t i = 0; i < numBits; i++) {
    bool bitOn = (value >> (numBits - 1 - i)) & 1;
    leds[indices[i]] = bitOn ? colorOn : COLOR_OFF;
  }
}

void displayTime(uint8_t hour, uint8_t minute, uint8_t second) {
  CRGB colorOn = timeColor(hour);
  writeRow(ROW1, 2, hour   / 10, colorOn);
  writeRow(ROW2, 4, hour   % 10, colorOn);
  writeRow(ROW3, 3, minute / 10, colorOn);
  writeRow(ROW4, 4, minute % 10, colorOn);
  writeRow(ROW5, 3, second / 10, colorOn);
  writeRow(ROW6, 4, second % 10, colorOn);
  FastLED.show();
}

// ── NTP ──────────────────────────────────────────────────────
bool getTime(uint8_t &h, uint8_t &m, uint8_t &s) {
  struct tm t;
  if (!getLocalTime(&t)) return false;
  // Apply button-driven hour offset, wrapping 0-23
  h = (uint8_t)((t.tm_hour + hourOffset + 24) % 24);
  m = t.tm_min;
  s = t.tm_sec;
  return true;
}

// ── Setup ────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println(F("\n=== TimeDilator ESP32 ==="));

  pinMode(BTN_FWD, INPUT_PULLUP);
  pinMode(BTN_BWD, INPUT_PULLUP);

  ledcAttach(BUZZER_PIN, 1000, 8);  // pin, initial freq (placeholder), resolution bits
  

  // FastLED
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(80);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  // WiFi — show dim blue while connecting
  fill_solid(leds, NUM_LEDS, CRGB(0, 0, 20));
  FastLED.show();
  Serial.print(F("Connecting to WiFi"));
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.print(F("\nConnected: "));
  Serial.println(WiFi.localIP());

  // NTP sync
  configTime(UTC_OFFSET_SEC, DST_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");
  Serial.print(F("Syncing NTP"));
  struct tm t;
  bool synced = false;
  for (int i = 0; i < 20; i++) {
    if (getLocalTime(&t)) { synced = true; break; }
    Serial.print('.');
    delay(500);
  }
  if (synced) {
    Serial.println(F(" OK"));
    // Flash green three times to confirm sync
    for (int i = 0; i < 3; i++) {
      fill_solid(leds, NUM_LEDS, CRGB(0, 40, 0));
      FastLED.show(); delay(150);
      fill_solid(leds, NUM_LEDS, CRGB::Black);
      FastLED.show(); delay(150);
    }
  } else {
    Serial.println(F(" FAILED — check credentials/network"));
    // Flash red to signal failure
    fill_solid(leds, NUM_LEDS, CRGB(40, 0, 0));
    FastLED.show(); delay(2000);
  }

  // Startup light + sound sequence
  startupSequence();

  Serial.println(F("Ready."));
}

// ── Main loop ────────────────────────────────────────────────
void loop() {
  uint32_t now_ms = millis();
  uint8_t h, m, s;

  // ── Hour forward button (GPIO12) ─────────────────────────
  if (digitalRead(BTN_FWD) == LOW && now_ms - lastPressFwd > DEBOUNCE_MS) {
    lastPressFwd = now_ms;
    hourOffset = constrain(hourOffset + 1, -12, 12);
    Serial.printf(">> +1 hour (offset now %d)\n", hourOffset);
  }

  // ── Hour backward button (GPIO14) ────────────────────────
  if (digitalRead(BTN_BWD) == LOW && now_ms - lastPressBwd > DEBOUNCE_MS) {
    lastPressBwd = now_ms;
    hourOffset = constrain(hourOffset - 1, -12, 12);
    Serial.printf("<< -1 hour (offset now %d)\n", hourOffset);
  }

  // ── Get current time ─────────────────────────────────────
  if (!getTime(h, m, s)) {
    // NTP not ready — flash dim red
    fill_solid(leds, NUM_LEDS, CRGB(20, 0, 0));
    FastLED.show();
    delay(500);
    return;
  }

  // ── Chime on the hour ────────────────────────────────────
  if (m == 0 && s == 0 && h != lastChimeHour) {
    lastChimeHour = h;
    playChime(h);
  }

  // ── Update display ───────────────────────────────────────
  displayTime(h, m, s);

  Serial.printf("%02d:%02d:%02d\n", h, m, s);

  delay(200);
}
