# 1. 写死的目录
## 1.1. 程序目录
程序目录（配置文件也在程序目录中）我们的部署脚本会自动创建。

   /usr/local/mytopling

## 1.2. 数据目录
数据目录最好在一块单独的 NVMe 盘上。

    /storage/wal
    /storage/mytopling
    /storage/mytopling/.rocksdb

## 1.3. 日志目录（给人读的日志）
日志目录我们的部署脚本会自动创建，如果长时间运行，就需要较大空间，可以自行创建在一块单独的盘上。

    /infolog
    /infolog/.rocksdb

## 2. 安装部署 & 启动数据库

如果需要，按照 1.2 中的说明将 /storage 目录设置在单独的 NVMe 盘上，不然部署脚本会自动创建 /storage 目录（创建在系统盘上）。

然后执行以下命令：

    mkdir tmpdir
    cd tmpdir
    unzip /path/to/mytopling.zip
    bash depoly.sh

需要使用 mysql 客户端自行修改 admin 用户的密码。

## 3. 说明
MyTopling 底层的存储引擎使用了 Topling 独有的压缩算法，压缩率高，读性能好，但压缩时需要较多的 CPU。

MyTopling 通过分布式 Compact，利用集群的算力调度，将压缩过程转移到富余的、对稳定性无要求的空闲结点上，
节省了对稳定性要求极高的数据库结点的宝贵算力。在云平台上，MyTopling 和分布式 Compact 可以自动部署、一键交付。

单机版不包含分布式 Compact，从而 Compact 运行在数据库结点本地，高压力写数据的时候数据库结点的 CPU 开销较大，所以主要用于功能演示。

