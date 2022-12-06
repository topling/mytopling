set -x

date

export NFS_MOUNT_ROOT=/tmp/mnt
export WORKER_DB_ROOT=/shared/dcompact

INSTANCE_NAME=mytopling-test-instance
mkdir  -p ${NFS_MOUNT_ROOT}/${INSTANCE_NAME}
umount -l ${NFS_MOUNT_ROOT}/${INSTANCE_NAME}

export LOG_LEVEL=2 # dcompact main binary debug level
export JsonOptionsRepo_DebugLevel=2

export NFS_DYNAMIC_MOUNT=1
export MAX_PARALLEL_COMPACTIONS=32
export MAX_WAITING_COMPACTIONS=64
export ROCKSDB_KICK_OUT_OPTIONS_FILE=1
export WEB_DOMAIN=topling.in

if [ -z "${type}" ]; then
    type=rls  # default is release
fi
export LD_LIBRARY_PATH=/node-shared/opt/gcc-12.1.0/lib64:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/node-shared/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/node-shared/mytopling-${type}/lib/plugin:$LD_LIBRARY_PATH

# begin ToplingZipTable env
# ToplingZipTable is not included in MyTopling & ToplingDB community,
# in community version, these env will be ignored
export ZIP_SERVER_OPTIONS=listening_ports=8090:num_threads=32
# ToplingZipTable_XXX can be override here by env, which will
# override the values defined in db Hoster side's json config.
#
# Hoster side's json config will be passed to compact worker through
# rpc, then it may be override by env defined here!
#
export DictZipBlobStore_zipThreads=28
export ToplingZipTable_debugLevel=2
export ToplingZipTable_nltBuildThreads=1
export ToplingZipTable_localTempDir=/tmp
export ToplingZipTable_warmupLevel=kValue
export ZipServer_nltBuildThreads=5
rm -f ${ToplingZipTable_localTempDir}/Terark-*
# end ToplingZipTable env

export ADVERTISE_ADDR=${HOSTNAME}:${PORT}

if [ ${type} = dbg ]; then
    dbg="gdb --args"
    cmd="./debug_dcompact_worker.exe"
    export MULTI_PROCESS=0
else
    cmd="./dcompact_worker.exe"
    export MULTI_PROCESS=1
fi

ulimit -n 100000

# one dcompact worker process can serve both Todis and MyTopling
#dbg="env $etcd_url LD_PRELOAD=libblackwidow.so:libmytopling_dc.so ${dbg}"

# libmytopling_dc.so is for MyTopling
dbg="env $etcd_url LD_PRELOAD=libmytopling_dc.so ${dbg}"
now=`date +%FT%T`

# dcompact worker's http service port
PORT=8080
run="$dbg $cmd \
        -D listening_ports=${PORT} \
        -D document_root=${WORKER_DB_ROOT} \
        -D num_threads=32"
if [ "${REDIRECT}" = "1" ]; then
    stderr=stderr.${PORT}.${now}
    stdout=stdout.${PORT}.${now}
    pushd ${WORKER_DB_ROOT}/stdlog
    touch ${stderr}
    touch ${stdout}
    ln -sf ${stderr} stderr.${PORT}
    ln -sf ${stdout} stdout.${PORT}
    popd
    stderr=${WORKER_DB_ROOT}/stdlog/${stderr}
    stdout=${WORKER_DB_ROOT}/stdlog/${stdout}
    $run > ${stdout} 2> ${stderr}
else
    $run
fi
wait
