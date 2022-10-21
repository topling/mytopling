在编译 MyTopling 之前，我们首先需要安装一些依赖，并编译 ToplingDB。

## 1. 环境配置

**本文所使用的机器为阿里云的 ECS，系统为 Alibaba Cloud Linux 3.2104 LTS 64位。**

\-

安装 `liburing-devel`

```shellag-0-1gfss1ndfag-1-1gfss1ndf
yum -y install liburing-devel
```

如果提示找不到包，尝试下面命令后再次安装：

```shell
yum config-manager --set-enabled powertool
```

安装下列包：

```shell
yum -y install openssl-devel \
               ncurses-devel \
               libtirpc-devel \
               rpcgen \
               bison \
               libudev-devel
```

## 2. 编译 ToplingDB

```shell
git clone https://github.com/topling/toplingdb.git
cd toplingdb
git submodule update --init --recursive
make -j`nproc` DEBUG_LEVEL=0 shared_lib
sudo make install-shared \
     PREFIX=/path/to/install/dir \
     LIBDIR=/path/to/install/dir/lib64
```

上面 `PREFIX` 和 `LIBDIR` 为可选项：

- `PREFIX`：默认为 `/usr/local`，为 ToplingDB 的安装位置。
  
- `LIBDIR`：默认为 `${PREFIX}/lib`，即 `/usr/local/lib`。
  

## 3. 编译 MyTopling

```shell
 git clone https://github.com/topling/mytopling.git
 cd mytopling
 git submodule update --init --recursive
 export CXX_HOME=/usr
 bash build.sh -DTOPLING_LIB_DIR=/path/to/LIBDIR \
               -DCMAKE_INSTALL_PREFIX=/path/to/mytopling/install/dir
```

`DTOPLING_LIB_DIR` 和 `DCMAKE_INSTALL_PREFIX` 是必选项。

- `DTOPLING_LIB_DIR`：即为上面 ToplingDB 编译时的 `LIBDIR`。
  
- `DCMAKE_INSTALL_PREFIX`：指定 MyToplingDB 的安装位置。
  

上面的过程会在当前目录生成一个 `build-rls` 目录，我们执行下面的内容：

```shell
 cd build-rls
 export LD_LIBRARY_PATH=`pwd`/lib:`pwd`/lib/plugin:plugin_output_directory
 make -j`nproc`
 sudo make install
```

到此，编译完成。
