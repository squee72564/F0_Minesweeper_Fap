#ifndef MINESWEEPER_HAPTIC_H
#define MINESWEEPER_HAPTIC_H

#include <notification/notification_messages.h>

void mine_sweeper_play_haptic_short(void* context);

void mine_sweeper_play_haptic_double_short(void* context);

void mine_sweeper_play_haptic_lose(void* context);

void mine_sweeper_play_haptic_win(void* context);


#endif
