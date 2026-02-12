#include "mine_sweeper_engine.h"

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
    const uint16_t i
) {
    return i % board->width;
}

uint8_t board_y(
    const MineSweeperGameBoard* board,
    const uint16_t i
) {
    return i / board->width;
}

bool board_in_bounds(
    const MineSweeperGameBoard* board,
    const int8_t x,
    const int8_t y
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
    board->revealed_count = 0;

    uint16_t total = width * height;

    for (uint16_t i = 0; i < total; ++i) {
        board->cells[i] = 0;
    }
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

void board_reveal_cell(
    MineSweeperGameBoard* board,
    uint8_t x,
    uint8_t y
) {
    uint16_t i = board_index(board, x, y);

    if (CELL_IS_REVEALED(board->cells[i]) ||
        CELL_IS_FLAGGED(board->cells[i]))
        return;

    CELL_SET_REVEALED(board->cells[i]);
    board->revealed_count++;
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
