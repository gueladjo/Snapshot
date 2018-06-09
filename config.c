#include "config.h"
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

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

