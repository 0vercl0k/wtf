#!/bin/bash

VM_IP=192.168.122.115
VM_HOME=/root

scp -o PreferredAuthentications=password stackoverflow root@${VM_IP}:${VM_HOME}/
