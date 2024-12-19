#!/bin/bash
PDIR=`cd ..; pwd`

type=${type:-rls}
CXX_BIN_DIR=`which ${CXX:-g++}`
CXX_BIN_DIR=`dirname ${CXX_BIN_DIR}`
CXX_HOME_DEFAULT=`realpath ${CXX_BIN_DIR}/..`
CXX_HOME=${CXX_HOME:-${CXX_HOME_DEFAULT}}

type_dbg=Debug
type_rls=Release
type_afr=RelWithDebInfo
eval 'CMAKE_BUILD_TYPE=$type_'${type}

# ROCKSDB_LIB_NAME_rls=librocksdb.so
# ROCKSDB_LIB_NAME_dbg=librocksdb_debug.so
# ROCKSDB_LIB_NAME_afr=librocksdb_debug_1.so
ROCKSDB_LIB_NAME_rls=rocksdb
ROCKSDB_LIB_NAME_dbg=rocksdb_debug
ROCKSDB_LIB_NAME_afr=rocksdb_debug_1
eval 'ROCKSDB_LIB_NAME=$ROCKSDB_LIB_NAME_'${type}

args=(
  -Wno-dev
  -DMYSQL_MAINTAINER_MODE=OFF
  -DWITH_BOOST=${PDIR}/boost_1_77_0
  -DCMAKE_C_COMPILER=${CXX_HOME}/bin/${CC:-gcc}
  -DCMAKE_CXX_COMPILER=${CXX_HOME}/bin/${CXX:-g++}
  -DCMAKE_CXX_FLAGS="-Wno-deprecated-declarations -Wno-attributes"
  -DCMAKE_CXX_FLAGS_RELEASE="-O3 -g3 -DNDEBUG"
  -DADD_GDB_INDEX=ON
  -DWITH_MYSQLD_LDFLAGS="-Wl,--no-as-needed"
  -DWITH_FB_VECTORDB=1
  -DWITH_ZLIB=system
  -DWITH_ZSTD=bundled
  -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
  -DCMAKE_INSTALL_PREFIX=/opt/myrocks-${type}
  -DMYSQL_UNIX_ADDR=/var/lib/mysql/mysql.sock
)
if [ "$1" = "toplingdb" ]; then
  shift
  core=${PDIR}/toplingdb/sideplugin/topling-core
  if [ ! -e $core ]; then
    core=${PDIR}/toplingdb/sideplugin/topling-zip
  fi
  if [ ! -e ${core} ]; then
    echo "Not Found ${core}, run commands:" >&2
    echo "  cd $PDIR" >&2
    echo "  git clone https://github.com/topling/toplingdb.git" >&2
    echo "  make -j`nproc`" >&2
    exit 1
  fi
  ROCKSDB_LIB_PATH=/opt/lib
  #LIBTERARK=`ldd ${ROCKSDB_LIB_PATH}/lib${ROCKSDB_LIB_NAME}.so | awk '/libterark/{printf("%s;",$3)}'`
  LIBTERARK=`ldd ${ROCKSDB_LIB_PATH}/lib${ROCKSDB_LIB_NAME}.so | perl -ane '$F[0]=~/.*lib(terark-.*)\.so.*/&&print(" -l$1")'`
  args+=(
    -DEXTERNAL_ROCKSDB_DEP_INCLUDE=${core}/src
    -DHAVE_EXTERNAL_ROCKSDB=ON
    -DROCKSDB_SRC_PATH=${PDIR}/toplingdb
    -DROCKSDB_LIB_PATH="-L${ROCKSDB_LIB_PATH}"
    -DROCKSDB_LIB_NAME=" -l${ROCKSDB_LIB_NAME} ${LIBTERARK}"
  )
  BUILD_DIR=${BUILD_DIR:-build-${type}-toplingdb}
else
  BUILD_DIR=${BUILD_DIR:-build-${type}}
fi

mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

env LC_ALL=C LANG=C cmake "${args[@]}" "$@" ..
