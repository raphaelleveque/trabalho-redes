#include "utils.h"

volatile sig_atomic_t flag_ctrl_c = 0;
volatile sig_atomic_t flag_exit = 1;
int client_socket = 0;
char name[NAME_LEN];

void handle_ctrlC(int sig);
void catch_exit(int sig);
void *process_send_message();
void *process_message_receiving();

int main() {
    // Disable canonical mode to handle input
    disable_canonical_mode();

    // Set up signal handler for Ctrl+C
    signal(SIGINT, handle_ctrlC);

    // Prompt the user to enter their name
    printf("Please enter your name: ");
    fgets(name, NAME_LEN, stdin);
    name[strlen(name) - 1] = '\0';
    printf("%s\n", name);

    // Validate the length of the name
    if (strlen(name) > NAME_LEN || strlen(name) < 2) {
        printf("Name must be less than 30 and more than 2 characters.\n");
        return EXIT_FAILURE;
    }

    // Prompt the user to connect or quit
    printf("Type '/connect' to connect or '/quit' to quit\n");

    // Wait for the '/connect' command or '/quit' command
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
            // Invalid command entered
            printf("Message: %s\n", msg);
            if (strcmp(msg, "/connect") == 0) {
                printf("You typed \"/connect\"");
            } else {
                printf("Try typing \"/connect\"");
            }
            return 0;
        }
    }

    // Create a socket for client
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Failed to create socket!");
    }

    // Set server address
    SA_IN server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVERPORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Connect to the server
    validate_expression((connect(client_socket, (SA *)&server_addr, sizeof(server_addr))), "Unable to connect");
    printf("Connected with server successfully\n");

    // Send the client's name to the server
    send(client_socket, name, NAME_LEN, 0);

    // Print welcome message
    printf("=== Welcome to the safest Chat ===\n");
    printf("To join a private chat, type '/join #<private-chat-name>'\n");

    // Create threads for sending and receiving messages
    pthread_t thread_send;
    pthread_create(&thread_send, NULL, (void *)process_send_message, NULL);

    pthread_t thread_recv;
    pthread_create(&thread_recv, NULL, (void *)process_message_receiving, NULL);

    // Keep running until the exit flag is set
    while (flag_exit) {
        if (flag_ctrl_c) {
            // Ctrl+C (SIGINT) was received
            printf("\n Type '/quit' to end the connection \n");
            printf("\r%s", "> ");
            fflush(stdout);
            flag_ctrl_c = 0;
        }
    }

    // Chat ended
    printf("\nThe chat has ended!\n");

    // Close the client socket
    close(client_socket);

    return 0;
}

/* Signal handler for Ctrl+C (SIGINT) */
void handle_ctrlC(int sig) {
    flag_ctrl_c = 1;  // Set flag_ctrl_c to indicate Ctrl+C (SIGINT) was received
}

/* Signal handler to exit the process */
void catch_exit(int sig) {
    flag_exit = 0;  // Set flag_exit to indicate the process should exit
}

/* Thread function for sending messages */
void *process_send_message() {
    char msg[MSG_LEN];  // Buffer for user input message
    char buffer[BUFF_LEN];  // Buffer to store the message to be sent

    while (1) {
        printf("\r%s", "> ");
        fflush(stdout);
        fgets(msg, MSG_LEN, stdin);
        msg[strlen(msg) - 1] = '\0';

        if (strlen(msg) == 0 || strcmp(msg, "/quit") == 0)
            break;
        else {
            char **tokens = str_get_tokens_(msg, ' ');  // Tokenize the message

            if (strcmp(tokens[0], "/nickname\n") == 0) {
                printf("%s nickname updated to ", name);
                strcpy(name, tokens[1]);  // Update the nickname
                printf("%s\n", name);
            }

            sprintf(buffer, "%s", msg);  // Copy the message to the buffer
            printf("%s: %s\n", name, msg);  // Print the sender's name and message
            send(client_socket, buffer, strlen(buffer), 0);  // Send the message
            for (int i = 0; tokens[i]; i++)
                free(tokens[i]);
            free(tokens);
        }

        bzero(msg, MSG_LEN);  // Clear the user input message buffer
        bzero(buffer, BUFF_LEN);  // Clear the message buffer
    }
    catch_exit(2);  // Call catch_exit to indicate the process should exit
    return NULL;
}

/* Thread function for receiving messages */
void *process_message_receiving() {
    char buff[BUFF_LEN];  // Buffer to receive incoming messages

    while (1) {
        int receive = recv(client_socket, buff, BUFF_LEN, 0);  // Receive message
        if (receive == 0)
            break;
        if (receive > 0) {
            printf("%s", buff);  // Print the received message
            printf("\r%s", "> ");
            fflush(stdout);

            bzero(buff, BUFF_LEN);  // Clear the message buffer
        }
    }
    catch_exit(2);  // Call catch_exit to indicate the process should exit
    return NULL;
}