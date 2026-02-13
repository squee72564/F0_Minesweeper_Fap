#ifndef MINE_SWEEPER_ENGINE_H
#define MINE_SWEEPER_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    NOOP,
    CHANGED,
    WIN,
    LOSE,
    INVALID
} MineSweeperGameResult;

/* ---- Cell Layout ----
 * bit 0 : mine
 * bit 1 : revealed
 * bit 2 : flagged
 * bits 3–6 : neighbor count (0–8)
 * bit 7 : reserved
 */
typedef uint8_t MineSweeperGameCell;

/* Bit masks */
#define CELL_MINE_MASK        (0x01u)
#define CELL_REVEALED_MASK    (0x02u)
#define CELL_FLAG_MASK        (0x04u)

#define CELL_NEIGHBOR_SHIFT   (3u)
#define CELL_NEIGHBOR_MASK    (0x0Fu << CELL_NEIGHBOR_SHIFT)

/* Board limits */
#define BOARD_MAX_WIDTH       (32u)
#define BOARD_MAX_HEIGHT      (32u)
#define BOARD_MAX_TILES       (BOARD_MAX_WIDTH * BOARD_MAX_HEIGHT)

/* ---- Queries ---- */
#define CELL_IS_MINE(c)       (((c) & CELL_MINE_MASK) != 0u)
#define CELL_IS_REVEALED(c)   (((c) & CELL_REVEALED_MASK) != 0u)
#define CELL_IS_FLAGGED(c)    (((c) & CELL_FLAG_MASK) != 0u)
#define CELL_GET_NEIGHBORS(c) \
    ((uint8_t)(((c) & CELL_NEIGHBOR_MASK) >> CELL_NEIGHBOR_SHIFT))

/* ---- Mutators ---- */
#define CELL_SET_MINE(c)         ((c) |= CELL_MINE_MASK)
#define CELL_CLEAR_MINE(c)       ((c) &= (uint8_t)~CELL_MINE_MASK)

#define CELL_SET_REVEALED(c)     ((c) |= CELL_REVEALED_MASK)
#define CELL_CLEAR_REVEALED(c)   ((c) &= (uint8_t)~CELL_REVEALED_MASK)

#define CELL_SET_FLAGGED(c)      ((c) |= CELL_FLAG_MASK)
#define CELL_CLEAR_FLAGGED(c)    ((c) &= (uint8_t)~CELL_FLAG_MASK)

#define CELL_SET_NEIGHBORS(c,n) \
    do { \
        (c) &= (uint8_t)~CELL_NEIGHBOR_MASK; \
        (c) |= (uint8_t)(((n & 0x0Fu) << CELL_NEIGHBOR_SHIFT)); \
    } while(0)

/* ---- Board ---- */
typedef struct {
    uint8_t width;
    uint8_t height;
    uint16_t mine_count;
    MineSweeperGameCell cells[BOARD_MAX_TILES];
} MineSweeperGameBoard;

typedef enum {
    MineGamePhasePlaying = 0,
    MineGamePhaseWon,
    MineGamePhaseLost,
} MineGamePhase;

typedef struct {
    uint8_t width;
    uint8_t height;
    uint8_t difficulty;
    bool ensure_solvable;
    bool wrap_enabled;
} MineGameConfig;

typedef struct {
    uint8_t cursor_row;
    uint8_t cursor_col;
    uint16_t mines_left;
    uint16_t flags_left;
    uint16_t tiles_left;
    uint32_t start_tick;
    MineGamePhase phase;
} MineGameRuntime;

typedef struct {
    MineSweeperGameBoard board;
    MineGameConfig config;
    MineGameRuntime rt;
} MineSweeperGameState;

typedef enum {
    MineGameActionMove = 0,
    MineGameActionReveal,
    MineGameActionFlag,
    MineGameActionChord,
    MineGameActionNewGame,
} MineGameActionType;

typedef struct {
    MineGameActionType type;
    int8_t dx;
    int8_t dy;
} MineGameAction;

extern const int8_t neighbor_offsets[8][2];

/* ---- BOARD API ---- */
uint16_t board_index(const MineSweeperGameBoard* board, uint8_t x, uint8_t y);

uint8_t board_x(const MineSweeperGameBoard* board, uint16_t i);

uint8_t board_y(const MineSweeperGameBoard* board, uint16_t i);

bool board_in_bounds(const MineSweeperGameBoard* board, int8_t x, int8_t y);

void board_init(MineSweeperGameBoard* board, uint8_t width, uint8_t height);

bool board_place_mine(MineSweeperGameBoard* board, uint8_t x, uint8_t y);

void board_compute_neighbor_counts(MineSweeperGameBoard* board);

bool board_reveal_cell(MineSweeperGameBoard* board, uint8_t x, uint8_t y);

uint16_t board_reveal_flood(MineSweeperGameBoard* board, uint8_t x, uint8_t y);

void board_toggle_flag(MineSweeperGameBoard* board, uint8_t x, uint8_t y);


/* ---- ENGINE API ---- */

void minesweeper_engine_new_game(MineSweeperGameState* game_state);

MineSweeperGameResult minesweeper_engine_reveal(MineSweeperGameState* game_state, uint16_t x, uint16_t y);

MineSweeperGameResult minesweeper_engine_chord(MineSweeperGameState* game_state, uint16_t x, uint16_t y);

MineSweeperGameResult minesweeper_engine_toggle_flag(MineSweeperGameState* game_state, uint16_t x, uint16_t y);

MineSweeperGameResult minesweeper_engine_move_cursor(MineSweeperGameState* game_state, int8_t dx, int8_t dy);

MineSweeperGameResult minesweeper_engine_reveal_all_mines(MineSweeperGameState* game_state);

MineSweeperGameResult minesweeper_engine_check_win_conditions(MineSweeperGameState* game_state);

MineSweeperGameResult minesweeper_engine_apply_action(MineSweeperGameState* game_state, MineGameAction action);

MineSweeperGameResult minesweeper_engine_set_config(
    MineSweeperGameState* game_state,
    const MineGameConfig* config);

MineSweeperGameResult minesweeper_engine_set_runtime(
    MineSweeperGameState* game_state,
    const MineGameRuntime* runtime);

MineSweeperGameResult minesweeper_engine_validate_state(const MineSweeperGameState* game_state);


#endif
