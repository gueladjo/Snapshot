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
    int * nodeIDs; 
    char ** hostNames; 
    int * portNumbers;
    int * neighborCount;
    int ** neighbors;
} config, *config_ptr;

int* read_config_file(config *);
void display_config(config);
void free_config(config);

#endif
