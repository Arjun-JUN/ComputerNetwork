#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

int PORT;
char *DIR;

//Get the mime type
char* mime_type(char *file){
    char type[1024];

    //Parsing
    int index = strlen(file)-1;
    while(index>=0){
        if(file[index]=='.'){
            strcpy(type,file+index+1);
            break;
        }
        index--;
    }
    //Finding mime type for a set of diff file types
    if(strcmp(type,"html")==0){
        return "text/html";
    }
    else if(strcmp(type,"css")==0){
        return "text/css";
    }
    else if(strcmp(type,"js")==0){
        return "text/javascript";
    }
    else if(strcmp(type,"jpeg")==0){
        return "text/jpeg";
    }
    else if(strcmp(type,"jpg")==0){
        return "text/jpeg";
    }
    else if(strcmp(type,"png")==0){
        return "text/png";
    }
}

//Slicing the second string
char *mid_string(char *req,char *delim){    
    char* temp = strstr(req, delim);
    char* buffer = (char *)malloc(10000);
    strcpy(buffer, temp);
    buffer=buffer+strlen(delim);
    *strstr(buffer,delim)='\0';

    return buffer;
}

//Function to send appropriate file content
void work(int c_sock,char *dir){
    FILE *file=fopen(dir,"rb");
    //FILE Not Found
    if(file==NULL){
        char *temp;
        temp = malloc(1000);
        strcpy(temp,DIR);
        work(c_sock,strcat(temp,"/404.html"));
        return;
    }

    char reply[10000];
    
    sprintf(reply,"HTTP/1.1 200 OK\r\nContent-Type: %s\r\n\r\n",mime_type(dir));
    send(c_sock,reply,strlen(reply),0);

    size_t bytes_read;
    while((bytes_read=fread(reply,1,sizeof(reply),file))>0){
        send(c_sock,reply,bytes_read,0);
    }

    fclose(file);
}

void get(int c_sock,char *req){
    char dir[10000];
    strcpy(dir,DIR);
    char *mid_str=mid_string(req," ");
    strcat(dir,mid_str);

    printf("[IN GET]: dir = %s\n",dir);
    //No Webpage mentioned
    if((strlen(mid_str)==1) && (mid_str[0]=='/')){
        work(c_sock,strcat(dir,"index.html"));
    }else{
        work(c_sock,dir);
    }
}

void post(int c_sock,char *req){
    char* mid_str=mid_string(req,"%**%");
    int count_charecters=0,count_words=0,count_sentences=0;

    //Counting
    for(int i=0;i<strlen(mid_str);i++){
        if(mid_str[i]==' '){
            count_words++;
        }else if(mid_str[i]=='.'){
            count_sentences++;
        }
        count_charecters++;
    }

    //Formatted reply to the webpage
    char reply[10000];
    sprintf(reply, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n"
                      "Number of characters: %d\n"
                      "Number of words: %d\n"
                      "Number of sentences: %d\n", count_charecters, count_words+1, count_sentences);

    send(c_sock,reply,strlen(reply),0);
}

//Handling webpage requests
void* handle_connection(void* client_point){
    int c_sock=*((int*)client_point);
    free(client_point);

    char http_req[10000];
    recv(c_sock, http_req,sizeof(http_req),0);
    //GET
    if(http_req[0]=='G',http_req[1]=='E',http_req[2]=='T'){
        printf("Get request\n");
        get(c_sock,http_req);
    //POST
    }else if(http_req[0]=='P',http_req[1]=='O',http_req[2]=='S',http_req[3]=='T'){
        printf("Post request\n");
        post(c_sock,http_req);
    }else{
        printf("Wrong request\n");
    }

    close(c_sock);
}


int main(int argc,char **argv){
    if(argc!=3){
        printf("Invalid no of commands\n");
        exit(1);
    }else{
        //Comand Line arguments
        PORT=atoi(argv[1]);
        DIR=argv[2];

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
        addr.sin_port=htons(PORT);

        // BIND - Bind socket to address and port
        if(bind(s_sock,(struct sockaddr *) &addr,sizeof(addr))<0){
            perror("Binding failed");
            exit(EXIT_FAILURE);
        }

        // LISTEN - Start listening for incoming connections
        if(listen(s_sock,10)<0){
            perror("Listening failed");
            exit(EXIT_FAILURE);
        }

        while(1){
            // ACCEPT - accept incoming connection
            printf("Waiting for new connection\n");
            if((c_sock=accept(s_sock, (struct sockaddr *)&addr, (socklen_t*)&addrlen))<0){
                perror("Acceptance failed");
                exit(EXIT_FAILURE);
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