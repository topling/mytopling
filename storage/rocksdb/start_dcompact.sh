#!/bin/bash

if [ -z "$type" ]; then
  type=rls
fi
libdirs=(
  /node-shared/mytopling-$type/lib/plugin
  /node-shared/mytopling-$type/lib/private
  /node-shared/mytopling-$type/lib
  /node-shared/lib
)
libdirs=("${libdirs[@]}") # join array to one string
export LD_LIBRARY_PATH=${libdirs// /:}:${LD_LIBRARY_PATH}

export ROCKSDB_KICK_OUT_OPTIONS_FILE=1
export MULTI_PROCESS=1

#export LOG_LEVEL=3

# MyTopling 企业版(包含 topling-rocks 模块) 必须配置该变量
export ZIP_SERVER_OPTIONS="listening_ports=8090:num_threads=320"
export ZipServer_nltBuildThreads=11

#export ToplingZipTable_debugLevel=2

#export LOG_LEVEL=3
#export SidePluginRepo_DebugLevel=3
export CPU_CORE_COUNT=`nproc`

# 本地测试，手工启动 http mock 竞价实例回收检测，http 不可达或 code 404 表示正常，其它表示即将回收
#export TERMINATION_CHECK_URL=http://192.168.31.100:2011

# AWS Spot Instance termination check, http 404 indicate ok, others for going to terminating
#export TERMINATION_CHECK_URL=http://169.254.169.254/latest/meta-data/spot/termination-time

# 阿里云抢占式实例回收检测
#export TERMINATION_CHECK_URL=http://100.100.100.200/latest/meta-data/instance/spot/termination-time

# 腾讯云竞价实例回收检测
#export TERMINATION_CHECK_URL=http://metadata.tencentyun.com/latest/meta-data/spot/termination-time

# 华为云竞价实例回收检测，AWS EC2 兼容 API？
#export TERMINATION_CHECK_URL=http://169.254.169.254/latest/meta-data/spot/termination-time

# 华为云 openstack
#export TERMINATION_CHECK_URL=http://169.254.169.254/openstack/latest/meta_data.json

#export MAX_PARALLEL_COMPACTIONS=3 # for test queue scheduling
export MAX_PARALLEL_COMPACTIONS=$[$CPU_CORE_COUNT * 4]
export MAX_WAITING_COMPACTIONS=$[$CPU_CORE_COUNT * 5]
export DEL_WORKER_TEMP_DB=0
export ENABLE_HTTP_STOP=1
export WORKER_DB_ROOT=/nvme-shared/infolog/dcompact-worker-1
export NFS_MOUNT_ROOT=/nvme-shared/mytopling
#export NFS_MOUNT_ROOT=/ram-shared/mytopling
export DictZipBlobStore_zipThreads=32
export ToplingZipTable_localTempDir=/dev/shm
rm -f ${ToplingZipTable_localTempDir}/Topling-* # 清理上次运行结束时的遗留垃圾文件

mkdir -p $WORKER_DB_ROOT

ulimit -n 100000
#sudo sysctl -w vm.max_map_count=1048576

if [ $type = dbg ]; then
  dbg="gdb --args"
fi
#dbg="ldd"
env LD_PRELOAD=libmytopling_dc.so $dbg /node-shared/bin/dcompact_worker.exe \
    -D listening_ports=8080 -D num_threads=50 \
    -D document_root=$WORKER_DB_ROOT #>> $WORKER_DB_ROOT/stdout 2>> $WORKER_DB_ROOT/stderr
