if ($rpl_inited)
{
  if (!$allow_rpl_inited)
  {
    --die ERROR IN TEST: When using the replication test framework (master-slave.inc, rpl_init.inc etc), use rpl_restart_server.inc instead of restart_mysqld.inc. If you know what you are doing and you really have to use restart_mysqld.inc, set allow_rpl_inited=1 before you source restart_mysqld.inc
  }
}

if (!$restart_parameters)
{
  let $restart_parameters = restart;
}

# We use --exec echo instead of simple "--echo" because
# replace_result works on --exec
--exec echo "# $restart_parameters"

# Write file to make mysql-test-run.pl expect the "crash", but don't start
# it until it's told to
--let $_server_id= `SELECT @@server_id`
--let $_expect_file_name= $MYSQLTEST_VARDIR/tmp/mysqld.$_server_id.expect

# Allow restarting different server than the one currently
# connected to, actually one where suffix is not .serverid but
# .serverid.clusterid
if ($mysqld_name)
{
  let $_expect_file_name= $MYSQLTEST_VARDIR/tmp/$mysqld_name.expect;
  # echo expect_file_name: $_expect_file_name;
}
--exec echo "wait" > $_expect_file_name

# Send shutdown to the connected server.

--let $ss_timeout= 60

if ($shutdown_server_timeout)
{
  --let $ss_timeout= $shutdown_server_timeout
}

--let $timeout_multiplier= 6

if ($VALGRIND_TEST)
{
  --expr $ss_timeout= $ss_timeout * $timeout_multiplier
}

let $have_sparc = `select convert(@@version_compile_machine using latin1) IN ('sparc')`;
if ($have_sparc)
{
  --expr $ss_timeout= $ss_timeout * $timeout_multiplier
}
--shutdown_server $ss_timeout

--source include/wait_until_disconnected.inc

# Write file to make mysql-test-run.pl start up the server again
--exec echo "$restart_parameters" > $_expect_file_name

# FORCE_AND_SHUTDOWN mode upgrades the server and shutdown the server.
# Inorder to run tests, we need to start server again without upgrade flag
if ($upgrade_mode == FORCE_AND_SHUTDOWN)
{
  --source include/wait_until_disconnected.inc
  --echo # Server shouldn't have restarted, so query should fail.
  --error CR_SERVER_LOST
  show session status;
  let $restart_parameters = restart;
  --exec echo "# $restart_parameters"
  --exec echo "$restart_parameters" > $_expect_file_name
}

# Turn on reconnect
--enable_reconnect

# Call script that will poll the server waiting for it to be back online again
--source include/wait_until_connected_again.inc

## Script xplugin_wait_for_interfaces.inc
#
#  $do_not_wait_for_x_interface -
#    set for which interfaces the script shouldn't wait
#    In default the script wait for both socket and tcp
#    on *NIX OS-es and only for TCP on windows
#
## Usage
#
# --let $wait_for_tcpsocket_status = valid_value|undefined_value|no_wait
# --let $wait_for_unixsocket_status = valid_value|undefined_value|no_wait
# --source include/xplugin_wait_for_interfaces.inc
#

--let $_xplugin_active= `SELECT plugin_status='ACTIVE' FROM information_schema.plugins WHERE plugin_name LIKE 'mysqlx'`
if ($_xplugin_active)
{
  --source include/xplugin_wait_for_interfaces.inc
}
# Turn off reconnect again
--disable_reconnect
