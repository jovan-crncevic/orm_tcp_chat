#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT   27015

void *ReceiveMessages(void *socketDesc);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: ./client <username>\n");
        return 1;
    }

    char *username = argv[1];

    int sock;
    struct sockaddr_in server;
    char message[DEFAULT_BUFLEN];
    pthread_t receiveThread;

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        printf("Could not create socket");
        return 1;
    }
    puts("Socket created");

    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons(DEFAULT_PORT);

    // Connect to the remote server
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Connect failed. Error");
        return 1;
    }

    // Send username to the server
    if (send(sock, username, strlen(username), 0) < 0) {
        puts("Send failed");
        return 1;
    }

    puts("Connected\n");

    // Create a thread for receiving messages
    if (pthread_create(&receiveThread, NULL, ReceiveMessages, (void *)&sock) < 0) {
        perror("Could not create thread");
        return 1;
    }

    // Send multiple messages
    while (1) {
        fgets(message, DEFAULT_BUFLEN, stdin);

        if (send(sock, message, strlen(message), 0) < 0) {
            puts("Send failed");
            break;
        }
    }

    // Wait for the receiving thread to finish
    pthread_join(receiveThread, NULL);

    close(sock);

    return 0;
}

void *ReceiveMessages(void *socketDesc) {
    int sock = *((int *)socketDesc);
    char message[DEFAULT_BUFLEN];

    // Receive and display messages from the server
    while (1) {
        memset(message, 0, DEFAULT_BUFLEN);

        if (recv(sock, message, DEFAULT_BUFLEN, 0) < 0) {
            puts("Receive failed");
            break;
        }

        printf("%s", message);
    }

    pthread_exit(NULL);
}