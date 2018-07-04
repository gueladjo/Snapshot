
## MAP protocol and termination detection

This program implements a program running in a distributed system node. It runs an MAP protocol 
and uses Lamport Snapshot protocol to detect termination of the MAP protocol. A protocol is then 
implemented to bring all nodes to a halt.

## Launcher script

The launcher.sh script logs into the machines specified in the configuration file and starts each
node individually. It uses UT Dallas "dcXX" distributed systems servers.

## Configuration file

A configuration file is supplied to determine the distributed system topology, host names, ports 
and application parameters.

## Compile the program:

make

## Run the program:

./node nodeID config_file

example: ./node 2 config_files/config.txt
