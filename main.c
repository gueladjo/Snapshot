#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>
#include<pthread.h>
#include"config.h"

typedef struct Neighbor {
    int id;
    int port;
    char hostname[100];
} Neighbor;

void* handle_neighbor(void* arg);

int main(int argc, char* argv[])
{
    // Global parameters
    config system_config;
    int nb_nodes;
    int min_per_active, max_per_active;
    int min_send_delay;
    int snapshot_delay;
    int max_number;

    // Node parameters
    int node_id;
    int port;

    read_config_file(&system_config);
    //display_config(system_config);

    nb_nodes = system_config.nodes_in_system;
    min_per_active = system_config.min_per_active;
    max_per_active = system_config.max_per_active;
    min_send_delay = system_config.min_send_delay;
    snapshot_delay = system_config.snapshot_delay;
    max_number = system_config.max_number;

    int nb_neighbors;
    Neighbor* neighbors = malloc(nb_neighbors * sizeof(Neighbor));

    // Client sockets information
    int* s_client = malloc(nb_neighbors * sizeof(int));
    struct hostent* h;

    // Server Socket information
    int s, s_current;
    int* s_neighbor;
    struct sockaddr_in sin;
    struct sockaddr_in pin;
    int addrlen;
    pthread_t tid;
    pthread_attr_t attr;

    // Create client sockets to neighbors of the node
    int j = 0;
    for (j = 0; j < nb_neighbors; j++) {
        // Create TCP socket
        if ((s_client[j] = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            printf("Error creating socket\n");
            exit(1); 
        }

        // Get host info
        if ((h = gethostbyname(neighbors[j].hostname)) == 0) {
            printf("Error on gethostbyname\n");
            exit(1);
        }

        // Fill in socket address structure with host info
        memset(&pin, 0, sizeof(pin));
        pin.sin_family = AF_INET;
        pin.sin_addr.s_addr = ((struct in_addr *)(h->h_addr))->s_addr;
        pin.sin_port = htons(neighbors[j].port);

        // Connect to port on neighbor
        if (connect(s_client[j], (struct sockaddr *) &pin, sizeof(pin)) == -1) {
            printf("Error when connecting to neighbor\n");
            exit(1);
        }
    }

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

    // Create thread for receiving each neighbor messages
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    addrlen = sizeof(pin);

    int i = 0;
    while (i < nb_neighbors) {
        if ((s_current = accept(s, (struct sockaddr *) &pin, (socklen_t*)&addrlen)) == -1) {
            printf("Error on accept call.\n");
            exit(1);
        }
        
        s_neighbor = (int*) (malloc(sizeof(s_current)));
        *s_neighbor = s_current;

        pthread_create(&tid, &attr, handle_neighbor, s_neighbor);
        i++;
    }
    exit(0);
}

// Reads incoming messages from neighbors and places them in a global queue
void* handle_neighbor(void* arg) 
{
    
}