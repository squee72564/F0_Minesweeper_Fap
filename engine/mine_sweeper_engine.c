#include "mine_sweeper_engine.h"
#include "mstarlib_helpers.h"

#include <furi_hal.h>
#include <stdint.h>

static const float difficulty_multiplier[3] = {
    0.15f,
    0.17f,
    0.19f,
};

// Bias Free Uniform Random Sample In Range [0, upper_exclusion]
static uint16_t random_uniform_u16(uint16_t upper_exclusion) {
    if (upper_exclusion <= 1u) return 0u;

    const uint32_t range = (uint32_t)upper_exclusion;

    // Compute largest multiple of range that fits in uint32_t
    // Values >= limit are rejected to avoid modulo bias
    const uint32_t limit = UINT32_MAX - (UINT32_MAX % range);

    uint32_t r = 0;
    do {
        r = furi_hal_random_get();
    } while (r >= limit);

    return (uint16_t)(r % range);
}

// Fisher-Yates Shuffle
static void board_shuffle(MineSweeperGameBoard* board) {
    uint16_t total = (uint16_t)board->width * board->height;
    if(total <= 1u) return;

    for(uint16_t i = total - 1u; i > 0u; --i) {
        // Generate j in [0, i]
        uint16_t j = random_uniform_u16(i + 1u);

        MineSweeperGameCell tmp = board->cells[i];
        board->cells[i] = board->cells[j];
        board->cells[j] = tmp;
    }
}

static void board_clear(MineSweeperGameBoard* board) {
    furi_assert(board);

    uint16_t total = board->height * board->width;

    for (uint16_t i = 0; i < total; ++i) {
        board->cells[i] = 0;
    }

}

static bool config_is_valid(const MineGameConfig* config) {
    if(!config) return false;

    return config->width > 0 &&
           config->height > 0 &&
           config->width <= BOARD_MAX_WIDTH &&
           config->height <= BOARD_MAX_HEIGHT &&
           config->difficulty <= 2;
}

static bool runtime_is_valid_for_board(
    const MineSweeperGameBoard* board,
    const MineGameRuntime* runtime
) {
    if(!board || !runtime) return false;
    if(board->width == 0 || board->height == 0) return false;

    if(runtime->cursor_col >= board->width || runtime->cursor_row >= board->height) {
        return false;
    }

    uint16_t total = (uint16_t)board->width * board->height;
    if(board->mine_count > total) return false;
    uint16_t safe_total = total - board->mine_count;

    if(runtime->tiles_left > safe_total) return false;
    if(runtime->flags_left > board->mine_count) return false;
    if(runtime->mines_left > board->mine_count) return false;

    if(runtime->phase != MineGamePhasePlaying &&
       runtime->phase != MineGamePhaseWon &&
       runtime->phase != MineGamePhaseLost) {
        return false;
    }

    return true;
}

const int8_t neighbor_offsets[8][2] = {
    {-1,  1}, {0,  1}, {1,  1},
    { 1,  0}, {1, -1}, {0, -1},
    {-1, -1}, {-1,  0}
};

uint16_t board_index(
    const MineSweeperGameBoard* board,
    const uint8_t x,
    const uint8_t y
) {
    return (uint16_t)y * board->width + x;
}

uint8_t board_x(
    const MineSweeperGameBoard* board,
    uint16_t i
) {
    return i % board->width;
}

uint8_t board_y(
    const MineSweeperGameBoard* board,
    uint16_t i
) {
    return i / board->width;
}

bool board_in_bounds(
    const MineSweeperGameBoard* board,
    int8_t x,
    int8_t y
) {
    return (x >= 0) && (y >= 0) &&
           (x < board->width) &&
           (y < board->height);
}

void board_init(
    MineSweeperGameBoard* board,
    uint8_t width,
    uint8_t height
) {
    board->width = width;
    board->height = height;
    board->mine_count = 0;
    
    board_clear(board);
}

bool board_place_mine(
    MineSweeperGameBoard* board,
    uint8_t x,
    uint8_t y
) {
    uint16_t i = board_index(board, x, y);

    if (CELL_IS_MINE(board->cells[i])) return false;

    CELL_SET_MINE(board->cells[i]);
    board->mine_count++;

    return true;
}

void board_compute_neighbor_counts(MineSweeperGameBoard* board) {
    for (uint8_t y = 0; y < board->height; ++y) {
        for (uint8_t x = 0; x < board->width; ++x) {
            
            uint16_t i = board_index(board, x, y);

            if (CELL_IS_MINE(board->cells[i])) continue;

            uint8_t count = 0;

            for (uint8_t n = 0; n < 8; ++n) {
                int8_t nx = x + neighbor_offsets[n][0];
                int8_t ny = y + neighbor_offsets[n][1];

                if (board_in_bounds(board, nx, ny)) {
                    uint16_t ni = board_index(board, nx, ny);
                    if (CELL_IS_MINE(board->cells[ni]))
                        count++;
                }
            }

            CELL_SET_NEIGHBORS(board->cells[i], count);
        }
    }
}

bool board_reveal_cell(
    MineSweeperGameBoard* board,
    uint8_t x,
    uint8_t y
) {
    uint16_t i = board_index(board, x, y);

    if (CELL_IS_REVEALED(board->cells[i]) ||
        CELL_IS_FLAGGED(board->cells[i]))
        return false;

    CELL_SET_REVEALED(board->cells[i]);
    return true;
}

uint16_t board_reveal_flood(MineSweeperGameBoard* board, uint8_t x, uint8_t y) {
    furi_assert(board);

    uint16_t cleared_tiles = 0;

    point_deq_t deq;
    point_set_t visited;

    point_deq_init(deq);
    point_set_init(visited);

    Point_t pos;
    pointobj_init(pos);

    pointobj_set_point(pos, (Point){.x = x, .y = y});

    point_deq_push_back(deq, pos);

    while (point_deq_size(deq) > 0) {
        point_deq_pop_front(&pos, deq);
        Point curr_pos = pointobj_get_point(pos);
        uint16_t curr_pos_1d = board_index(board,  curr_pos.x,  curr_pos.y);

        if (point_set_cget(visited, pos) != NULL ||
            CELL_IS_REVEALED(board->cells[curr_pos_1d]) ||
            CELL_IS_FLAGGED(board->cells[curr_pos_1d])) {
            continue;
        }

        board_reveal_cell(board, curr_pos.x, curr_pos.y);

        point_set_add(visited, pos);

        cleared_tiles++;

        if (!CELL_GET_NEIGHBORS(board->cells[curr_pos_1d])) {
            for (uint8_t n = 0; n < 8; ++n) {
                int16_t dx = (int16_t)curr_pos.x + neighbor_offsets[n][0];
                int16_t dy = (int16_t)curr_pos.y + neighbor_offsets[n][1];

                if (board_in_bounds(board, (int8_t)dx, (int8_t)dy)) {
                    pointobj_set_point(pos, (Point){.x = (uint8_t)dx, .y = (uint8_t)dy});
                    if (point_set_cget(visited, pos) != NULL) continue;
                    point_deq_push_back(deq, pos);
                }
            }
        }
    }

    point_set_clear(visited);
    point_deq_clear(deq);

    return cleared_tiles;
}

void board_toggle_flag(
    MineSweeperGameBoard* board,
    uint8_t x,
    uint8_t y
) {
    uint16_t i = board_index(board, x, y);

    if(CELL_IS_REVEALED(board->cells[i]))
        return;

    if(CELL_IS_FLAGGED(board->cells[i]))
        CELL_CLEAR_FLAGGED(board->cells[i]);
    else
        CELL_SET_FLAGGED(board->cells[i]);
}

void minesweeper_engine_new_game(MineSweeperGameState* game_state) {
    furi_assert(game_state);

    uint16_t total_cells = game_state->board.width * game_state->board.height;

    uint8_t difficulty = game_state->config.difficulty >= 3
        ? 2
        : game_state->config.difficulty;

    uint16_t number_mines = total_cells * difficulty_multiplier[difficulty];

    // Clear board and randomize mines
    board_clear(&game_state->board);
    
    for (uint16_t i = 0; i < number_mines; ++i) {
        CELL_SET_MINE(game_state->board.cells[i]);
    }

    game_state->board.mine_count = number_mines;

    board_shuffle(&game_state->board);

    // Set the number of neighboring mines for each cell
    board_compute_neighbor_counts(&game_state->board);

    game_state->rt.tiles_left = total_cells - number_mines;
    game_state->rt.flags_left = number_mines;
    game_state->rt.mines_left = number_mines;
    game_state->rt.phase = MineGamePhasePlaying;

    if(game_state->rt.cursor_col >= game_state->board.width) {
        game_state->rt.cursor_col = 0;
    }
    if(game_state->rt.cursor_row >= game_state->board.height) {
        game_state->rt.cursor_row = 0;
    }
}

MineSweeperGameResult minesweeper_engine_reveal(MineSweeperGameState* game_state, uint16_t x, uint16_t y) {
    furi_assert(game_state);

    MineSweeperGameBoard* board = &game_state->board;

    uint16_t cursor_pos_1d = board_index(board, x, y);

    if (CELL_IS_MINE(board->cells[cursor_pos_1d])) {
        minesweeper_engine_reveal_all_mines(game_state);
        return LOSE;
    }

    uint16_t revealed_delta = board_reveal_flood(board, x, y);
    if(revealed_delta == 0) {
        return NOOP;
    }

    game_state->rt.tiles_left -= revealed_delta;

    MineSweeperGameResult result = minesweeper_engine_check_win_conditions(game_state);
    return result == WIN ? WIN : CHANGED;
}


MineSweeperGameResult minesweeper_engine_chord(MineSweeperGameState* game_state, uint16_t x, uint16_t y) {
    furi_assert(game_state);

    MineSweeperGameBoard* board = &game_state->board;

    uint16_t cursor_pos_1d = board_index(board, x, y);
    uint8_t tile_num = CELL_GET_NEIGHBORS(board->cells[cursor_pos_1d]);

    if (!CELL_IS_REVEALED(board->cells[cursor_pos_1d]) || tile_num == 0) {
        return NOOP;
    }
    
    uint8_t flagged_neighbors = 0;
    
    for (uint8_t n = 0; n < 8; ++n) {
        int16_t dx = (int16_t)x + neighbor_offsets[n][0];
        int16_t dy = (int16_t)y + neighbor_offsets[n][1];

        if (board_in_bounds(board, (int8_t)dx,  (int8_t)dy)) {
            uint16_t neighbor_pos_1d = board_index(board, (uint8_t)dx, (uint8_t)dy);
            if (CELL_IS_FLAGGED(board->cells[neighbor_pos_1d])) {
                flagged_neighbors++;
            }
        }
    }

    // chord only if flagged neighbor count == tile number
    if (flagged_neighbors == tile_num) {
        uint16_t revealed_delta_total = 0;

        for (uint8_t n = 0; n < 8; ++n) {
            int16_t dx = (int16_t)x + neighbor_offsets[n][0];
            int16_t dy = (int16_t)y + neighbor_offsets[n][1];

            if (board_in_bounds(board, (int8_t)dx,  (int8_t)dy)) {
                uint16_t neighbor_pos_1d = board_index(board, (uint8_t)dx, (uint8_t)dy);
                if(
                !CELL_IS_REVEALED(board->cells[neighbor_pos_1d]) &&
                !CELL_IS_FLAGGED(board->cells[neighbor_pos_1d])) {
                    if (CELL_IS_MINE(board->cells[neighbor_pos_1d])) {
                        minesweeper_engine_reveal_all_mines(game_state);
                        return LOSE;
                    }
                    uint16_t revealed_delta =
                        board_reveal_flood(board, (uint8_t)dx, (uint8_t)dy);
                    game_state->rt.tiles_left -= revealed_delta;
                    revealed_delta_total += revealed_delta;
                }
            }
        }

        if(revealed_delta_total == 0) {
            return NOOP;
        }

        MineSweeperGameResult result = minesweeper_engine_check_win_conditions(game_state);
        return result == WIN ? WIN : CHANGED;
    }

    return NOOP;
}

MineSweeperGameResult minesweeper_engine_check_win_conditions(MineSweeperGameState* game_state) {
    furi_assert(game_state);

    if(game_state->rt.tiles_left == 0 && game_state->rt.flags_left == game_state->rt.mines_left) {
        game_state->rt.phase = MineGamePhaseWon;
        return WIN;
    }

    return CHANGED;
}

MineSweeperGameResult minesweeper_engine_toggle_flag(MineSweeperGameState* game_state, uint16_t x, uint16_t y) {
    furi_assert(game_state);

    if(game_state->rt.phase != MineGamePhasePlaying) {
        return NOOP;
    }

    MineSweeperGameBoard* board = &game_state->board;
    uint16_t tile_pos_1d = board_index(board, x, y);
    MineSweeperGameCell tile = board->cells[tile_pos_1d];

    if(CELL_IS_REVEALED(tile)) {
        return NOOP;
    }

    bool was_flagged = CELL_IS_FLAGGED(tile);
    bool is_mine = CELL_IS_MINE(tile);

    if(!was_flagged && game_state->rt.flags_left == 0) {
        return NOOP;
    }

    board_toggle_flag(board, x, y);

    if(was_flagged) {
        game_state->rt.flags_left++;
        if(is_mine) {
            game_state->rt.mines_left++;
        }
    } else {
        game_state->rt.flags_left--;
        if(is_mine) {
            game_state->rt.mines_left--;
        }
    }

    return minesweeper_engine_check_win_conditions(game_state);
}

MineSweeperGameResult minesweeper_engine_move_cursor(
    MineSweeperGameState* game_state,
    int8_t dx,
    int8_t dy
) {
    furi_assert(game_state);

    MineSweeperGameBoard* board = &game_state->board;
    if(board->width == 0 || board->height == 0) {
        return INVALID;
    }

    int16_t next_col = (int16_t)game_state->rt.cursor_col + dx;
    int16_t next_row = (int16_t)game_state->rt.cursor_row + dy;

    if(game_state->config.wrap_enabled) {
        next_col = (next_col % board->width + board->width) % board->width;
        next_row = (next_row % board->height + board->height) % board->height;
    } else {
        if(next_col < 0) {
            next_col = 0;
        } else if(next_col >= board->width) {
            next_col = board->width - 1;
        }

        if(next_row < 0) {
            next_row = 0;
        } else if(next_row >= board->height) {
            next_row = board->height - 1;
        }
    }

    if(
        game_state->rt.cursor_col == (uint8_t)next_col &&
        game_state->rt.cursor_row == (uint8_t)next_row
    ) {
        return NOOP;
    }

    game_state->rt.cursor_col = (uint8_t)next_col;
    game_state->rt.cursor_row = (uint8_t)next_row;
    return CHANGED;
}

MineSweeperGameResult minesweeper_engine_reveal_all_mines(MineSweeperGameState* game_state) {
    furi_assert(game_state);

    MineSweeperGameBoard* board = &game_state->board;
    uint16_t total = (uint16_t)board->width * board->height;
    bool changed = false;

    for(uint16_t i = 0; i < total; ++i) {
        if(CELL_IS_MINE(board->cells[i]) && !CELL_IS_REVEALED(board->cells[i])) {
            CELL_SET_REVEALED(board->cells[i]);
            changed = true;
        }
    }

    game_state->rt.phase = MineGamePhaseLost;
    return changed ? CHANGED : NOOP;
}

MineSweeperGameResult minesweeper_engine_apply_action(
    MineSweeperGameState* game_state,
    MineGameAction action
) {
    furi_assert(game_state);

    if(action.type != MineGameActionNewGame &&
       game_state->rt.phase != MineGamePhasePlaying) {
        return NOOP;
    }

    switch(action.type) {
    case MineGameActionMove:
        return minesweeper_engine_move_cursor(game_state, action.dx, action.dy);

    case MineGameActionReveal:
        return minesweeper_engine_reveal(
            game_state,
            game_state->rt.cursor_col,
            game_state->rt.cursor_row);

    case MineGameActionFlag:
        return minesweeper_engine_toggle_flag(
            game_state,
            game_state->rt.cursor_col,
            game_state->rt.cursor_row);

    case MineGameActionChord:
        return minesweeper_engine_chord(
            game_state,
            game_state->rt.cursor_col,
            game_state->rt.cursor_row);

    case MineGameActionNewGame:
        minesweeper_engine_new_game(game_state);
        return CHANGED;

    default:
        return INVALID;
    }
}

MineSweeperGameResult minesweeper_engine_set_config(
    MineSweeperGameState* game_state,
    const MineGameConfig* config
) {
    furi_assert(game_state);

    if(!config_is_valid(config)) {
        return INVALID;
    }

    game_state->config = *config;
    game_state->board.width = config->width;
    game_state->board.height = config->height;

    return CHANGED;
}

MineSweeperGameResult minesweeper_engine_set_runtime(
    MineSweeperGameState* game_state,
    const MineGameRuntime* runtime
) {
    furi_assert(game_state);

    if(!runtime_is_valid_for_board(&game_state->board, runtime)) {
        return INVALID;
    }

    game_state->rt = *runtime;
    return CHANGED;
}

MineSweeperGameResult minesweeper_engine_validate_state(const MineSweeperGameState* game_state) {
    furi_assert(game_state);

    const MineSweeperGameBoard* board = &game_state->board;
    const MineGameRuntime* runtime = &game_state->rt;
    const MineGameConfig* config = &game_state->config;

    if(!config_is_valid(config)) {
        return INVALID;
    }

    if(board->width != config->width || board->height != config->height) {
        return INVALID;
    }

    uint16_t total = (uint16_t)board->width * board->height;
    if(total == 0 || total > BOARD_MAX_TILES) {
        return INVALID;
    }

    if(board->mine_count > total) {
        return INVALID;
    }

    uint16_t mine_count_actual = 0;
    uint16_t revealed_safe_tiles = 0;

    for(uint16_t i = 0; i < total; ++i) {
        MineSweeperGameCell cell = board->cells[i];

        if(CELL_IS_MINE(cell)) {
            mine_count_actual++;
        } else if(CELL_IS_REVEALED(cell)) {
            revealed_safe_tiles++;
        }
    }

    if(mine_count_actual != board->mine_count) {
        return INVALID;
    }

    if(!runtime_is_valid_for_board(board, runtime)) {
        return INVALID;
    }

    uint16_t safe_total = total - board->mine_count;
    if((uint16_t)(runtime->tiles_left + revealed_safe_tiles) != safe_total) {
        return INVALID;
    }

    if(runtime->phase == MineGamePhaseWon &&
       !(runtime->tiles_left == 0 && runtime->flags_left == runtime->mines_left)) {
        return INVALID;
    }

    return NOOP;
}
