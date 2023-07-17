#include "utils.h"

// Variáveis globais
static _Atomic unsigned int n_clients = 0; // Número de clientes conectados
static int uid = 10; // Próximo ID de usuário disponível

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
} client_t; // Estrutura que representa um cliente

typedef struct {
    char key[CNL_LEN]; // Nome do canal
    client_t* connected_clients[MAX_CLIENTS_PER_CHANNEL]; // Lista de clientes conectados ao canal
    int num_clients; // Número de clientes conectados ao canal
    int admin_id; // ID do administrador do canal
} channel_t; // Estrutura que representa um canal de chat

client_t* clients[MAX_CLIENTS]; // Array de todos os clientes conectados
channel_t* channels[MAX_CHANNELS]; // Array de todos os canais criados

// besteirinhas pra imprimir a ascii art
#define MAX_LEN 128
void print_image(FILE *fptr); 

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex para proteger operações com clientes
pthread_mutex_t channels_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex para proteger operações com canais

// Funções de busca
int find_client(client_t* client); // Retorna o índice de um cliente no array all_clients
int find_channel_of_client(client_t* client); // Retorna o índice do canal que contém o cliente especificado
int find_channel_and_client(char* name, int* client_idx); // Retorna o índice do canal e do cliente com o nome especificado
int find_channel(char* name); // Retorna o índice do canal com o nome especificado

// Funções para manipular canais e clientes
int add_client_to_channel(char* name, client_t* client); // Adiciona um cliente a um canal
void remove_client_from_channel(int channel_idx, client_t* client); // Remove um cliente de um canal
void queue_add(client_t* client); // Adiciona um cliente à fila
void queue_remove(int uid); // Remove um cliente da fila

// Função para enviar mensagem para todos os clientes no canal, exceto o remetente
void send_message_to_channel(char* message, client_t* sender);

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

        // inicializando a struct do cliente
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

    // Fechando o socket:
    close(server_socket);
    return 0;
}
 /* Rretorna o índice do cliente */
int find_client (client_t* cli) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i] == cli) {
            return i;
        }
    }
    return -1;
}

/* Retorna o índice do canal que contém o cliente especificado */
int find_channel_of_client(client_t* cli) {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i]) {

            for (int j = 0; j < MAX_CLIENTS_PER_CHANNEL; j++) {
                if (channels[i]->connected_cli[j] && channels[i]->connected_cli[j] == cli) {
                    return i;
                }
            }
        }
    }
    return -1;
}

/* Retorna o índice do canal e do cliente com o nome especificado */
int find_channel_and_client (char* str, int* idx) {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i]) {

            for (int j = 0; j < MAX_CLIENTS_PER_CHANNEL; j++) {
                if (channels[i]->connected_cli[j] && strcmp(channels[i]->connected_cli[j]->name, str) == 0) {
                    *idx = j;
                    return i;
                }
            }
        }
    }
    return -1;
}

/* Retorna o id do canal buscado pelo nome */
int find_channel(char* str) {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i] && strcmp(channels[i]->key, str) == 0)
            return i;
    }
    return -1;
}

int add_client_to_channel(char* str, client_t* cli) {
	pthread_mutex_lock(&channels_mutex);

    // valida o nome do canal
    if (str[0] != '#' && str[0] != '&') {
	    pthread_mutex_unlock(&channels_mutex);
        return 0;
    }

    // procura por um canal existente
    int idx;
    if ((idx = find_channel(str)) > -1) {
        for (int i = 0; i < MAX_CLIENTS_PER_CHANNEL; ++i){
            if(!channels[idx]->connected_cli[i]) {
                channels[idx]->connected_cli[i] = cli;
                channels[idx]->n_cli++;
                pthread_mutex_unlock(&channels_mutex);
                return 1;
            }                          
        }
	}
    // cria novo canal
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

void remove_client_from_channel(int idx, client_t* cli) {
	pthread_mutex_lock(&channels_mutex);
    
    for (int i = 0; i < MAX_CLIENTS_PER_CHANNEL; i++) {
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

/* Adiciona um cliente à fila */
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

/* Remove um cliente da fila */
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


/* Função para enviar mensagem para todos os clientes no canal, exceto o remetente */
void send_message(char *s, client_t* cli){
    if (cli->is_muted) return;
	pthread_mutex_lock(&clients_mutex);

    int idx;
    if ((idx = find_channel_of_client(cli)) < 0) {
        pthread_mutex_unlock(&clients_mutex);
        return;
    }

	for(int i = 0; i < MAX_CLIENTS_PER_CHANNEL; ++i){
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

/* Função da thread para lidar com cada cliente */
void* handle_client(void *arg) {
    char msg[MSG_LEN];
    char buffer_out[BUFF_LEN];
    char name[NAME_LEN];
    client_t *cli = (client_t *) arg;

    // lendo nome do cliente
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
                    printf("command admin");
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

void print_image(FILE *fptr){
    char read_string[MAX_LEN];
 
    while(fgets(read_string,sizeof(read_string),fptr) != NULL)
        printf("%s",read_string);
}