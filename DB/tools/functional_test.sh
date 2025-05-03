#!/bin/bash -x

if [[ "x${TEST}" == "xyes" ]]; then 

  # wait for cluster to be ready
  hlt=1
  while [[ $hlt == 1 ]]; do
    N=()
    for i in `seq 5`; do
      N+=`curl -m 1 -s -L -X "GET" http://node${i}:40404/`
    done
    for ans in "${N[@]}"; do
      if [[ `echo $ans | grep "master"` != "" ]]; then
        hlt=0
        break
      fi
    done
  done

  T1=`curl -m 1 -s -L -X "GET" http://node1:40404/kvs/x`
  T2=`curl -m 1 -s -L -X "POST" http://node1:40404/kvs/x?value=14`
  T3=`curl -m 1 -s -L -X "GET" http://node3:40404/kvs/x`
  T4=`curl -m 1 -s -L -X "PATCH" http://node4:40404/kvs/x?value=128`
  T5=`curl -m 1 -s -L -X "POST" http://node5:40404/kvs/x?value=14`
  T6=`curl -m 1 -s -L -X "GET" http://node1:40404/kvs/x`
  T7=`curl -m 1 -s -L -X "DELETE" http://node2:40404/kvs/x`
  T8=`curl -m 1 -s -L -X "GET" http://node3:40404/kvs/x`

  if [[ $T1 != "key x does not exist in data base" || \
        $T2 != "{\"operand\":\"x\",\"value\":\"14\"}" || \
        $T3 != "{\"operand\":\"x\",\"value\":\"14\"}" || \
        $T4 != "{\"operand\":\"x\",\"value\":\"128\"}" || \
        $T5 != "Key x already exist" || \
        $T6 != "{\"operand\":\"x\",\"value\":\"128\"}" || \
        $T7 != "{\"operand\":\"x\"}" || \
        $T8 != "key x does not exist in data base" ]]
  then
    exit 1
  fi
fi