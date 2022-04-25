#!/bin/bash

source ../vars

scp -o PreferredAuthentications=password stackoverflow root@${VM_IP}:${VM_HOME}/
