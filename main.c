#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>
#include<pthread.h>

typedef struct Neighbor {
    int id;
    int port;
    char hostname[100];
} Neighbor;

typedef struct config {
    int nodes_in_system;
    int min_per_active;
    int max_per_active;
    int min_send_delay;
    int snapshot_delay;
    int max_number;
    // 5 parallel arrays for node information   
    int * nodeIDs; 
    char ** hostNames; 
    int * portNumbers;
    int * neighborCount;
    int ** neighbors;
} config, *config_ptr;

void* handle_neighbor(void* arg);
int* read_config_file(config *);
void display_config(config);
void free_config(config);

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

int* read_config_file(config * system)
{    
    char * fileName = "config.txt";
    FILE * fp = fopen(fileName, "r");

    int tokensRead = 0;
    int tokensInLine = 6; // First valid line has 6 tokens
    int linesRead = 0;

    if (fp)
    {
        int system_info[6];
        while (tokensRead < tokensInLine) // First Line
        {
            int input;
            char charInput;
            fscanf(fp, "%d", &input);
            if (input > 0)
            {
                system_info[tokensRead] = input;
                tokensRead++;                
            }
            else 
            {
                fscanf(fp, "%c", &charInput);
            }
        } 
        // First line done

        system->nodes_in_system = system_info[0];
        system->min_per_active = system_info[1];
        system->max_per_active = system_info[2];
        system->min_send_delay = system_info[3];
        system->snapshot_delay = system_info[4];
        system->max_number = system_info[5];
        
        system->nodeIDs = (int*)malloc(system->nodes_in_system * sizeof(int));
        system->hostNames = (char **)malloc(system->nodes_in_system * sizeof(char*));
        int i;
        for (i = 0; i < system->nodes_in_system; i++)
        {
            system->hostNames[i] = (char*)malloc(5 * sizeof(char));// 5 = length of string (dc##) + 1
        }
        system->portNumbers = (int*)malloc(system->nodes_in_system * sizeof(int));
        system->neighborCount = (int*)malloc(system->nodes_in_system * sizeof(int)); 
        system->neighbors = (int**)malloc(system->nodes_in_system * sizeof(int*));

        tokensRead = 0;
        tokensInLine = 3; // nodeID hostName listenPort
        linesRead = 0;

        // Begin second part TODO: stop at comments (#)
        while (linesRead < system->nodes_in_system) // nodes_in_system = nodes in system
        {
            tokensRead = 0;
            while (tokensRead < tokensInLine)
            {
                int input;
                char stringInput[5];
                char charInput;                   
                
                fscanf(fp, "%d", &input);

                if (input >= 0)
                {
                    system->nodeIDs[linesRead] = input;
                    tokensRead++;                    
                }

                fscanf(fp, "%s", stringInput);

                strcpy(system->hostNames[linesRead], stringInput); // Not sure why I'm getting a warning here?
                tokensRead++;                                   

                fscanf(fp, "%d", &input);

                if (input >= 0)
                {
                    
                    system->portNumbers[linesRead] = input;
                    tokensRead++;
                }

                fscanf(fp, "%c", &charInput);    
                            
                linesRead++;
            }
        } // Done with second part

        linesRead = 0;
        tokensRead = 0;
        int tempIndex = 0;

        // Third Part
        int * tempArray = (int*)malloc((system->nodes_in_system - 1)* sizeof(int)); //max possible neighbors 
        
        while (linesRead < system->nodes_in_system && !feof(fp))
        {
            char charInput = '\0';
            int input;
            int valuesRead;
            int neighborIndex;
            
            
            valuesRead = fscanf(fp, "%d%c", &input, &charInput);

            if (valuesRead)
            {
                tempArray[tempIndex] = input;
                tempIndex++;

                if (charInput == 13 || feof(fp))
                {
                    system->neighbors[linesRead] = (int*)malloc((tempIndex+1) * sizeof(int));
                    for (neighborIndex = 0; neighborIndex < tempIndex; neighborIndex++) // tempIndex = neighborCount for node at lineRead (first line = fisrt node)
                    {
                        system->neighbors[linesRead][neighborIndex] = tempArray[neighborIndex];
                    }
               
                    
                    system->neighborCount[linesRead] = tempIndex;
                    linesRead++;
                    tempIndex = 0;
                }
            }
        }
        free(tempArray);
        fclose(fp);
    }
    else
    {
        printf("Error opening file\n");
    }
}

void display_config(config system_config)
{
    printf("Nodes: %d\nminPerActive: %d\nmaxPerActive: %d\nminSendDelay: %d\nsnapshopDelay: %d\nmaxNumber: %d\n\n",
    system_config.nodes_in_system,
    system_config.min_per_active,
    system_config.max_per_active,
    system_config.min_send_delay,
    system_config.snapshot_delay,
    system_config.max_number);

    // Testing Host Names
    int i, j;    
    for (i = 0; i < system_config.nodes_in_system; i++)
    {
        printf("Node %d: Host: %s\n", system_config.nodeIDs[i], system_config.hostNames[i]);
    }
    printf("\n");

    for (i = 0; i < system_config.nodes_in_system; i++)
    {
        printf("Node %d: Neighbor Count: %d\n", system_config.nodeIDs[i], system_config.portNumbers[i]);
    }
    printf("\n");

    // Testing neighbor Count and list of neighbors 

    for (i = 0; i < system_config.nodes_in_system; i++)
    {
        printf("Node %d: Neighbor Count: %d\n", system_config.nodeIDs[i], system_config.neighborCount[i]);
    }
    printf("\n");

    for (i = 0; i < system_config.nodes_in_system; i++)
    {
        printf ("Node %d Neighbors: ", system_config.nodeIDs[i]);
        for (j= 0; j < system_config.neighborCount[i]; j++)
        {
            printf("%d ", system_config.neighbors[i][j]);
        }
        printf("\n");
    }
}

void free_config(config system_config)
{
    int i;
    free(system_config.nodeIDs);
    for (i = 0; i < system_config.nodes_in_system; i++)
    {
        free(system_config.hostNames[i]);
    }
    free(system_config.hostNames);
    free(system_config.portNumbers);
    free(system_config.neighborCount);

    for (i = 0; i < system_config.nodes_in_system; i++)
    {
        free(system_config.neighbors[i]);
    }
    free(system_config.neighbors);
}
