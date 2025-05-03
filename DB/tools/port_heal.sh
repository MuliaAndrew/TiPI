#!/bin/bash -ex

sudo iptables -F INPUT || true
sudo iptables -F OUTPUT || true