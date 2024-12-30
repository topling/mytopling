#!/bin/bash

# type is env var
if [ -z "${type}" ]; then
  type=rls
fi
mydir=`dirname $0`
mydir=`cd $mydir; pwd`
export LANG=C
export LC_ALL=C
if [ -z "$LD_LIBRARY_PATH" ]; then
  if [ -f /node-shared/mytopling-${type}/bin/mysqld ]; then
    export LD_LIBRARY_PATH=/node-shared/opt/gcc-12.1.0/lib64:/node-shared/lib
    export PATH=/node-shared/mytopling-${type}/bin:${PATH}
  else
    export LD_LIBRARY_PATH=/opt/lib
    export PATH=/opt/mytopling-${type}/bin:${PATH}
  fi
fi
if ! which mysqld; then
  build_dir=`realpath ${mydir}/../../build-${type}`
  if [ -f $build_dir/bin/mysqld ]; then
    export PATH="$build_dir/bin:$PATH"
    export LD_LIBRARY_PATH="$build_dir/library_output_directory:$build_dir/plugin_output_directory:$LD_LIBRARY_PATH"
    echo Not found mysqld, use build $build_dir
  else
    echo Not found mysqld, stop >&2
    exit 1
  fi
fi

export ROCKSDB_KICK_OUT_OPTIONS_FILE=1
export TOPLING_SIDEPLUGIN_CONF=${mydir}/mytopling.json
#export TOPLING_SIDEPLUGIN_CONF=${mydir}/mytopling-community.json
export SidePluginRepo_DebugLevel=0
export csppDebugLevel=0
export TOPLINGDB_CACHE_SST_FILE_ITER=1
#export TOPLINGDB_WARMUP_PROVIDER=mlock
#export MULTI_PROCESS=1
#export ZipServer_nltBuildThreads=5
#export ZIP_SERVER_OPTIONS=listening_ports=8090:num_threads=32
export BULK_LOAD_DEL_TMP=1
#export OutputValidator_full_check=1
#export TOPLING_IO_PROVIDER=posix

datadir=/nvme-shared/mytopling/datadir
#datadir=/nvme-shared/mytopling/lifuzhou-data/test_data
rm -rf ${datadir}/.rocksdb/job-*
rm -rf ${datadir}/.rocksdb/cspp-*.memtab-*
rm -rf /ram-shared/mytopling/copydir/.rocksdb/job-*
export ToplingZipTable_localTempDir=/tmp
rm -f ${ToplingZipTable_localTempDir}/Topling-*
ulimit -n 100000
#sudo sysctl -w vm.max_map_count=8388608
if [ `sysctl -n vm.max_map_count` -lt $((8<<20)) ]; then
  sysctl vm.max_map_count >&2
  echo please run: sudo sysctl -w vm.max_map_count=$((8<<20)) >&2
  exit 1
fi

common_args=(
  --no-defaults
  --gdb
 #--debug
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
 #--verbose
  --log-error-verbosity=3 # information
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
  --innodb_log_buffer_size=134217728
  --innodb_log_compressed_pages=OFF
  --innodb_flush_method=O_DIRECT
  --innodb_log_file_size=1572864000
  --innodb_page_cleaners=8
  --innodb_strict_mode=OFF
  #--innodb_lock_wait_timeout=500
  #--innodb_max_undo_log_size=17179869184
)

if [ $# -eq 0 ]; then
  rocksdb_args=(
   #--plugin-load=ha_rocksdb_se.so # static link does not need
    --rocksdb --default-storage-engine=rocksdb
   #--rocksdb_bulk_load
    --rocksdb_allow_concurrent_memtable_write=on
    --rocksdb_force_compute_memtable_stats=off
   #--rocksdb_write_disable_wal=ON  --rocksdb_flush_log_at_trx_commit=0
   #--rocksdb_write_disable_wal=OFF --rocksdb_flush_log_at_trx_commit=2
   #--rocksdb_info_log_level=debug_level
   #--rocksdb_info_log_level=info_level
    --rocksdb_info_log_level=warn_level
   #--rocksdb_bulk_load=ON
   #--rocksdb_enable_bulk_load_api=on
   #--rocksdb_master_skip_tx_api=on
    --rocksdb_max_row_locks=104857600
    --rocksdb_reuse_iter=on
    --rocksdb_bulk_load_subcompactions=3
    --rocksdb_bulk_sst_size=1073741824 # 1G
    --rocksdb_bulk_sst_parallel_num=5
    --rocksdb_parallel_read_threads=32
    --rocksdb_check_iterate_bounds=off
   #--rocksdb_write_policy=write_unprepared
    --rocksdb_write_policy=write_committed
    --rocksdb_write_reduce_cpu=ON # use futex in WriteThread
   #--rocksdb_deadlock_detect=ON
    --rocksdb_mrr_batch_size=32 --rocksdb_async_queue_depth=32
    --rocksdb_lock_wait_timeout=10
    --rocksdb_print_snapshot_conflict_queries=1
    --rocksdb_compaction_sequential_deletes=1
   #--rocksdb_compaction_sequential_deletes_window=150000 # default=150000
    --rocksdb_skip_bloom_filter_on_read=ON
  )
elif [ "${1:0:12}" = "--initialize" ]; then
  rm -rf ${datadir}/*
  rm -rf ${datadir}/.rocksdb
  rm -rf /dev/shm/mytopling/wal
  mkdir -p /dev/shm/mytopling/wal
fi
if [ $type = dbg ]; then
  dbg="gdb --args"
fi
#dbg="strace -f -e signal,sigaction,signalfd"
#dbg="gdb --args"
#dbg="valgrind"
binlog_args=(
 #--disable-log-bin --gtid_mode=OFF --enforce_gtid_consistency=OFF
 #--sync_binlog=0 --binlog-order-commits=OFF
  --binlog-ddl-only=ON --binlog-order-commits=ON
)
#dbg='numactl -N 0 --preferred 0'
${dbg} mysqld ${common_args[@]} ${binlog_args[@]} ${innodb_args[@]} ${rocksdb_args[@]} $@
