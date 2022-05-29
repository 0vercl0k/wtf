#!/bin/bash

# check to see if the user specified a target folder to fuzz
if [[ -z $1 ]]; then echo "specify target folder name"; exit; fi;

# convert the raw dump to mem.dmp
../raw2dmp/raw2dmp raw

# sets the target folder for fuzzing
TARGET_FOLDER=${WTF}/targets/$1

# creates the target folder
mkdir ${TARGET_FOLDER}

# create the required directories for wtf
mkdir ${TARGET_FOLDER}/crashes
mkdir ${TARGET_FOLDER}/inputs
mkdir ${TARGET_FOLDER}/outputs
mkdir ${TARGET_FOLDER}/state

# move created files into the target folder
mv mem.dmp ${TARGET_FOLDER}/state/
mv regs.json ${TARGET_FOLDER}/state/
mv symbol-store.json ${TARGET_FOLDER}/state/

# move recompilation script to target folder
cp recompile_wtf.sh ${TARGET_FOLDER}/