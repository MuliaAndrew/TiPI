#!/bin/bash -ex

pkill DBserver
sleep $1
/root/DBserver --addr $SELF_IP --config $CONFIG --log /var/log/DB.log 