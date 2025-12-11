#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>
#include <signal.h>
#include "../include/common.h"

SharedMemory *shm;
int my_id = -1;

void cleanup() {
    if (my_id != -1 && shm) {
        // Попытаться уведомить сервер, что мы выходим
        pthread_mutex_lock(&shm->players[my_id].mutex);
        shm->players[my_id].cmd_type = 6; // ВЫХОД
        shm->players[my_id].has_command = true;
        pthread_mutex_unlock(&shm->players[my_id].mutex);
    }
    // Мы не удаляем, просто отключаем отображение
    if (shm) munmap(shm, sizeof(SharedMemory));
}

void handle_sigint(int sig) {
    (void)sig;
    cleanup();
    exit(0);
}

void send_command(int type, const char* data) {
    PlayerSlot *p = &shm->players[my_id];
    
    pthread_mutex_lock(&p->mutex);
    
    p->cmd_type = type;
    if (data) strncpy(p->cmd_data, data, 127);
    else p->cmd_data[0] = '\0';
    
    p->has_command = true;
    
    // Ждать ответа
    while (!p->has_response) {
        pthread_cond_wait(&p->cond_client, &p->mutex);
    }
    
    // Ответ готов
    p->has_response = false; // Поглощено
    
    pthread_mutex_unlock(&p->mutex);
}

void print_board(GameBoard *b, int hide_ships) {
    printf("  0 1 2 3 4 5 6 7 8 9\n");
    for (int i = 0; i < BOARD_SIZE; i++) {
        printf("%d ", i);
        for (int j = 0; j < BOARD_SIZE; j++) {
            char c = b->grid[i][j];
            if (hide_ships && c == CELL_SHIP) c = CELL_EMPTY;
            printf("%c ", c);
        }
        printf("\n");
    }
}

int main() {
    signal(SIGINT, handle_sigint);

    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd == -1) {
        printf("Could not open shared memory. Is server running?\n");
        return 1;
    }

    shm = mmap(NULL, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    // Найти слот
    pthread_mutex_lock(&shm->global_mutex);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (shm->players[i].state == STATE_EMPTY) {
            my_id = i;
            // Зарезервировать сразу, чтобы избежать гонки
            shm->players[i].state = STATE_LOBBY; // Временное состояние до логина
            break;
        }
    }
    pthread_mutex_unlock(&shm->global_mutex);

    if (my_id == -1) {
        printf("Server is full.\n");
        return 1;
    }

    printf("Connected to server. Slot %d.\n", my_id);

    // Логин
    char name[NAME_LEN];
    printf("Enter login: ");
    scanf("%s", name);
    send_command(1, name);
    printf("Logged in as %s.\n", name);

    while (1) {
        PlayerSlot *p = &shm->players[my_id];
        
        // Обновить состояние с сервера (опрос)
        send_command(7, NULL); // ПРОВЕРКА_ОБНОВЛЕНИЙ
        
        if (p->state == STATE_LOBBY) {
            printf("\n--- LOBBY ---\n");
            printf("1. Invite Player\n");
            printf("2. Refresh/Check Invites\n");
            printf("3. Quit\n");
            printf("> ");
            
            int choice;
            if (scanf("%d", &choice) != 1) {
                while(getchar() != '\n'); // очистить буфер
                continue;
            }
            
            if (choice == 1) {
                char target[NAME_LEN];
                printf("Enter username to invite: ");
                scanf("%s", target);
                send_command(2, target);
                printf("Server: %s\n", p->resp_data);
            } else if (choice == 2) {
                // Цикл обновится
                printf("Refreshed.\n");
            } else if (choice == 3) {
                cleanup();
                break;
            }
        }
        else if (p->state == STATE_INVITE_SENT) {
            printf("\nWaiting for response... (1. Cancel/Refresh)\n");
            int c;
            scanf("%d", &c);
        }
        else if (p->state == STATE_INVITE_RECEIVED) {
            printf("\n--- INVITE ---\n");
            printf("%s\n", p->resp_data); // "Приглашение от X"
            printf("1. Accept\n");
            printf("2. Decline\n");
            printf("> ");
            int choice;
            scanf("%d", &choice);
            if (choice == 1) send_command(3, NULL);
            else send_command(4, NULL);
        }
        else if (p->state == STATE_PLAYING) {
            printf("\n--- BATTLESHIP ---\n");
            printf("Your Board:\n");
            print_board(&p->board, 0);
            printf("\nEnemy Board (Known):\n");
            print_board(&p->enemy_view, 0);
            
            if (p->is_turn) {
                printf("\nYOUR TURN. Enter coordinates (x y): ");
                int x, y;
                if (scanf("%d %d", &x, &y) == 2) {
                    char buf[32];
                    sprintf(buf, "%d %d", x, y);
                    send_command(5, buf);
                    printf("Result: %s\n", p->resp_data);
                } else {
                    while(getchar() != '\n');
                }
            } else {
                printf("\nOpponent's turn... Waiting...\n");
                sleep(1);
            }
        }
        else if (p->state == STATE_GAME_OVER) {
            printf("\n--- GAME OVER ---\n");
            printf("%s\n", p->resp_data);
            printf("Press 1 to return to lobby.\n");
            int c;
            scanf("%d", &c);
            // Отправим ОТКЛОНИТЬ, чтобы вернуться в лобби
            send_command(4, NULL); 
        }
    }

    return 0;
}
