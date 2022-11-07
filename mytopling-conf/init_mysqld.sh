#!/bin/bash:

source ./conf_mysqld.sh

${dbg} mysqld ${common_args[@]} ${binlog_args[@]} ${innodb_args[@]} ${rocksdb_args[@]} $@ --initialize-insecure
