#ifndef TOUCH_H
#define TOUCH_H

// #define TOUCH_XPT2046
 #define TOUCH_GT911
// #define TOUCH_FT6X36


extern int16_t touch_last_x;
extern int16_t touch_last_y;
bool touch_has_signal();
bool touch_touched();
bool touch_released();
void touch_init();

#endif
