#include "minesweeper_game_screen.h"

#include <input/input.h>

#include <furi.h>
#include <furi_hal.h>

typedef enum {
    MineSweeperGameScreenTileNone = 0,
    MineSweeperGameScreenTileZero,
    MineSweeperGameScreenTileOne,
    MineSweeperGameScreenTileTwo,
    MineSweeperGameScreenTileThree,
    MineSweeperGameScreenTileFour,
    MineSweeperGameScreenTileFive,
    MineSweeperGameScreenTileSix,
    MineSweeperGameScreenTileSeven,
    MineSweeperGameScreenTileEight,
    MineSweeperGameScreenTileMine,
    MineSweeperGameScreenTileTypeCount,
} MineSweeperGameScreenTileType;

typedef enum {
    MineSweeperGameScreenTileStateFlagged,
    MineSweeperGameScreenTileStateUncleared,
    MineSweeperGameScreenTileStateCleared,
} MineSweeperGameScreenTileState;

struct MineSweeperGameScreen {
    View* view;
    void* context;
};

typedef struct {
    int16_t x_abs, y_abs;
} CurrentPosition;

typedef struct {
    MineSweeperGameScreenTileState tile_state;
    MineSweeperGameScreenTileType tile_type;
} MineSweeperTile;

typedef struct {
    MineSweeperTile board[ MINESWEEPER_BOARD_MAX_TILES ];
    CurrentPosition curr_pos;
    FuriString* str;
    uint8_t board_width, board_height, board_difficulty;
    uint16_t mines_left;
    uint16_t flags_left;
    bool ensure_solvable_board;
} MineSweeperGameScreenModel;

static const float difficulty_multiplier[7] = {
    0.11f,
    0.12f,
    0.13f,
    0.14f,
    0.15f,
    0.16f,
    0.17f,
};

static const int8_t offsets[8][2] = {
    {-1,1},
    {0,1},
    {1,1},
    {1,0},
    {1,-1},
    {0,-1},
    {-1,-1},
    {-1,0},
};

static MineSweeperTile board_t[MINESWEEPER_BOARD_MAX_TILES];

/****************************************************************
 * Function declarations
 *
 * Non public function declarations
 ***************************************************************/

static void setup_board(MineSweeperGameScreen* instance);

static bool check_board_with_verifier(
        MineSweeperTile* board,
        const uint8_t board_width,
        const uint8_t board_height,
        uint16_t total_mines);

static inline void bfs_tile_clear_verifier(
        MineSweeperTile* board,
        const uint8_t board_width,
        const uint8_t board_height,
        const uint16_t x,
        const uint16_t y,
        point_deq_t* edges,
        point_set_t* visited);

static void mine_sweeper_game_screen_set_board_information(
        MineSweeperGameScreen* instance,
        const uint8_t width,
        const uint8_t height,
        const uint8_t difficulty,
        bool is_solvable);

static void mine_sweeper_game_screen_view_enter(void* context);
static void mine_sweeper_game_screen_view_exit(void* context);

static void mine_sweeper_game_screen_view_draw_callback(Canvas* canvas, void* _model);

static bool mine_sweeper_game_screen_view_input_callback(InputEvent* event, void* context);


/**************************************************************
 * Function definitions
 *************************************************************/

static void setup_board(MineSweeperGameScreen* instance) {
    furi_assert(instance);

    uint16_t board_tile_count = 0;
    uint8_t board_width = 0, board_height = 0, board_difficulty = 0;

    with_view_model(
        instance->view,
        MineSweeperGameScreenModel * model,
        {
            board_width = model->board_width;
            board_height = model->board_height;
            board_tile_count =  (model->board_width*model->board_height);
            board_difficulty = model->board_difficulty;
        },
        false
    );

    uint16_t num_mines = board_tile_count * difficulty_multiplier[ board_difficulty ];

    /** We can use a temporary buffer to set the tile types initially
     * and manipulate then save to actual model
     */
    MineSweeperGameScreenTileType tiles[MINESWEEPER_BOARD_MAX_TILES];
    memset(&tiles, MineSweeperGameScreenTileNone, sizeof(tiles));

    for (uint16_t i = 0; i < num_mines; i++) {

        uint16_t rand_pos;
        uint16_t x;
        uint16_t y;
        bool is_invalid_position;
        do {

            rand_pos = furi_hal_random_get() % board_tile_count;
            x = rand_pos / board_width;
            y = rand_pos % board_width;

            is_invalid_position = ((rand_pos == 0)                      ||
                                         (x==0 && y==1)                 ||
                                         (x==1 && y==0)                 ||
                                         rand_pos == board_tile_count-1 ||
                                         (x==0 && y==board_width-1)     ||
                                         (x==board_height-1 && y==0));


        } while (tiles[rand_pos] == MineSweeperGameScreenTileMine || is_invalid_position);

        tiles[rand_pos] = MineSweeperGameScreenTileMine;
    }

    /** All mines are set so we look at each tile for surrounding mines */
    for (uint16_t i = 0; i < board_tile_count; i++) {
        MineSweeperGameScreenTileType tile_type = tiles[i];

        if (tile_type == MineSweeperGameScreenTileMine) {
            continue;
        }

        uint16_t mine_count = 0;

        uint16_t x = i / board_width;
        uint16_t y = i % board_width;

        for (uint8_t j = 0; j < 8; j++) {
            int16_t dx = x + (int16_t)offsets[j][0];
            int16_t dy = y + (int16_t)offsets[j][1];

            if (dx < 0 || dy < 0 || dx >= board_height || dy >= board_width) {
                continue;
            }

            uint16_t pos = dx * board_width + dy;
            if (tiles[pos] == MineSweeperGameScreenTileMine) {
                mine_count++;
            }

        }

        tiles[i] = (MineSweeperGameScreenTileType) mine_count+1;

    }

    with_view_model(
        instance->view,
        MineSweeperGameScreenModel * model,
        {
            for (uint16_t i = 0; i < board_tile_count; i++) {
                model->board[i].tile_type = tiles[i];
                model->board[i].tile_state = MineSweeperGameScreenTileStateUncleared;
            }

            model->mines_left = num_mines;
            model->flags_left = num_mines;
            model->curr_pos.x_abs = 0;
            model->curr_pos.y_abs = 0;
        },
        true
    );

}
static bool check_board_with_verifier(
        MineSweeperTile* board,
        const uint8_t board_width,
        const uint8_t board_height,
        uint16_t total_mines) {

    furi_assert(board);

    point_deq_t deq;
    point_set_t visited;
    
    point_deq_init(deq);
    point_set_init(visited);

    bool is_solvable = false;

    Point_t pos;
    pointobj_init(pos);

    Point start_pos = (Point){.x = 0, .y = 0};
    pointobj_set_point(pos, start_pos);


    bfs_tile_clear_verifier(board, board_width, board_height, 0, 0, &deq, &visited);
    //bfs_tile_clear_verifier(board, board_width, board_height, 0, board_width-1, &deq, &visited);
    //bfs_tile_clear_verifier(board, board_width, board_height, board_height-1, 0, &deq, &visited);
    //bfs_tile_clear_verifier(board, board_width, board_height, board_height-1, board_width-1, &deq, &visited);
                                                             
    while (!is_solvable && point_deq_size(deq) > 0) {

        bool is_stuck = true; 
                              
        uint16_t deq_size = point_deq_size(deq);

        while (deq_size-- > 0) {

            point_deq_pop_front(&pos, deq);
            Point curr_pos = pointobj_get_point(pos);
            uint16_t curr_pos_1d = curr_pos.x * board_width + curr_pos.y;

            MineSweeperTile tile = board[curr_pos_1d];
            uint8_t tile_num = tile.tile_type - 1;
            
            uint8_t num_surrounding_tiles = 0;
            uint8_t num_flagged_tiles = 0;

            for (uint8_t j = 0; j < 8; j++) {
                int16_t dx = curr_pos.x + (int16_t)offsets[j][0];
                int16_t dy = curr_pos.y + (int16_t)offsets[j][1];

                if (dx < 0 || dy < 0 || dx >= board_height || dy >= board_width) {
                    continue;
                }

                uint16_t pos = dx * board_width + dy;
                if (board[pos].tile_state == MineSweeperGameScreenTileStateUncleared) {
                    num_surrounding_tiles++;
                } else if (board[pos].tile_state == MineSweeperGameScreenTileStateFlagged) {
                    num_surrounding_tiles++;
                    num_flagged_tiles++;
                }

            }
            
            if (num_flagged_tiles == tile_num) {

                for (uint8_t j = 0; j < 8; j++) {
                    int16_t dx = curr_pos.x + (int16_t)offsets[j][0];
                    int16_t dy = curr_pos.y + (int16_t)offsets[j][1];

                    if (dx < 0 || dy < 0 || dx >= board_height || dy >= board_width) {
                        continue;
                    }

                    uint16_t pos = dx * board_width + dy;
                    if (board[pos].tile_state == MineSweeperGameScreenTileStateUncleared) {
                        bfs_tile_clear_verifier(board, board_width, board_height, dx, dy, &deq, &visited);
                    }

                }

                is_stuck = false;

            } else if (num_surrounding_tiles == tile_num) {

                for (uint8_t j = 0; j < 8; j++) {
                    int16_t dx = curr_pos.x + (int16_t)offsets[j][0];
                    int16_t dy = curr_pos.y + (int16_t)offsets[j][1];

                    if (dx < 0 || dy < 0 || dx >= board_height || dy >= board_width) {
                        continue;
                    }

                    uint16_t pos = dx * board_width + dy;
                    if (board[pos].tile_state == MineSweeperGameScreenTileStateUncleared) {
                        board[pos].tile_state = MineSweeperGameScreenTileStateFlagged;
                    }
                }

                total_mines -= (num_surrounding_tiles - num_flagged_tiles);

                if (total_mines == 0) is_solvable = true;
                 
                is_stuck = false;

            } else if (num_surrounding_tiles != 0) {

                point_deq_push_back(deq, pos);

            }
        }
        
        if (is_stuck) {
            break;
        }
    }

    point_set_clear(visited);
    point_deq_clear(deq);

    return is_solvable;

}

static inline void bfs_tile_clear_verifier(
        MineSweeperTile* board,
        const uint8_t board_width,
        const uint8_t board_height,
        const uint16_t x,
        const uint16_t y,
        point_deq_t* edges,
        point_set_t* visited) {

    furi_assert(board);
    furi_assert(edges);
    furi_assert(visited);
    
    point_deq_t deq;
    point_set_t set;

    point_deq_init(deq);
    point_set_init(set);

    Point_t pos;
    pointobj_init(pos);

    Point start_pos = (Point){.x = x, .y = y};
    pointobj_set_point(pos, start_pos);

    point_deq_push_back(deq, pos);
    
    while (point_deq_size(deq) > 0) {

        point_deq_pop_front(&pos, deq);
        Point curr_pos = pointobj_get_point(pos);
        uint16_t curr_pos_1d = curr_pos.x * board_width + curr_pos.y;
        
        if (point_set_cget(set, pos) != NULL || point_set_cget(*visited, pos) != NULL) {
        } 
        
        if (board[curr_pos_1d].tile_state == MineSweeperGameScreenTileStateCleared) {
            continue;
        }
        
        board[curr_pos_1d].tile_state = MineSweeperGameScreenTileStateCleared;
        
        point_set_push(set, pos);

        if (board[curr_pos_1d].tile_type != MineSweeperGameScreenTileZero) {

            if (point_set_cget(*visited, pos) == NULL) {
                point_deq_push_back(*edges, pos);
                point_set_push(*visited, pos);
            }

            continue;
        }

        for (uint8_t i = 0; i < 8; i++) {
            int16_t dx = curr_pos.x + (int16_t)offsets[i][0];
            int16_t dy = curr_pos.y + (int16_t)offsets[i][1];

            if (dx < 0 || dy < 0 || dx >= board_height || dy >= board_width) {
                continue;
            }
            
            Point neighbor = (Point) {.x = dx, .y = dy};
            pointobj_set_point(pos, neighbor);

            if (point_set_cget(set, pos) != NULL || point_set_cget(*visited, pos) != NULL) continue;

            point_deq_push_back(deq, pos);
        }
    }

    point_set_clear(set);
    point_deq_clear(deq);
}

static void mine_sweeper_game_screen_set_board_information(
        MineSweeperGameScreen* instance,
        uint8_t width,
        uint8_t height,
        uint8_t difficulty,
        bool is_solvable) {

    furi_assert(instance);

    if (width  > 32) {width = 32;}
    if (width  < 16 ) {width = 16;}
    if (height > 32 ) {height = 32;}
    if (height < 7  ) {height = 7;}
    if (difficulty > 2 ) {difficulty = 2;}
    
    with_view_model(
        instance->view,
        MineSweeperGameScreenModel * model,
        {
            model->board_width = width;
            model->board_height = height;
            model->board_difficulty = difficulty;
            model->ensure_solvable_board = is_solvable;
        },
        true
    );
}


static void mine_sweeper_game_screen_view_enter(void* context) {
    furi_assert(context);
    UNUSED(context);
}

static void mine_sweeper_game_screen_view_exit(void* context) {
    furi_assert(context);
    UNUSED(context);
}

static void mine_sweeper_game_screen_view_draw_callback(Canvas* canvas, void* _model) {
    furi_assert(canvas);
    furi_assert(_model);
    MineSweeperGameScreenModel* model = _model;

    canvas_clear(canvas);

    canvas_set_color(canvas, ColorBlack);
    
    furi_string_printf(
            model->str,
            "PROFILER DONE.\nPRESS BACK");

    canvas_draw_str_aligned(
            canvas,
            20,
            40,
            AlignLeft,
            AlignBottom,
            furi_string_get_cstr(model->str));
}

static bool mine_sweeper_game_screen_view_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    furi_assert(event);

    MineSweeperGameScreen* instance = context;
    UNUSED(instance);
    return false;
}

MineSweeperGameScreen* mine_sweeper_game_screen_alloc_and_profile(uint8_t width, uint8_t height, uint8_t difficulty, bool ensure_solvable, uint16_t iter, Stream* fs) {
    MineSweeperGameScreen* mine_sweeper_game_screen = (MineSweeperGameScreen*)malloc(sizeof(MineSweeperGameScreen));
    
    mine_sweeper_game_screen->view = view_alloc();

    view_set_context(mine_sweeper_game_screen->view, mine_sweeper_game_screen);
    view_allocate_model(mine_sweeper_game_screen->view, ViewModelTypeLocking, sizeof(MineSweeperGameScreenModel));

    view_set_draw_callback(mine_sweeper_game_screen->view, mine_sweeper_game_screen_view_draw_callback);
    view_set_input_callback(mine_sweeper_game_screen->view, mine_sweeper_game_screen_view_input_callback);
    
    view_set_enter_callback(mine_sweeper_game_screen->view, mine_sweeper_game_screen_view_enter);
    view_set_exit_callback(mine_sweeper_game_screen->view, mine_sweeper_game_screen_view_exit);

    with_view_model(
        mine_sweeper_game_screen->view,
        MineSweeperGameScreenModel * model,
        {
            model->str = furi_string_alloc();
        },
        true
    );

    mine_sweeper_game_screen_set_board_information(mine_sweeper_game_screen, width, height, difficulty, ensure_solvable);

    size_t memsz = sizeof(MineSweeperTile) * MINESWEEPER_BOARD_MAX_TILES;
    uint16_t total_iter = iter;
    uint16_t successes = 0;

    uint32_t start_tick = furi_get_tick();

    do {
        setup_board(mine_sweeper_game_screen);

        uint16_t num_mines = 1;

        uint16_t board_width = 16; 
        uint16_t board_height = 7; 

        with_view_model(
            mine_sweeper_game_screen->view,
            MineSweeperGameScreenModel * model,
            {
                num_mines = model->mines_left;
                board_width = model->board_width;
                board_height = model->board_height;

                memset(board_t, 0, memsz);
                memcpy(board_t, model->board, sizeof(MineSweeperTile) * (board_width * board_height));
            },
            true
        );
    
        if (check_board_with_verifier(board_t, board_width, board_height, num_mines)) successes++;

        iter--;

    } while (iter >= 1);

    uint32_t ticks_elapsed = furi_get_tick() - start_tick;
    double sec = (double)ticks_elapsed / (double)furi_kernel_get_tick_frequency();
    double milliseconds = sec * 1000.0L;
    append_to_profiling_file(
            fs,
            "%d,%.05f,%.05f,%.05f\n",
            total_iter,
            (double)successes/(double)total_iter,
            milliseconds,
            milliseconds/(double)total_iter);


    close_profiling_file(fs);

    return mine_sweeper_game_screen;
}

void mine_sweeper_game_screen_free(MineSweeperGameScreen* instance) {
    furi_assert(instance);

    with_view_model(
        instance->view,
        MineSweeperGameScreenModel * model,
        {
            furi_string_free(model->str);
        },
        false
    );

    view_free(instance->view);
    free(instance);
}

View* mine_sweeper_game_screen_get_view(MineSweeperGameScreen* instance) {
    furi_assert(instance);
    return instance->view;
}

void mine_sweeper_game_screen_set_context(MineSweeperGameScreen* instance, void* context) {
    furi_assert(instance);
    instance->context = context;
}
