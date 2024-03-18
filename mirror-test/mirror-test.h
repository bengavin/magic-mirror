#ifndef MIRROR_H
#define MIRROR_H


enum mirror_mode_t {
  MODE_OFF = 0,
  MODE_GOOD = 1,
  MODE_BAD = 2
};

enum wave_state_t {
  WAVE_STEADY,
  WAVE_RISING,
  WAVE_FALLING
};

#endif
