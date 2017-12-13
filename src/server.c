#include<stdio.h>
#include<string.h>    //strlen
#include<stdlib.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>    //write

#include<pthread.h> //for threading , link with lpthread

void *connection_handler(void *);
void recieve_handler(int sock, char client_message[4096]);
void send_to_all(char client_message[4096]);
void *setup_socket_server();

int sockets[10];
int client_count = 0;

void *setup_socket_server()
{
    int socket_desc, new_socket, c , *new_sock;
    struct sockaddr_in server, client;
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
    fflush(stdout);
    c = sizeof(struct sockaddr_in);
    while( (new_socket = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) ) {

        pthread_t sniffer_thread;
        new_sock = malloc(1);
        *new_sock = new_socket;

        if( pthread_create( &sniffer_thread, NULL, connection_handler, (void*) new_sock) < 0) {
            perror("could not create thread");
            exit(1);
        }

        client_count++;

        printf("client connected - %d client(s)\n", client_count);
        fflush(stdout);
        //Now join the thread , so that we dont terminate before the thread
        pthread_join( sniffer_thread , NULL);
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
void send_to_all(char client_message[4096]) {
    printf("sending to %d clients", client_count);
    int i;
    for(i = 1; i <= client_count; i++) {
        printf("socket %d", sockets[i]);
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
    char client_message[4096];

    // assign new client to list
    sockets[client_count] = sock;

    while( (read_size = recv(sock , client_message , 4096 , 0)) > 0 ) {
        // Received message from client
        recieve_handler(sock, client_message);
    }

    if(read_size == 0) {
        // disconnected
        fflush(stdout);
    } else if(read_size == -1) {
        perror("recv failed");
    }

    client_count--;
    printf("client disconnected - %d client(s)\n", client_count);

    // Free the socket pointer
    free(socket_desc);

    return 0;
}

void recieve_handler(int sock, char client_message[4096]) {
    printf("message from client %i: %s\n", sock, client_message);
}
