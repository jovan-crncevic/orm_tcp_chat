#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT   27015
#define MAX_CLIENTS    20

typedef struct {
    int socket;
    struct sockaddr_in address;
    char username[20];
} ClientInfo;

ClientInfo clients[MAX_CLIENTS];
pthread_mutex_t clientsMutex = PTHREAD_MUTEX_INITIALIZER;

void *HandleClient(void *clientInfo);
void BroadcastMessage(char *username, char *message, int senderSocket);

int main(int argc, char *argv[]) {
    int socketDesc, clientSock, c;
    struct sockaddr_in server, client;
    pthread_t threadId;

    // Initialize clients array
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        clients[i].socket = -1;
    }

    // Create socket
    socketDesc = socket(AF_INET, SOCK_STREAM, 0);
    if (socketDesc == -1) {
        printf("Could not create socket");
        return 1;
    }
    puts("Socket created");

    // Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(DEFAULT_PORT);

    // Bind
    if (bind(socketDesc, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Bind failed");
        return 1;
    }
    puts("Bind done");

    // Listen
    listen(socketDesc, SOMAXCONN);

    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);

    while (1) {
        // Accept connection from an incoming client
        clientSock = accept(socketDesc, (struct sockaddr *)&client, (socklen_t *)&c);
        if (clientSock < 0) {
            perror("Accept failed");
            return 1;
        }
        puts("Connection accepted");

        // Receive username from client
        char username[20];
        memset(username, 0, sizeof username);
        if (recv(clientSock, username, sizeof(username), 0) < 0) {
            perror("Receive failed");
            close(clientSock);
            continue;
        }

        pthread_mutex_lock(&clientsMutex);

        // Find an empty slot in the clients array
        int i;
        for (i = 0; i < MAX_CLIENTS; ++i) {
            if (clients[i].socket == -1) {
                clients[i].socket = clientSock;
                clients[i].address = client;
                strncpy(clients[i].username, username, sizeof(clients[i].username));
                break;
            }
        }

        pthread_mutex_unlock(&clientsMutex);

        // Create a new thread to handle the client
        if (pthread_create(&threadId, NULL, HandleClient, (void *)&clients[i]) < 0) {
            perror("Could not create thread");
            return 1;
        }

        // Detach the thread so that its resources are automatically released
        pthread_detach(threadId);
    }

    return 0;
}

void *HandleClient(void *clientInfo) {
    ClientInfo *client = (ClientInfo *)clientInfo;
    int readSize;
    char clientMessage[DEFAULT_BUFLEN];

    // Receive a message from client
    while ((readSize = recv(client->socket, clientMessage, DEFAULT_BUFLEN, 0)) > 0) {
        printf("%s: %s\n", client->username, clientMessage);

        // Broadcast the received message to all clients
        BroadcastMessage(client->username, clientMessage, client->socket);
        memset(clientMessage, 0, DEFAULT_BUFLEN);
    }

    if (readSize == 0) {
        printf("Client disconnected: %s:%d\n", inet_ntoa(client->address.sin_addr), ntohs(client->address.sin_port));
        fflush(stdout);
    }
    else if (readSize == -1) {
        perror("Receive failed");
    }

    // Remove the client from the array
    pthread_mutex_lock(&clientsMutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (&clients[i] == client) {
            clients[i].socket = -1;
            break;
        }
    }
    pthread_mutex_unlock(&clientsMutex);

    // Close the socket for this client
    close(client->socket);

    // Exit the thread
    pthread_exit(NULL);
}

void BroadcastMessage(char *username, char *message, int senderSocket) {
    char formattedMessage[DEFAULT_BUFLEN + 30];

    pthread_mutex_lock(&clientsMutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].socket != -1 && clients[i].socket != senderSocket) {
            snprintf(formattedMessage, sizeof(formattedMessage), "%s: %s", username, message);
            ssize_t sentBytes = send(clients[i].socket, formattedMessage, strlen(formattedMessage), 0);
            if (sentBytes <= 0) {
                // Handle error or client disconnect
                close(clients[i].socket);
                clients[i].socket = -1;
            }
        }
    }
    pthread_mutex_unlock(&clientsMutex);
}