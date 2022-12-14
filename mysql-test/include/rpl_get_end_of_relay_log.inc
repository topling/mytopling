# ==== Purpose ====
#
# Get the file name and file size of the last relay log.
#
# ==== Usage ====
#
# [--let $rpl_channel_name= 'NAME']
# --source include/rpl_get_end_of_relay_log.inc
# --echo Last relay log: $relay_log_file
# --echo Size of last relay log: $relay_log_size

--let $include_filename= include/rpl_get_end_of_relay_log.inc
if ($rpl_channel_name)
{
  --let $include_filename= $include_filename [FOR CHANNEL $rpl_channel_name]
}
--source include/begin_include_file.inc

--let $gen_binlog_index_file= `SELECT @@GLOBAL.RELAY_LOG_INDEX`
--let $gen_binlog_output_file= $MYSQLTEST_VARDIR/tmp/relay_index.txt
--source include/rpl_gen_binlog_index_file.inc

--let _RGEORL_CHANNEL_NAME= $rpl_channel_name
--let _RGEORL_OUTPUT_FILE= `SELECT CONCAT('$MYSQLTEST_VARDIR/tmp/_end_of_relay_log-', UUID(), '.txt')`
--let _RGEORL_INDEX_FILE= $gen_binlog_output_file
--let _RGEORL_DATADIR= `SELECT @@GLOBAL.DATADIR`
if ($rpl_debug)
{
  --echo _RGEORL_OUTPUT_FILE='$_RGEORL_OUTPUT_FILE'
  --echo _RGEORL_INDEX_FILE='$_RGEORL_INDEX_FILE'
  --echo _RGEORL_DATADIR='$_RGEORL_DATADIR'
}

perl;
  # Compute relay log filename.
  my $index_file= $ENV{'_RGEORL_INDEX_FILE'};
  my $channel_name= $ENV{'_RGEORL_CHANNEL_NAME'};
  # Remove the single quotes from $channel_name
  $channel_name=~ s/\'//g;
  if ($channel_name) {
    $index_file =~ s/(.*)(\.index)/$1-$channel_name$2/;
  }

  # Read relay log filename.
  open FILE, "$index_file" or die "Error $? opening $index_file: $!";
  my $relay_log_file= '';
  while (<FILE>) {
    # Index file may contain gtid sets that need to be skipped
    @arr= split(/ /, $_);
    $relay_log_file= $arr[0];
    if ($#arr > 0) {
      seek(FILE, $arr[1] + 1, 1);
    }
  }
  chomp($relay_log_file);
  $relay_log_file =~ s{^\.[/\\]}{};
  close FILE or die "Error $? closing $index_file: $!";

  # Get relay log size.
  my $datadir= $ENV{'_RGEORL_DATADIR'};
  my $relay_log_size= (stat("$datadir/$relay_log_file"))[7];

  # Write output file.
  my $output_file= $ENV{'_RGEORL_OUTPUT_FILE'};
  open FILE, "> $output_file" or die "Error $? opening $output_file: $!";
  printf FILE "% 10d%s", $relay_log_size, $relay_log_file or die "Error $? writing to $output_file: $!";
  close FILE or die "Error $? writing to $output_file: $!";
EOF

# Read output file into mtr variables
--let $size_and_file= `SELECT LOAD_FILE('$_RGEORL_OUTPUT_FILE')`
--remove_file $_RGEORL_OUTPUT_FILE
--let $relay_log_size= `SELECT TRIM(SUBSTR('$size_and_file', 1, 10))`
--let $relay_log_file= `SELECT TRIM(SUBSTR('$size_and_file', 11))`

# Take into account the encryption header if necessary
--let $_binlog_encryption=`SELECT @@GLOBAL.binlog_encryption`
if ($_binlog_encryption)
{
  --let $relay_log_size=`SELECT $relay_log_size - 512`
}

if ($rpl_debug)
{
  --echo relay_log_file=$relay_log_file relay_log_size=$relay_log_size
}

--remove_file $gen_binlog_output_file

--let $include_filename= include/rpl_get_end_of_relay_log.inc
--source include/end_include_file.inc
