#ifndef _CONFIG
#define _CONFIG

typedef struct config {
    int nodes_in_system;
    int min_per_active;
    int max_per_active;
    int min_send_delay;
    int snapshot_delay;
    int max_number;
    // 5 parallel arrays for node information   
    char * config_name;
    int * nodeIDs; 
    char ** hostNames; 
    int * portNumbers;
    int * neighborCount;
    int ** neighbors;
} config, *config_ptr;

int* read_config_file(config *, char*);
void display_config(config);
void free_config(config);
int ** create_spanning_tree(int ** out_tree_neighbors, int **,  int* nodeIDs, int** nodeNeighbors, int * nodeNeighborCount, int numNodes);
int **  DFS(int atNodeIndex, int ** neighborIndices, int * nodeNeighborCount, int numNodes, int ** tree, int * tree_neighbor_count, int * visited, int *);
int ** convertToIndex(int * nodeIDs, int ** nodeNeighbors, int * nodeNeighborCount, int numNodes);
int find(int value, int * array, int length);
void printArray(int * array, int length);

#endif
