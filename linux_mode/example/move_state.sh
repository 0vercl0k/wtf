#!/bin/bash

if [[ -z $1 ]]; then echo "specify target"; exit; fi;

TARGET=$1

if [[ ! -z $TARGET ]] && [[ -d $TARGET ]]
then
    mv mem.dmp regs.json symbol-store.json $TARGET/fuzzer/state/
    mv $TARGET/$TARGET.cov $TARGET/fuzzer/coverage/
fi
