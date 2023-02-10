#!/bin/bash

# type is env var
if [ -z "${type}" ]; then
  type=rls
fi
mydir=`dirname $0`
mydir=`cd $mydir; pwd`
export LD_LIBRARY_PATH=/node-shared/opt/gcc-12.1.0/lib64:/node-shared/lib
export PATH=/node-shared/mytopling-${type}/bin:${PATH}
export ROCKSDB_KICK_OUT_OPTIONS_FILE=1
export TOPLING_SIDEPLUGIN_CONF=${mydir}/mytopling.json
#export TOPLING_SIDEPLUGIN_CONF=${mydir}/mytopling-community.json
export JsonOptionsRepo_DebugLevel=2
export csppDebugLevel=0
export TOPLINGDB_CACHE_SST_FILE_ITER=1
#export TOPLINGDB_WARMUP_PROVIDER=mlock
#export MULTI_PROCESS=1
#export ZipServer_nltBuildThreads=5
#export ZIP_SERVER_OPTIONS=listening_ports=8090:num_threads=32
export BULK_LOAD_DEL_TMP=1

datadir=/nvme-shared/mytopling/datadir
#datadir=/nvme-shared/mytopling/lifuzhou-data/test_data
rm -rf ${datadir}/.rocksdb/job-*
ulimit -n 100000

common_args=(
  --gdb
 #--skip-stack-trace
  --datadir=${datadir}
  --bind-address=0.0.0.0
  --disabled_storage_engines=myisam
 #--have_openssl=DISABLED
 #--have_ssl=DISABLED
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
 #--thread_handling=pool-of-threads
 #--admin-host=127.0.0.1
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
 #--innodb_dedicated_server
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
  #--innodb_lock_wait_timeout=500
  #--innodb_max_undo_log_size=17179869184
)

if [ $# -eq 0 ]; then
  rocksdb_args=(--plugin-load=ha_rocksdb_se.so
    --rocksdb --default-storage-engine=rocksdb
    --rocksdb_datadir=${datadir}/.rocksdb
    #--rocksdb_bulk_load
    --rocksdb_allow_concurrent_memtable_write=on
    --rocksdb_force_compute_memtable_stats=off
   #--rocksdb_write_disable_wal=ON  --rocksdb_flush_log_at_trx_commit=0
   #--rocksdb_write_disable_wal=OFF --rocksdb_flush_log_at_trx_commit=2
   #--rocksdb_info_log_level=debug_level
   #--rocksdb_bulk_load=ON
   #--rocksdb_enable_bulk_load_api=on
   #--rocksdb_master_skip_tx_api=on
    --rocksdb_reuse_iter=on
   #--rocksdb_write_policy=write_unprepared
    --rocksdb_write_policy=write_committed
   #--rocksdb_deadlock_detect=ON
    --rocksdb_mrr_batch_size=32 --rocksdb_async_queue_depth=32
    --rocksdb_lock_wait_timeout=10
    --rocksdb_print_snapshot_conflict_queries=1
  )
fi
if [ $type = dbg ]; then
  dbg="gdb --args"
fi
#dbg="gdb --args"
binlog_args=(
 #--disable-log-bin --gtid_mode=OFF --enforce_gtid_consistency=OFF
 #--sync_binlog=0 --binlog-order-commits=OFF
  --binlog-ddl-only=ON --binlog-order-commits=ON
)
#dbg='numactl -N 0 --preferred 0'
${dbg} mysqld ${common_args[@]} ${binlog_args[@]} ${innodb_args[@]} ${rocksdb_args[@]} $@
