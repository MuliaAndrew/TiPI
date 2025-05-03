#!/bin/bash -ex

SELF_NAME=$1
NODE_NAMES=(node1 node2 node3 node4 node5)
CONFIG=$CFG_PATH
DB=/root/DB/db
LOG=/var/log/DB.log

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

if [[ "x$DELAY_MS" == "x" ]]; then
  DELAY_MS=200
fi

## adding DELAY_MS ms latency between any nodes
sudo tc qdisc add dev eth0 root netem delay ${DELAY_MS}ms

/root/DBserver --addr $SELF_IP --config $CONFIG --log $LOG --db $DB
