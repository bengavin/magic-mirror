#include <ADebouncer.h>
#include <Adafruit_NeoPixel.h>

#include "mirror-test.h"
#undef SERIAL_DEBUG

#define GOODMODEPIN 4
#define BADMODEPIN 5
#define FRONTPIN 7
#define FRONTCOUNT 38
#define BACKPIN 8
#define BACKCOUNT 153

#define FADESPEED 25     // make this higher to slow down
#define WAVESPEED 250
#define SHIMMERSPEED 250
#define SHIMMERSINESPEED 50
#define SHIMMERBADSINESPEED 50
#define SHIMMERFRAMES 30
#define SHIMMERSINEFRAMES 60
#define OFFSPEED 250

ADebouncer goodBouncer;
ADebouncer badBouncer;
Adafruit_NeoPixel frontpixels(FRONTCOUNT, FRONTPIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel backpixels(BACKCOUNT, BACKPIN, NEO_GRB + NEO_KHZ800);

#define NUMBER_OF_BUCKETS 7
#define FRONT_LEDS_PER_BUCKET ((FRONTCOUNT)/(NUMBER_OF_BUCKETS))
#define BACK_LEDS_PER_BUCKET ((BACKCOUNT)/(NUMBER_OF_BUCKETS))

uint8_t front_led_bucket[FRONTCOUNT];
uint8_t back_led_bucket[BACKCOUNT];

void setup() {
  #ifdef SERIAL_DEBUG
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  delay(1000);
  Serial.println(F("Starting up..."));
  #endif

  //pinMode(REDPIN, OUTPUT);
  //pinMode(GREENPIN, OUTPUT);
  //pinMode(BLUEPIN, OUTPUT);
  
  pinMode(GOODMODEPIN, INPUT);
  goodBouncer.mode(DELAYED, 500, false);

  pinMode(BADMODEPIN, INPUT);
  badBouncer.mode(DELAYED, 500, false);

  //analogWrite(REDPIN, 0);
  //analogWrite(BLUEPIN, 0);
  //analogWrite(GREENPIN, 0);

  frontpixels.begin();
  backpixels.begin();
  
  frontpixels.show();
  backpixels.show();

  frontpixels.setBrightness(200);
  backpixels.setBrightness(200);

  // Initialize buckets
  uint8_t bbuckets[NUMBER_OF_BUCKETS] = { 
    BACK_LEDS_PER_BUCKET + 1, // 0s
    BACK_LEDS_PER_BUCKET + 1, // 1s
    BACK_LEDS_PER_BUCKET + 1, // 2s
    BACK_LEDS_PER_BUCKET + 1, // 3s
    BACK_LEDS_PER_BUCKET + 1, // 4s
    BACK_LEDS_PER_BUCKET + 1, // 5s
    BACK_LEDS_PER_BUCKET + 1
    };

  #ifdef SERIAL_DEBUG
  Serial.println("Assigning Back Buckets...");
  #endif
  for(int i = 0; i < (BACKCOUNT); i++) {
    uint8_t bucket = (uint8_t)random(0, NUMBER_OF_BUCKETS);
    while (bbuckets[bucket] <= 0) { bucket = (uint8_t)random(0, NUMBER_OF_BUCKETS); }
    back_led_bucket[i] = bucket; 
    bbuckets[bucket]--;
  }

  #ifdef SERIAL_DEBUG
  Serial.println("Assigning Front Buckets...");
  #endif

  uint8_t fbuckets[NUMBER_OF_BUCKETS] = { 
    FRONT_LEDS_PER_BUCKET + 1, // 0s
    FRONT_LEDS_PER_BUCKET + 1, // 1s
    FRONT_LEDS_PER_BUCKET + 1, // 2s
    FRONT_LEDS_PER_BUCKET + 1, // 3s
    FRONT_LEDS_PER_BUCKET + 1, // 4s
    FRONT_LEDS_PER_BUCKET + 1, // 5s
    FRONT_LEDS_PER_BUCKET + 1
    };
  for(int i = 0; i < (FRONTCOUNT); i++) {
    uint8_t bucket = (uint8_t)random(0, NUMBER_OF_BUCKETS);
    while (fbuckets[bucket] <= 0) { bucket = (uint8_t)random(0, NUMBER_OF_BUCKETS); }
    front_led_bucket[i] = bucket; 
    fbuckets[bucket]--;
  }

  #ifdef SERIAL_DEBUG
  Serial.println("Buckets Assigned...");
  #endif
}

mirror_mode_t lastMode = MODE_OFF;

mirror_mode_t get_current_mode() {
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

wave_state_t waveState = WAVE_STEADY;
int rdest, rstep;
int gdest, gstep;
int bdest, bstep;

// GOOD MODE:
// -- Fade in to purple [slowish]
// -- Shimmer
// -- WAVE_RISING - determine next dest color, switch to WAVE_FALLING immediately
// -- WAVE_FALLING - 'fall' towards that color, switch to WAVE_RISING when color reached
uint8_t frameSineOffset = 0;
uint64_t nextBrightnessMillis;
uint32_t sineFrameColor;
void drawColor(uint32_t color) {
  sineFrameColor = color;
}

void drawBrightness(uint64_t currentMillis) {
  uint32_t pixelColor = sineFrameColor;
  uint8_t b = pixelColor & 0xFF;
  uint8_t g = (pixelColor & 0xFF00) >> 8;
  uint8_t r = (pixelColor & 0xFF0000) >> 16;

  #ifdef SERIAL_DEBUG
  Serial.print(F("Drawing Brightness at Color: "));
  Serial.print(r);
  Serial.print(F(","));
  Serial.print(g);
  Serial.print(F(","));
  Serial.println(b);
  #endif

  for (int i = 0; i < (FRONTCOUNT); i++) {
    // which 'timeslot' are you in [1/3 of the pixels follow together]
    int slot = front_led_bucket[i]; //i % 7;
    uint8_t sineOffset = frameSineOffset;
    switch (slot)
    {
      case 6: sineOffset += 50;
      case 5: sineOffset += 50;
      case 4: sineOffset += 50;
      case 3: sineOffset += 50;
      case 2: sineOffset += 50;
      case 1: sineOffset += 50; break;
    }

    //if (slot == 1) { sineOffset += 60; } 
    //else if (slot == 2) { sineOffset += 120; } 
    //else if (slot == 3) { sineOffset += 180; }

    // Only want to use the 'upper half' of the sine wave
    float sineVal = frontpixels.sine8(sineOffset) / 255.0; //max(0, (int8_t)frontpixels.sine8(sineOffset)) / 128.0;
    //Serial.print(F("Sine Val"));
    //Serial.println(sineVal);
    //uint32_t gammaR = frontpixels.gamma32(pixelColor * sineVal / 255.0);

    //if (i < (FRONTCOUNT))
    //{
      frontpixels.setPixelColor(
        i, 
        frontpixels.gamma8((uint8_t)(r * sineVal)),
        frontpixels.gamma8((uint8_t)(g * sineVal)),
        frontpixels.gamma8((uint8_t)(b * sineVal))
        );
      /*frontpixels.setPixelColor(
        i, 
        (uint8_t)(r * sineVal),
        (uint8_t)(g * sineVal),
        (uint8_t)(b * sineVal)
        );*/
    //}

    //if (i < (BACKCOUNT))
    //{
    //  backpixels.setPixelColor(
    //    i, 
    //    frontpixels.gamma8((uint8_t)(r * sineVal)),
    //    frontpixels.gamma8((uint8_t)(g * sineVal)),
    //    frontpixels.gamma8((uint8_t)(b * sineVal))
    //    );
    //  /*backpixels.setPixelColor(
    //    i, 
    //    (uint8_t)(r * sineVal),
    //    (uint8_t)(g * sineVal),
    //    (uint8_t)(b * sineVal)
    //    );*/
    //}
  }

  for (int i = 0; i < (BACKCOUNT); i++) {
    // which 'timeslot' are you in [1/3 of the pixels follow together]
    int slot = back_led_bucket[i]; //i % 7;
    uint8_t sineOffset = frameSineOffset;
    switch (slot)
    {
      case 6: sineOffset += 50;
      case 5: sineOffset += 50;
      case 4: sineOffset += 50;
      case 3: sineOffset += 50;
      case 2: sineOffset += 50;
      case 1: sineOffset += 50; break;
    }

    //if (slot == 1) { sineOffset += 60; } 
    //else if (slot == 2) { sineOffset += 120; } 
    //else if (slot == 3) { sineOffset += 180; }

    // Only want to use the 'upper half' of the sine wave
    float sineVal = frontpixels.sine8(sineOffset) / 255.0; //max(0, (int8_t)frontpixels.sine8(sineOffset)) / 128.0;
    sineVal = max(0.5, sineVal);

    //Serial.print(F("Sine Val"));
    //Serial.println(sineVal);
    //uint32_t gammaR = frontpixels.gamma32(pixelColor * sineVal / 255.0);

      backpixels.setPixelColor(
        i, 
        frontpixels.gamma8((uint8_t)(r * sineVal)),
        frontpixels.gamma8((uint8_t)(g * sineVal)),
        frontpixels.gamma8((uint8_t)(b * sineVal))
        );
      /*backpixels.setPixelColor(
        i, 
        (uint8_t)(r * sineVal),
        (uint8_t)(g * sineVal),
        (uint8_t)(b * sineVal)
        );*/
  }

  // setup the next frame
  frameSineOffset += (uint8_t)(256 / (SHIMMERSINEFRAMES));
  nextBrightnessMillis = currentMillis + SHIMMERSINESPEED;

  //frontpixels.fill(pixelColor);
  frontpixels.show();
  //backpixels.clear();
  //backpixels.fill(pixelColor);
  backpixels.show();
}

void good_mode_step(unsigned long currentMillis, bool start) {
  if (start) {
    // baseline our mode variables
    red = 0;
    blue = 0;
    green = 0;
    waveState = WAVE_STEADY;
    frameSineOffset = 0;
  }

  if (waveState == WAVE_STEADY && blue < 255) {
    // fade in purple
    red++;
    red = min(200, red);
    blue+=5;
  }
  else if (waveState == WAVE_STEADY) {
    // switch into wave rising mode
    waveState = WAVE_RISING;
    #ifdef SERIAL_DEBUG
    Serial.println(F("FADE complete, switching to shimmer"));
    #endif
  }

  // We're in 'shimmer' mode, so just cycle through random purple/blue colors
  if (waveState == WAVE_RISING) {
    rdest = 0; //(int)random(25, 200);
    bdest = 255; //(int)random(75, 255);
    gdest = 0; //(int)random(0, 30);

    // Never want green larger than blue
    gdest = min(gdest, bdest);

    rstep = (rdest - red) / SHIMMERFRAMES;
    bstep = (bdest - blue) / SHIMMERFRAMES;
    gstep = (gdest - green) / SHIMMERFRAMES;

    if (rstep == 0 && rdest != red) { rstep = (rdest > red ? (1) : (-1)); }
    if (bstep == 0 && bdest != blue) { bstep = (bdest > blue ? (1) : (-1)); }
    if (gstep == 0 && gdest != green) { gstep = (gdest > green ? (1) : (-1)); }
    #ifdef SERIAL_DEBUG
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
    #endif

    waveState = WAVE_FALLING;
  }

  if (waveState == WAVE_FALLING) {
    if (abs(rdest - red) > abs(rstep)) { red += rstep; } else { red = rdest; }
    if (abs(bdest - blue) > abs(bstep)) { blue += bstep; } else { blue = bdest; }
    if (abs(gdest - green) > abs(green)) { green += gstep; } else { green = gdest; }
  }

  if (red == rdest && blue == bdest && green == gdest) {
    waveState = WAVE_RISING;
    #ifdef SERIAL_DEBUG
    Serial.println(F("Destination reached, switching to RISING"));
    #endif
  }

  /*if (waveState == WAVE_RISING) { red++; blue+=2; }
  else if (waveState == WAVE_FALLING) { red--; blue-=1; }

  if (red >= 255) { waveState = waveState == WAVE_RISING ? WAVE_FALLING : WAVE_RISING; }
  else if (waveState == WAVE_FALLING && red <= 20) { waveState = WAVE_RISING; }
  else if (waveState == WAVE_RISING && red >= 128) { waveState = WAVE_FALLING; }
  */

  // Cap at 255
  red = 0; //max(0, min(255, red));
  blue = max(0, min(255, blue));
  green = 0; //max(0, min(255, green));
  
  //analogWrite(REDPIN, red);
  //analogWrite(BLUEPIN, blue);
  //analogWrite(GREENPIN, green);

  // We want a 'sine wave' of color to move through the strip, so take
  // this base color, and then walk the strip, each pixel should complete
  // a single sine-wave pulse every SHIMMERSPEED millis()
  uint32_t pixelColor = frontpixels.Color(red, green, blue, 0); 

  drawColor(pixelColor);

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

  int increment = (waveState == WAVE_STEADY ? (5) : ((int)random(0, 20)));

  if (waveState == WAVE_STEADY || waveState == WAVE_RISING) { red += increment; }
  //else { red -= increment * 3; }

  if (waveState == WAVE_STEADY && red >= 250) 
  { 
    #ifdef SERIAL_DEBUG
    Serial.println(F("FADE complete, entering falling wave")); 
    #endif
    waveState = WAVE_FALLING; 
    gdest = 0; //(uint8_t)random(0,60); 
  }
  /*else if (waveState == WAVE_FALLING && red <= 90) {
    #ifdef SERIAL_DEBUG
    Serial.println(F("FALLING complete, entering wave RISE")); 
    #endif
    waveState = WAVE_RISING; 
    gdest = (uint8_t)random(0,60); 
  }
  else if (waveState == WAVE_RISING && red >= 250) { 
    #ifdef SERIAL_DEBUG
    Serial.println(F("RISING complete, entering wave FALL")); 
    #endif
    waveState = WAVE_FALLING; 
    gdest = (uint8_t)random(0,60); 
  }*/

  // Cap at 255
  red = min(255, red);
  blue = min(255, blue);
  green = min(255, min(red / 2, gdest));
  
  //analogWrite(REDPIN, red);
  //analogWrite(GREENPIN, green);
  //analogWrite(BLUEPIN, blue);

  uint32_t pixelColor = frontpixels.Color(red, green, blue, 0); 
  drawColor(pixelColor);
  //frontpixels.fill(pixelColor);
  //frontpixels.show();
  //backpixels.fill(pixelColor);
  //backpixels.show();

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
    //analogWrite(REDPIN, 0);
    //analogWrite(GREENPIN, 0);
    //analogWrite(BLUEPIN, 0);
    frontpixels.clear();
    frontpixels.show();
    backpixels.clear();
    backpixels.show();
  }

  nextMillis = currentMillis + OFFSPEED;
}

void loop() {
  bool forceUpdate = false;
  mirror_mode_t mode = get_current_mode();
  if (mode != lastMode) {
    #ifdef SERIAL_DEBUG
    Serial.println("Switching Mode...");
    #endif
    forceUpdate = true;
    lastMode = mode;
  }

  // check to see if it's time to blink the LED; that is, if the difference
  // between the current time and last time you blinked the LED is bigger than
  // the interval at which you want to blink the LED.
  unsigned long currentMillis = millis();
  if (!forceUpdate && mode != MODE_OFF && currentMillis >= nextBrightnessMillis) { drawBrightness(currentMillis); }
  if (!forceUpdate && currentMillis < nextMillis) { return; } // no more update required

  switch (mode) {
    case MODE_GOOD: good_mode_step(currentMillis, forceUpdate); break;
    case MODE_BAD: bad_mode_step(currentMillis, forceUpdate); break;
    case MODE_OFF: off_mode_step(currentMillis, forceUpdate); break;
  }
}
