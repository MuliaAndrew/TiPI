#!/bin/bash -ex

SELF_NAME=$1
NODE_NAMES=(node1 node2 node3 node4 node5)
CONFIG=/root/DB.config

export SELF_IP=`hostname -I`

touch $CONFIG
for name in "${NODE_NAMES[@]}"; do
  if [[ x"$name" != x"$SELF_NAME" ]]; then 
    node_addr=$(dig +short $name)
    while [[ "x$node_addr" = "x" ]]; do
      sleep 1
      node_addr=$(dig +short $name)
    done 
    echo $node_addr >> $CONFIG
  fi
done

cat $CONFIG

ls /root

/root/DBserver --addr $SELF_IP --config $CONFIG --log /var/log/DB.log 
