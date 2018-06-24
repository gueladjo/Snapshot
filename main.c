#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<time.h>
#include<netinet/in.h>
#include<netdb.h>
#include<pthread.h>
#include "config.h"

#define BUFFERSIZE 512
#define APP_MSG 'A'
#define MARKER_MSG 'M'

typedef struct Neighbor {
    int id;
    int port;
    char hostname[100];
    int server_socket; // I'm not sure if this is the right terminology to call this the 'server' socket, but it's the new socket created from the accept() call
    int client_socket;
} Neighbor;

enum Color {Blue = 0, Red = 1};
enum Channel {Empty = 0, NotEmpty = 1};
enum State {Passive = 0, Active = 1};
enum Marker {Received = 0, NotReceived = 1};

typedef struct Snapshot {
    enum Color color;
    enum State state;
    int* timestamp;
    enum Channel channel;
    enum Marker* neighbors;
    int nb_marker;
} Snapshot;

Snapshot* snapshot;

int nb_snapshots;

void* handle_neighbor(void* arg);
void parse_buffer(char* buffer, size_t* rcv_len);
int handle_message(char* message, size_t length);
void send_marker_messages(int sent_by, int snapshot_id) ;
void send_msg(int sockfd, char * buffer, int msglen);
void record_snapshot();
void activate_node();
void* detect_termination();

// Global parameters
int nb_nodes;
int min_per_active, max_per_active;
int min_send_delay;
int snapshot_delay;
int max_number;

// Node Paramters
int node_id;
int port;
enum State node_state;
int* timestamp;
int* s_neighbor; 

int msgs_sent; // Messages sent by this node, to be compared with max_number
int msgs_to_send; // Messages to send on this active session (between min and maxperactive)
Neighbor* neighbors;
int nb_neighbors;

int main(int argc, char* argv[])
{

    // Config struct filled when config file parsed
    config system_config; 

    srand(time(NULL));

    read_config_file(&system_config);
    //display_config(system_config); 

    nb_nodes = system_config.nodes_in_system;
    min_per_active = system_config.min_per_active;
    max_per_active = system_config.max_per_active;
    min_send_delay = system_config.min_send_delay;
    snapshot_delay = system_config.snapshot_delay;
    max_number = system_config.max_number;

    // Set up neighbors information and initialize vector timestamp
    neighbors =  malloc(nb_neighbors * sizeof(Neighbor));
    snapshot =  malloc(100 * sizeof(Snapshot));
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
    struct hostent* h;

    // Server Socket information
    int s;
    struct sockaddr_in sin;
    struct sockaddr_in pin;
    int addrlen;
    pthread_t tid;
    pthread_attr_t attr;

    // Create client sockets to neighbors of the node
    int j = 0;
    for (j = 0; j < nb_neighbors; j++) {
        // Create TCP socket
        if ((neighbors[j].client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
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
        if (connect(neighbors[j].client_socket, (struct sockaddr *) &pin, sizeof(pin)) == -1) {
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
        if ((neighbors[i].server_socket = accept(s, (struct sockaddr *) &pin, (socklen_t*)&addrlen)) == -1) {
            printf("Error on accept call.\n");
            exit(1);
        }
        pthread_create(&tid, &attr, handle_neighbor, &(neighbors[i].server_socket));
        i++;
    }

    // Create termination detection thread if node id is 0
    if (node_id == 0) {
        pthread_t pid;
        pthread_create(&pid, &attr, detect_termination, NULL);
    }

    long last_sent_time = 0;
    char msg[5];
    while (1)
    {
        if (node_state == Active)
        {
            if (msgs_to_send > 0)
            {
                struct timespec current_time;
                clock_gettime(CLOCK_REALTIME, &current_time);
                if (min_send_delay < current_time.tv_nsec/1000000 - last_sent_time);
                {
                    // Source | Dest | Protocol | Length | Payload                    
                    Neighbor neighborToSend = neighbors[(rand() % nb_neighbors)];
                    snprintf(msg, 5, "%d%dA0", node_id, neighborToSend.id);
                    send_msg(neighborToSend.server_socket, msg, 4);
                    last_sent_time = current_time.tv_nsec/100000;
                    msgs_to_send--;
                }
            }
            else
            {
                node_state = Passive;
            }
        }
        else // if node is passive
        {

        }
    }

    exit(0);
}

// Reads incoming messages from neighbors and places them in a global queue
void* handle_neighbor(void* arg) 
{
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
// Source | Dest | Protocol | Length | Payload
int handle_message(char* message, size_t length)
{
    if (message[2] == APP_MSG)
    {
        if (node_state == Passive)
        {
            if (msgs_sent < max_number) // Turn active if max_number hasn't been reached, otherwise stay passive
            {
                activate_node();
            }
        }
        else // if node_state already active, do nothing
        {
        }
    }
    else if (message[2] == MARKER_MSG)
    {
        record_snapshot(message);
    }
}

void send_marker_messages(int sent_by, int snapshot_id) 
{
    int i;
    char msg[5];
    for (i = 0; i < nb_neighbors; i++)
    {
        if (neighbors[i].id != sent_by) // Don't sent marker msg to sender
        {
            snprintf(msg, 5, "%d%dM1%d", node_id, neighbors[i].id, snapshot_id);
            send_msg(neighbors[i].client_socket, msg, 4);
        }
    }
}

// Function to send whole message
void send_msg(int sockfd, char * buffer, int msglen)
{
    int bytes_to_send = msglen; // |Source | Destination | Protocol ('M') | Length (0)
    while (bytes_to_send > 0)
    {
        bytes_to_send -= send(sockfd, buffer + (msglen - bytes_to_send), msglen, 0);
    }
}

void activate_node()
{
    msgs_to_send = (rand() % (max_per_active + 1 - min_per_active)) + min_per_active; // Between max and min per active
    node_state = Active;
}


void record_snapshot(char* message)
{
    char sent_by = message[0];
    // Format up to change
    char snapshot_id = message[4];

    // If this is the first marker received, record state and send marker messages
    if (snapshot[snapshot_id].color == Blue) {
        snapshot[snapshot_id].state = node_state;
        memcpy(snapshot[snapshot_id].timestamp, timestamp, sizeof(timestamp));
        snapshot[snapshot_id].color = Red;
        send_marker_messages(node_id, atoi(&snapshot_id));
    }

    else {
        // Marker message by neighbor received: stop recording channel for this neighbor
        snapshot[snapshot_id].neighbors[atoi(&sent_by)] = Received;  
        snapshot[snapshot_id].nb_marker = snapshot[snapshot_id].nb_marker + 1;   

        // Detect if snapshot is ready to be sent to node 0
        if (nb_neighbors == snapshot[snapshot_id].nb_marker) {
            // Converge cast state (vector clock timestamp + node state + channel state) to node 0
        }
    }
}

void* detect_termination()
{
    long last_snapshot = 0;
    int snapshot_id = 0;

    // Need to fix clock
    while(1) {
        struct timespec current_time;
        clock_gettime(CLOCK_REALTIME, &current_time);
        if (snapshot_delay < current_time.tv_nsec/1000000 - last_snapshot) {
            last_snapshot = current_time.tv_nsec/1000000;
            
            // Record node 0 state and mark it as "red"
            snapshot[snapshot_id].state = node_state;
            memcpy(snapshot[snapshot_id].timestamp, timestamp, sizeof(timestamp));
            snapshot[snapshot_id].color = Red;
            send_marker_messages(node_id, snapshot_id);
            snapshot_id++;
        }
    }
}














