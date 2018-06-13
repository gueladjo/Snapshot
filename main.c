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

#include "config.h"

#define BUFFERSIZE 512

typedef struct Neighbor {
    int id;
    int port;
    char hostname[100];
} Neighbor;

<<<<<<< HEAD
void* handle_neighbor(void* arg);
=======
enum State {Passive = 0, Active = 1};

void* handle_neighbor(void* arg);
void parse_buffer(char* buffer, size_t* rcv_len);
int handle_message(char* message, size_t length);

enum State node_state;
int* timestamp;
>>>>>>> pr/1

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

    // Set up neighbors information and initialize vector timestamp
    int nb_neighbors;
    Neighbor* neighbors = malloc(nb_neighbors * sizeof(Neighbor));
    timestamp = malloc(nb_nodes * sizeof(int));
    memset(timestamp, 0, nb_nodes * sizeof(int));

    // Set state of the node
    if ((node_id % 2) == 0) {
        node_state = Active;
    }
    else {
        node_state = Passive;
    }

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
<<<<<<< HEAD
    
}
=======
    // Initialize buffer and size variable
    int count = 0;
    size_t rcv_len = 0;
    char buffer[BUFFERSIZE];

    int s = *((int*) arg);
    free(arg);

    while (1) {
        if ((count = recv(s, buffer + rcv_len, BUFFERSIZE - rcv_len, 0) == -1)) {
            printf("Error during socket read.\n");
            close(s);
            exit(1); 
        }
        else if (count > 0) {
            rcv_len = rcv_len + count;
            parse_buffer(buffer, &rcv_len);
        }

    }
}

void parse_buffer(char* buffer, size_t* rcv_len)
{
    // Check if we have enough byte to read message length
    while (*rcv_len > 4 ) {
        size_t message_len = buffer[3];

        // Check if we received a whole message
        if (*rcv_len < 4 + message_len) 
           break; 

        // Handle message received
        handle_message(buffer, message_len + 4);

        // Remove message from buffer and shuffle bytes of next message to start of the buffer
        *rcv_len = *rcv_len - 4 - message_len;
        if (*rcv_len != 0) {
            memmove(buffer, buffer + 4 + message_len, *rcv_len);
        }
    }
}

// Check type of message (application or marker) and process it
int handle_message(char* message, size_t length)
{
    return 0;
}

>>>>>>> pr/1
