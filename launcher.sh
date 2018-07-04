#!/bin/bash

# Change this to your netid
netid=mxb173430

# Root directory of program and config file
PROG=/home/013/m/mx/mxb173430/snapshot/node
CONFIG=/home/013/m/mx/mxb173430/snapshot/conf2.txt

# Directory where the config file is located on your local system
CONFIGLOCAL=$HOME/launch/conf2.txt

n=0

# sed commands to ignore lines starting with # or _
cat $CONFIGLOCAL | sed -e "s/#.*//" | sed -e "/^\s*$/d" |
(
    read -n 2 i
    echo $i
    read line
    while [[ $n -lt $i ]]
    do
    	read line
    	p=$( echo $line | awk '{ print $1 }' )
	echo $p
        host=$( echo $line | awk '{ print $2 }' )
	echo $host
	
	ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no $netid@$host.utdallas.edu $PROG $p $CONFIG &

        n=$(( n + 1 ))
    done
)
