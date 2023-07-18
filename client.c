/* 
SSC0142 - Redes de Computadores
Trabalho 2 - Internet Relay Chat

Alunos:
- Thaís Ribeiro Lauriano - 12542518
- João Pedro Duarte Nunes - 12542460
- Raphael David Leveque - 12542522 */

#include "utils.h"

volatile sig_atomic_t flag_ctrl_c = 0;
volatile sig_atomic_t flag_exit = 1;
int client_socket = 0;
char name[NAME_LEN];

void *process_send_message();
void *process_message_receiving();
void handle_ctrlC();

int main() {
    // Desativa o modo canônico para manipular a entrada
    disable_canonical_mode();

    // Configura o manipulador de sinal para Ctrl+C
    signal(SIGINT, handle_ctrlC);

    // Solicita ao usuário para digitar seu nome
    printf("Please enter your name: ");
    fgets(name, NAME_LEN, stdin);
    name[strcspn(name, "\n")] = '\0'; // Remove a quebra de linha final

    // Valida o tamanho do nome
    if (strlen(name) > NAME_LEN || strlen(name) < 2) {
        printf("Name must be between 2 and 30 characters.\n");
        return EXIT_FAILURE;
    }

    // Solicita ao usuário para se conectar ou sair
    printf("Type '/connect' to connect or '/quit' to quit\n");

    char msg[10];
    while (1) {
        printf("\r%s", "> ");
        fflush(stdout);
        fgets(msg, sizeof(msg), stdin);
        msg[strcspn(msg, "\n")] = '\0'; // Remove a quebra de linha final

        // Verifica se o usuário digitou "/connect" para conectar ou "/quit" para sair
        if (strcmp(msg, "/connect") == 0)
            break;
        else if (strcmp(msg, "/quit") == 0)
            return 0;
        else {
            printf("Invalid command. Try typing '/connect'\n");
            return 0;
        }
    }

    // Cria o socket do cliente
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Failed to create socket!");
        return EXIT_FAILURE;
    }

    // Configura o endereço do servidor
    SA_IN server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVERPORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Conecta ao servidor
    if (connect(client_socket, (SA *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Unable to connect");
        return EXIT_FAILURE;
    }

    printf("Connected with server successfully\n");

    // Envia o nome do cliente para o servidor
    send(client_socket, name, NAME_LEN, 0);

    printf("=== Welcome to the safest Chat ===\n");
    printf("To join a private chat, type '/join #<private-chat-name>'\n");

    // Cria threads para enviar e receber mensagens
    pthread_t thread_send, thread_recv;
    pthread_create(&thread_send, NULL, process_send_message, NULL);
    pthread_create(&thread_recv, NULL, process_message_receiving, NULL);

    // Mantém a execução até que a flag de saída seja definida
    while (flag_exit) {
        if (flag_ctrl_c) {
            printf("\n Type '/quit' to end the connection \n");
            printf("\r%s", "> ");
            fflush(stdout);
            flag_ctrl_c = 0;
        }
    }

    printf("\nThe chat has ended!\n");

    // Fecha o socket do cliente
    close(client_socket);

    return 0;
}

/* Função da thread para enviar mensagens */
void *process_send_message() {
    char msg[MSG_LEN];
    char buffer[BUFF_LEN];

    while (1) {
        printf("\r%s", "> ");
        fflush(stdout);
        fgets(msg, MSG_LEN, stdin);
        msg[strcspn(msg, "\n")] = '\0'; // Remove a quebra de linha final

        // Verifica se o usuário deseja sair
        if (strlen(msg) == 0 || strcmp(msg, "/quit") == 0)
            break;
        else {
            char **tokens = get_tokens_from_string(msg, ' '); // Tokeniza a mensagem

            // Verifica se o usuário está alterando o apelido
            if (strcmp(tokens[0], "/nickname") == 0) {
                printf("%s nickname updated to %s\n", name, tokens[1]);
                strcpy(name, tokens[1]); // Atualiza o apelido
            }

            sprintf(buffer, "%s", msg); // Copia a mensagem para o buffer
            printf("%s: %s\n", name, msg); // Imprime o nome do remetente e a mensagem
            send(client_socket, buffer, strlen(buffer), 0); // Envia a mensagem

            for (int i = 0; tokens[i]; i++)
                free(tokens[i]);
            free(tokens);
        }

        memset(msg, 0, MSG_LEN); // Limpa o buffer de entrada do usuário
        memset(buffer, 0, BUFF_LEN); // Limpa o buffer de mensagem
    }

    flag_exit = 0; // Seta flag_exit para 0 indicando que o processo deve sair
    return NULL;
}

/* Função da thread para receber mensagens */
void *process_message_receiving() {
    char buff[BUFF_LEN];

    while (1) {
        int receive = recv(client_socket, buff, BUFF_LEN, 0); // Recebe a mensagem
        if (receive == 0)
            break;
        if (receive > 0) {
            printf("%s", buff); // Imprime a mensagem recebida
            printf("\r%s", "> ");
            fflush(stdout);

            memset(buff, 0, BUFF_LEN); // Limpa o buffer de mensagem
        }
    }

    flag_exit = 0; // Seta flag_exit para 0 indicando que o processo deve sair
    return NULL;
}

void handle_ctrlC(){
    flag_ctrl_c = 1;
}