#!/bin/bash

# check to see whether the user has specified a file to upload
if [[ -z $1 ]]; then echo "specify file to upload"; exit; fi;

# upload the file to the home of root on the qemu image
scp -P 10022 $1 root@localhost:/root/