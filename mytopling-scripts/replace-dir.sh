#!/bin/bash
mydir=`dirname $0`
mydir=`realpath $mydir`

if [ $# -lt 1 ]; then
  echo usage: $0 TargetDir >&2
  exit 1
fi
target=`realpath $1`
sed -i "s:/mnt/mynfs/:$1/:g"  $mydir/mytopling*.json $mydir/start*.sh
sed -i "s:$target/opt/:/mnt/mynfs/opt/:g"  $mydir/*.json $mydir/*.sh