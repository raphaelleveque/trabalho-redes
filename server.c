#include "utils.h"

static _Atomic unsigned int n_clients = 0;
static int uid = 10;

typedef struct {
    SA_IN address;
    int sockfd;
    int uid;
    char name[NAME_LEN];
    int is_admin;
    int is_muted;
    int kick;
    pthread_t thread;
} client_t;

typedef struct {
    char key[CNL_LEN];
    client_t* connected_cli[MAX_CLIENTS];
    int n_cli;
    int admin_id;
} channel_t;

client_t* clients[MAX_CLI_PER_CHANNEL];
channel_t* channels[MAX_CHANNELS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t channels_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Returns index of client in array */
int find_client (client_t* cli) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i] == cli) {
            return i;
        }
    }
    return -1;
}

/* Returns index of channel with desired client */
int find_channel_of_client(client_t* cli) {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i]) {

            for (int j = 0; j < MAX_CLI_PER_CHANNEL; j++) {
                if (channels[i]->connected_cli[j] && channels[i]->connected_cli[j] == cli) {
                    return i;
                }
            }
        }
    }
    return -1;
}

/* Returns channel and client of desired client name */
int find_channel_and_client (char* str, int* idx) {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i]) {

            for (int j = 0; j < MAX_CLI_PER_CHANNEL; j++) {
                if (channels[i]->connected_cli[j] && strcmp(channels[i]->connected_cli[j]->name, str) == 0) {
                    *idx = j;
                    return i;
                }
            }
        }
    }
    return -1;
}

/* Returns channel of desired name */
int find_channel(char* str) {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i] && strcmp(channels[i]->key, str) == 0)
            return i;
    }
    return -1;
}

/* Add clients to channel */
int add_client_to_channel(char* str, client_t* cli) {
	pthread_mutex_lock(&channels_mutex);

    // validate_expressions if channel name is valid
    if (str[0] != '#' && str[0] != '&') {
	    pthread_mutex_unlock(&channels_mutex);
        return 0;
    }

    // looks for existing channel
    int idx;
    if ((idx = find_channel(str)) > -1) {
        for (int i = 0; i < MAX_CLI_PER_CHANNEL; ++i){
            if(!channels[idx]->connected_cli[i]) {
                channels[idx]->connected_cli[i] = cli;
                channels[idx]->n_cli++;
                pthread_mutex_unlock(&channels_mutex);
                return 1;
            }                          
        }
	}
    // create new channel
    else {
        channel_t* new_cnl = malloc(sizeof(*new_cnl));
        strcpy(new_cnl->key, str);
        new_cnl->admin_id = cli->uid;
        new_cnl->connected_cli[0] = cli;
        new_cnl->n_cli = 1;
        cli->is_admin = 1;
        for (int i = 0; i < MAX_CHANNELS; i++) {
            if(!channels[i]) {
                channels[i] = new_cnl;
                pthread_mutex_unlock(&channels_mutex);
                return 1;
            }
        }
    }
	pthread_mutex_unlock(&channels_mutex);
    return 0;
}

/* Remove client from channel */
void remove_client_from_channel(int idx, client_t* cli) {
	pthread_mutex_lock(&channels_mutex);
    
    for (int i = 0; i < MAX_CLI_PER_CHANNEL; i++) {
        if (channels[idx]->connected_cli[i] && channels[idx]->connected_cli[i] == cli) {
            channels[idx]->connected_cli[i] = NULL;
            channels[idx]->n_cli--;
            break;
        }
    }
    if (channels[idx]->n_cli == 0) {
        free(channels[idx]);
    }

	pthread_mutex_unlock(&channels_mutex);
}

/* Add clients to queue */
void queue_add(client_t *cl){
	pthread_mutex_lock(&clients_mutex);
	for(int i = 0; i < MAX_CLIENTS; ++i){
		if(!clients[i]){
			clients[i] = cl;
			break;
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}

/* Remove clients to queue */
void queue_remove(int uid){
	pthread_mutex_lock(&clients_mutex);
	for(int i = 0; i < MAX_CLIENTS; ++i){
		if(clients[i] && clients[i]->uid == uid){
            clients[i] = NULL;
            break;
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}


/* Send message to all clients in channel except sender */
void send_message(char *s, client_t* cli){
    if (cli->is_muted) return;
	pthread_mutex_lock(&clients_mutex);

    int idx;
    if ((idx = find_channel_of_client(cli)) < 0) {
        pthread_mutex_unlock(&clients_mutex);
        return;
    }

	for(int i = 0; i < MAX_CLI_PER_CHANNEL; ++i){
		if(channels[idx]->connected_cli[i] && channels[idx]->connected_cli[i] != cli && channels[idx]->connected_cli[i]->kick == 0){
            
            int bytes_sent = validate_expression(write(channels[idx]->connected_cli[i]->sockfd, s, strlen(s)),"ERROR: write to descriptor failed");
                
            if (bytes_sent == 0) {
                close(channels[idx]->connected_cli[i]->sockfd);
                queue_remove(channels[idx]->connected_cli[i]->uid);
                remove_client_from_channel(idx, cli);
            }
        }
	}
	pthread_mutex_unlock(&clients_mutex);
}

/* Build server socket */
int setup_server(int port, int backlog, SA_IN* server_addr) {
    int server_socket;

    validate_expression((server_socket = socket(AF_INET, SOCK_STREAM, 0)), "Failed to create socket!");

    // initialize the address struct
    (*server_addr).sin_family = AF_INET;
    (*server_addr).sin_port = htons(SERVERPORT);
    (*server_addr).sin_addr.s_addr = inet_addr("127.0.0.1");

    signal(SIGPIPE, SIG_IGN);
    validate_expression(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)), "setsockopt(SO_REUSEADDR) failed");

    validate_expression(bind(server_socket, (SA*)server_addr, sizeof(*server_addr)), "Bind Failed");
    validate_expression(listen(server_socket, backlog), "Listen Failed");

    printf("Listening on: %s:%d\n", inet_ntoa((*server_addr).sin_addr), ntohs((*server_addr).sin_port));

    return server_socket;
}

/* Accept connection */
int accept_new_connection(int server_socket, SA_IN* client_addr) {
    socklen_t addr_size = sizeof(*client_addr);
    int client_socket;
    
    validate_expression(client_socket = accept(server_socket, (SA*)client_addr, &addr_size), "Accept failed");

    validate_expression(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)), "setsockopt(SO_REUSEADDR) failed");

    // validate_expression limit number of clients
    if((n_clients + 1) == MAX_CLIENTS) {
        printf("Max clients reached, bye bye: ");
        printf("%s:%d\n", inet_ntoa((*client_addr).sin_addr), ntohs((*client_addr).sin_port));
        close(client_socket);
        return -1;
    }

    printf("Client connected at IP: %s and port: %d\n", inet_ntoa((*client_addr).sin_addr), ntohs((*client_addr).sin_port));
    
    n_clients++;
    return client_socket;
}

/* Thread function to handle each client */
void* handle_client(void *arg) {
    char msg[MSG_LEN];
    char buffer_out[BUFF_LEN];
    char name[NAME_LEN];
    client_t *cli = (client_t *) arg;

    // get client name
    if (recv(cli->sockfd, name, NAME_LEN, 0) <= 0 || strlen(name) < 2 || strlen(name) >= NAME_LEN - 1) {
        printf("Failed to get name or invalid name\n");
        cli->kick = 1;
    } else {
        strcpy(cli->name, name);
        sprintf(buffer_out, "%s has joined chat\n", cli->name);
        printf("%s", buffer_out);
        send_message(buffer_out, cli);
    }

    bzero(buffer_out, BUFF_LEN);
    while (1) {
        if (cli->kick) break;
        printf("\r%s", "> ");
        fflush(stdout);
        
        int command = 0;
        int receive = recv(cli->sockfd, msg, MSG_LEN, 0);
        if (cli->kick) break;
        if (receive > 0) {

            int n_tokens = 0;
            char** tokens = str_get_tokens_(msg, ' ');
            for (int i = 0; tokens[i]; i++) {
                //printf("%d: \'%s\'\n", i, tokens[i]);
                n_tokens++;
            }

            if (strcmp(msg, "/ping") == 0) {
                validate_expression(write(cli->sockfd, "server: pong\n", strlen("server: pong\n")),"ERROR: write to descriptor failed");
                sprintf(buffer_out, "%s: %s\n", name, msg);
                printf("%s", buffer_out);
                command = 1;
            }
            if (n_tokens == 2) {
                if (strcmp(tokens[0], "/join") == 0) {
                    if (add_client_to_channel(tokens[1], cli)) {

                        sprintf(buffer_out, "%s joined channel %s\n", name, tokens[1]);
                        validate_expression(write(cli->sockfd, buffer_out, BUFF_LEN), "ERROR: write to descriptor failed");
                        printf("%s", buffer_out);
                    }
                    else {
                        sprintf(buffer_out, "channel %s is invalid or full\n", tokens[1]);
                        validate_expression(write(cli->sockfd, buffer_out, BUFF_LEN), "ERROR: write to descriptor failed");
                        printf("%s", buffer_out);
                    }
                    command = 1;
                }
                else if (strcmp(tokens[0], "/nickname") == 0) {
                    sprintf(buffer_out, "%s nickname updated to %s\n", cli->name, tokens[1]);
                    strcpy(cli->name, tokens[1]);
                    printf("%s", buffer_out);
                    send_message(buffer_out, cli);
                    command = 1;
                }
                else if (cli->is_admin && (strcmp(tokens[0], "/kick") == 0 || strcmp(tokens[0], "/mute") == 0 || strcmp(tokens[0], "/unmute") == 0 || strcmp(tokens[0], "/whois") == 0)) {
                    printf("command admin\n");
                    int adm_idx;
                    int adm_cnl = find_channel_and_client(cli->name, &adm_idx);

                    int cli_idx;
                    tokens[1][strlen(tokens[1])] = '\0';
                    int cnl_idx = find_channel_and_client(tokens[1], &cli_idx);

                    if (cnl_idx != adm_cnl || cnl_idx < 0) {
                        printf("Client não encontrado ou não está no mesmo chat\n");
                        continue;
                    }
                    if (strcmp(tokens[0], "/kick") == 0) {
                        int i = find_client(channels[cnl_idx]->connected_cli[cli_idx]);
                        
                        sprintf(buffer_out, "%s was kicked from channel '%s' by admin '%s'\n", clients[i]->name, channels[adm_cnl]->key, cli->name);
                        send_message(buffer_out, cli);
                        printf("%s", buffer_out);

                        pthread_mutex_lock(&clients_mutex);
                        channels[cnl_idx]->connected_cli[cli_idx]->kick = 1;
                        pthread_mutex_unlock(&clients_mutex);

                        command = 1;
                    }
                    else if (strcmp(tokens[0], "/mute") == 0) {
                        channels[cnl_idx]->connected_cli[cli_idx]->is_muted = 1;
                        command = 1;
                    }
                    else if (strcmp(tokens[0], "/unmute") == 0) {
                        channels[cnl_idx]->connected_cli[cli_idx]->is_muted = 0;
                        command = 1;
                    }
                    else if (strcmp(tokens[0], "/whois") == 0) {
                        sprintf(buffer_out, "%s IPv4 is %s\n", tokens[1], inet_ntoa(channels[cnl_idx]->connected_cli[cli_idx]->address.sin_addr));
                        validate_expression(write(cli->sockfd, buffer_out, BUFF_LEN),"ERROR: write to descriptor failed");
                        command = 1;
                    }
                }
            }
            if (!command) {
                sprintf(buffer_out, "%s: %s\n", cli->name, msg);
                send_message(buffer_out, cli);
                printf("%s",buffer_out);
            }
            for (int i = 0; tokens[i]; i++)
                free(tokens[i]);
            free(tokens);
        }
        else if(receive == 0 || strcmp(msg, "/quit") == 0) {
            sprintf(buffer_out, "%s has left\n", cli->name);
            printf("%s", buffer_out);
            send_message(buffer_out, cli);
            cli->kick = 1;
        }
        else {
            printf("ERROR: -1\n");
            cli->kick = 1;
        }
        bzero(msg, MSG_LEN);
        bzero(buffer_out, BUFF_LEN);
    }

    int idx = find_channel_of_client(cli);
    close(cli->sockfd);
    queue_remove(cli->uid);
    if (idx != -1) remove_client_from_channel(idx, cli);
    free(cli);
    pthread_detach(pthread_self());
    return NULL;
}

int main() {
    disable_canonical_mode();
    SA_IN* client_addr = (SA_IN*)malloc(sizeof(SA_IN));
    SA_IN* server_addr = (SA_IN*)malloc(sizeof(SA_IN));

    int server_socket = setup_server(SERVERPORT, SERVER_BACKLOG, server_addr);

    printf("=== Welcome to the safest Chat ===\n");

    while (1) {
        pthread_t thread = NULL;
        int client_socket;
        if((client_socket = accept_new_connection(server_socket, client_addr)) < 0) continue;

        // setting client struct
        client_t* cli = (client_t *) malloc(sizeof(client_t));
        cli->address = *client_addr;
        cli->sockfd = client_socket;
        cli->uid = uid++;
        cli->is_admin = 0;
        cli->is_muted = 0;
        cli->kick = 0;
        cli->thread = thread;
        
        queue_add(cli);

        pthread_create(&thread, NULL, &handle_client,(void*)cli);

        sleep(1);
    }

    // Closing the socket:
    close(server_socket);
    return 0;
}