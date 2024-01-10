#include "../minesweeper.h"
#include "../views/minesweeper_game_screen.h"

#include <input/input.h>

void minesweeper_scene_game_screen_on_enter(void* context) {
    furi_assert(context);
    MineSweeperApp* app = context;
    
    furi_assert(app->game_screen);

    mine_sweeper_game_screen_set_context(app->game_screen, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, MineSweeperGameScreenView);
}

bool minesweeper_scene_game_screen_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);

    MineSweeperApp* app = context;
    bool consumed = false;
    UNUSED(app);
    UNUSED(event);


    return consumed;
}

void minesweeper_scene_game_screen_on_exit(void* context) {
    furi_assert(context);
    MineSweeperApp* app = context;
    
    // Do not call reset function for mine sweeper module
    //unless you want to reset the state of the board
    UNUSED(app);
}
