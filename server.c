/* 
SSC0142 - Redes de Computadores
Trabalho 2 - Internet Relay Chat

Alunos:
- Thaís Ribeiro Lauriano - 12542518
- João Pedro Duarte Nunes - 12542460
- Raphael David Leveque - 12542522 */

#include "utils.h"

// Variáveis globais
static _Atomic unsigned int num_clients_connected = 0; // Número de clientes conectados
static int next_user_id = 10; // Próximo ID de usuário disponível

// Definição de estruturas de dados
typedef struct {
    SA_IN address; // Endereço do cliente
    int sockfd; // Descritor do socket do cliente
    int uid; // ID único do cliente
    char name[NAME_LEN]; // Nome do cliente
    int is_admin; // Indica se o cliente é um administrador
    int is_muted; // Indica se o cliente está mudo
    int kick; // Indica se o cliente foi removido do chat
    pthread_t thread; // Thread associada ao cliente
} Client; // Estrutura que representa um cliente

typedef struct {
    char key[CHANNEL_LEN]; // Nome do canal
    Client* connected_clients[MAX_CLIENTS_PER_CHANNEL]; // Lista de clientes conectados ao canal
    int num_clients; // Número de clientes conectados ao canal
    int admin_id; // ID do administrador do canal
} Channel; // Estrutura que representa um canal de chat

Client* clients[MAX_CLIENTS]; // Array de todos os clientes conectados
Channel* channels[MAX_CHANNELS]; // Array de todos os canais criados

// besteirinhas pra imprimir a ascii art
#define MAX_LEN 128
void print_image(FILE *fptr); 

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex para proteger operações com clientes
pthread_mutex_t channels_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex para proteger operações com canais

// Funções de busca
int find_client(Client* client); // Retorna o índice de um cliente no array clients
int find_channel_of_client(Client* client); // Retorna o índice do canal que contém o cliente especificado
int find_channel_and_client(char* name, int* client_id); // Retorna o índice do canal e do cliente com o nome especificado
int find_channel(char* name); // Retorna o índice do canal com o nome especificado

// Funções para manipular canais e clientes
int add_client_to_channel(char* channel_name, Client* client); // Adiciona um cliente a um canal
void remove_client_from_channel(int channel_id, Client* client); // Remove um cliente de um canal
void enqueue_client(Client* client); // Adiciona um cliente à fila de clientes
void dequeue_client(int uid); // Remove um cliente da fila de clientes

// Função para enviar mensagem para todos os clientes no canal, exceto o remetente
void send_message(char* message, Client* sender);

// Funções para configurar e aceitar conexões do servidor
int setup_server(int port, int backlog, SA_IN* server_addr); // Configura o socket do servidor
int accept_new_connection(int server_socket, SA_IN* client_addr); // Aceita uma nova conexão

// Função da thread para lidar com cada cliente
void* handle_client(void *arg);

int main() {
    disable_canonical_mode();
    SA_IN* client_addr = (SA_IN*)malloc(sizeof(SA_IN));
    SA_IN* server_addr = (SA_IN*)malloc(sizeof(SA_IN));

    int server_socket = setup_server(SERVERPORT, SERVER_BACKLOG, server_addr);

    printf("=== Welcome to the safest Chat ===\n");
    char *filename = "image.txt";
    FILE *fptr = NULL;
 
    if((fptr = fopen(filename,"r")) == NULL)
    {
        fprintf(stderr,"error opening %s\n",filename);
        return 1;
    }
 
    print_image(fptr);
 
    fclose(fptr);

    while (1) {
        pthread_t thread = NULL;
        int client_socket;
        if((client_socket = accept_new_connection(server_socket, client_addr)) < 0) continue;

        // Inicializando a struct do cliente
        Client* client = (Client*) malloc(sizeof(Client));
        client->address = *client_addr;
        client->sockfd = client_socket;
        client->uid = next_user_id++;
        client->is_admin = 0;
        client->is_muted = 0;
        client->kick = 0;
        client->thread = thread;
        
        enqueue_client(client);

        pthread_create(&thread, NULL, &handle_client,(void*)client);

        sleep(1);
    }

    // Fechando o socket:
    close(server_socket);
    return 0;
}

/* Retorna o índice do cliente no array clients */
int find_client (Client* client) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i] == client) {
            return i;
        }
    }
    return -1;
}

/* Retorna o índice do canal que contém o cliente especificado */
int find_channel_of_client(Client* client) {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i]) {

            for (int j = 0; j < MAX_CLIENTS_PER_CHANNEL; j++) {
                if (channels[i]->connected_clients[j] && channels[i]->connected_clients[j] == client) {
                    return i;
                }
            }
        }
    }
    return -1;
}

/* Retorna o índice do canal e do cliente com o nome especificado */
int find_channel_and_client (char* name, int* id) {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i]) {

            for (int j = 0; j < MAX_CLIENTS_PER_CHANNEL; j++) {
                if (channels[i]->connected_clients[j] && strcmp(channels[i]->connected_clients[j]->name, name) == 0) {
                    *id = j;
                    return i;
                }
            }
        }
    }
    return -1;
}

/* Retorna o id do canal buscado pelo nome */
int find_channel(char* name) {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i] && strcmp(channels[i]->key, name) == 0)
            return i;
    }
    return -1;
}

int add_client_to_channel(char* channel_name, Client* client) {
    pthread_mutex_lock(&channels_mutex);

    // valida o nome do canal
    if (channel_name[0] != '#' && channel_name[0] != '&') {
        pthread_mutex_unlock(&channels_mutex);
        return 0;
    }

    // procura por um canal existente
    int id;
    if ((id = find_channel(channel_name)) > -1) {
        for (int i = 0; i < MAX_CLIENTS_PER_CHANNEL; ++i){
            if(!channels[id]->connected_clients[i]) {
                channels[id]->connected_clients[i] = client;
                channels[id]->num_clients++;
                pthread_mutex_unlock(&channels_mutex);
                return 1;
            }                          
        }
    }
    // cria novo canal
    else {
        Channel* new_channel = malloc(sizeof(*new_channel));
        strcpy(new_channel->key, channel_name);
        new_channel->admin_id = client->uid;
        new_channel->connected_clients[0] = client;
        new_channel->num_clients = 1;
        client->is_admin = 1;
        for (int i = 0; i < MAX_CHANNELS; i++) {
            if(!channels[i]) {
                channels[i] = new_channel;
                pthread_mutex_unlock(&channels_mutex);
                return 1;
            }
        }
    }
    pthread_mutex_unlock(&channels_mutex);
    return 0;
}

void remove_client_from_channel(int id, Client* client) {
    pthread_mutex_lock(&channels_mutex);
    
    for (int i = 0; i < MAX_CLIENTS_PER_CHANNEL; i++) {
        if (channels[id]->connected_clients[i] && channels[id]->connected_clients[i] == client) {
            channels[id]->connected_clients[i] = NULL;
            channels[id]->num_clients--;
            break;
        }
    }
    if (channels[id]->num_clients == 0) {
        free(channels[id]);
    }

    pthread_mutex_unlock(&channels_mutex);
}

/* Adiciona um cliente à fila de clientes */
void enqueue_client(Client* client) {
    pthread_mutex_lock(&clients_mutex);
    for(int i = 0; i < MAX_CLIENTS; ++i){
        if(!clients[i]){
            clients[i] = client;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

/* Remove um cliente da fila de clientes */
void dequeue_client(int uid) {
    pthread_mutex_lock(&clients_mutex);
    for(int i = 0; i < MAX_CLIENTS; ++i){
        if(clients[i] && clients[i]->uid == uid){
            clients[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

/* Função para enviar mensagem para todos os clientes no canal, exceto o remetente */
void send_message(char* message, Client* sender) {
    if (sender->is_muted) return;
    pthread_mutex_lock(&clients_mutex);

    int id;
    if ((id = find_channel_of_client(sender)) < 0) {
        pthread_mutex_unlock(&clients_mutex);
        return;
    }

    for(int i = 0; i < MAX_CLIENTS_PER_CHANNEL; ++i){
        if(channels[id]->connected_clients[i] && channels[id]->connected_clients[i] != sender && channels[id]->connected_clients[i]->kick == 0){
            
            int bytes_sent = validate_expression(write(channels[id]->connected_clients[i]->sockfd, message, strlen(message)),"ERROR: write to descriptor failed");
                
            if (bytes_sent == 0) {
                close(channels[id]->connected_clients[i]->sockfd);
                dequeue_client(channels[id]->connected_clients[i]->uid);
                remove_client_from_channel(id, sender);
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

/* Configura o socket do servidor */
int setup_server(int port, int backlog, SA_IN* server_addr) {
    int server_socket;

    validate_expression((server_socket = socket(AF_INET, SOCK_STREAM, 0)), "Failed to create socket!");

    // inicializa a struct de endereço
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

/* Aceita nova conexão */
int accept_new_connection(int server_socket, SA_IN* client_addr) {
    socklen_t addr_size = sizeof(*client_addr);
    int client_socket;
    
    validate_expression(client_socket = accept(server_socket, (SA*)client_addr, &addr_size), "Accept failed");

    validate_expression(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)), "setsockopt(SO_REUSEADDR) failed");

    // Validar limite de número de clientes
    if((num_clients_connected + 1) == MAX_CLIENTS) {
        printf("Max clients reached, bye bye: ");
        printf("%s:%d\n", inet_ntoa((*client_addr).sin_addr), ntohs((*client_addr).sin_port));
        close(client_socket);
        return -1;
    }

    printf("Client connected at IP: %s and port: %d\n", inet_ntoa((*client_addr).sin_addr), ntohs((*client_addr).sin_port));
    
    num_clients_connected++;
    return client_socket;
}

/* Função da thread para lidar com cada cliente */
void* handle_client(void *arg) {
    char msg[MSG_LEN];
    char buffer_out[BUFF_LEN];
    char name[NAME_LEN];
    Client *client = (Client *) arg;

    // lendo nome do cliente
    if (recv(client->sockfd, name, NAME_LEN, 0) <= 0 || strlen(name) < 2 || strlen(name) >= NAME_LEN - 1) {
        printf("Failed to get name or invalid name\n");
        client->kick = 1;
    } else {
        strcpy(client->name, name);
        sprintf(buffer_out, "%s has joined chat\n", client->name);
        printf("%s", buffer_out);
        send_message(buffer_out, client);
    }

    bzero(buffer_out, BUFF_LEN);
    while (1) {
        if (client->kick) break;
        printf("\r%s", "> ");
        fflush(stdout);
        
        int command = 0;
        int receive = recv(client->sockfd, msg, MSG_LEN, 0);
        if (client->kick) break;
        if (receive > 0) {

            int n_tokens = 0;
            char** tokens = get_tokens_from_string(msg, ' ');
            for (int i = 0; tokens[i]; i++) {
                n_tokens++;
            }

            if (strcmp(msg, "/ping") == 0) {
                validate_expression(write(client->sockfd, "server: pong\n", strlen("server: pong\n")),"ERROR: write to descriptor failed");
                sprintf(buffer_out, "%s: %s\n", name, msg);
                printf("%s", buffer_out);
                command = 1;
            }
            if (n_tokens == 2) {
                if (strcmp(tokens[0], "/join") == 0) {
                    if (add_client_to_channel(tokens[1], client)) {

                        sprintf(buffer_out, "%s joined channel %s\n", name, tokens[1]);
                        validate_expression(write(client->sockfd, buffer_out, BUFF_LEN), "ERROR: write to descriptor failed");
                        printf("%s", buffer_out);
                    }
                    else {
                        sprintf(buffer_out, "channel %s is invalid or full\n", tokens[1]);
                        validate_expression(write(client->sockfd, buffer_out, BUFF_LEN), "ERROR: write to descriptor failed");
                        printf("%s", buffer_out);
                    }
                    command = 1;
                }
                else if (strcmp(tokens[0], "/nickname") == 0) {
                    sprintf(buffer_out, "%s nickname updated to %s\n", client->name, tokens[1]);
                    strcpy(client->name, tokens[1]);
                    printf("%s", buffer_out);
                    send_message(buffer_out, client);
                    command = 1;
                }
                else if (client->is_admin && (strcmp(tokens[0], "/kick") == 0 || strcmp(tokens[0], "/mute") == 0 || strcmp(tokens[0], "/unmute") == 0 || strcmp(tokens[0], "/whois") == 0)) {
                    printf("command admin");
                    int adm_idx;
                    int adm_cnl = find_channel_and_client(client->name, &adm_idx);

                    int cli_idx;
                    tokens[1][strlen(tokens[1])] = '\0';
                    int cnl_idx = find_channel_and_client(tokens[1], &cli_idx);

                    if (cnl_idx != adm_cnl || cnl_idx < 0) {
                        printf("Client não encontrado ou não está no mesmo chat\n");
                        continue;
                    }
                    if (strcmp(tokens[0], "/kick") == 0) {
                        int i = find_client(channels[cnl_idx]->connected_clients[cli_idx]);
                        
                        sprintf(buffer_out, "%s was kicked from channel '%s' by admin '%s'\n", clients[i]->name, channels[adm_cnl]->key, client->name);
                        send_message(buffer_out, client);
                        printf("%s", buffer_out);

                        pthread_mutex_lock(&clients_mutex);
                        channels[cnl_idx]->connected_clients[cli_idx]->kick = 1;
                        pthread_mutex_unlock(&clients_mutex);

                        command = 1;
                    }
                    else if (strcmp(tokens[0], "/mute") == 0) {
                        channels[cnl_idx]->connected_clients[cli_idx]->is_muted = 1;
                        command = 1;
                    }
                    else if (strcmp(tokens[0], "/unmute") == 0) {
                        channels[cnl_idx]->connected_clients[cli_idx]->is_muted = 0;
                        command = 1;
                    }
                    else if (strcmp(tokens[0], "/whois") == 0) {
                        sprintf(buffer_out, "%s IPv4 is %s\n", tokens[1], inet_ntoa(channels[cnl_idx]->connected_clients[cli_idx]->address.sin_addr));
                        validate_expression(write(client->sockfd, buffer_out, BUFF_LEN),"ERROR: write to descriptor failed");
                        command = 1;
                    }
                }
            }
            if (!command) {
                sprintf(buffer_out, "%s: %s\n", client->name, msg);
                send_message(buffer_out, client);
                printf("%s",buffer_out);
            }
            for (int i = 0; tokens[i]; i++)
                free(tokens[i]);
            free(tokens);
        }
        else if(receive == 0 || strcmp(msg, "/quit") == 0) {
            sprintf(buffer_out, "%s has left\n", client->name);
            printf("%s", buffer_out);
            send_message(buffer_out, client);
            client->kick = 1;
        }
        else {
            printf("ERROR: -1\n");
            client->kick = 1;
        }
        bzero(msg, MSG_LEN);
        bzero(buffer_out, BUFF_LEN);
    }

    int idx = find_channel_of_client(client);
    close(client->sockfd);
    dequeue_client(client->uid);
    if (idx != -1) remove_client_from_channel(idx, client);
    free(client);
    pthread_detach(pthread_self());
    return NULL;
}

void print_image(FILE *fptr){
    char read_string[MAX_LEN];
 
    while(fgets(read_string,sizeof(read_string),fptr) != NULL)
        printf("%s",read_string);
}