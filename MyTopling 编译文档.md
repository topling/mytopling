在编译 MyTopling 之前，我们首先需要安装一些依赖，并编译 ToplingDB。

## 1. 环境配置

**本文所使用的机器为阿里云的 ECS，系统为 Alibaba Cloud Linux 3.2104 LTS 64位。**
\-
安装 `liburing-devel`

安装下列包：

```shell
yum -y install liburing-devel \
               openssl-devel \
               ncurses-devel \
               libtirpc-devel \
               rpcgen \
               bison \
               libudev-devel \
               git \
               gcc-c++ \
               gflags-devel \
               libaio-devel \
               cmake
```

> 如果提示找不到 `liburing-devel` 包，尝试下面命令后再次安装：
>
> `yum config-manager --set-enabled powertool`

## 2. 编译 ToplingDB

```shell
git clone --depth=1 https://github.com/topling/toplingdb.git
cd toplingdb
git submodule update --init --recursive
make -j`nproc` DEBUG_LEVEL=0 shared_lib
make install-shared
```

上面 `make install-shared` 有可选变量  `PREFIX` 和 `LIBDIR`项：

- `PREFIX`：默认为 `/usr/local`，为 ToplingDB 的安装位置。
  
- `LIBDIR`：默认为 `${PREFIX}/lib`，即 `/usr/local/lib`。

## 3. 编译 MyTopling

我们需要保证，clone 下来的 MyTopling 目录，与刚刚 clone 的 ToplingDB 目录同级。

```shell
git clone --depth=1 https://github.com/topling/mytopling.git
cd mytopling
git submodule update --init --recursive
export CXX_HOME=/usr
bash build.sh -DTOPLING_LIB_DIR=/path/to/LIBDIR \
              -DCMAKE_INSTALL_PREFIX=/path/to/mytopling/install/dir
```

`DTOPLING_LIB_DIR` 和 `DCMAKE_INSTALL_PREFIX` 是必选项：

- `DTOPLING_LIB_DIR`：即为上面 ToplingDB 编译时的 `LIBDIR`。
  
- `DCMAKE_INSTALL_PREFIX`：指定 MyToplingDB 的安装位置。

如果编译 ToplingDB 的时候没有指定 `PREFIX` 和 `LIBDIR`，那么这里 `DTOPLING_LIB_DIR` 应当为 `/usr/local`。`DCMAKE_INSTALL_PREFIX` 可以和 ToplingDB 一起放在 `/usr/local` 目录下，即这里最后一个语句可以为：

```shell
bash build.sh -DTOPLING_LIB_DIR=/usr/local/lib \
              -DCMAKE_INSTALL_PREFIX=/usr/local
```

上面的过程会在当前目录生成一个 `build-rls` 目录，我们执行下面的内容：

```shell
cd build-rls
export LD_LIBRARY_PATH=`pwd`/lib:`pwd`/lib/plugin:plugin_output_directory
make -j`nproc`
make install
```

到此，编译完成。



## 4. 运行 MyTopling

### 

### 4.1. 下载脚本与配置文件



下载链接：https://github.com/topling/mytopling/conf

```
conf_mysqld.sh
init_mysqld.sh
start_mysqld.sh
mytopling.json
```

建议将这四个文件放在同一目录下（不然需要自行修改路径相关的配置），后面逐个说明需要编辑修改的内容。

我这里在 `/root/topling` 下。



### 4.2. 初始化



首先确定一个启动 MyTopling 的位置，我这里是 `/root/topling`：

```
cd /root/topling
```

然后确定一个数据存放位置，我这里是 `/root/mytopling/data`：

创建数据目录：

```shell
mkdir -p /root/mytopling/data
```

执行初始化脚本：

```shell
bash init_mysqld.sh
```



### 4.3. 配置并运行



将 `mytopling.json` 中的 `http`-> `document_root` 字段修改为上面的数据目录字段。为了安全起见，可以为 log (mysql log, toplingdb log) 设置单独的目录，然后将 document_root 设置到 log 目录，如果不想通过 web 访问 log 文件，就不要设置 document_root。

然后执行启动脚本：

```shell
bash start_mysqld.sh
```



最后输出如下表示成功：

```
2022-11-04T10:37:59.557896Z 0 [System] [MY-010931] [Server] /home/terark/topling/mytopling-install/bin/mysqld: ready for connections. Version: '8.0.28'  socket: '/tmp/mysql.sock'  port: 3306  MyTopling Enterprise.
```













