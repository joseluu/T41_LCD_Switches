## 0. context
This program is supposed to display a 6x3 button array on an ESP32
connected to a display. The display is rotated so that the 6 lines
of 3 buttons are stacked vertically using the long side of the display.
This program does not work, however a very similar program using only 2 buttons
is in a working state in directory ~/hobby_w/ESP32/lvgl_button_test5
rewrite this program so that it works with the working program as a model.
remove all tests in the main.cpp
use library lvgl 8.3.11
The hardware type to be used is "ESP32 Dev Module"
use the arduino-cli for compilation, to use it: source ~/hobby_w/arduino-cli-setup.sh  then attempt compilation with core 3.3.6 and the Arduino_GFX directly from github
use the lv_conf.h from here https://raw.githubusercontent.com/joseluu/Demo-CYD-3.2inch-ESP32-2432S032/refs/heads/main/LVGL%20configuration%20replacement%20file/lv_conf.h
The hardware type to be used is "ESP32 Dev Module"
write your report in docs/done_changes.md write the finish time
use the shell command date to get the actual time
## 1. remove extras
Remove the initial 3 full colored screens that appear when starting.
Disable via macro the FPS and CPU stats in the lower right corner
## 2. improve button background colors
The inactive color should pale white with a light redish yellow glow
like an undervoltaged incandescent bulb. Pressed color should be
white like a normally lighted incandescent bulb. Active color
should be orange like a neon indicator.
## 3. improve button bitmap
Find or generate a button bitmap with some transparency to look like
a plastic translucent push button. If python is needed source
~/hobby_w/jupyter_nbs/setup.sh to have it in the path