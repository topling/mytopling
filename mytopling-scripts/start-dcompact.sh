#!/bin/bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/mynfs/opt/lib
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/mynfs/opt/lib/plugin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/mynfs/opt/lib/private


export ROCKSDB_KICK_OUT_OPTIONS_FILE=1
export MULTI_PROCESS=1

# MyTopling 企业版(包含 topling-rocks 模块) 必须配置该变量
export ZIP_SERVER_OPTIONS="listening_ports=8090:num_threads=32"
rm -f /tmp/Topling-* # 清理上次运行结束时的遗留垃圾文件

DOCUMENT_ROOT=/mnt/mynfs/infolog/dcompact-worker-1

#export LOG_LEVEL=3
#export JsonOptionsRepo_DebugLevel=3
export CPU_CORE_COUNT=`nproc`
export MAX_PARALLEL_COMPACTIONS=$[$CPU_CORE_COUNT * 1]
export MAX_WAITING_COMPACTIONS=$[$CPU_CORE_COUNT * 2]
export WORKER_DB_ROOT=$DOCUMENT_ROOT/worker
export NFS_MOUNT_ROOT=/mnt/mynfs/datadir

STD_ROOT=$DOCUMENT_ROOT/stdlog

mkdir -p $DOCUMENT_ROOT
mkdir -p $WORKER_DB_ROOT
mkdir -p $STD_ROOT

env LD_PRELOAD=libmytopling_dc.so /mnt/mynfs/opt/bin/dcompact_worker.exe \
    -D listening_ports=8000 -D num_threads=50 \
    -D document_root=$DOCUMENT_ROOT >> $STD_ROOT/stdout 2>> $STD_ROOT/stderr
