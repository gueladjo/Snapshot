#include "config.h"
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

int* read_config_file(config * system, char* fileName)
{    
    system->config_name = fileName;
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
            int matched = 0;
            matched = fscanf(fp, "%d", &input);
            if (matched)
            {
                system_info[tokensRead] = input;
                tokensRead++;                
            }
            else 
            {
                fscanf(fp, "%c", &charInput);
                if (charInput == '#')
                {
                    if (tokensRead > 0)
                    {
                        while (charInput != '\n')
                            fscanf(fp, "%c", &charInput);
                        break;
                    }
                    else
                    {
                        while (charInput != '\n')
                            fscanf(fp, "%c", &charInput);                            
                    }
                }
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
           system->hostNames[i] = (char*)malloc(18 * sizeof(char));// 18 = length of string (dc##.utdallas.edu) + 1
        }
        system->portNumbers = (int*)malloc(system->nodes_in_system * sizeof(int));
        system->neighborCount = (int*)malloc(system->nodes_in_system * sizeof(int)); 
        system->neighbors = (int**)malloc(system->nodes_in_system * sizeof(int*));

        tokensRead = 0;
        tokensInLine = 3; // nodeID hostName listenPort
        linesRead = 0;

        while (linesRead < system->nodes_in_system) 
        {            
            tokensRead = 0;
            while (tokensRead < tokensInLine)
            {
                int input;
                char stringInput[5];
                char charInput;
                int matched = 0;                   
                

                while (!matched)
                {
                    matched = fscanf(fp, "%d", &input);            
                    if (input >= 0 && matched)
                    {
                        system->nodeIDs[linesRead] = input;
                        tokensRead++;   
                    }
                    else 
                    {
                        fscanf(fp, "%c", &charInput);
                        while (charInput != '\n')
                        {                
                                        
                            fscanf(fp, "%c", &charInput);                        
                        }
                    }
                }

                fscanf(fp, "%s", stringInput);

                //strcpy(system->hostNames[linesRead], stringInput); // Not sure why I'm getting a warning here?
                snprintf(system->hostNames[linesRead], 18, "%s.utdallas.edu", stringInput);
                tokensRead++;                                   

                matched = fscanf(fp, "%d", &input);

                if (matched)
                {
                    system->portNumbers[linesRead] = input;
                    tokensRead++;
                }
                else 
                {
                    fscanf(fp, "%c", &charInput);
                    while (charInput != '\n')
                    {
                        
                        fscanf(fp, "%c", &charInput);                        
                    }
                }      
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
            int matched;
            int neighborIndex;     


            charInput = fgetc(fp);
            
            if (charInput != ' ' && charInput != '#' && charInput != '\n')
            {
                if (charInput <= 57 && charInput >= 48)
                {
                    char tensPlace = charInput;
                    char onesPlace;
                    charInput = fgetc(fp);
                    if (charInput <= 57 && charInput >= 48)
                    {
                        onesPlace = charInput;
                        input = atoi(&tensPlace)*10 + atoi(&onesPlace);
                        
                    }
                    else 
                    {
                        input = atoi(&tensPlace);
                        tempArray[tempIndex] = input;
                        tempIndex++;
                        if (charInput == '\n' ||charInput == '#' || feof(fp))
                        {
                            if (tempIndex >0)
                            {
                            system->neighbors[linesRead] = (int*)malloc((tempIndex+1) * sizeof(int));
                                
                                for (neighborIndex = 0; neighborIndex < tempIndex; neighborIndex++) // tempIndex = neighborCount for node at lineRead (first line = fisrt node)
                                {
                                    system->neighbors[linesRead][neighborIndex] = tempArray[neighborIndex];
                                }
                                system->neighborCount[linesRead] = tempIndex;
                                tempIndex = 0;
                                linesRead++;
                            }
                            if (charInput == '#')
                            {
                                while (charInput != '\n' )
                                {
                                    charInput = fgetc(fp);
                                }
                            }
                        }
                    }
                }
            }
            else if (charInput == '\n' ||charInput == '#' || feof(fp))
            {
                if (tempIndex >0)
                {
                system->neighbors[linesRead] = (int*)malloc((tempIndex+1) * sizeof(int));
                    
                    for (neighborIndex = 0; neighborIndex < tempIndex; neighborIndex++) // tempIndex = neighborCount for node at lineRead (first line = fisrt node)
                    {
                        system->neighbors[linesRead][neighborIndex] = tempArray[neighborIndex];
                    }
                    system->neighborCount[linesRead] = tempIndex;
                    tempIndex = 0;
                    linesRead++;
                }
 
                if (charInput == '#')
                {
                    while (charInput != '\n')
                    {
                        charInput = fgetc(fp);
                    }
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

// Create Spanning tree returns an array that is parallel to nodeIDs
// each element is another array that is the indices (of nodeID) that are spanning tree neighbors
// Example: If nodeIDs are      2 4 1 6 10
// and the spanning tree neighbors of node 2 are 4 and 1. 
// then tree[0] would be [1 | 2]
// That is, the neighbors of node at index 0 are index 1 and 2

                            // Pass by reference, not a 2d array
int ** create_spanning_tree(int ** out_tree_neighbor_count, int ** out_parents, int* node_ids, int** neighbor_at, int * num_neighbors_at, int num_nodes)
{
    int * visited = (int * )malloc(num_nodes * sizeof(int));
    int ** tree = (int **)malloc(num_nodes * sizeof(int*));
    int j;
    for (j = 0; j < num_nodes; j++)
    {
        tree[j] = (int*)malloc(0);
    }

    *out_tree_neighbor_count = (int * )malloc(num_nodes *  sizeof(int));
    *out_parents = (int * )malloc(num_nodes *  sizeof(int));
    
    int ** neighbor_index_at = convertToIndex(node_ids, neighbor_at,num_neighbors_at, num_nodes);
    memset(visited, 0, num_nodes*sizeof(int));
    memset(*out_tree_neighbor_count, 0, num_nodes*sizeof(int));
    DFS(0, neighbor_index_at, num_neighbors_at, num_nodes, tree, *out_tree_neighbor_count, visited, *out_parents);

    free (visited);
    return tree;
}

int **  DFS(int current_index, int ** neighbor_indices, int * num_neighbors_at, int num_nodes, int ** tree, int * tree_neighbor_count, int * visited, int * parents)
{
    int i = 0;
    visited[current_index] = 1;

    while ( i < num_neighbors_at[current_index])
    {      
        if (!visited[neighbor_indices[current_index][i]])
        {
            // Set unvisited neighbor as tree neighbor
            tree_neighbor_count[current_index]++;
            tree[current_index] = (int*)realloc(tree[current_index], tree_neighbor_count[current_index] * sizeof(int));
            tree[current_index][tree_neighbor_count[current_index] - 1] = neighbor_indices[current_index][i];

            //Reverse: set neighbor's neighbor as self
            tree_neighbor_count[neighbor_indices[current_index][i]]++;            
            tree[neighbor_indices[current_index][i]]  = (int*)realloc(tree[neighbor_indices[current_index][i]], tree_neighbor_count[neighbor_indices[current_index][i]] * sizeof(int));
            tree[neighbor_indices[current_index][i]][tree_neighbor_count[neighbor_indices[current_index][i]] - 1] = current_index;

            // set neighbor's parent as self
            parents[neighbor_indices[current_index][i]] = current_index;
            // visit neighbor 
            DFS(neighbor_indices[current_index][i], neighbor_indices, num_neighbors_at, num_nodes, tree, tree_neighbor_count, visited, parents);
        }
        else
            i++;
    }
    return tree; 
}


// Converts neighbor_ids array into neighbor_indices array
// Example: If nodeIDs are      2 4 1 6 10
// and the neighbors of 2 (index 0) are 1, 6, and 10
// (input) neighbor_ids[0] = [1 | 6 | 10]
// (output) neighbor_indices[0] = [2 | 3 | 4]
int ** convertToIndex(int * node_ids, int ** neighbor_ids, int * num_neighbors_at, int num_nodes)
{
    return neighbor_ids;
}

void printArray(int * array, int length)
{
    int i;
    for (i = 0; i < length; i++)
    {
        printf("%d ", array[i]);
    }
    printf("\n");
}

// returns index of value in array
int find(int value, int * array, int length)
{
    return value;
}
