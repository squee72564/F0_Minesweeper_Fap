#ifndef MINESWEEPER_LED_H
#define MINESWEEPER_LED_H
#include <notification/notification_messages.h>

void mine_sweeper_led_set_rgb(void* context, NotificationApp* na, int red, int green, int blue);

void mine_sweeper_led_reset(void* context, NotificationApp* na);

#endif
