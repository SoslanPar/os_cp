#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "../include/game.h"

void init_board(GameBoard *board) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            board->grid[i][j] = CELL_EMPTY;
        }
    }
    board->ships_alive = 10; // 1+2+3+4
}

int is_valid_placement(GameBoard *board, int x, int y, int size, int orientation) {
    // Проверка границ
    if (orientation == 0) { // Горизонтально
        if (x + size > BOARD_SIZE) return 0;
    } else { // Вертикально
        if (y + size > BOARD_SIZE) return 0;
    }

    // Проверка перекрытия и соседей
    int start_x = (x > 0) ? x - 1 : x;
    int start_y = (y > 0) ? y - 1 : y;
    int end_x = (orientation == 0) ? x + size : x + 1;
    int end_y = (orientation == 1) ? y + size : y + 1;

    if (end_x < BOARD_SIZE) end_x++;
    if (end_y < BOARD_SIZE) end_y++;

    for (int i = start_y; i < end_y; i++) {
        for (int j = start_x; j < end_x; j++) {
            if (board->grid[i][j] != CELL_EMPTY) return 0;
        }
    }
    return 1;
}

int place_ship(GameBoard *board, int x, int y, int size, int orientation) {
    if (!is_valid_placement(board, x, y, size, orientation)) return 0;

    for (int i = 0; i < size; i++) {
        if (orientation == 0) {
            board->grid[y][x + i] = CELL_SHIP;
        } else {
            board->grid[y + i][x] = CELL_SHIP;
        }
    }
    return 1;
}

void random_place_ships(GameBoard *board) {
    init_board(board);
    int ships[] = {4, 3, 3, 2, 2, 2, 1, 1, 1, 1};
    int num_ships = 10;

    for (int i = 0; i < num_ships; i++) {
        int placed = 0;
        while (!placed) {
            int x = rand() % BOARD_SIZE;
            int y = rand() % BOARD_SIZE;
            int orientation = rand() % 2;
            if (place_ship(board, x, y, ships[i], orientation)) {
                placed = 1;
            }
        }
    }
}

int shoot(GameBoard *target, int x, int y) {
    if (x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE) return -1;

    char cell = target->grid[y][x];
    if (cell == CELL_HIT || cell == CELL_MISS) return -1; // Уже стреляли

    if (cell == CELL_SHIP) {
        target->grid[y][x] = CELL_HIT;
        return 1; 
    } else {
        target->grid[y][x] = CELL_MISS;
        return 0;
    }
}

int check_win(GameBoard *target) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (target->grid[i][j] == CELL_SHIP) return 0;
        }
    }
    return 1;
}
