#!/bin/bash
export STORAGE_DIR=/storage
export TOPLINGDB_INSTALL_DIR=$STORAGE_DIR/toplingdb
export MYTOPLING_INSTALL_DIR=$STORAGE_DIR/mytopling
export LD_LIBRARY_PATH=$TOPLINGDB_INSTALL_DIR/lib64/:$MYTOPLING_INSTALL_DIR/lib/private:$MYTOPLING_INSTALL_DIR/lib/plugin

export NFS_MOUNT_ROOT=/storage
export ROCKSDB_KICK_OUT_OPTIONS_FILE=1
export MULTI_PROCESS=1

#export LOG_LEVEL=3
#export JsonOptionsRepo_DebugLevel=3
export CPU_CORE_COUNT=`nproc`
export MAX_PARALLEL_COMPACTIONS=$[$CPU_CORE_COUNT * 1]
export MAX_WAITING_COMPACTIONS=$[$CPU_CORE_COUNT * 2]
export DOCUMENT_ROOT=/infolog/
export WORKER_DB_ROOT=$DOCUMENT_ROOT/infolog
export STD_ROOT=$DOCUMENT_ROOT/stdlog


mkdir $DOCUMENT_ROOT -p
mkdir $WORKER_DB_ROOT -p
mkdir $STD_ROOT -p

LD_PRELOAD=libmytopling_dc.so $STORAGE_DIR/dcompact/dcompact_worker.exe \
    -D listening_ports=8000 -D num_threads=50 \
    -D document_root=$DOCUMENT_ROOT >> $STD_ROOT/stdout 2>> $STD_ROOT/stderr
