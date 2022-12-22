#!/bin/bash
STORAGE=/storage
MYTOPLING_INSTALL_DIR=$STORAGE/mytopling
MYTOPLING_DATA_DIR=$STORAGE/mytopling-instance-1
TOPLINGDB_INSTALL_DIR=$STORAGE/toplingdb

export LD_LIBRARY_PATH=$TOPLINGDB_INSTALL_DIR/lib64/:$MYTOPLING_INSTALL_DIR/lib/private
export ROCKSDB_KICK_OUT_OPTIONS_FILE=1
export TOPLING_SIDEPLUGIN_CONF=${STORAGE}/mytopling-scripts/mytopling.json

#export JsonOptionsRepo_DebugLevel=2
#export csppDebugLevel=0
export TOPLINGDB_CACHE_SST_FILE_ITER=1
export BULK_LOAD_DEL_TMP=1



rm -rf ${MYTOPLING_DATA_DIR}/.rocksdb/job-*
ulimit -n 100000

common_args=(
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
innodb_args1=(
  --innodb_flush_log_at_trx_commit=0
  --innodb_buffer_pool_chunk_size=$((dram/2/part))
  --innodb_buffer_pool_instances=${part}
  --innodb_buffer_pool_size=$((dram/2))
  --innodb_adaptive_hash_index=OFF
  --innodb_disable_sort_file_cache=ON
  --innodb_doublewrite_pages=64
  --innodb_purge_threads=1
  --innodb_io_capacity=1000000
  --innodb_io_capacity_max=1000000
  --innodb_log_buffer_size=8388608
  --innodb_log_compressed_pages=OFF
  --innodb_flush_method=O_DIRECT
  --innodb_log_file_size=1572864000
  --innodb_page_cleaners=8
  --innodb_strict_mode=OFF
)

if [ $# -eq 0 ]; then
  rocksdb_args=(--plugin-load=ha_rocksdb_se.so
    --rocksdb --default-storage-engine=rocksdb
    --rocksdb_datadir=${MYTOPLING_DATA_DIR}/.rocksdb
    --rocksdb_allow_concurrent_memtable_write=on
    --rocksdb_force_compute_memtable_stats=off
    --rocksdb_reuse_iter=on
    --rocksdb_write_policy=write_committed
    --rocksdb_mrr_batch_size=32 --rocksdb_async_queue_depth=32
    --rocksdb_lock_wait_timeout=10
    --rocksdb_print_snapshot_conflict_queries=1
  )
fi

binlog_args=(
    #使用binlog-ddl-only时MyTopling不能做上游数据库
  --binlog-ddl-only=ON --binlog-order-commits=ON
)
rm -rf ${MYTOPLING_DATA_DIR}/.rocksdb/job*
$MYTOPLING_INSTALL_DIR/bin/mysqld ${common_args[@]} ${binlog_args[@]} ${innodb_args[@]} ${rocksdb_args[@]} $@ > /infolog/stdlog/stdlog 2> /infolog/stdlog/stderr


