#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --------------------------------------------------
// LCD SETTINGS
// --------------------------------------------------
// If your LCD stops showing text, try changing 0x27 to 0x3F.
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --------------------------------------------------
// PIN SETTINGS
// --------------------------------------------------
const int LED_1_PIN = 3;
const int LED_2_PIN = 5;
const int LED_3_PIN = 6;
const int LED_4_PIN = 9;

const int LCD_BACKLIGHT_PWM_PIN = 10;

const int LED_BRIGHTNESS_POT_PIN = A0;
const int LCD_BRIGHTNESS_POT_PIN = A1;

const int BPM_PLUS_BUTTON_PIN = 7;
const int BPM_MINUS_BUTTON_PIN = 8;

// --------------------------------------------------
// METRONOME SETTINGS
// --------------------------------------------------
const int MIN_BPM = 40;
const int MAX_BPM = 240;
int bpm = 120;

const int NUM_BEATS = 4;

int ledPins[NUM_BEATS] = {
  LED_1_PIN,
  LED_2_PIN,
  LED_3_PIN,
  LED_4_PIN
};

int currentBeat = 0;
int previousBeat = 3;

unsigned long lastBeatTime = 0;
unsigned long beatInterval = 500;

// --------------------------------------------------
// BUTTON SETTINGS
// --------------------------------------------------
const unsigned long DEBOUNCE_TIME = 35;
const unsigned long FIRST_REPEAT_DELAY = 500;
const unsigned long REPEAT_INTERVAL = 90;

bool lastPlusReading = HIGH;
bool lastMinusReading = HIGH;

bool stablePlusState = HIGH;
bool stableMinusState = HIGH;

unsigned long lastPlusDebounceTime = 0;
unsigned long lastMinusDebounceTime = 0;

unsigned long plusPressedTime = 0;
unsigned long minusPressedTime = 0;

unsigned long lastPlusRepeatTime = 0;
unsigned long lastMinusRepeatTime = 0;

bool plusHeld = false;
bool minusHeld = false;

// --------------------------------------------------
// DISPLAY UPDATE SETTINGS
// --------------------------------------------------
unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_UPDATE_INTERVAL = 100;

int lastDisplayedBPM = -1;
int lastDisplayedBeat = -1;

// --------------------------------------------------
// POT SMOOTHING
// --------------------------------------------------
int smoothedLedPot = 0;
int smoothedLcdPot = 0;

// --------------------------------------------------
// SETUP
// --------------------------------------------------
void setup() {
  for (int i = 0; i < NUM_BEATS; i++) {
    pinMode(ledPins[i], OUTPUT);
    analogWrite(ledPins[i], 0);
  }

  pinMode(LCD_BACKLIGHT_PWM_PIN, OUTPUT);
  analogWrite(LCD_BACKLIGHT_PWM_PIN, 255);

  pinMode(BPM_PLUS_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BPM_MINUS_BUTTON_PIN, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Metronome");
  lcd.setCursor(0, 1);
  lcd.print("4/4 Mode");
  delay(1000);
  lcd.clear();

  smoothedLedPot = analogRead(LED_BRIGHTNESS_POT_PIN);
  smoothedLcdPot = analogRead(LCD_BRIGHTNESS_POT_PIN);

  lastBeatTime = millis();

  // Start with beat 1 ON immediately
  currentBeat = 0;
  previousBeat = 3;
  turnOffAllLeds();
  analogWrite(ledPins[currentBeat], readLedBrightness());
}

// --------------------------------------------------
// MAIN LOOP
// --------------------------------------------------
void loop() {
  unsigned long now = millis();

  int ledBrightness = readLedBrightness();
  int lcdBrightness = readLcdBrightness();

  handleBpmButtons(now);

  beatInterval = 60000UL / bpm;

  analogWrite(LCD_BACKLIGHT_PWM_PIN, lcdBrightness);

  handleMetronome(now, ledBrightness);
  updateCurrentLedBrightness(ledBrightness);
  updateDisplay(now);
}

// --------------------------------------------------
// READ LED BRIGHTNESS POT
// --------------------------------------------------
int readLedBrightness() {
  int raw = analogRead(LED_BRIGHTNESS_POT_PIN);

  smoothedLedPot = (smoothedLedPot * 9 + raw) / 10;

  int brightness = map(smoothedLedPot, 0, 1023, 0, 255);

  // True off zone
  if (brightness < 8) {
    brightness = 0;
  }

  // Eye-friendly dimming curve
  brightness = (brightness * brightness) / 255;

  return constrain(brightness, 0, 255);
}

// --------------------------------------------------
// READ LCD BACKLIGHT BRIGHTNESS POT
// --------------------------------------------------
int readLcdBrightness() {
  int raw = analogRead(LCD_BRIGHTNESS_POT_PIN);

  smoothedLcdPot = (smoothedLcdPot * 9 + raw) / 10;

  int brightness = map(smoothedLcdPot, 0, 1023, 0, 255);

  // True off zone
  if (brightness < 8) {
    brightness = 0;
  }

  // Eye-friendly dimming curve
  brightness = (brightness * brightness) / 255;

  return constrain(brightness, 0, 255);
}

// --------------------------------------------------
// HANDLE BPM BUTTONS
// --------------------------------------------------
void handleBpmButtons(unsigned long now) {
  handleSingleBpmButton(
    now,
    BPM_PLUS_BUTTON_PIN,
    lastPlusReading,
    stablePlusState,
    lastPlusDebounceTime,
    plusPressedTime,
    lastPlusRepeatTime,
    plusHeld,
    +1
  );

  handleSingleBpmButton(
    now,
    BPM_MINUS_BUTTON_PIN,
    lastMinusReading,
    stableMinusState,
    lastMinusDebounceTime,
    minusPressedTime,
    lastMinusRepeatTime,
    minusHeld,
    -1
  );
}

void handleSingleBpmButton(
  unsigned long now,
  int pin,
  bool &lastReading,
  bool &stableState,
  unsigned long &lastDebounceTime,
  unsigned long &pressedTime,
  unsigned long &lastRepeatTime,
  bool &held,
  int change
) {
  bool reading = digitalRead(pin);

  if (reading != lastReading) {
    lastDebounceTime = now;
    lastReading = reading;
  }

  if ((now - lastDebounceTime) > DEBOUNCE_TIME) {
    if (reading != stableState) {
      stableState = reading;

      if (stableState == LOW) {
        changeBpm(change);
        pressedTime = now;
        lastRepeatTime = now;
        held = true;
      } else {
        held = false;
      }
    }
  }

  if (held && stableState == LOW) {
    if ((now - pressedTime) >= FIRST_REPEAT_DELAY) {
      if ((now - lastRepeatTime) >= REPEAT_INTERVAL) {
        changeBpm(change);
        lastRepeatTime = now;
      }
    }
  }
}

void changeBpm(int amount) {
  bpm += amount;
  bpm = constrain(bpm, MIN_BPM, MAX_BPM);
}

// --------------------------------------------------
// HANDLE METRONOME LED STEPS
// --------------------------------------------------
void handleMetronome(unsigned long now, int ledBrightness) {
  if (now - lastBeatTime >= beatInterval) {
    lastBeatTime += beatInterval;

    previousBeat = currentBeat;
    currentBeat++;

    if (currentBeat >= NUM_BEATS) {
      currentBeat = 0;
    }

    // Previous beat down
    analogWrite(ledPins[previousBeat], 0);

    // Current beat up
    analogWrite(ledPins[currentBeat], ledBrightness);
  }
}

// --------------------------------------------------
// KEEP CURRENT LED MATCHING BRIGHTNESS POT
// --------------------------------------------------
void updateCurrentLedBrightness(int ledBrightness) {
  analogWrite(ledPins[currentBeat], ledBrightness);
}

// --------------------------------------------------
// TURN OFF ALL BEAT LEDS
// --------------------------------------------------
void turnOffAllLeds() {
  for (int i = 0; i < NUM_BEATS; i++) {
    analogWrite(ledPins[i], 0);
  }
}

// --------------------------------------------------
// UPDATE LCD DISPLAY
// --------------------------------------------------
void updateDisplay(unsigned long now) {
  if (now - lastDisplayUpdate < DISPLAY_UPDATE_INTERVAL) {
    return;
  }

  lastDisplayUpdate = now;

  int beatNumber = currentBeat + 1;

  if (bpm != lastDisplayedBPM || beatNumber != lastDisplayedBeat) {
    // Top row: BPM on left, beat on right
    lcd.setCursor(0, 0);
    lcd.print("BPM: ");
    lcd.print(bpm);

    // Clear the spaces between BPM and beat display
    lcd.print("      ");

    // Put beat display at the right side
    lcd.setCursor(13, 0);
    lcd.print(beatNumber);
    lcd.print("/4");

    // Bottom row empty for now
    lcd.setCursor(0, 1);
    lcd.print("                ");

    lastDisplayedBPM = bpm;
    lastDisplayedBeat = beatNumber;
  }
}