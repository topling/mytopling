let $datadir = `SELECT @@datadir`;

--disable_warnings
DROP TABLE IF EXISTS t1,t2,t3,t4,t5,t6;
--enable_warnings
create table t1 (id int primary key, value int, value2 varchar(200), index(value)) engine=rocksdb;
create table t2 (id int primary key, value int, value2 varchar(200), index(value)) engine=rocksdb;
create table t3 (id int primary key, value int, value2 varchar(200), index(value)) engine=rocksdb;
create table t4 (id int, value int, value2 varchar(200), primary key (id) comment 'rev:cf_i', index(value) comment 'rev:cf_i') engine=rocksdb;
create table t5 (id int, value int, value2 varchar(200), primary key (id) comment 'rev:cf_i', index(value) comment 'rev:cf_i') engine=rocksdb;
create table t6 (id int, value int, value2 varchar(200), primary key (id) comment 'rev:cf_i', index(value) comment 'rev:cf_i') engine=rocksdb;

--disable_query_log
let $t = 1;
while ($t <= 6) {
  let $i = 1;
  while ($i <= 2000) {
    let $insert = INSERT INTO t$t VALUES($i, $i, REPEAT('x', 150));
    inc $i;
    eval $insert;
  }
  inc $t;
}
--enable_query_log

# Disable auto compaction so that effects of optimize table are stable
let $restart_file= $MYSQLTEST_VARDIR/tmp/mysqld.1.expect;
--exec echo "wait" > $restart_file
--shutdown_server 10
--source include/wait_until_disconnected.inc
-- exec echo "restart:--rocksdb_default_cf_options=write_buffer_size=64k;target_file_size_base=64k;max_bytes_for_level_base=1m;compression_per_level=kNoCompression;disable_auto_compactions=true;level0_stop_writes_trigger=1000  " > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
-- enable_reconnect
-- source include/wait_until_connected_again.inc

select count(*) from t1;
select count(*) from t2;
select count(*) from t3;
select count(*) from t4;
select count(*) from t5;
select count(*) from t6;
delete from t1 where id <= 1900;
delete from t2 where id <= 1900;
delete from t3 where id <= 1900;
delete from t4 where id <= 1900;
delete from t5 where id <= 1900;
delete from t6 where id <= 1900;

--let $size_cmd = du -ks $datadir/.rocksdb/*.sst | awk '{t=t+\$1} END{print t}' >> $MYSQL_TMP_DIR/sst_size.dat
--exec $size_cmd
optimize table t1;
--sleep 5
--exec $size_cmd
optimize table t3;
--sleep 5
--exec $size_cmd
optimize table t4;
--sleep 5
--exec $size_cmd
optimize table t6;
--sleep 5
--exec $size_cmd

select count(*) from t1;
select count(*) from t2;
select count(*) from t3;
select count(*) from t4;
select count(*) from t5;
select count(*) from t6;

# run a check script to verify sst files reduced enough during each optimize table
--exec perl suite/rocksdb/optimize_table_check_sst.pl $MYSQL_TMP_DIR/sst_size.dat

#cleanup
optimize table t2;
optimize table t5;
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;
DROP TABLE t4;
DROP TABLE t5;
DROP TABLE t6;
--remove_file $MYSQL_TMP_DIR/sst_size.dat
