#!/bin/bash

existing=`pgrep mysqld`
if [ $? -eq 0 ] ; then
	echo There is a running mysqld process: $existing
	exit 1
fi

export LD_LIBRARY_PATH=/mnt/mynfs/opt/gcc_12_lib64:/mnt/mynfs/opt/lib:$LD_LIBRARY_PATH
export ROCKSDB_KICK_OUT_OPTIONS_FILE=1
export TOPLING_SIDEPLUGIN_CONF=/mnt/mynfs/opt/mytopling-scripts/mytopling-enterprise.json

#export JsonOptionsRepo_DebugLevel=2
#export csppDebugLevel=0
export TOPLINGDB_CACHE_SST_FILE_ITER=1
export BULK_LOAD_DEL_TMP=1

MYTOPLING_DATA_DIR=/mnt/mynfs/datadir/mytopling-instance-1
MYTOPLING_LOG_DIR=/mnt/mynfs/infolog/mytopling-instance-1
rm -rf ${MYTOPLING_DATA_DIR}/.rocksdb/job-*
ulimit -n 100000

if [ -z "${MY_USER}" ]; then
  if [ -z "${USER}" ]; then
    echo env USER is not defined
    exit 1
  else
    MY_USER=$USER
  fi
fi

mkdir -p /var/lib/mysql # for sock file

if [ ! -e /mnt/mynfs/datadir/mytopling-instance-1/.rocksdb/IDENTITY ]; then
  if [ -e /mnt/mynfs/datadir ]; then
    echo "Dir '/mnt/mynfs/datadir' exists, but '/mnt/mynfs/datadir/mytopling-instance-1/.rocksdb/IDENTITY' does not exists"
    read -p 'Are you sure delete /mnt/mynfs/datadir and re-initialize database? yes(y)/no(n)' yn
    if [ "$yn" != "y" ]; then
      exit 1
    fi
  fi
  rm -rf /mnt/mynfs/{datadir,stdlog,log-bin,wal}
  echo Initializing mysql datadir...
  mkdir -p /mnt/mynfs/{datadir,log-bin,wal}/mytopling-instance-1
  mkdir -p /mnt/mynfs/infolog/mytopling-instance-1/stdlog
  cp /mnt/mynfs/opt/mytopling-scripts/{index.html,style.css} /mnt/mynfs/infolog/mytopling-instance-1
  if [ "${MY_USER}" = "root" ]; then
    /mnt/mynfs/opt/bin/mysqld --initialize-insecure --skip-grant-tables --datadir=/mnt/mynfs/datadir/mytopling-instance-1
  else
    groupadd -g 27 ${MY_USER}
    useradd ${MY_USER} -u 27 -g 27 --no-create-home -s /sbin/nologin
    chown ${MY_USER}:${MY_USER} -R /mnt/mynfs/infolog

    chown ${MY_USER}:${MY_USER} -R /mnt/mynfs/{datadir,log-bin,wal}/mytopling-instance-1
    /mnt/mynfs/opt/bin/mysqld --initialize-insecure --skip-grant-tables --datadir=/mnt/mynfs/datadir/mytopling-instance-1
    chown ${MY_USER}:${MY_USER} -R /mnt/mynfs/{datadir,log-bin,wal}/mytopling-instance-1 # must
  fi
fi

common_args=(
  --user=${MY_USER}
  --datadir=${MYTOPLING_DATA_DIR}
  --bind-address=0.0.0.0
  --disabled_storage_engines=myisam
  --host_cache_size=644
  --internal_tmp_mem_storage_engine=MEMORY
  --join_buffer_size=1048576
  --key_buffer_size=16777216
  --max_binlog_size=524288000
  --max_connections=8000
  --max_heap_table_size=67108864
  --read_buffer_size=1048576
  --skip_name_resolve=ON
  --table_open_cache=8192
  --thread_cache_size=200
  --enable_optimizer_cputime_with_wallclock=on
  --optimizer_switch=mrr=on,mrr_cost_based=off
  --performance_schema=off
  --default_authentication_plugin=mysql_native_password
  --secure_file_priv=''
  --transaction_isolation=READ-COMMITTED
)

rocksdb_args=(
  --plugin-load=ha_rocksdb_se.so
  --rocksdb --default-storage-engine=rocksdb
  --rocksdb_datadir=${MYTOPLING_DATA_DIR}/.rocksdb
  --rocksdb_allow_concurrent_memtable_write=on
  --rocksdb_force_compute_memtable_stats=off
  --rocksdb_reuse_iter=on # 此选项打开时，长期空闲的数据库连接会导致内存泄露，请谨慎使用
  --rocksdb_write_policy=write_committed
  --rocksdb_mrr_batch_size=32 --rocksdb_async_queue_depth=32
  --rocksdb_lock_wait_timeout=10
  --rocksdb_print_snapshot_conflict_queries=1
)

binlog_args=(
  # 使用 binlog-ddl-only 时 MyTopling 可配置为基于共享存储的多副本集群，
  # 但此配置下 MyTopling 不能做为传统 MySQL 主从中的上游数据库，因为此
  # 配置下 binlog 只会记录 DDL 操作
  --binlog-ddl-only=ON
  --binlog-order-commits=ON
)

# 修复引擎监控日志链接
sudo ln -sf $MYTOPLING_LOG_DIR $MYTOPLING_LOG_DIR/.rocksdb
sudo ln -sf $MYTOPLING_LOG_DIR/mnt_mynfs_datadir_mytopling-instance-1_.rocksdb_LOG \
           $MYTOPLING_LOG_DIR/LOG
rm -rf ${MYTOPLING_DATA_DIR}/.rocksdb/job*
/mnt/mynfs/opt/bin/mysqld ${common_args[@]} ${binlog_args[@]} ${rocksdb_args[@]} $@ \
  1> $MYTOPLING_LOG_DIR/stdlog/stdout \
  2> $MYTOPLING_LOG_DIR/stdlog/stderr &
sleep 1
echo mysqld started successfully and put into background
tail /mnt/mynfs/infolog/mytopling-instance-1/stdlog/stderr
