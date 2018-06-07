#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>
#include<pthread.h>


void* handle_neighbor(void* arg);

int main(int argc, char* argv[])
{
    // Global parameters
    int nb_nodes;
    int min_per_active, max_per_active;
    int min_send_delay;
    int snapshot_delay;
    int max_number;

    // Node parameters
    int node_id;
    int port;

    int* neighbors;
    int nb_neighbors;

    // Socket information
    int s, s_current;
    int* s_client;
    struct sockaddr_in sin;
    struct sockaddr_in pin;
    int addrlen;
    pthread_t tid;
    pthread_attr_t attr;

    // Create TCP server socket
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("Error creating socket\n");
        exit(1);
    }

    // Fill in socket with host information
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);

    // Bind socket to address and port number
    if (bind(s, (struct sockaddr*) &sin, sizeof(sin)) == -1) {
        printf("Error on bind call.\n");
        exit(1);
    }

    // Set queuesize of pending connections
    if (listen(s, nb_neighbors + 10) == -1) {
        printf("Error on listen call\n");
        exit(1);
    }

    // Wait for all neighbors to connect to the server socket
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    addrlen = sizeof(pin);

    int i = 0;
    while (i < nb_neighbors) {
        if ((s_current = accept(s, (struct sockaddr *) &pin, (socklen_t*)&addrlen)) == -1) {
            printf("Error on accept call.\n");
            exit(1);
        }
        
        s_client = (int*) (malloc(sizeof(s_current)));
        *s_client = s_current;

        pthread_create(&tid, &attr, handle_neighbor, s_client);
        i++;
    }

    exit(0);
}

void* handle_neighbor(void* arg) 
{
}
