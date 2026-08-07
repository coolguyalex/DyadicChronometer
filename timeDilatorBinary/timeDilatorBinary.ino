// ============================================================
// Time Dilator by Alexander Sousa June 2026
//
//  Binary LED Clock — RTC + WS2811 LED String
//  Hardware: Elegoo Nano/Uno R3 + DS1307 RTC + WS2811 RGB LEDs
//
//  DS1307 Wiring:
//    VCC  → 5V
//    GND  → GND
//    SDA  → A4
//    SCL  → A5
//
//  WS2811 Wiring:
//    VCC  → 5V
//    GND  → GND
//    DATA → Pin 6
//
//  Buttons:
//    Hour Forward  → D2 (momentary, other leg to GND)
//    Hour Backward → D3 (momentary, other leg to GND)
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
//  Physical string positions (1-based) for reference:
//    Col:               2^3  2^2  2^1  2^0
//    Row 1 (H1, 2 bits):  -,   -, 20,  19
//    Row 2 (H2, 4 bits): 15,  16, 17,  18
//    Row 3 (M1, 3 bits):  -,  14, 13,  12
//    Row 4 (M2, 4 bits):  8,   9, 10,  11
//    Row 5 (S1, 3 bits):  -,   7,  6,   5
//    Row 6 (S2, 4 bits):  1,   2,  3,   4
//
//  Each row displays its digit in binary, MSB on the left.
//  ON  = bit is 1  (ROYGBIV colour based on current hour)
//  OFF = bit is 0  (dim white so you can see the LED is there)
// ============================================================

#include <Wire.h>
#include <RTClib.h>
#include <FastLED.h>

// ── FastLED config ───────────────────────────────────────────
#define LED_PIN      6
#define NUM_LEDS     20
#define LED_TYPE     WS2811
#define COLOR_ORDER  RGB

CRGB leds[NUM_LEDS];

// ── Button pins ──────────────────────────────────────────────
#define BTN_FWD  2    // advance +1 hour
#define BTN_BWD  3    // retreat -1 hour

uint32_t lastPressFwd = 0;
uint32_t lastPressBwd = 0;
#define DEBOUNCE_MS 300

// ── Colours ──────────────────────────────────────────────────
//  Uses FastLED's HSV colour wheel (0-255) to cycle ROYGBIV:
//    Midnight (0)  -> Red        HUE =   0
//    3:00 AM       -> Orange     HUE ~  21
//    6:00 AM       -> Yellow     HUE ~  43
//    9:00 AM       -> Green      HUE ~  85
//    Noon    (12)  -> Cyan/Teal  HUE = 128
//    3:00 PM       -> Blue       HUE ~ 171
//    6:00 PM       -> Indigo     HUE ~ 192
//    9:00 PM       -> Violet     HUE ~ 213
//    Midnight (24) -> Red again  (wraps back)
CRGB COLOR_OFF = CRGB(5, 5, 5);   // bit = 0  (dim white)

// Returns the ON colour for the current hour (0-23)
CRGB timeColor(uint8_t hour) {
  uint8_t hue = (uint8_t)((uint16_t)hour * 255 / 24);
  return CRGB(CHSV(hue, 255, 255));
}

// ── RTC ──────────────────────────────────────────────────────
RTC_DS1307 rtc;

// ── LED index map (0-based, MSB first per row) ───────────────
const uint8_t ROW1[] = {19, 18};           // H1 — 2 bits  (MSB left)
const uint8_t ROW2[] = {14, 15, 16, 17};   // H2 — 4 bits  (MSB left)
const uint8_t ROW3[] = {13, 12, 11};       // M1 — 3 bits  (MSB left)
const uint8_t ROW4[] = { 7,  8,  9, 10};   // M2 — 4 bits  (MSB left)
const uint8_t ROW5[] = { 6,  5,  4};       // S1 — 3 bits  (MSB left)
const uint8_t ROW6[] = { 0,  1,  2,  3};   // S2 — 4 bits  (MSB left)

// ── Setup ────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  Serial.println(F("=== Binary LED Clock ==="));

  pinMode(BTN_FWD, INPUT_PULLUP);
  pinMode(BTN_BWD, INPUT_PULLUP);

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(80);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  if (!rtc.begin()) {
    Serial.println(F("ERROR: DS1307 not found. Check wiring!"));
    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.show();
    while (true) delay(10);
  }

  // ── Time set ───────────────────────────────────────────────
  // Uncomment the line below, upload once to sync time, then
  // comment it out again and re-upload.
  // Adjust the hour offset for your timezone:
  //   EDT (summer): 4    EST (winter): 5
  //
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)) - TimeSpan(0, 4, 0, 0));

  Serial.println(F("RTC OK. D2=hour forward, D3=hour backward."));
}

// ── Main loop ────────────────────────────────────────────────
void loop() {
  uint32_t now_ms = millis();

  // ── Forward button (D2) ──────────────────────────────────
  if (digitalRead(BTN_FWD) == LOW && now_ms - lastPressFwd > DEBOUNCE_MS) {
    lastPressFwd = now_ms;
    DateTime now = rtc.now();
    rtc.adjust(now + TimeSpan(0, 1, 0, 0));
    Serial.println(F(">> +1 hour"));
  }

  // ── Backward button (D3) ─────────────────────────────────
  if (digitalRead(BTN_BWD) == LOW && now_ms - lastPressBwd > DEBOUNCE_MS) {
    lastPressBwd = now_ms;
    DateTime now = rtc.now();
    rtc.adjust(now - TimeSpan(0, 1, 0, 0));
    Serial.println(F("<< -1 hour"));
  }

  // ── Update display ───────────────────────────────────────
  DateTime now = rtc.now();

  uint8_t H1 = now.hour()   / 10;
  uint8_t H2 = now.hour()   % 10;
  uint8_t M1 = now.minute() / 10;
  uint8_t M2 = now.minute() % 10;
  uint8_t S1 = now.second() / 10;
  uint8_t S2 = now.second() % 10;

  CRGB colorOn = timeColor(now.hour());

  writeRow(ROW1, 2, H1, colorOn);
  writeRow(ROW2, 4, H2, colorOn);
  writeRow(ROW3, 3, M1, colorOn);
  writeRow(ROW4, 4, M2, colorOn);
  writeRow(ROW5, 3, S1, colorOn);
  writeRow(ROW6, 4, S2, colorOn);

  FastLED.show();

  Serial.print(now.hour());   Serial.print(':');
  Serial.print(now.minute()); Serial.print(':');
  Serial.println(now.second());

  delay(200);
}

// ── writeRow ─────────────────────────────────────────────────
void writeRow(const uint8_t* indices, uint8_t numBits, uint8_t value, CRGB colorOn) {
  for (uint8_t i = 0; i < numBits; i++) {
    bool bitOn = (value >> (numBits - 1 - i)) & 1;
    leds[indices[i]] = bitOn ? colorOn : COLOR_OFF;
  }
}
