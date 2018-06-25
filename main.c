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
#define MSG_BUFFER_SIZE 100

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

char * create_vector_msg(int * vector_clk);
int * parse_vector(char * char_vector);
void send_marker_messages(int sent_by, int snapshot_id) ;

void send_msg(int sockfd, char * buffer, int msglen);
int receive_message(char * message, int length);
int compare_timestamps(int * incoming_ts);
void deliver_message(char * message, int length);
void buffer_message(char * message, int length);
void check_buffered_messages();
void remove_from_buffer(int index);

int message_source(char * msg);
int message_dst(char * msg);
char message_type(char * msg);
char * message_payload(char * msg);


void record_snapshot();
void activate_node();
void* detect_termination();

// Global parameters
int nb_nodes;
int min_per_active, max_per_active;
int min_send_delay;
int snapshot_delay;
int max_number;

config system_config; 

// Node Paramters
int node_id;
int this_index; // Index of node information in system_config. This would be unneeded if node_id = index, but i dont think we can assume that.
int port;
enum State node_state;
int* timestamp;
int* s_neighbor;

char * msg_buffer[MSG_BUFFER_SIZE];
int buffer_msg_length[MSG_BUFFER_SIZE]; // length of each message in buffer;
int buffer_length; // number of messages in buffer


int msgs_sent; // Messages sent by this node, to be compared with max_number
int msgs_to_send; // Messages to send on this active session (between min and maxperactive)
Neighbor* neighbors;
int nb_neighbors;
Neighbor* snapshot_neighbors; // Neighbors in the spanning tree

int main(int argc, char* argv[])
{
    // Config struct filled when config file parsed

    srand(time(NULL));

    read_config_file(&system_config);
    display_config(system_config); 

    nb_nodes = system_config.nodes_in_system;
    min_per_active = system_config.min_per_active;
    max_per_active = system_config.max_per_active;
    min_send_delay = system_config.min_send_delay;
    snapshot_delay = system_config.snapshot_delay;
    max_number = system_config.max_number;

    node_id = 0;

    this_index= find(node_id, system_config.nodeIDs, system_config.nodes_in_system);
    nb_neighbors = system_config.neighborCount[this_index];
    // Set up neighbors information and initialize vector timestamp
    neighbors =  malloc(nb_neighbors * sizeof(Neighbor));
    snapshot =  malloc(100 * sizeof(Snapshot));
    timestamp = malloc(nb_nodes * sizeof(int));
    memset(timestamp, 0, nb_nodes * sizeof(int));


    int *  tree_count; // num of elements in each of tree's arrays 
    int * parent;
    int ** tree = create_spanning_tree(&tree_count, &parent, system_config.nodeIDs, system_config.neighbors, system_config.neighborCount, system_config.nodes_in_system);
    // allocate snapshot_neighbors array
    int i, k;
    for (i = 0; i < system_config.neighborCount[this_index]; i++)
    {
        neighbors[i].id = system_config.neighbors[this_index][i];
    }


    for(i = 0; i < tree_count[this_index]; i++)
    {
        for (k = 0; k < nb_neighbors; k++)
        {
            if (neighbors[k].id == tree[this_index][i])
            {
                snapshot_neighbors[i] = neighbors[k];
            }
        }
    }

    // initialize message buffers
    for (i = 0; i < MSG_BUFFER_SIZE; i++)
    {
        buffer_msg_length[i] = 0;
    }
    buffer_length = 0;

    // Set state of the node
    if ((node_id % 2) == 0) {
        node_state = Active;
    }
    else {
        node_state = Passive;
    }

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

    i = 0;
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

    int last_sent_time = 0;
    int last_sent_time_s = 0;
    int total_length;
    while (1)
    {
        if (node_state == Active)
        {
            if (msgs_to_send > 0)
            {
                struct timespec current_time;
                clock_gettime(CLOCK_REALTIME, &current_time);
                 
                    // Not totally correct but will do for now
                if ((current_time.tv_sec - last_sent_time_s > min_send_delay/1000)  ||  (min_send_delay < current_time.tv_nsec/1000000 - last_sent_time);
                {
                    // Source | Dest | Protocol | Length | Payload                    
                    Neighbor neighborToSend = neighbors[(rand() % nb_neighbors)];
                    total_length = 5 + determine_msg_length(timestamp);
                    snprintf(msg, total_length, "%d%dA%s", node_id, neighborToSend.id, create_vector_msg(timestamp));
                    send_msg(neighborToSend.server_socket, msg, 4);
                    last_sent_time_ns = current_time.tv_nsec/100000;
                    last_sent_time_s = current_time.tv_sec;
                    msgs_to_send--;
                    timestamp[this_index]++;                    
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

//Src dst prot payload ->
//|##|##|Char|###|###|###|...|###| (Pipes not included in actual messages)
void parse_buffer(char* buffer, size_t* rcv_len)
{
    // Need to modify this for new message format 

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
    if (message_type(message) == APP_MSG)
    {
        if (node_state == Passive)
        {
            if (msgs_sent < max_number) // Turn active if max_number hasn't been reached, otherwise stay passive
            {
                activate_node();
            }
        }
        receive_message(message, (int)length);
    }
    else if (message_type(message) == MARKER_MSG)
    {
<<<<<<< HEAD
        record_snapshot();
        send_marker_messages(message_source(message)));
=======
        record_snapshot(message);
>>>>>>> cd6d9cc0f2eb19198f59f9fea027cd423b6a3833
    }
}


// Needs to be changed from broadcast to tree 
<<<<<<< HEAD
void send_marker_messages(int sent_by) 
{    
=======
void send_marker_messages(int sent_by, int snapshot_id) 
{
>>>>>>> cd6d9cc0f2eb19198f59f9fea027cd423b6a3833
    int i;
    char msg[200];
        // src  dst prot     vector     \0
    length = 2 + 2 + 1 + nb_nodes * 3 + 1; 
    for (i = 0; i < nb_neighbors; i++)
    {
        
        if (neighbors[i].id != sent_by) // Don't send marker msg to sender
        {
<<<<<<< HEAD
                            // Still needs to be filled in with snapshot payload
            snprintf(msg, length, "%02d%02dM000", node_id, neighbors[i].id);
            //send_msg(neighbors[i].client_socket, msg, 8);
            printf("%s\n", msg);
=======
            snprintf(msg, 5, "%d%dM1%d", node_id, neighbors[i].id, snapshot_id);
            send_msg(neighbors[i].client_socket, msg, 4);
>>>>>>> cd6d9cc0f2eb19198f59f9fea027cd423b6a3833
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


// Takes the message and determines whether it should be delivered or not
int receive_message(char * message, int length)
{
    int * received_clk = parse_vector(message_payload(message));

    int deliver = compare_timestamps(received_clk);

    if (deliver)
        deliver_message(message, length);
    else
        buffer_message(message, length);

    free(received_clk);
    check_buffered_messages();
}

// returns 1 when incoming <= own, 0 when not
// this may not be right...i'm getting confused with the message passing algorithm and the snapshot algorithm
int compare_timestamps(int * incoming_ts)
{
    int i;
    int deliver = 1;
    for (i = 0; i < nb_nodes; i++)
    {
        if (incoming_ts[i] > timestamp[i])
            deliver = 0;
    }
    return deliver;
}

// Takes the message and updates the vector clock accordingly
void deliver_message(char * message, int length)
{

    int source = message[0] - '0';
    int source_index = find(source, system_config.nodeIDs, nb_nodes);

    merge_timestamps(parse_vector(message_payload(message)));
    timestamp[source_index]++;
    
}

void buffer_message(char * message, int length)
{
    msg_buffer[buffer_length] = message;
    buffer_msg_length[buffer_length] = length;
    buffer_length++;
    if (buffer_length > MSG_BUFFER_SIZE)
    {printf("Node %d buffer overflow", node_id);}
        //error, buffer overflow
    
}

void check_buffered_messages()
{
    int * buffered_vector;
    int i;
    for(i = 0; i < buffer_length; i++)
    {
        buffered_vector = parse_vector(message_payload(msg_buffer[i]));
        if (compare_timestamps(buffered_vector))
        {
            deliver_message(msg_buffer[i], buffer_msg_length[i]);
            remove_from_buffer(i); // possibly dangerous? messing with array while iterating
            i--;                   // Go back a step to account for removing
        }
    }
}

void remove_from_buffer(int index)
{
    int i = index;
    for(i; i< buffer_length - 1; i++)
    {
        msg_buffer[i] == msg_buffer[i+1]; // shift to fill buffer without empty 
    }
    buffer_length--;
}

//given a vector clock, create message payload in the form of "C0-C1-C2 - ... - CN"  Each element corresponds to the clock at the node at the index (C0 = index 0)
char * create_vector_msg(int * vector_clk)
{
    int length = nb_nodes * 3;
    char * vector_msg = (char*)malloc(length * sizeof(char) + 1); 
    int i = 0, node_counter = 0;
    while (i < length && node_counter < nb_nodes)
    {
        sprintf(vector_msg + i, "%03d", vector_clk[node_counter]);
        i+=3;
        node_counter++;
    }
    vector_msg[i] = '\0';
    //printf("%s\n", vector_msg);
    return vector_msg;
}

// Given a vector from a message, turn into an int array
int * parse_vector(char * char_vector) // Input vector in format C0-C1-C2 - ... - CN. Each element corresponds to the clock at the node at the index (C0 = index 0)
{
    int * vector_clock = (int *)malloc(nb_nodes*sizeof(int));

    int i, clock; 
    char charInput;
    for (i = 0; i < nb_nodes; i++)
    {
        sscanf(char_vector, "%3d", &clock);
        char_vector += 3;
        vector_clock[i] = clock;
    }
    return vector_clock;
}

<<<<<<< HEAD
int merge_timestamps(int * incoming_ts)
{
    int i ;
    for (i = 0; i < nb_nodes; i++)
    {
        timestamp[i] = timestamp[i] > incoming_ts[i] ? timestamp[i] : incoming_ts[i]; // take the larger
    }
}


// Message accessor functions, for easy reading
int message_source(char * msg)
{
    int source;
    sscanf(msg, "%2d", &source);
    return source; 
}

int message_dst(char * msg)
{
    int dest;
    sscanf(msg+2, "%2d", &dest);
    return dest; 
}

char message_type(char * msg)
{
    return msg[4];
}

char * message_payload(char * msg)
{
    return msg+5;
}

void record_snapshot()
=======
void record_snapshot(char* message)
>>>>>>> cd6d9cc0f2eb19198f59f9fea027cd423b6a3833
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
