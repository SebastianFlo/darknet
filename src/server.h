#ifndef SERVER_H
#define SERVER_H

void *connection_handler(void *);
void recieve_handler(int sock, char client_message[2000]);
void send_to_all(char client_message[2000]);
void *setup_socket_server();

#endif
