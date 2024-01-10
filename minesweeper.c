#include "minesweeper.h"

static bool minesweeper_custom_event_callback(void* context, uint32_t custom_event) {
    furi_assert(context);
    MineSweeperApp* app = (MineSweeperApp*)context;
    return scene_manager_handle_custom_event(app->scene_manager, custom_event);
}

static bool minesweeper_navigation_event_callback(void* context) {
    furi_assert(context);
    MineSweeperApp* app = (MineSweeperApp*)context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void minesweeper_tick_event_callback(void* context) {
    furi_assert(context);
    MineSweeperApp* app = (MineSweeperApp*)context;
    return scene_manager_handle_tick_event(app->scene_manager);
}

static MineSweeperApp* app_alloc(uint16_t w, uint16_t h, uint8_t d, bool solvable, uint16_t iter, Stream* fs) { 
    MineSweeperApp* app = (MineSweeperApp*)malloc(sizeof(MineSweeperApp));
    
    // NotificationApp Service
    NotificationApp* notification_app = furi_record_open(RECORD_NOTIFICATION);

    // Turn backlight on when app starts
    notification_message(notification_app, &sequence_display_backlight_on);

    furi_record_close(RECORD_NOTIFICATION);

    // Alloc Scene Manager and set handlers for on_enter, on_event, on_exit 
    app->scene_manager = scene_manager_alloc(&minesweeper_scene_handlers, app);
    
    // Alloc View Dispatcher and enable queue
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    // Set View Dispatcher event callback context and callbacks
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, minesweeper_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, minesweeper_navigation_event_callback);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, minesweeper_tick_event_callback, 500);


    app->game_screen = mine_sweeper_game_screen_alloc_and_profile(
            w,
            h,
            d,
            solvable,
            iter,
            fs);

    if (app->game_screen == NULL) {
        // Free View Dispatcher and Scene Manager
        scene_manager_free(app->scene_manager);
        view_dispatcher_free(app->view_dispatcher);

        return NULL;

    }

    view_dispatcher_add_view(
        app->view_dispatcher,
        MineSweeperGameScreenView,
        mine_sweeper_game_screen_get_view(app->game_screen));

    Gui* gui = furi_record_open(RECORD_GUI);

    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    
    furi_record_close(RECORD_GUI);

    return app;
}

static void app_free(MineSweeperApp* app) {
    furi_assert(app);
    
    // Remove each view from View Dispatcher
    for (MineSweeperView minesweeper_view = (MineSweeperView)0; minesweeper_view < MineSweeperViewCount; minesweeper_view++) {

        view_dispatcher_remove_view(app->view_dispatcher, minesweeper_view);
    }

    // Free View Dispatcher and Scene Manager
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    // Free views
    mine_sweeper_game_screen_free(app->game_screen);  

    // Free app structure
    free(app);

}

int32_t minesweeper_app(void* p) {
    UNUSED(p);

    uint8_t width = 32;
    uint8_t height = 32;
    uint8_t difficulty = 0;
    bool solvable = true;
    uint16_t iter = 30000;

    Stream* fs = open_profiling_file();

    if (fs == NULL) {

        return 1;
    }

    append_to_profiling_file(
            fs,
            "total_iter,s_rate,run_time(ms),avs(ms)\n");

    MineSweeperApp* app = app_alloc(width, height, difficulty, solvable, iter, fs);

    if (app == NULL) {
        return 1;
    }


    // This will be the initial scene on app startup
    scene_manager_next_scene(app->scene_manager, MineSweeperSceneGameScreen);

    view_dispatcher_run(app->view_dispatcher);

    app_free(app);
    //FURI_LOG_I(TAG, "Mine Sweeper app freed");

    return 0;
}
