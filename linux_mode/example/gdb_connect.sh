#!/bin/bash

source vars

if [[ -z $1 ]]; then echo "specify target"; exit; fi;

TARGET=$1

gdb \
	${KERNEL} \
	-x $TARGET/bpkt.py \
	-ex continue
