#!/bin/bash


# Change this to your netid
netid=mxb173430

#
# Root directory of your project
PROJDIR=$HOME/snapshot

#
# Directory where the config file is located on your local system
CONFIGLOCAL=$HOME/launch/config.txt

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
        host=$( echo $line | awk '{ print $2 }' )

        echo $host
        ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no $netid@$host.utdallas.edu killall -u $netid &
        sleep 1

        n=$(( n + 1 ))
    done
   
)


echo "Cleanup complete"
