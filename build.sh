#!/bin/bash
PDIR=`cd ..; pwd`

BUILD_DIR=${BUILD_DIR:-build-rls}
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}
#CXX_HOME=${CXX_HOME:-/opt/rh/gcc-toolset-9/root}
#CXX_HOME=${CXX_HOME:-/opt/rh/gcc-toolset-10/root}
#CXX_HOME=${CXX_HOME:-/opt/rh/gcc-toolset-11/root}
CXX_BIN_DIR=`which ${CXX:-g++}`
CXX_BIN_DIR=`dirname ${CXX_BIN_DIR}`
CXX_HOME_DEFAULT=`realpath ${CXX_BIN_DIR}/..`
CXX_HOME=${CXX_HOME:-${CXX_HOME_DEFAULT}}

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

# both TOPLING_LIB_DIR=/node-shared/lib and -L/node-shared/lib need
# be specified
cmake -DHAVE_EXTERNAL_ROCKSDB=1 -DROCKSDB_SRC_PATH=${PDIR}/toplingdb \
      -DTOPLING_CORE_HOME=${core} \
      -DTOPLING_LIB_DIR=/opt/toplingdb/lib \
      -DWITH_BOOST=${core}/boost-include \
      -DCMAKE_C_COMPILER=${CXX_HOME}/bin/${CC:-gcc} \
      -DCMAKE_CXX_COMPILER=${CXX_HOME}/bin/${CXX:-g++} \
      -DCMAKE_CXX_FLAGS="-Wno-deprecated-declarations -Wno-attributes" \
      -DCMAKE_CXX_FLAGS_RELEASE="-O3 -g3 -DNDEBUG" \
      -DADD_GDB_INDEX=ON \
      -DCOMPILATION_COMMENT="MyTopling Enterprise" \
      -DROCKSDB_DISABLE_MARCH_NATIVE=on \
      -DFORCE_CPU_ARCH=haswell \
      -DWITH_ZLIB=system \
      -DWITH_ZSTD=bundled \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/opt/mytopling-rls \
      -DMYSQL_UNIX_ADDR=/var/lib/mysql/mysql.sock \
      "$@" ..
