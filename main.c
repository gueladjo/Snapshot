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
#include<semaphore.h>
#include "config.h"

#define BUFFERSIZE 512
#define APP_MSG 'A'
#define MARKER_MSG 'M'
#define CONVERGE_CAST 'C'
#define HALT 'H'

// Mutex
sem_t send_marker;

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

int last_snapshot_id = -1;
int last_cast_id = -1;

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

int dimension = 300;

config system_config; 

// Node Paramters
int node_id;
int port;
enum State node_state;
int* timestamp;
int* s_neighbor;
int * parent;
int halt_received;

int msgs_sent; // Messages sent by this node, to be compared with max_number
int msgs_to_send; // Messages to send on this active session (between min and maxperactive)
Neighbor* neighbors;
int nb_neighbors;

int main(int argc, char* argv[])
{
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

    sscanf(argv[1], "%d", &node_id);

    nb_neighbors = system_config.neighborCount[node_id];
    port = system_config.portNumbers[node_id];

    // Set up neighbors information and initialize vector timestamp
    msgs_sent = 0;
    neighbors =  malloc(nb_neighbors * sizeof(Neighbor));

    snapshot =  malloc(dimension * sizeof(Snapshot));
    int i, k;
    for (i = 0; i < dimension; i++) {
        snapshot[i].timestamp = malloc(nb_nodes * sizeof(int));
        memset(snapshot[i].timestamp, 0, nb_nodes * sizeof(int));
        snapshot[i].neighbors = malloc(nb_nodes * sizeof(enum Marker));

        snapshot[i].color = Blue;
        snapshot[i].channel = Empty;
        snapshot[i].nb_marker = 0;

        for (k = 0; k < nb_nodes; k++) {
            snapshot[i].neighbors[k] = NotReceived;
        }
    }

    timestamp = malloc(nb_nodes * sizeof(int));
    memset(timestamp, 0, nb_nodes * sizeof(int));
    
    number_received = malloc(dimension * sizeof(int));
    memset(number_received, 0, dimension * sizeof(int));
    snapshots = malloc(dimension * sizeof(Snapshot*));

    for (i = 0; i < dimension; i++) {
        snapshots[i] = malloc(nb_nodes * sizeof(Snapshot));
    }

    int *  tree_count; // num of elements in each of tree's arrays 
    int ** tree = create_spanning_tree(&tree_count, &parent, system_config.nodeIDs, system_config.neighbors, system_config.neighborCount, system_config.nodes_in_system);

    // allocate neighbors array
    for (i = 0; i < system_config.neighborCount[node_id]; i++)
    {
        neighbors[i].id = system_config.neighbors[node_id][i];
        neighbors[i].port = system_config.portNumbers[neighbors[i].id];
        memmove(neighbors[i].hostname, system_config.hostNames[neighbors[i].id], 18);
    }

    // Initialize mutex
    if (sem_init(&send_marker, 0, 1) == -1) {
        printf("Error during mutex init.\n");
        exit(1);
    }

    // Set state of the node
    if ((node_id % 2) == 0) {
        activate_node();
    }
    else {
        node_state = Passive;
    }

    // Halt initialize
    if (node_id == 0) 
        halt_received = 1;
    else
        halt_received = 0;

    printf("Node state: %d\n", node_state);

    // Client sockets information
    struct hostent* h;

    // Server Socket information
    int s;
    struct sockaddr_in sin;
    struct sockaddr_in sin2;
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
    printf("PORT : %d\n", port);
    sin.sin_port = htons(port);


    // Reuse port
    int yes = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        printf("Error changing bind option\n");
        exit(1);
    }

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
        printf("Node %d trying to connect to node %d.\n", node_id, neighbors[j].id);
        while (connect_return == -1) {
            connect_return = connect(neighbors[j].send_socket, (struct sockaddr *) &pin, sizeof(pin));
            sleep(1);
        }
        printf("Node %d connected to neighbor %d.\n", node_id, neighbors[j].id);
    }

    // Create thread for receiving each neighbor messages
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    addrlen = sizeof(sin2);

    i = 0;
    while (i < nb_neighbors) {
        if ((neighbors[i].receive_socket = accept(s, (struct sockaddr *) &sin2, (socklen_t*)&addrlen)) == -1) {
            printf("Error on accept call.\n");
            exit(1);
        }
        pthread_create(&tid, &attr, handle_neighbor, &(neighbors[i].receive_socket));
        i++;
    }

    // Create snapshot thread if node id is 0
    if (node_id == 0) {
        pthread_t pid;
        pthread_create(&pid, &attr, snapshot_handler, NULL);
    }

    int total_length = 5 + 3 * nb_nodes + 1;
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
                delta_ms = (current_time.tv_sec - previous_time.tv_sec) * 1000 +
                    (current_time.tv_nsec - previous_time.tv_nsec) / 1000000;
                if (delta_ms > min_send_delay)
                {
                    // Source | Dest | Protocol | Length | Payload                    
                    Neighbor neighborToSend = neighbors[(rand() % nb_neighbors)];

                    if (sem_wait(&send_marker) == -1) {
                        printf("Error during wait on mutex.\n");
                        exit(1);
                    }
                    
                    char* vector_msg = create_vector_msg(timestamp);
                    timestamp[node_id]++;                    
                    snprintf(msg, total_length, "%02d%02dA%s", node_id,
                            neighborToSend.id, vector_msg);
                    send_msg(neighborToSend.send_socket, msg, total_length - 1);
                    free(vector_msg);
                    if (sem_post(&send_marker) == -1) {
                        printf("Error during signal on mutex.\n");
                        exit(1);
                    } 

                    msgs_sent++;
                    previous_time.tv_sec = current_time.tv_sec;
                    previous_time.tv_nsec = current_time.tv_nsec;
                    msgs_to_send--;
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

    while (1) {
        if (((count = recv(s, buffer + rcv_len, BUFFERSIZE - rcv_len, 0)) == -1)) {
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
                message_len = 3;
                break;

            default:
                printf("WRONG!\n");
        }

        if (*rcv_len < 5 + message_len) 
           break; 

        // Handle message received
        handle_message(buffer, message_len + 5);

        // Remove message from buffer and shuffle bytes of next message to start of the buffer
        *rcv_len = *rcv_len - 5 - message_len;
        if (*rcv_len != 0) {
            memmove(buffer, buffer + 5 + message_len, *rcv_len);
        }
    }
}

// Check type of message (application or marker) and process it
// Source | Dest | Protocol | Length | Payload
int handle_message(char* message, size_t length)
{
    char temp[300];
    strcpy(temp, message);
    temp[length] = '\0';
    printf("MSG RCVD: %s LENGTH: %d\n", temp, (int) length);
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
            int parent_id = system_config.nodeIDs[parent[node_id]];
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

            if (number_received[snapshot_id] == nb_nodes) {
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
                    printf("Node %d:", i);
                    for (k = 0; (k < nb_nodes) && consistent; k++) {
                        printf(" %d", snapshots[snapshot_id][i].timestamp[k]);
                    }
                    printf("\n");
                } 

                for (i = 0; (i < nb_nodes) && consistent; i++) {
                    max = snapshots[snapshot_id][i].timestamp[i]; 
                    for (k = 0; (k < nb_nodes) && consistent; k++) {
                        if (snapshots[snapshot_id][k].timestamp[i] > max) {
                            consistent = 0;
                        } 
                    }
                } 
                    
                if (termination_detected) 
                {
                    last_cast_id = snapshot_id;
                    char msg[10];
                    for (i = 0; i < nb_neighbors; i++)
                    {
                        snprintf(msg, 9, "%02d%02dH%03d", node_id, neighbors[i].id, last_cast_id);
                        send_msg(neighbors[i].send_socket, msg, 8);
                    }
                    output();
                    exit(0);
                }
            }
        }
    }

    else if (message_type(message) == HALT) {
        int i;
        if (!halt_received) {
            halt_received = 1;
            sscanf(message_payload(message), "%3d", &last_cast_id);
            for (i = 0; i < nb_neighbors; i++) {
                if (neighbors[i].id != message_source(message)) {
                    char msg[10];
                    snprintf(msg, 9, "%02d%02dH%03d", node_id, neighbors[i].id, last_cast_id);
                    send_msg(neighbors[i].send_socket, msg, (int) length);
                }
            }
            if (node_id) // output to file if not node 0, node 0 already outputs elsewhere and this prevents double
                output();
            exit(0);
        }
    }
}

// Needs to be changed from broadcast to tree 
void send_marker_messages(int snapshot_id) 
{
    int i;
    char msg[50];

    for (i = 0; i < nb_neighbors; i++)
    {
            snprintf(msg, 9, "%02d%02dM%03d", node_id, neighbors[i].id, snapshot_id);
            send_msg(neighbors[i].send_socket, msg, 8);
    }
}

// Function to send whole message
void send_msg(int sockfd, char * buffer, int msglen)
{
    int bytes_to_send = msglen; // |Source | Destination | Protocol ('M') | Length (0)
    buffer[msglen] = '\0';
    printf("MSG SENT: %s \n", buffer);
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
    sscanf(message_payload(message), "%3d", &snapshot_id);

    // Marker message by neighbor received: stop recording channel for this neighbor
    snapshot[snapshot_id].neighbors[sent_by] = Received;  
    snapshot[snapshot_id].nb_marker = snapshot[snapshot_id].nb_marker + 1;   

    // If this is the first marker received, record state and send marker messages
    if (snapshot[snapshot_id].color == Blue) {
        if (sem_wait(&send_marker) == -1) {
            printf("Error during wait on mutex.\n");
            exit(1);
        } 

        snapshot[snapshot_id].color = Red;
        snapshot[snapshot_id].state = node_state;
        memcpy(snapshot[snapshot_id].timestamp, timestamp, sizeof(int) * nb_nodes);
        send_marker_messages(snapshot_id);
        last_snapshot_id = snapshot_id;

        if (sem_post(&send_marker) == -1) {
            printf("Error during signal on mutex.\n");
            exit(1);
        } 
    }

    // Detect if snapshot is ready to be sent to node 0
    if (nb_neighbors == snapshot[snapshot_id].nb_marker) {
        // Converge cast state (vector clock timestamp + node state + channel state) to node 0

        if (node_id == 0) {
            snapshots[snapshot_id][0].state = snapshot[snapshot_id].state;
            snapshots[snapshot_id][0].channel = snapshot[snapshot_id].channel;
            snapshots[snapshot_id][0].timestamp = snapshot[snapshot_id].timestamp;
            number_received[snapshot_id]++;
        }

        else {
            int length = nb_nodes * 3 +  12;
            int timestamp_length = nb_nodes * 3;
            char* converge_msg = (char*)malloc(length * sizeof(char) + 3);
            char* vector_msg = create_vector_msg(timestamp);
            snprintf(converge_msg, length + 1, "%02d%02dC%02d%03d%d%d%s", node_id, parent[node_id]
                   , node_id, snapshot_id, snapshot[snapshot_id].state, snapshot[snapshot_id].channel, vector_msg);
            int i = 0;
            for (i = 0; i < nb_neighbors; i++) {
                if (neighbors[i].id == parent[node_id]) {
                    send_msg(neighbors[i].send_socket, converge_msg, length);
                    break;
                }
            }
            free(vector_msg);
            free(converge_msg);
        }
    }
}

// Check if message should be included in channel state of snapshot
void snapshot_channel(char* message)
{
    int sent_by = message_source(message);
    int i = 0;
    for (i = 0; i < (last_snapshot_id + 1); i++) {
        if ((snapshot[i].nb_marker != nb_neighbors) || (snapshot[i].neighbors[sent_by] == NotReceived)) {
            snapshot[i].channel = NotEmpty;
        }    
    }
}

void* snapshot_handler()
{
    int snapshot_id = 0;

    struct timespec current_time, previous_time;
    clock_gettime(CLOCK_REALTIME, &previous_time);
    uint64_t delta_ms;

    while(last_cast_id == -1) {
        struct timespec current_time;
        clock_gettime(CLOCK_REALTIME, &current_time);
        delta_ms = (current_time.tv_sec - previous_time.tv_sec) * 1000 + (current_time.tv_nsec - previous_time.tv_nsec) / 1000000;

        if (delta_ms > snapshot_delay) {
            // Record node 0 state and mark it as "red"
            previous_time.tv_sec = current_time.tv_sec;
            previous_time.tv_nsec = current_time.tv_nsec;
 
            if (sem_wait(&send_marker) == -1) {
                printf("Error during wait on mutex.\n");
                exit(1);
            } 

            snapshot[snapshot_id].state = node_state;
            memcpy(snapshot[snapshot_id].timestamp, timestamp, sizeof(int) * nb_nodes);
            snapshot[snapshot_id].color = Red;
            last_snapshot_id = snapshot_id;
            send_marker_messages(snapshot_id);

            if (sem_post(&send_marker) == -1) {
                printf("Error during signal on mutex.\n");
                exit(1);
            } 
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
    printf("OUTPUT\n");
    int txtlength = strlen(system_config.config_name);
    int outlength = strlen(system_config.config_name) + 10;
    char * partial = malloc(txtlength-3);
    char * file = malloc(outlength);
    memmove(partial, system_config.config_name, txtlength-4);
    partial[txtlength-4] = '\0';

    snprintf(file, outlength, "%s-%d.out", partial, node_id);
    FILE * fp = fopen(file, "w");
    int snapshot_counter;
    int vector_counter;
    for (snapshot_counter = 0; snapshot_counter < last_cast_id + 1; snapshot_counter++)
    {
        for (vector_counter = 0; vector_counter < nb_nodes; vector_counter++)
        {
            fprintf(fp, "%d ", snapshot[snapshot_counter].timestamp[vector_counter]);
        }

        fprintf(fp, "\n");
    }
    fclose(fp);
    free (partial);
    free (file); 
    free(neighbors);
    int i, j;
    for (i = 0; i < dimension; i++)
    {
        free(snapshot[i].timestamp);
        free(snapshot[i].neighbors);

        if (node_id == 0) {
            for (j = 1; j < nb_nodes; j++)
            {
                if (i < last_cast_id)
                {
                    free(snapshots[i][j].timestamp);
                }
            }
        }
        free(snapshots[i]);
    }
    free(snapshot);
    free(snapshots);
    free(timestamp);
    free(number_received);
}
