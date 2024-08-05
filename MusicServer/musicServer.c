#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

int P,N;
char *DIR;

int client_count=0;

//structure to store ip addresses of the clients as a linked list
struct client_fd_ip {
    int client_fd;
    char *client_ip;
    struct client_fd_ip *next;
};

struct client_fd_ip *head=NULL;
struct client_fd_ip *tail=NULL;

pthread_mutex_t lock;

//Finding the descriptor in linklist
void find(int cl_fd,int song_num){
    struct client_fd_ip *temp=head;
    while(temp!=NULL){
        if(temp->client_fd==cl_fd){
            printf("Client with IP: %s asking for song number %d\n\n",temp->client_ip,song_num);
            break;
        }
        temp=temp->next;
    }
}

void* handle_connection(void* client_point){
    int c_sock=*((int*)client_point);
    free(client_point);

    char song_str[100];
    char dir[1000];
    recv(c_sock, song_str,sizeof(song_str),0);

    int song=atoi(song_str);

    //Function to find ip of client
    find(c_sock,song);

    //Directory parsing with song name
    sprintf(song_str, "%d", song);
    strcat(song_str, ".mp3");
    sprintf(dir, "%s/%s",DIR,song_str);

    FILE *music=fopen(dir,"rb");

    if(music==NULL){
        printf("Doesnt exist\n");
    }else{
        char buffer[4096];
        ssize_t bytes_sent;

        //Sending music files to client
        while((bytes_sent=fread(buffer,1,sizeof(buffer),music))> 0){
            if(send(c_sock,buffer,bytes_sent,0)==-1){
                perror("Error sending music file");
                exit(EXIT_FAILURE);
            }
        }        

        //using locks to isolate clients updating the client_count
        pthread_mutex_lock(&lock);
        client_count--;
        pthread_mutex_unlock(&lock);

        fclose(music);
        close(c_sock);
    }

}


int main(int argc,char **argv){
    if(argc!=4){
        printf("Invalid no of commands\n");
        exit(1);
    }else{
        //Comand Line arguments
        P=atoi(argv[1]);
        DIR=argv[2];
        N=atoi(argv[3]);

        struct sockaddr_in addr;
        int addrlen = sizeof(addr);
        int s_sock,c_sock;

        // SOCKET - Create TCP socket
        if((s_sock=socket(AF_INET,SOCK_STREAM,0))==0){
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
        }

        // Set server address details
        addr.sin_family=AF_INET;
        addr.sin_addr.s_addr=htonl(INADDR_ANY);
        addr.sin_port=htons(P);

        // BIND - Bind socket to address and port
        if(bind(s_sock,(struct sockaddr *) &addr,sizeof(addr))<0){
            perror("Binding failed");
            exit(EXIT_FAILURE);
        }

        // LISTEN - Start listening for incoming connections
        if(listen(s_sock,N)<0){
            perror("Listening failed");
            exit(EXIT_FAILURE);
        }

        while(1){
            // Condition of more than N clients,we skip the connection till some client closes.
            pthread_mutex_lock(&lock);
            if(client_count>N){
                continue;
            }else{
                printf("Waiting for new connection\n");
            }
            pthread_mutex_unlock(&lock);

            // ACCEPT - accept incoming connection
            if((c_sock=accept(s_sock, (struct sockaddr *)&addr, (socklen_t*)&addrlen))<0){
                perror("Acceptance failed");
                exit(EXIT_FAILURE);
            }

            pthread_mutex_lock(&lock);
            client_count++;
            printf("Connection established\n");
            pthread_mutex_unlock(&lock);

            //Getting client address
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(addr.sin_addr), client_ip, INET_ADDRSTRLEN);

            //Inserting node in ll
            if(head==NULL){
                struct client_fd_ip *node=(struct client_fd_ip *)malloc(sizeof(struct client_fd_ip));
                node->client_fd=c_sock;
                node->client_ip=client_ip;
                node->next=NULL;
                head=node;
                tail=node;
            }else{
                struct client_fd_ip *node=(struct client_fd_ip *)malloc(sizeof(struct client_fd_ip));
                node->client_fd=c_sock;
                node->client_ip=client_ip;
                node->next=NULL;
                tail->next=node;
                tail=node;
            }

            //Creating Threads of clients to work simultaneoulsy and effiecintly
            pthread_t thread;
            int *client_pointer=malloc(sizeof(int));
            *client_pointer=c_sock;
            if(pthread_create(&thread, NULL, handle_connection, (void *)client_pointer) < 0){
                perror("Failed to create thread");
                exit(EXIT_FAILURE);
            }    
            printf("\n");
        }
        close(s_sock);
    }
}