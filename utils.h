#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <termios.h> 
#include <signal.h>
#include <limits.h>

#define NAME_LEN 50
#define CNL_LEN 200
#define MSG_LEN 4096
#define BUFF_LEN (MSG_LEN + NAME_LEN + 4)
#define PORT 1337
#define SERVERPORT 1337
#define SOCKETERROR (-1)
#define SERVER_BACKLOG 100
#define MAX_CHANNELS 10
#define MAX_CLIENTS_PER_CHANNEL 20
#define MAX_CLIENTS (MAX_CLIENTS_PER_CHANNEL * MAX_CHANNELS)

typedef struct sockaddr_in SA_IN;
typedef struct sockaddr SA;

int disable_canonical_mode();
int validate_expression(int exp, const char *msg);
char* _build_token(char**t, int t_size, char c);
char** str_get_tokens_(char* str, const char d);

#endif