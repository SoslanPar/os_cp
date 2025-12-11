#ifndef GAME_H
#define GAME_H

#include "common.h"

void init_board(GameBoard *board);
int place_ship(GameBoard *board, int x, int y, int size, int orientation); // 0: гориз, 1: верт
void random_place_ships(GameBoard *board);
int shoot(GameBoard *target, int x, int y); /*
Возвращает:
0: Промах
1: Попадание
2: Потопил
-1: Неверно/Уже стреляли
*/
int check_win(GameBoard *target);

#endif
