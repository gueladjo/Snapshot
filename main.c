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
#define CONVERGE_CAST 'C'
#define HALT 'H'
#define MSG_BUFFER_SIZE 100

typedef struct Neighbor {
    int id;
    int port;
    char hostname[100];
    int receive_socket; // I'm not sure if this is the right terminology to call this the 'server' socket, but it's the new socket created from the accept() call
    int send_socket;
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
Snapshot** snapshots;
int* number_received;

int nb_snapshots;
int last_snapshot_id = -1;

void* handle_neighbor(void* arg);
void parse_buffer(char* buffer, size_t* rcv_len);
int handle_message(char* message, size_t length);

char * create_vector_msg(int * vector_clk);
int * parse_vector(char * char_vector);
void send_marker_messages(int snapshot_id) ;

void send_msg(int sockfd, char * buffer, int msglen);
int receive_message(char * message, int length);
int compare_timestamps(int * incoming_ts);
int merge_timestamps(int * incoming_ts);

int message_source(char * msg);
int message_dst(char * msg);
char message_type(char * msg);
char * message_payload(char * msg);

void record_snapshot();
void snapshot_channel(char* message);
void activate_node();
void* snapshot_handler();
void output();

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
int * parent;

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
    sleep(10);
    // Config struct filled when config file parsed
    srand(time(NULL));

    read_config_file(&system_config, argv[2]);
    display_config(system_config); 

    nb_nodes = system_config.nodes_in_system;
    min_per_active = system_config.min_per_active;
    max_per_active = system_config.max_per_active;
    min_send_delay = system_config.min_send_delay;
    snapshot_delay = system_config.snapshot_delay;
    max_number = system_config.max_number;

    sscanf(argv[1], "%i", &node_id);

    this_index= find(node_id, system_config.nodeIDs, system_config.nodes_in_system);
    nb_neighbors = system_config.neighborCount[this_index];

    // Set up neighbors information and initialize vector timestamp
    neighbors =  malloc(nb_neighbors * sizeof(Neighbor));

    snapshot =  malloc(100 * sizeof(Snapshot));
    int i, k;
    for (i = 0; i < 100; i++) {
        snapshot[i].timestamp = malloc(nb_nodes * sizeof(int));
        snapshot[i].neighbors = malloc(nb_neighbors * sizeof(enum Marker));

        snapshot[i].color = Blue;
        snapshot[i].channel = Empty;
        snapshot[i].nb_marker = 0;

        for (k = 0; k < nb_neighbors; k++) {
            snapshot[i].neighbors[k] = NotReceived;
        }
    }

    timestamp = malloc(nb_nodes * sizeof(int));
    memset(timestamp, 0, nb_nodes * sizeof(int));
    
    int dimension = 100;
    number_received = malloc(dimension * sizeof(int));
    snapshots = malloc(dimension * sizeof(Snapshot*));

    for (i = 0; i < dimension; i++) {
        snapshots[i] = malloc(nb_nodes * sizeof(Snapshot));
    }

    int *  tree_count; // num of elements in each of tree's arrays 
    int ** tree = create_spanning_tree(&tree_count, &parent, system_config.nodeIDs, system_config.neighbors, system_config.neighborCount, system_config.nodes_in_system);
    snapshot_neighbors = (Neighbor*)(malloc(sizeof(Neighbor)*tree_count[node_id]));
    // allocate snapshot_neighbors array
    for (i = 0; i < system_config.neighborCount[this_index]; i++)
    {
        neighbors[i].id = system_config.neighbors[this_index][i];
        neighbors[i].port = system_config.portNumbers[neighbors[i].id];
        memmove(neighbors[i].hostname, system_config.hostNames[neighbors[i].id], 18);
        
        
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

    // Client sockets information
    struct hostent* h;

    // Server Socket information
    int s;
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

    // Create thread for receiving each neighbor messages
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    addrlen = sizeof(pin);


    //  Create client sockets to neighbors of the node
    int j = 0;
    for (j = 0; j < nb_neighbors; j++) {
        // Create TCP socket
        if ((neighbors[j].send_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            printf("Error creating socket\n");
            exit(1); 
        }

        // Get host info
        int fail_count = 0;    
        while ((h = gethostbyname(neighbors[j].hostname)) == 0) 
        {
            printf("Error on gethostbyname\n");
            fail_count++;
            if (fail_count > 10)
                exit(1);
            else
                sleep(1);
        }

        // Fill in socket address structure with host info
        memset(&pin, 0, sizeof(pin));
        pin.sin_family = AF_INET;
        pin.sin_addr.s_addr = ((struct in_addr *)(h->h_addr))->s_addr;
        pin.sin_port = htons(neighbors[j].port);

        // Connect to port on neighbor
        if (connect(neighbors[j].send_socket, (struct sockaddr *) &pin, sizeof(pin)) == -1) {
            printf("Error when connecting to neighbor\n");
            exit(1);
        }
    }

    i = 0;
    while (i < nb_neighbors) {
        if ((neighbors[i].receive_socket = accept(s, (struct sockaddr *) &pin, (socklen_t*)&addrlen)) == -1) {
            printf("Error on accept call.\n");
            exit(1);
        }
        pthread_create(&tid, &attr, handle_neighbor, &(neighbors[i].receive_socket));
        i++;
    }

    // Create client sockets to neighbors of the node
    int j = 0;
    for (j = 0; j < nb_neighbors; j++) {
        // Create TCP socket
        if ((neighbors[j].send_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
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
        int connect_return = connect(neighbors[j].send_socket, (struct sockaddr *) &pin, sizeof(pin));
        while (connect_return == -1) {
            connect_return = connect(neighbors[j].send_socket, (struct sockaddr *) &pin, sizeof(pin));
            printf("Node %d retrying to connect.\n", node_id);
        }
    }

    // Create snapshot thread if node id is 0
    if (node_id == 0) {
        pthread_t pid;
        pthread_create(&pid, &attr, snapshot_handler, NULL);
    }

    int total_length;
    struct timespec current_time, previous_time;
    clock_gettime(CLOCK_REALTIME, &previous_time);
    uint64_t delta_ms;
    char msg[250];
    while (1)
    {
        if (node_state == Active)
        {
            if (msgs_to_send > 0)
            {
                clock_gettime(CLOCK_REALTIME, &current_time);
                delta_ms = (current_time.tv_sec - previous_time.tv_sec) * 1000 + (current_time.tv_nsec - previous_time.tv_nsec) / 1000000;
                    // Not totally correct but will do for now
                if (delta_ms > min_send_delay)
                {
                    // Source | Dest | Protocol | Length | Payload                    
                    Neighbor neighborToSend = neighbors[(rand() % nb_neighbors)];
                    total_length = 5 + 3 * nb_nodes + 1;
                    snprintf(msg, total_length, "%02d%02dA%s", node_id, neighborToSend.id, create_vector_msg(timestamp));
                    send_msg(neighborToSend.send_socket, msg, total_length - 1);
                    previous_time.tv_sec = current_time.tv_sec;
                    previous_time.tv_nsec = current_time.tv_nsec;
                    msgs_to_send--;
                    timestamp[node_id]++;                    
                }
            }
            else
            {
                node_state = Passive;
            }
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
    // Check if we have enough byte to read message length
    int message_len = 0;
    while (*rcv_len > 4 ) {
        // Check if we received a whole message
        char protocol = buffer[4];
        switch (protocol) {
            case 'A':
                message_len = 3 * nb_nodes;
                break;
            case 'M':
                message_len = 3;
                break;
            case 'C':
                message_len = 7 + 3 * nb_nodes;
                break;
            case 'H':
                message_len = 0;
                break;

            default:
                printf("WRONG!\n");
        }

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
        if (last_snapshot_id != -1) 
            snapshot_channel(message);
    }
    else if (message_type(message) == MARKER_MSG)
    {
        record_snapshot(message);
    }

    else if (message_type(message) == CONVERGE_CAST) {
        if (node_id != 0) {
            int parent_id = system_config.nodeIDs[parent[this_index]];
            int i;
            for (i = 0; i < nb_neighbors; i++) {
                if (neighbors[i].id == parent_id) {
                    send_msg(neighbors[i].send_socket, message, (int) length);
                    break;
                }
            }
        }

        else {
            char* payload = message_payload(message);
            int source, snapshot_id, state, channel_state;

            sscanf(payload, "%2d%3d%1d%1d", &source, &snapshot_id, &state, &channel_state);
            int* timestamp_vec = parse_vector(payload + 7);

            snapshots[snapshot_id][source].state = state;
            snapshots[snapshot_id][source].channel = channel_state;
            snapshots[snapshot_id][source].timestamp = timestamp_vec;

            number_received[snapshot_id]++;

            if (number_received[snapshot_id] == node_id) {
                int i, k, max;
                
                int termination_detected = 1;
                for (i = 0; i < nb_nodes; i++) {
                    if (snapshots[snapshot_id][i].state == Active)
                        termination_detected = 0;
                    if (snapshots[snapshot_id][i].channel == NotEmpty) 
                        termination_detected = 0;
                }

                int consistent = 1;
                for (i = 0; (i < nb_nodes) && consistent; i++) {
                    max = snapshots[snapshot_id][i].timestamp[i]; 
                    for (k = 0; (k < nb_nodes) && consistent; k++) {
                        if (snapshots[snapshot_id][k].timestamp[i] >= max) {
                            printf("INCONSISTENT SNAPSHOT\n");
                            consistent = 0;
                        } 
                    }
                } 
                    
                if (termination_detected) 
                {
                    char msg[10];
                    for (i = 0; i < nb_neighbors; i++)
                    {
                        snprintf(msg, 6, "%02d%02dH", node_id, neighbors[i].id);
                        send_msg(neighbors[i].send_socket, msg, 5);
                    }
                    output();
                }
            }
        }
    }

    else if (message_type(message) == HALT) {
        int i;
        for (i = 0; i < nb_neighbors; i++) {
            if (neighbors[i].id != message_source(message)) {
                char msg[10];
                snprintf(msg, 6, "%02d%02dH", node_id, neighbors[i].id);
                send_msg(neighbors[i].send_socket, message, (int) length);
            }
        }
        if (node_id) // output to file if not node 0, node 0 already outputs elsewhere and this prevents double
            output();
        exit(0);
    }
}

// Needs to be changed from broadcast to tree 
void send_marker_messages(int snapshot_id) 
{
    int i;
    char msg[50];

    for (i = 0; i < nb_neighbors; i++)
    {
            snprintf(msg, 9, "%02d%02dM%3d", node_id, neighbors[i].id, snapshot_id);
            send_msg(neighbors[i].send_socket, msg, 8);
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
    int sent_by = message_source(message);
    int snapshot_id;
    sscanf(message_payload(message), "%i", &snapshot_id);

    // If this is the first marker received, record state and send marker messages
    if (snapshot[snapshot_id].color == Blue) {
        last_snapshot_id = snapshot_id;
        snapshot[snapshot_id].state = node_state;
        memcpy(snapshot[snapshot_id].timestamp, timestamp, sizeof(timestamp));
        snapshot[snapshot_id].color = Red;
        send_marker_messages(snapshot_id);
    }

    else {
        // Marker message by neighbor received: stop recording channel for this neighbor
        snapshot[snapshot_id].neighbors[sent_by] = Received;  
        snapshot[snapshot_id].nb_marker = snapshot[snapshot_id].nb_marker + 1;   

        // Detect if snapshot is ready to be sent to node 0
        if (nb_neighbors == snapshot[snapshot_id].nb_marker) {
            // Converge cast state (vector clock timestamp + node state + channel state) to node 0

            if (node_id == 0) {
                snapshots[snapshot_id][0].state = snapshot[snapshot_id].state;
                snapshots[snapshot_id][0].channel = snapshot[snapshot_id].channel;
                snapshots[snapshot_id][0].timestamp = snapshot[snapshot_id].timestamp;

                number_received[snapshot_id]++;

                if (number_received[snapshot_id] == node_id) {
                    int i, k, max;
                    
                    int termination_detected = 1;
                    for (i = 0; i < nb_nodes; i++) {
                        if (snapshots[snapshot_id][i].state == Active)
                            termination_detected = 0;
                        if (snapshots[snapshot_id][i].channel == NotEmpty) 
                            termination_detected = 0;
                    }

                    int consistent = 1;
                    for (i = 0; (i < nb_nodes) && consistent; i++) {
                        max = snapshots[snapshot_id][i].timestamp[i]; 
                        for (k = 0; (k < nb_nodes) && consistent; k++) {
                            if (snapshots[snapshot_id][k].timestamp[i] >= max) {
                                printf("INCONSISTENT SNAPSHOT\n");
                                consistent = 0;
                            } 
                        }
                    } 
                        
                    if (termination_detected) 
                    {
                        char msg[10];
                        for (i = 0; i < nb_neighbors; i++)
                        {
                            snprintf(msg, 6, "%02d%02dH", node_id, neighbors[i].id);
                            send_msg(neighbors[i].send_socket, msg, 5);
                        }
                        output();
                    }
                }
            }

            else {
                int length = nb_nodes * 3 +  12;
                int timestamp_length = nb_nodes * 3;
                char* converge_msg = (char*)malloc(length * sizeof(char) + 3);

                snprintf(converge_msg, length + 1, "%02d%02dC%02d%03d%c%c%s", node_id, parent[node_id]
                       , node_id, snapshot_id, snapshot[snapshot_id].state, snapshot[snapshot_id].channel, create_vector_msg(timestamp));
                int i = 0;
                for (i = 0; i < nb_neighbors; i++) {
                    if (neighbors[i].id == parent[node_id]) {
                        send_msg(neighbors[i].send_socket, converge_msg, length);
                        break;
                    }
                }
            }
        }
    }
}

// Check if message should be included in channel state of snapshot
void snapshot_channel(char* message)
{
    int sent_by = message_source(message);
    if (snapshot[last_snapshot_id].neighbors[sent_by] == NotReceived) {
        snapshot[last_snapshot_id].channel = NotEmpty;
    }    
}

void* snapshot_handler()
{
    int snapshot_id = 0;

    struct timespec current_time, previous_time;
    clock_gettime(CLOCK_REALTIME, &previous_time);
    uint64_t delta_ms;

    while(1) {
        struct timespec current_time;
        clock_gettime(CLOCK_REALTIME, &current_time);
        delta_ms = (current_time.tv_sec - previous_time.tv_sec) * 1000 + (current_time.tv_nsec - previous_time.tv_nsec) / 1000000;

        if (delta_ms > snapshot_delay) {
            // Record node 0 state and mark it as "red"
            snapshot[snapshot_id].state = node_state;
            previous_time.tv_sec = current_time.tv_sec;
            previous_time.tv_nsec = current_time.tv_nsec;
 
            memcpy(snapshot[snapshot_id].timestamp, timestamp, sizeof(timestamp));
            snapshot[snapshot_id].color = Red;
            send_marker_messages(snapshot_id);
            snapshot_id++;
        }
    }
}

// Takes the message and determines whether it should be delivered or not
int receive_message(char * message, int length)
{
    int * received_clk = parse_vector(message_payload(message));
    merge_timestamps(received_clk);
    timestamp[node_id]++;

    free(received_clk);
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

void output()
{
    int txtlength = strlen(system_config.config_name);
    int outlength = strlen(system_config.config_name) + 5;
    char * partial = malloc(txtlength-4);
    char * file = malloc(outlength);
    memmove(partial, txtlength, txtlength-4);

    snprintf(file, outlength, "%s-%d.out", partial, node_id);
    FILE * fp = fopen(file, "w");
    int snapshot_counter;
    int vector_counter;
    for (snapshot_counter = 0; snapshot_counter < last_snapshot_id; snapshot_counter++)
    {
        for (vector_counter = 0; vector_counter < nb_nodes; vector_counter++)
        {
            fprintf(fp, "%d ", snapshot[snapshot_counter].timestamp[vector_counter]);
        }
    }
    fclose(fp);
    free (partial);
    free (file);

}
