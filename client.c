#include "utils.h"

volatile sig_atomic_t flag_ctrl_c = 0;
volatile sig_atomic_t flag_exit = 1;
int client_socket = 0;
char name[NAME_LEN];

void try_quit(int sig);
void catch_exit(int sig);
void *process_send_message();
void *process_message_receiving();

int main() {
    clear_icanon();
    signal(SIGINT, try_quit);

    printf("Please enter your name: ");
    fgets(name, NAME_LEN, stdin);
    name[strlen(name) - 1] = '\0';
    printf("%s\n", name);

    if (strlen(name) > NAME_LEN || strlen(name) < 2) {
        printf("Name must be less than 30 and more than 2 characters.\n");
        return EXIT_FAILURE;
    }
    printf("Type '/connect' to connect or '/quit' to quit\n");

    // Wait for /connect command
    char msg[10];
    while (1) {
        printf("\r%s", "> ");
        fflush(stdout);
        fgets(msg, sizeof(msg), stdin);
        msg[strlen(msg) - 1] = '\0';
        if (strcmp(msg, "/connect") == 0)
            break;
        else if (strcmp(msg, "/quit") == 0)
            return 0;
        else {
            printf("Message: %s\n", msg);
            if (strcmp(msg, "/connect") == 0) {
                printf("You typed \"/connect\"");
            } else {
                printf("Try typing \"/connect\"");
            }
            return 0;
        }
    }

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Failed to create socket!");
    }

    // Set port and IP the same as server-side:
    SA_IN server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVERPORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Send connection request to server:
    check((connect(client_socket, (SA *)&server_addr, sizeof(server_addr))), "Unable to connect");
    printf("Connected with server successfully\n");

    send(client_socket, name, NAME_LEN, 0);
    printf("=== Welcome to the safest Chat ===\n");
    printf("To join a private chat Type '/join #<private-chat-name>'\n");

    // creating threads to send and receive messages
    pthread_t thread_send;
    pthread_create(&thread_send, NULL, (void *)process_send_message, NULL);

    pthread_t thread_recv;
    pthread_create(&thread_recv, NULL, (void *)process_message_receiving, NULL);

    // If SIGNAL is active
    while (flag_exit) {
        if (flag_ctrl_c) {
            printf("\n Type '/quit' to end connection \n");
            str_overwrite_stdout();
            flag_ctrl_c = 0;
        }
    }
    printf("\nChat ended\n");

    close(client_socket);
    return 0;
}

/* SIGNAL to unable CTRL+C */
void try_quit(int sig) {
    flag_ctrl_c = 1;
}

/* SIGNAL to exit process */
void catch_exit(int sig) {
    flag_exit = 0;
}

/* Thread function to send messages */
void *process_send_message() {
    char msg[MSG_LEN];
    char buffer[BUFF_LEN];

    while (1) {
        str_overwrite_stdout();
        fgets(msg, MSG_LEN, stdin);
        // str_trim_lf(msg);
        msg[strlen(msg) - 1] = '\0';

        if (strlen(msg) == 0 || strcmp(msg, "/quit") == 0)
            break;
        else {
            char **tokens = str_get_tokens_(msg, ' ');

            if (strcmp(tokens[0], "/nickname\n") == 0) {
                printf("%s nickname updated to ", name);
                strcpy(name, tokens[1]);
                printf("%s\n", name);
            }

            sprintf(buffer, "%s", msg);
            printf("%s: %s\n", name, msg);
            send(client_socket, buffer, strlen(buffer), 0);
            str_free_tokens(tokens);
        }

        bzero(msg, MSG_LEN);
        bzero(buffer, BUFF_LEN);
    }
    catch_exit(2);
    return NULL;
}

/* Thread function to receive messages */
void *process_message_receiving() {
    char buff[BUFF_LEN];

    while (1) {
        int receive = recv(client_socket, buff, BUFF_LEN, 0);
        if (receive == 0)
            break;
        if (receive > 0) {
            //str_trim_lf(buff);
            printf("%s", buff);
            printf("\r%s", "> ");
            fflush(stdout);

            bzero(buff, BUFF_LEN);
        }
    }
    catch_exit(2);
    return NULL;
}
