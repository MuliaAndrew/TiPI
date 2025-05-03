#!/bin/bash -ex

if [[ `sudo iptables -L INPUT --line-numbers | grep DROP ` == "" ]]; then
  sudo iptables -A INPUT -p tcp --destination-port 40404 -j DROP
fi 

if [[ `sudo iptables -L OUTPUT --line-numbers | grep DROP ` == "" ]]; then
  sudo iptables -A OUTPUT -p tcp --destination-port 40404 -j DROP
fi 