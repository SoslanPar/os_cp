/*Заголовочный файл с описанием структур данных,
которые хранятся в разделяемой памяти
(состояние игроков, игровые поля, мутексы).*/

#ifndef COMMON_H
#define COMMON_H

#include <pthread.h>
#include <stdbool.h>
#include <sys/types.h>

#define SHM_NAME "/battleship_game_shm"
#define MAX_PLAYERS 10
#define NAME_LEN 32
#define BOARD_SIZE 10

// Состояния ячеек
#define CELL_EMPTY '.'
#define CELL_SHIP 'S'
#define CELL_HIT 'X'
#define CELL_MISS 'O'

typedef enum {
    STATE_EMPTY = 0,
    STATE_LOBBY,
    STATE_INVITE_SENT,
    STATE_INVITE_RECEIVED,
    STATE_PLAYING,
    STATE_GAME_OVER
} PlayerState;

typedef struct {
    char grid[BOARD_SIZE][BOARD_SIZE]; 
    int ships_alive;
} GameBoard;

typedef struct {
    int id;
    char name[NAME_LEN];
    pid_t pid;
    PlayerState state;
    int opponent_id; 
    
    // Для синхронизации между Клиентом и Сервером для этого конкретного слота
    pthread_mutex_t mutex;
    pthread_cond_t cond_client; // Клиент ждет здесь
    pthread_cond_t cond_server; // Сервер ждет здесь (или опрашивает)

    // Механизм Команда/Ответ
    int cmd_type;
    /*0: Нет
    1: Логин
    2: Пригласить
    3: Принять
    4: Отклонить
    5: Выстрел
    6: Выход
    7: Проверка обновлений
    */
    char cmd_data[128];
    
    int resp_type; // 0: ОК, 1: Ошибка, 2: Обновление
    char resp_data[128];
    
    bool has_command; // Клиент ставит true, Сервер ставит false
    bool has_response; // Сервер ставит true, Клиент ставит false

    // Состояние игры для этого игрока
    GameBoard board;
    GameBoard enemy_view; // Что этот игрок знает о противнике
    bool is_turn; // true, если сейчас ход этого игрока
    char last_game_msg[128];

} PlayerSlot;

typedef struct {
    PlayerSlot players[MAX_PLAYERS];
    pthread_mutex_t global_mutex; // Защищает поиск свободного слота
    bool server_running;
} SharedMemory;

#endif
