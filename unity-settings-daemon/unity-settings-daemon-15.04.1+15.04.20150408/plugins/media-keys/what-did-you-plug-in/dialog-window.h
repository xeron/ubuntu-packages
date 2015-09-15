#ifndef __DIALOG_WINDOW_H__
#define __DIALOG_WINDOW_H__

#include <stdbool.h>

#define WDYPI_DIALOG_CANCELLED 0
#define WDYPI_DIALOG_HEADPHONES 1
#define WDYPI_DIALOG_HEADSET 2
#define WDYPI_DIALOG_MICROPHONE 3
#define WDYPI_DIALOG_SOUND_SETTINGS 4

typedef void (*wdypi_dialog_cb)(int response, void *userdata);

void wdypi_dialog_run(bool show_headset, bool show_mic, wdypi_dialog_cb cb, void *cb_userdata);

void wdypi_dialog_kill();

#endif
