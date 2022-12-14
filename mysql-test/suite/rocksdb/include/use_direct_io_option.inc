# Common test pattern for options that control direct i/o
#
# Required input:
#   $io_option - name and assignment to enable on server command line

--source include/have_direct_io.inc

--echo Checking direct reads
--let restart_parameters="restart: $io_option"
--source include/restart_mysqld.inc

CREATE TABLE t1 (pk INT PRIMARY KEY DEFAULT '0', a INT(11), b CHAR(8)) ENGINE=rocksdb CHARSET=latin1;
SHOW CREATE TABLE t1;
INSERT INTO t1 VALUES (1, 1,'a');
INSERT INTO t1 (a,b) VALUES (2,'b');
set global rocksdb_force_flush_memtable_now=1;
--sorted_result
SELECT a,b FROM t1;
DROP TABLE t1;

# cleanup
--let restart_parameters=
--source include/restart_mysqld.inc
