#ifndef MINESWEEPER_H
#define MINESWEEPER_H

#include <string.h> // memset

#include <furi.h>
#include <furi_hal.h>

#include <notification/notification_messages.h>
#include <toolbox/stream/file_stream.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>

#include "views/minesweeper_game_screen.h"
#include "scenes/minesweeper_scene.h"

#define TAG "Mine Sweeper Application"

// Main MineSweeperApp
typedef struct MineSweeperApp {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;

    MineSweeperGameScreen* game_screen;
} MineSweeperApp;

// View Id Enumeration
typedef enum {
    MineSweeperGameScreenView,
    MineSweeperViewCount,
} MineSweeperView;

// Enumerations for hardware states
// Will be used in later implementation
typedef enum {
    MineSweeperHapticOff,
    MineSweeperHapticOn,
} MineSweeperHapticState;

typedef enum {
    MineSweeperSpeakerOff,
    MineSweeperSpeakerOn,
} MineSweeperSpeakerState;

typedef enum {
    MineSweeperLedOff,
    MineSweeperLedOn,
} MineSweeperLedState;

#endif
