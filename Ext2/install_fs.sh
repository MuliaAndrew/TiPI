#!/bin/bash -x

GIT_DIR=$(git rev-parse --show-toplevel)
FS_DIR="${GIT_DIR}/Ext2/fs"
IMAGE="${GIT_DIR}/Ext2/floppy.img"

if [ ! -f $IMAGE ] 
then
  dd if=/dev/zero of=$IMAGE bs=512 count=$(expr 1024 \* 64)
  mkfs.ext2 $IMAGE
fi

if [ ! -d $FS_DIR ]
then
  mkdir $FS_DIR
fi

mountpoint $FS_DIR
if [ $? -eq 32 ]
then 
  mount -t ext2 $IMAGE $FS_DIR/
  chmod 777 $FS_DIR
fi