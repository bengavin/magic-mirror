#include <ADebouncer.h>
#include <Adafruit_NeoPixel.h>

#include "mirror-test.h"

#define REDPIN 11
#define GREENPIN 9
#define BLUEPIN 10
#define GOODMODEPIN 4
#define BADMODEPIN 5
#define NEOPIXELPIN 7

#define FADESPEED 50     // make this higher to slow down
#define WAVESPEED 150
#define SHIMMERSPEED 50
#define SHIMMERFRAMES 15
#define OFFSPEED 250

ADebouncer goodBouncer;
ADebouncer badBouncer;
Adafruit_NeoPixel pixels(16, NEOPIXELPIN, NEO_GRBW + NEO_KHZ800);

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  delay(2000);
  Serial.println(F("Starting up..."));

  pinMode(REDPIN, OUTPUT);
  pinMode(GREENPIN, OUTPUT);
  pinMode(BLUEPIN, OUTPUT);
  
  pinMode(GOODMODEPIN, INPUT);
  goodBouncer.mode(DELAYED, 500, false);

  pinMode(BADMODEPIN, INPUT);
  badBouncer.mode(DELAYED, 500, false);

  analogWrite(REDPIN, 0);
  analogWrite(BLUEPIN, 0);
  analogWrite(GREENPIN, 0);

  pixels.begin();
  pixels.show();
}

mode_t lastMode = MODE_OFF;

mode_t get_current_mode() {
  if (goodBouncer.debounce(digitalRead(GOODMODEPIN))) { return MODE_GOOD; }
  if (badBouncer.debounce(digitalRead(BADMODEPIN))) { return MODE_BAD; }
  return MODE_OFF;
}

// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long nextMillis = 0; // when should we do the next update

int red = 0;
int blue = 0;
int green = 0;

wave_t waveState = WAVE_STEADY;
int rdest, rstep;
int gdest, gstep;
int bdest, bstep;

// GOOD MODE:
// -- Fade in to purple [slowish]
// -- Shimmer
// -- WAVE_RISING - determine next dest color, switch to WAVE_FALLING immediately
// -- WAVE_FALLING - 'fall' towards that color, switch to WAVE_RISING when color reached
void good_mode_step(unsigned long currentMillis, bool start) {
  if (start) {
    // baseline our mode variables
    red = 0;
    blue = 0;
    green = 0;
    waveState = WAVE_STEADY;
  }

  if (waveState == WAVE_STEADY && blue < 125) {
    // fade in purple
    red++;
    red = min(200, red);
    blue++;
  }
  else if (waveState == WAVE_STEADY) {
    // switch into wave rising mode
    waveState = WAVE_RISING;
    Serial.println(F("FADE complete, switching to shimmer"));
  }

  // We're in 'shimmer' mode, so just cycle through random purple/blue colors
  if (waveState == WAVE_RISING) {
    rdest = (int)random(0, 200);
    bdest = (int)random(20, 255);
    gdest = (int)random(0, 30);

    // Never want green larger than blue
    gdest = min(gdest, bdest);

    rstep = (rdest - red) / SHIMMERFRAMES;
    bstep = (bdest - blue) / SHIMMERFRAMES;
    gstep = (gdest - green) / SHIMMERFRAMES;

    if (rstep == 0 && rdest != red) { rstep = (rdest > red ? (1) : (-1)); }
    if (bstep == 0 && bdest != blue) { bstep = (bdest > blue ? (1) : (-1)); }
    if (gstep == 0 && gdest != green) { gstep = (gdest > green ? (1) : (-1)); }
    Serial.print(F("New dest color (r g b, rd gd bd, rs gs bs): "));
    Serial.print(red);Serial.print(F(" "));
    Serial.print(green);Serial.print(F(" "));
    Serial.print(blue);Serial.print(F(", "));
    Serial.print(rdest);Serial.print(F(" "));
    Serial.print(gdest);Serial.print(F(" "));
    Serial.print(bdest);Serial.print(F(", "));
    Serial.print(rstep);Serial.print(F(" "));
    Serial.print(gstep);Serial.print(F(" "));
    Serial.println(bstep);

    waveState = WAVE_FALLING;
  }

  if (waveState == WAVE_FALLING) {
    if (abs(rdest - red) > abs(rstep)) { red += rstep; } else { red = rdest; }
    if (abs(bdest - blue) > abs(bstep)) { blue += bstep; } else { blue = bdest; }
    if (abs(gdest - green) > abs(green)) { green += gstep; } else { green = gdest; }
  }

  if (red == rdest && blue == bdest && green == gdest) {
    waveState = WAVE_RISING;
    Serial.println(F("Destination reached, switching to RISING"));
  }

  /*if (waveState == WAVE_RISING) { red++; blue+=2; }
  else if (waveState == WAVE_FALLING) { red--; blue-=1; }

  if (red >= 255) { waveState = waveState == WAVE_RISING ? WAVE_FALLING : WAVE_RISING; }
  else if (waveState == WAVE_FALLING && red <= 20) { waveState = WAVE_RISING; }
  else if (waveState == WAVE_RISING && red >= 128) { waveState = WAVE_FALLING; }
  */

  // Cap at 255
  red = max(0, min(255, red));
  blue = max(0, min(255, blue));
  green = max(0, min(255, green));
  
  analogWrite(REDPIN, red);
  analogWrite(BLUEPIN, blue);
  analogWrite(GREENPIN, green);

  uint32_t pixelColor = pixels.Color(red, green, blue, 0); 
  pixels.fill(pixelColor);
  pixels.show();

  if (waveState == WAVE_STEADY) {
    nextMillis = currentMillis + FADESPEED;
  }
  else {
    nextMillis = currentMillis + SHIMMERSPEED;
  }
}

int badSteps = 0;
void bad_mode_step(unsigned long currentMillis, bool start) {
  if (start) {
    red = 0;
    blue = 0;
    green = 0;
    waveState = WAVE_STEADY;
  }

  int increment = (waveState == WAVE_STEADY ? (1) : ((int)random(0, 20)));

  if (waveState == WAVE_STEADY || waveState == WAVE_RISING) { red += increment; }
  else { red -= increment; }

  if (waveState == WAVE_STEADY && red >= 200) { Serial.println(F("FADE complete, entering falling wave")); waveState = WAVE_FALLING; }
  else if (waveState == WAVE_FALLING && red <= 20) { Serial.println(F("FALLING complete, entering wave RISE")); waveState = WAVE_RISING; }
  else if (waveState == WAVE_RISING && red >= 128) { Serial.println(F("RISING complete, entering wave FALL")); waveState = WAVE_FALLING; }

  // Cap at 255
  red = min(255, red);
  blue = min(255, blue);
  green = min(255, green);
  
  analogWrite(REDPIN, red);
  analogWrite(GREENPIN, green);
  analogWrite(BLUEPIN, blue);

  uint32_t pixelColor = pixels.Color(red, green, blue, 0); 
  pixels.fill(pixelColor);
  pixels.show();
  
  if (waveState == WAVE_STEADY) {
    nextMillis = currentMillis + FADESPEED;
  }
  else {
    nextMillis = currentMillis + WAVESPEED;
  }
}

void off_mode_step(unsigned long currentMillis, bool start) {
  if (start)
  {
    analogWrite(REDPIN, 0);
    analogWrite(GREENPIN, 0);
    analogWrite(BLUEPIN, 0);
    pixels.clear();
    pixels.show();
  }

  nextMillis = currentMillis + OFFSPEED;
}

void loop() {
  bool forceUpdate = false;
  mode_t mode = get_current_mode();
  if (mode != lastMode) {
    Serial.println("Switching Mode...");
    forceUpdate = true;
    lastMode = mode;
  }

  // check to see if it's time to blink the LED; that is, if the difference
  // between the current time and last time you blinked the LED is bigger than
  // the interval at which you want to blink the LED.
  unsigned long currentMillis = millis();
  if (!forceUpdate && currentMillis < nextMillis) { return; } // no more update required

  switch (mode) {
    case MODE_GOOD: good_mode_step(currentMillis, forceUpdate); break;
    case MODE_BAD: bad_mode_step(currentMillis, forceUpdate); break;
    case MODE_OFF: off_mode_step(currentMillis, forceUpdate); break;
  }
}
