#!/bin/bash

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/mynfs/opt/lib
export ROCKSDB_KICK_OUT_OPTIONS_FILE=1
export TOPLING_SIDEPLUGIN_CONF=/mnt/mynfs/opt/mytopling-scripts/cluster/mytopling-instance-2.json

#export JsonOptionsRepo_DebugLevel=2
#export csppDebugLevel=0
export TOPLINGDB_CACHE_SST_FILE_ITER=1
export BULK_LOAD_DEL_TMP=1

export DictZipBlobStore_zipThreads=$((`nproc`/2))

MYTOPLING_DATA_DIR=/mnt/mynfs/datadir/mytopling-instance-2
MYTOPLING_LOG_DIR=/mnt/mynfs/infolog/mytopling-instance-2
MYTOPLING_ROCKSDB_DIR=/mnt/mynfs/datadir/mytopling-instance-1/.rocksdb
rm -rf ${MYTOPLING_DATA_DIR}/.rocksdb/job-*
ulimit -n 100000
sudo sysctl -w vm.max_map_count=8388608

common_args=(
  --server-id=2
  --enforce_gtid_consistency=ON
  --gtid_mode=ON
  --disable-log-bin
  --read_only
  --user=mysql
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
dram=`awk '$1 == "MemTotal:"{print $2*1024}' /proc/meminfo`
part=`nproc`
part=$((part<64?part:64)) # innodb_buffer_pool_instances max is 64

if [ $# -eq 0 ]; then
  rocksdb_args=(
    --plugin-load=ha_rocksdb_se.so
    --rocksdb --default-storage-engine=rocksdb
    --rocksdb_datadir=${MYTOPLING_ROCKSDB_DIR}
    --rocksdb_allow_concurrent_memtable_write=on
    --rocksdb_force_compute_memtable_stats=off
    --rocksdb_reuse_iter=on # 此选项打开时，长期空闲的数据库连接会导致内存泄露，请谨慎使用
    --rocksdb_write_policy=write_committed
    --rocksdb_mrr_batch_size=32 --rocksdb_async_queue_depth=32
    --rocksdb_lock_wait_timeout=10
    --rocksdb_print_snapshot_conflict_queries=1
  )
fi

binlog_args=(
  # 使用 binlog-ddl-only 时 MyTopling 可配置为基于共享存储的多副本集群，
  # 但此配置下 MyTopling 不能做为传统 MySQL 主从中的上游数据库，因为此
  # 配置下 binlog 只会记录 DDL 操作
  --binlog-ddl-only=ON --binlog-order-commits=ON
)
# 修复引擎监控日志链接
sudo ln -sf $MYTOPLING_LOG_DIR $MYTOPLING_LOG_DIR/.rocksdb
sudo ln -sf $MYTOPLING_LOG_DIR/mnt_mynfs_datadir_mytopling-instance-2_.rocksdb_LOG \
           $MYTOPLING_LOG_DIR/LOG
rm -rf ${MYTOPLING_DATA_DIR}/.rocksdb/job*
/mnt/mynfs/opt/bin/mysqld ${common_args[@]} ${binlog_args[@]} ${rocksdb_args[@]} $@ \
  1> $MYTOPLING_LOG_DIR/stdlog/stdout \
  2> $MYTOPLING_LOG_DIR/stdlog/stderr



