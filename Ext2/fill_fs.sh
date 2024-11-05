#!/bin/bash -x

GIT_DIR=$(git rev-parse --show-toplevel)
FS_DIR="${GIT_DIR}/Ext2/fs/"

N_FILES=($1)
N_KBYTES=($2)
DIR_TO_FILL=($3)

if [ -z $N_FILES ] && [ -z $N_KBYTES ] && [ -z $DIR_TO_FILL ]
then
  echo $N_KBYTES $N_FILES $DIR_TO_FILL
  exit
else
  cd $FS_DIR
  rm -rf *.txt
  
  for f_num in $(seq 1 ${N_FILES})
  do 
    dd if=/dev/urandom of=$DIR_TO_FILL/$f_num.txt count=$(expr $N_KBYTES \* 2)
  done
fi



