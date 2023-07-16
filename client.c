#include "utils.h"

volatile sig_atomic_t flag_ctrl_c = 0;
volatile sig_atomic_t flag_exit = 1;
int client_socket = 0;
char name[NAME_LEN];

/* SIGNAL to unable CTRL+C */
void catch_ctrl_c(int sig)
{
    flag_ctrl_c = 1;
}

/* SIGNAL to exit process */
void catch_exit(int sig)
{
    flag_exit = 0;
}

/* Set self client name */
int set_name()
{
    printf("Please enter your name: ");
    fgets(name, NAME_LEN, stdin);
    // scanf("%49[^\n]", name);
    // printf("%s oioi\n", name);
    name[strlen(name) - 1] = '\0';
    // str_trim_lf(name);
    printf("%s", name);

    if (strlen(name) > NAME_LEN || strlen(name) < 2)
    {
        printf("Name must be less than 30 and more than 2 characters.\n");
        return EXIT_FAILURE;
    }
    return 0;
}

/* Thread function to send messages */
void *send_msg_handler()
{
    char msg[MSG_LEN];
    char buffer[BUFF_LEN];

    while (1)
    {
        str_overwrite_stdout();
        fgets(msg, MSG_LEN, stdin);
        // str_trim_lf(msg);
        msg[strlen(msg) - 1] = '\0';

        if (strlen(msg) == 0 || strcmp(msg, "/quit") == 0)
            break;
        else
        {
            char **tokens = str_get_tokens_(msg, ' ');

            if (strcmp(tokens[0], "/nickname\n") == 0)
            {
                printf("%s nickname updated to ", name);
                strcpy(name, tokens[1]);
                printf("%s\n", name);
            }

            sprintf(buffer, "%s\n", msg);
            printf("mensagem: %s\n", msg);
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
void *recv_msg_handler()
{
    char buff[BUFF_LEN];

    while (1)
    {
        int receive = recv(client_socket, buff, BUFF_LEN, 0);
        if (receive == 0)
            break;
        if (receive > 0)
        {
            //str_trim_lf(buff);
            printf("%s\n", buff);
            str_overwrite_stdout();

            bzero(buff, BUFF_LEN);
        }
    }
    catch_exit(2);
    return NULL;
}

/* Try to connect to the server */
int try_new_connection()
{
    int client_socket;

    check((client_socket = socket(AF_INET, SOCK_STREAM, 0)), "Failed to create socket!");

    // Set port and IP the same as server-side:
    SA_IN server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVERPORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Send connection request to server:
    check((connect(client_socket, (SA *)&server_addr, sizeof(server_addr))), "Unable to connect");
    printf("Connected with server successfully\n");

    return client_socket;
}

int main()
{
    clear_icanon();
    signal(SIGINT, catch_ctrl_c);

    if (set_name())
        return EXIT_FAILURE;

    // Wait for /connect command
    char msg[10];
    while (1)
    {
        str_overwrite_stdout();
        fgets(msg, sizeof(msg), stdin);
        msg[strlen(msg) - 1] = '\0';
        if (strcmp(msg, "/connect") == 0)
            break;
        else if (strcmp(msg, "/quit") == 0)
            return 0;
        else
        {
            printf("%s", msg);
            printf("%d", strcmp(msg, "/connect"));
            return 0;
        };
    }

    client_socket = try_new_connection();

    send(client_socket, name, NAME_LEN, 0);
    printf("=== WELCOME TO ZAP ZAP ===\n");

    // creating threads to send and receive messages
    pthread_t thread_send;
    pthread_create(&thread_send, NULL, (void *)send_msg_handler, NULL);

    pthread_t thread_recv;
    pthread_create(&thread_recv, NULL, (void *)recv_msg_handler, NULL);

    // If SIGNAL is active
    while (flag_exit)
    {
        if (flag_ctrl_c)
        {
            printf("\n Cannot be terminated using Ctrl+C \n");
            str_overwrite_stdout();
            flag_ctrl_c = 0;
        }
    }
    printf("\nBye Bye, hate all you!\n");

    close(client_socket);
    return 0;
}