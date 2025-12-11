#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include "../include/common.h"
#include "../include/game.h"

SharedMemory *shm;
int shm_fd;

void cleanup() {
    if (shm) {
        munmap(shm, sizeof(SharedMemory));
    }
    if (shm_fd != -1) {
        close(shm_fd);
    }
    shm_unlink(SHM_NAME);
    printf("Server stopped and shared memory cleaned up.\n");
}

void handle_sigint(int sig) {
    (void)sig;
    cleanup();
    exit(0);
}

void init_shm() {
    shm_unlink(SHM_NAME); // Обеспечить чистый старт

    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    if (ftruncate(shm_fd, sizeof(SharedMemory)) == -1) {
        perror("ftruncate");
        exit(1);
    }

    shm = mmap(NULL, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    // Инициализация мьютексов
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    /*Флаг PTHREAD_PROCESS_SHARED говорит системе:
    "Этот мьютекс будет лежать в разделяемой памяти, и его будут использовать разные процессы".
    Без этого флага синхронизация между клиентом и сервером не сработала бы
    */

    pthread_mutex_init(&shm->global_mutex, &attr);

    pthread_condattr_t cattr;
    pthread_condattr_init(&cattr);
    pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);

    for (int i = 0; i < MAX_PLAYERS; i++) {
        shm->players[i].id = i;
        shm->players[i].state = STATE_EMPTY;
        pthread_mutex_init(&shm->players[i].mutex, &attr);
        pthread_cond_init(&shm->players[i].cond_client, &cattr);
        pthread_cond_init(&shm->players[i].cond_server, &cattr);
        shm->players[i].has_command = false;
        shm->players[i].has_response = false;
    }

    shm->server_running = true;
}

int find_player_by_name(const char* name) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (shm->players[i].state != STATE_EMPTY && strcmp(shm->players[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

void process_command(int id) {
    PlayerSlot *p = &shm->players[id];
    
    // Ответ по умолчанию
    p->resp_type = 0; // ОК
    strcpy(p->resp_data, "OK");

    if (p->cmd_type == 1) { // ЛОГИН
        strncpy(p->name, p->cmd_data, NAME_LEN - 1);
        p->state = STATE_LOBBY;
        printf("Player %d logged in as %s\n", id, p->name);
    }
    else if (p->cmd_type == 2) { // ПРИГЛАШЕНИЕ
        int target_id = find_player_by_name(p->cmd_data);
        if (target_id == -1 || target_id == id) {
            p->resp_type = 1; // Ошибка
            strcpy(p->resp_data, "Player not found or invalid.");
        } else if (shm->players[target_id].state != STATE_LOBBY) {
            p->resp_type = 1;
            strcpy(p->resp_data, "Player is busy.");
        } else {
            // Обновление состояний
            p->state = STATE_INVITE_SENT;
            p->opponent_id = target_id;
            
            shm->players[target_id].state = STATE_INVITE_RECEIVED;
            shm->players[target_id].opponent_id = id;
            
            char target_name[NAME_LEN];
            strncpy(target_name, p->cmd_data, NAME_LEN);
            target_name[NAME_LEN-1] = '\0';
            snprintf(p->resp_data, sizeof(p->resp_data), "Invite sent to %s", target_name);
        }
    }
    else if (p->cmd_type == 3) { // ПРИНЯТЬ
        if (p->state == STATE_INVITE_RECEIVED) {
            int opp_id = p->opponent_id;
            PlayerSlot *opp = &shm->players[opp_id];
            
            if (opp->state == STATE_INVITE_SENT && opp->opponent_id == id) {
                // Начать игру
                p->state = STATE_PLAYING;
                opp->state = STATE_PLAYING;
                
                random_place_ships(&p->board);
                random_place_ships(&opp->board);
                
                // Инициализация вида противника (пусто)
                memset(&p->enemy_view, 0, sizeof(GameBoard));
                memset(&opp->enemy_view, 0, sizeof(GameBoard));
                for(int r=0; r<BOARD_SIZE; r++) 
                    for(int c=0; c<BOARD_SIZE; c++) {
                        p->enemy_view.grid[r][c] = CELL_EMPTY;
                        opp->enemy_view.grid[r][c] = CELL_EMPTY;
                    }

                p->is_turn = true; // Принимающий ходит первым? Или случайно. Пусть принимающий ходит первым.
                opp->is_turn = false;
                
                strcpy(p->resp_data, "Game Started! Your turn.");
                // Мы не можем легко отправить данные противнику без опроса, 
                // но при следующем опросе они увидят STATE_PLAYING.
            } else {
                p->state = STATE_LOBBY;
                p->resp_type = 1;
                strcpy(p->resp_data, "Invitation expired."); // Приглашение истекло
            }
        }
    }
    else if (p->cmd_type == 4) { // ОТКЛОНИТЬ
         if (p->state == STATE_INVITE_RECEIVED) {
            int opp_id = p->opponent_id;
            PlayerSlot *opp = &shm->players[opp_id];
            if (opp->state == STATE_INVITE_SENT && opp->opponent_id == id) {
                opp->state = STATE_LOBBY;
                opp->opponent_id = -1;
            }
            p->state = STATE_LOBBY;
            p->opponent_id = -1;
            strcpy(p->resp_data, "Declined.");
         }
    }
    else if (p->cmd_type == 5) { // ВЫСТРЕЛ x y
        if (p->state == STATE_PLAYING && p->is_turn) {
            int x, y;
            if (sscanf(p->cmd_data, "%d %d", &x, &y) == 2) {
                PlayerSlot *opp = &shm->players[p->opponent_id];
                int result = shoot(&opp->board, x, y);
                
                if (result == -1) {
                    p->resp_type = 1;
                    strcpy(p->resp_data, "Invalid shot.");
                } else {
                    // Обновление вида
                    char mark = (result == 1) ? CELL_HIT : CELL_MISS;
                    p->enemy_view.grid[y][x] = mark;
                    
                    if (result == 1) {
                        snprintf(p->resp_data, sizeof(p->resp_data), "HIT at %d %d!", x, y);
                        if (check_win(&opp->board)) {
                            p->state = STATE_GAME_OVER;
                            opp->state = STATE_GAME_OVER;
                            strcat(p->resp_data, " YOU WIN!");
                        }
                    } else {
                        snprintf(p->resp_data, sizeof(p->resp_data), "MISS at %d %d.", x, y);
                        p->is_turn = false;
                        opp->is_turn = true;
                    }
                }
            }
        } else {
            p->resp_type = 1;
            strcpy(p->resp_data, "Not your turn or not playing.");
        }
    }
    else if (p->cmd_type == 6) { // ВЫХОД
        p->state = STATE_EMPTY;
        if (p->opponent_id != -1) {
            shm->players[p->opponent_id].state = STATE_LOBBY; // Сброс противника
            shm->players[p->opponent_id].opponent_id = -1;
        }
        p->opponent_id = -1;
        printf("Player %d quit.\n", id);
    }
    else if (p->cmd_type == 7) { // ПРОВЕРКА_ОБНОВЛЕНИЙ
        // Просто вернуть информацию о текущем состоянии
        if (p->state == STATE_INVITE_RECEIVED) {
            p->resp_type = 2;
            char inviter_name[NAME_LEN];
            strncpy(inviter_name, shm->players[p->opponent_id].name, NAME_LEN);
            inviter_name[NAME_LEN-1] = '\0';
            snprintf(p->resp_data, sizeof(p->resp_data), "Invite from %s", inviter_name);

        } else if (p->state == STATE_PLAYING) {
             p->resp_type = 2;
             if (p->is_turn) strcpy(p->resp_data, "Your turn");
             else strcpy(p->resp_data, "Opponent's turn");
             
        } else if (p->state == STATE_GAME_OVER) {
             p->resp_type = 2;
             // Проверка кто победил
             if (check_win(&shm->players[p->opponent_id].board)) {
                 strcpy(p->resp_data, "YOU WIN!");
             } else {
                 strcpy(p->resp_data, "YOU LOSE!");
             }
        }
    }
}

int main() {
    signal(SIGINT, handle_sigint);
    srand(time(NULL));
    
    init_shm();
    printf("Server started. Shared memory initialized.\n");

    while (shm->server_running) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            pthread_mutex_lock(&shm->players[i].mutex);
            if (shm->players[i].has_command) {
                process_command(i);
                shm->players[i].has_command = false;
                shm->players[i].has_response = true;
                pthread_cond_signal(&shm->players[i].cond_client);
            }
            pthread_mutex_unlock(&shm->players[i].mutex);
        }
        struct timespec ts = {0, 10000000}; // 10мс
        nanosleep(&ts, NULL);
    }

    cleanup();
    return 0;
}
