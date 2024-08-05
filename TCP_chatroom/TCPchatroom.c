#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>

#define BUFFER_SIZE 1024
#define NAME_SIZE 50
/**
 * CS3205 Assignment 2
 * Summary:
 * This file contains the code to run a chatroom client.
 */

// -------- Structs --------
typedef struct client_info{
    char name[NAME_SIZE];
    int client_id;
    int fd;
    bool flag_timeout_enabled;
    time_t last_time;
    struct sockaddr_in address;
} client_info;

typedef struct thread_args{
    struct client_info* client;
    struct client_info** clients;
} thread_args;
// -------------------------


// -------- Global variables ----------
int PORT;
int MAX_CLIENTS;
int TIMEOUT;
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for thread safety
pthread_mutex_t timeout_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for thread safety
// ------------------------------------


/*==================== Check for max clients =================*/
bool max_clients_reached(struct client_info** clients){
    /*------------- LOCKED -------------*/
    pthread_mutex_lock(&client_mutex); 
    int count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++){
        // check for non-empty slot by iterating array
        if (clients[i] != NULL){
            count ++;
            break;
        }
    }
    pthread_mutex_unlock(&client_mutex);
    /*------------- UNLOCKED -------------*/
}
/*==============================================================*/


/*================ Adding to queue =================*/
void add_client(struct client_info** clients, struct client_info* client){
    /*------------- LOCKED -------------*/
    pthread_mutex_lock(&client_mutex); 
    for (int i = 0; i < MAX_CLIENTS; i++){
        // check for empty slot by iterating array
        if (clients[i] == NULL){
            clients[i] = client;
            break;
        }
    }
    pthread_mutex_unlock(&client_mutex);
    /*------------- UNLOCKED -------------*/
}
void remove_client(struct client_info** clients, int client_id){
    /*------------- LOCKED -------------*/
    pthread_mutex_lock(&client_mutex); 
    for (int i = 0; i < MAX_CLIENTS; i++){
        if (clients[i] != NULL && clients[i]->client_id == client_id){
            free(clients[i]);
            clients[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&client_mutex);
    /*------------- UNLOCKED -------------*/
}
/*================ Adding to queue =================*/


/*================ SENDING MESSAGE ==================*/
void broadcast_message(struct client_info** clients, struct client_info* client, char *message){
    /*------------- LOCKED ----------------*/
	pthread_mutex_lock(&client_mutex);
    int client_id = client->client_id; 
	for(int i=0; i<MAX_CLIENTS; ++i){
		if(clients[i] != NULL){
			if(clients[i]->client_id != client_id){
                char buffer[2*BUFFER_SIZE];
                memset(buffer,0,2*BUFFER_SIZE);
                strncpy(buffer, client->name,strlen(client->name));
                strcat(buffer, ": ");
                strcat(buffer,message);
				if(write(clients[i]->fd, buffer, strlen(buffer)) < 0){
					perror("[broadcast_message]: write to descriptor failed");
					break;
				}
			}
		}
	}
	pthread_mutex_unlock(&client_mutex);
    /*-------------- UNLOCKED --------------*/
}
void list_users(struct client_info** clients, int fd){
    /*------------- LOCKED ----------------*/
    pthread_mutex_lock(&client_mutex);
    write(fd, "Following users are online: \n", 29);
    for(int i = 0; i < sizeof(clients); i++){
        if (clients[i] != NULL){
            write(fd, clients[i]->name, strlen(clients[i]->name));
            write(fd, "\n", 1);
        }
    }
	pthread_mutex_unlock(&client_mutex);
    /*-------------- UNLOCKED --------------*/
}
/*================ SENDING MESSAGE ==================*/


/*================ CLIENT HANDLER ==================*/
void client_handler(void* arg){
    struct client_info** clients = (struct client_info**) ((struct thread_args*)arg)->clients;
    struct client_info* client = (struct client_info*) ((struct thread_args*)arg)->client;
    // We get the client info from the thread arguments structure

    int fd = client->fd;

    /*------------------------- GET USERNAME -----------------------*/
    char username[NAME_SIZE];
    write(fd, "Enter your username: \n", 22);
    int read_val = read(fd, username, NAME_SIZE);
    client->last_time = time(NULL);
    username[read_val] = '\0';
    if(read_val<0){
        // READ FAILED
        perror("[client_handler]: Read failed \n");
        remove_client(clients, client->client_id);
        close(fd);
        return;
    }
    if(read_val == 0){
        // abrupt exit 
        remove_client(clients, client->client_id);
        close(fd);
        return;
    }
    if(strlen(username) >= NAME_SIZE){
        // Username exceeding size limit
        write(fd, "Username exceeding size limit. EXITING\n", 43);
        remove_client(clients, client->client_id);
        close(fd);
        return;
    }
    /* CHECK USERNAME */
    for(int i = 0; i < sizeof(clients); i++){
        if (clients[i] != NULL){
            if (strcmp(clients[i]->name, username) == 0){
                write(fd, "Username already taken. EXITING\n", 41);
                remove_client(clients, client->client_id);
                close(fd);
                return;
            }
        }
    }
    strncpy(client->name, username, strlen(username));
    /* ------------------------------------------------------------- */

    /* -------------------- WELCOME MESSAGE ------------------------ */
    write(fd, "Welcome to the chatroom\n", 24);
    list_users(clients, client->fd); // list all users

    // broadcast message.
    char joining_message[7 + strlen(client->name) + 25 + 1];
    strncpy(joining_message, "Client ", 7);
    strcat(joining_message, client->name);
    strcat(joining_message, " has joined the chatroom\n");
    broadcast_message(clients, client, joining_message);
    /*---------------------------------------------------------------*/
    
    client->last_time = time(NULL);
    client->flag_timeout_enabled = true; // setting flag so that timeout checker can check for timeout

    char buffer[BUFFER_SIZE];
    while (1){
        printf("Waiting for message from %d\n", fd);
        int read_val = recv(fd, buffer, BUFFER_SIZE, 0);

        printf("%d %d\n", read_val, fd);
        
        if (read_val < 0){
            // READ FAILED
            printf("[client_handler]: Read failed in fd %d \n", fd);
            // exit(EXIT_FAILURE);
            return;
        }
        else if (read_val == 0){
            // abrupt exit 
            char message[7+strlen(username)+23+1];
            strncpy(message, "Client ",7);
            strcat(message, client->name);
            strcat(message, " has left the chatroom\n");
            broadcast_message(clients, client, message);
            remove_client(clients, client->client_id);
            break;
        }
        buffer[read_val]='\0';
        client->last_time = time(NULL);
        if (strncmp(buffer, "\\list",6)==0){
            // CLIENT COMMAND to list users in network
            list_users(clients, client->fd);
            continue;
        }
        if (strcmp(buffer, "\\bye")==0){
            // CLIENT COMMAND to Exit chatroom
            char message[7+strlen(username)+23+1];
            strncpy(message, "Client ",7);
            strcat(message, client->name);
            strcat(message, " has left the chatroom\n");
            broadcast_message(clients, client, message);
            remove_client(clients, client->client_id);
            break;
        }
        // Broadcast normal message
        broadcast_message(clients, client, buffer);
        memset(buffer, 0, BUFFER_SIZE);
    }
    close(fd);
    printf("closed fd: %d\n", fd);
}
/*================ CLIENT HANDLER ==================*/


/*================ TIMEOUT CHECKER ==================*/
void timeout_checker(void* arg){
    struct client_info** clients = (struct client_info**) arg;
    while(1){
        for(int i = 0 ; i < MAX_CLIENTS; i++){
            if (clients[i] != NULL){
                if (clients[i]->flag_timeout_enabled && (time(NULL) - clients[i]->last_time >= TIMEOUT)){
                    char message[7+strlen(clients[i]->name)+23+1];
                    strncpy(message, "Client ",7);
                    strcat(message, clients[i]->name);
                    strcat(message, " has left the chatroom\n");
                    broadcast_message(clients, clients[i], message);
                    int fd = clients[i]->fd;
                    remove_client(clients, clients[i]->client_id);
                    write(fd, "You have been timed out\n", 24);
                    close(fd);
                    printf("closed fd: %d\n", fd);
                }
            }
        }
    }
}
/*================ TIMEOUT CHECKER ==================*/

// Subroutines in main flow
int create_socket();
void bind_server(int, struct sockaddr_in);
int accept_client(int, struct sockaddr_in);


int main(int argv, char** argc)
{
    // Get command line args
    if(argv != 4){
        printf("invalid number of arguments\n");
        exit(EXIT_FAILURE);
    }
    PORT = atoi(argc[1]);
    MAX_CLIENTS = atoi(argc[2]);
    TIMEOUT = atoi(argc[3]); // in seconds
    // ---------------------


    // Setup array for clients
    struct client_info** clients;
    clients = (client_info**)malloc((sizeof(struct client_info*))*MAX_CLIENTS);
    // ---------------------


    // Setup server and listen
    int server_fd = create_socket();
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    bind_server(server_fd, serv_addr);
    if (listen(server_fd, MAX_CLIENTS) < 0){
        perror("[listen_for_client]: Listen failed \n");
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d\n", PORT);
    // ---------------------
    

    // Create thread for timeout checker
    pthread_t timeout_thread;
    pthread_create(&timeout_thread, NULL, timeout_checker, (void *) clients);
    //---------------------


    int client_count = 0;


    while (1){
        // Connecting to client
        int client_fd = accept_client(server_fd, serv_addr);
	    char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(serv_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        // Check for available client slot
        if (max_clients_reached(clients)){
            printf("Client limit reached. Connection rejected\n");
            printf("Closing connection with %s:%d\n", client_ip, ntohs(serv_addr.sin_port));
            close(client_fd);
            continue;
        }
        // Connected
        printf("Client connected from %s:%d\n", client_ip, ntohs(serv_addr.sin_port));
        // ---------------------


        // Setup client information
        struct client_info* client = (struct client_info*) malloc(sizeof(struct client_info));
        client->client_id = client_count++;
        client->fd = client_fd;
        client->address = serv_addr;
        client->flag_timeout_enabled = false;
        add_client(clients, client);
        // ---------------------


        // Create thread for client
        pthread_t client_thread;
        struct thread_args* args = (struct thread_args*) malloc(sizeof(struct thread_args));
        args->client = client;
        args->clients = clients;
        pthread_create(&client_thread, NULL, client_handler, (void *) args);
        // ---------------------
    }
    close(server_fd);
    return EXIT_SUCCESS;
}

int create_socket()
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("[create_socket]: Socket creation error \n");
        exit(EXIT_FAILURE);
    }
    return sock;
}
void bind_server(int sock, struct sockaddr_in serv_addr)
{
    if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("[bind_server]: Bind failed \n");
        exit(EXIT_FAILURE);
    }
}
int accept_client(int sock, struct sockaddr_in serv_addr)
{
    int addrlen = sizeof(serv_addr);
    int new_socket = accept(sock, (struct sockaddr *)&serv_addr, (socklen_t *)&addrlen);
    if (new_socket < 0)
    {
        perror("[accept_client]: Accept failed \n");
        exit(EXIT_FAILURE);
    }
    return new_socket;
}

