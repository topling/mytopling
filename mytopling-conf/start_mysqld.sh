#!/bin/bash:

source ./conf_mysqld.sh

if [ $# -eq 0 ]; then
  rocksdb_args=(--plugin-load=ha_rocksdb_se.so
    --rocksdb --default-storage-engine=rocksdb
    --rocksdb_datadir=${datadir}/.rocksdb
   #--rocksdb_bulk_load
    --rocksdb_allow_concurrent_memtable_write=on
    --rocksdb_force_compute_memtable_stats=off
   #--rocksdb_write_disable_wal=ON  --rocksdb_flush_log_at_trx_commit=0
    --rocksdb_write_disable_wal=OFF --rocksdb_flush_log_at_trx_commit=2
   #--rocksdb_info_log_level=debug_level
   #--rocksdb_bulk_load=ON
   #--rocksdb_enable_bulk_load_api=on
   #--rocksdb_master_skip_tx_api=on
    --rocksdb_reuse_iter=on
    --rocksdb_write_policy=write_unprepared
  )
fi

${dbg} mysqld ${common_args[@]} ${binlog_args[@]} ${innodb_args[@]} ${rocksdb_args[@]} $@
