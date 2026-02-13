#ifndef MINE_SWEEPER_SOLVER_H
#define  MINE_SWEEPER_SOLVER_H

#include "mstarlib_helpers.h"
#include "mine_sweeper_engine.h"
#include <stdbool.h>

bool check_board_with_solver(
    MineSweeperGameBoard* board
);

void bfs_tile_clear_solver(
    MineSweeperGameBoard* board,
    uint16_t x,
    uint16_t y,
    point_deq_t* edges,
    point_set_t* visited
);

#endif // MINE_SWEEPER_SOLVER_H
