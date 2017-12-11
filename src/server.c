#include<stdio.h>
#include<string.h>    //strlen
#include<stdlib.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>    //write

#include<pthread.h> //for threading , link with lpthread

void *connection_handler(void *);
void recieve_handler(int sock, char client_message[2000]);
void send_to_all(char client_message[2000]);
void *setup_socket_server();

int sockets[10];
int socket_count = 0;

void *setup_socket_server() 
{
    int socket_desc;
    int new_socket;
    int c;
    int *new_sock;
    struct sockaddr_in server, client;
    char *message;
    int port = 6000;

    // Create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) {
        printf("Could not create socket");
    }

    // Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( port );

    //Bind
    if( bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0) {
        puts("bind failed");
        exit(1);
    }

    //Listen
    listen(socket_desc, 3);

    //Accept and incoming connection
    printf("Waiting for incoming connections on port %i...\n", port);
    c = sizeof(struct sockaddr_in);
    while( (new_socket = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) ) {

        pthread_t sniffer_thread;
        new_sock = malloc(sizeof *new_sock);
        *new_sock = new_socket;

        if( pthread_create( &sniffer_thread, NULL, connection_handler, (void*) new_sock) < 0) {
            perror("could not create thread");
            exit(1);
        }

        socket_count++;

        printf("client connected - %d client(s)\n", socket_count);

        //Now join the thread , so that we dont terminate before the thread
        // pthread_join( sniffer_thread , NULL);
        puts("Handler assigned");
    }

    if (new_socket<0) {
        perror("accept failed");
        exit(1);
    }
    return 0;
}

/**
 * Broadcast message to all connected clients
 *
 * @param {char[2000]} message to send
 */
void send_to_all(char client_message[2000]) {
    int i;
    for(i = 1; i <= socket_count; i++) {
        write(sockets[i], client_message, strlen(client_message));
    }
}

/*
 * This will handle connection for each client
 * */
void *connection_handler(void *socket_desc) {
    // Get the socket descriptor
    int sock = *(int*)socket_desc;
    int read_size;
    char *message , client_message[2000];

    // assign new client to list
    sockets[socket_count] = sock;

    while( (read_size = recv(sock , client_message , 2000 , 0)) > 0 ) {
        // Received message from client
        recieve_handler(sock, client_message);
    }

    if(read_size == 0) {
        // disconnected
        fflush(stdout);
    } else if(read_size == -1) {
        perror("recv failed");
    }

    socket_count--;
    printf("client disconnected - %d client(s)\n", socket_count);

    // Free the socket pointer
    free(socket_desc);

    return 0;
}

void recieve_handler(int sock, char client_message[2000]) {
    printf("message from client %i: %s\n", sock, client_message);
}
