### [English version](./README.mytopling.md)

# MyTopling
MyTopling 是拓扑岭([topling.cn](https://topling.cn))开发的 MySQL 兼容数据库。

MyTopling fork 自 facebook 的 [myrocks-8.0.28](https://github.com/facebook/mysql-5.6/tree/fb-mysql-8.0.28)，进行了大幅修改：
1. 将存储引擎从 RocksDB 替换为 ToplingDB
1. 修改了 MyRocks 的 bug
   * https://github.com/facebook/mysql-5.6/issues/1211
   * https://github.com/facebook/mysql-5.6/pull/1213
   * https://github.com/facebook/mysql-5.6/pull/1218
1. 对 myrocks 的接入层进行了深度优化
1. 对 ToplingDB Transaction 进行了深度优化
   * Transaction 代码虽然在 ToplingDB/RocksDB 中，但其主要是为 MyTopling/MyRocks 服务的
1. 支持 ToplingDB 的 [分布式 Compact](https://github.com/topling/rockside/wiki/Distributed-Compaction)
1. 支持 ToplingDB 的 [CSPP\_WBWI](https://github.com/topling/cspp-wbwi)(CSPP WriteBatchWithIndex)
1. 支持 ToplingDB 的 [CSPP MemTable](https://github.com/topling/rockside/wiki/ToplingCSPPMemTab)
1. 支持 ToplingDB 的 [ToplingFastTable](https://github.com/topling/rockside/wiki/ToplingFastTable)
1. 支持 ToplingDB 的 [ToplingZipTable](https://github.com/topling/rockside/wiki/ToplingZipTable)
1. 支持 ToplingDB 的 [引擎级监控（grafana）](https://github.com/topling/rockside/wiki/grafana%E5%B1%95%E7%A4%BAtoplingdb%E8%BF%90%E8%A1%8C%E6%8C%87%E6%A0%87-%E6%89%8B%E5%8A%A8%E9%85%8D%E7%BD%AE)
1. 支持 ToplingDB 的 [引擎观测](https://github.com/topling/rockside/wiki/WebView)

# (一). MyTopling 的 创建索引 及 bulk\_load 加速
在 MyRocks 中，bulk\_load 和 创建索引走的是同一套逻辑，其处理方式大致为：
1. 如果输入数据有序，直接使用 RocksDB 的 SstWriter 创建 SST 文件，然后将创建出来的 SST 文件注入到 LSM Tree 中
2. 如果输入数据无序，先对数据排序（内排+外排），然后再执行 1 中的步骤

在 sysbench 性能测试中，我们发现 prepare 阶段创建索引的速度很慢，进一步定位到是在内存中的排序很慢，其排序方式非常原始粗暴，使用了 `std::set`，并且在应该给 `std::set` 指定自定义 KeyComparator 的地方，把 RocksDB 中的 Comparator 指针放到每个 set 元素中。所以，即便使用最普通的优化方式，也能对性能有所提升，并降低内存用量。但我们最终进行了非常彻底的优化：
1. 在 ToplingDB SstWriter 中，允许乱序输入
2. 实现 AutoSortTable，以支持乱序输入
3. 修改 MyRocks 相关代码，支持乱序输入

这中间的核心是 VecAutoSortTable，我们知道，RocksDB 的 SST 是 **S**tatic **S**orted **T**able（也有称 Sorted String Table），创建 SST 时，输入数据必须是按 Key 有序的。支持乱序输入，其本质就是把排序从 SST 外部移到内部，貌似没啥可圈可点的。

然而，这里核心中的核心是：VecAutoSortTable，以最快的速度将数据收集起来，然后在 `TableBuilder::Finish` 中进行排序，排序时使用了去虚拟化技术，大幅优化了性能。
> 举例来说，对于 sysbench 中的 create index，其 secondary key 的物理存储是 tuple(index_id, k, id) 三个 32 位整数，对于单个 index，所有 tuple 的 index_id 都是相同的，所以我们把后面的 (k, id) 拼起来放到一个数组中，经过转化，当作 64 位整数的数组来排序，既减小了内存占用，又因为避免了通过 String（Key 的通用类型）中的指针间接访问内存的开销，大幅提升了排序的速度。

> 更早时，使用 AutoSortTable 使用了 CSPP Trie，将 Key 逐个插入 CSPP Trie，比在内存中排序还要快，并且 CSPP Trie 只使用一整块内存，这意味着创建好的 CSPP Trie 数据结构可以通过一次 write 操作写到 SST 文件中。
> 但是基于 CSPP Trie 的 AutoSortTable 中 KeyValue 的存储顺序是乱的，从而在 Compact 多个 AutoSortTable 时，这些 AutoSortTable SST 都需要全部加载到内存中，从而对于很大的 Index 会引发 OOM。

最终效果是，优化后，索引创建的整体速度提升了 5 倍
> 其中我们优化的关键环节性能提升了 30 倍以上，整体性能被其余环节拖了后腿！

# (二). ToplingDB 的 Transaction 优化
1. 在 MyRocks 的 Transaction 中，原本其性能热点是 SkipList （SkipList MemTable 和 SkipList WBWI），我们将这两个都用 CSPP Trie 替换之后，这个性能热点就消失了
1. SkipList 性能热点消除后，原本不是性能热点的 Lock 管理，成了新的性能热点，这一块，主要通过 topling 的 hash_strmap 和 gold_hash_map 解决，并且，把调用链上的 std::string，改成了无需内存拷贝的 Slice

# (三). MyTopling 的 分布式 Compact
分布式 Compact 的核心是把 CompactionFilter 和 TablePropertiesCollector 需要的数据字典转移到 Compact Worker 结点上，这个，我们只需要仔细研读相关代码，将数据字典进行序列化/反序列化即可，并且，ToplingDB 提供了完善 SidePlugin 机制，使得实现这一点变得相对容易。

# (四). Compaction Worker 多进程化
MyRocks 中 CompactionFilter 和 TablePropertiesCollector 使用了一些全局变量，而我们的 Compact Worker 是多线程为多个 DB 实例服务的，全局变量在多线程中是共享的，这样就会导致来自多个 DB 的数据发生混淆。所以，我们将 Compact Worker 改成多线程的，将执行 Compact Job 的代码放到 fork 出来的新进程中，关于这一点，我们[在知乎上有个回答](https://www.zhihu.com/question/25390536/answer/2510470886)

# (五). 对上游 RocksDB 的 Pull Request
我们的诸多修改，有一些代码使用了 topling-core，对 RocksDB 的改动也较大，但还有一些修改没有外部依赖，我们将没有外部依赖的代码，都向上游官方 RocksDB 发起了 [Pull Request](https://github.com/facebook/rocksdb/pulls/rockeet)

# (六). 性能测试，参见: [MyTopling sysbench 测试报告](https://zhuanlan.zhihu.com/p/573217285)

# (七). MyTopling 更多地为随机写优化
这个 sysbench 命令其实更适合 InnoDB 和原版 MyRocks，因为 prepare 是按照自增 id 顺序写数据！
1. 对于 InnoDB，顺序写对 BTree 是非常友好的操作，对 1000 个表中的单独每个表顺序写，表面上看并不是真正的顺序写，但实际上，到 BTree 的下层时，相当于是对 1000 个 range 的顺序写
2. 如果 table 数量为 1，对于原版 MyRocks，顺序写数据，SST flush 之后，只需要 trivial move，直到压缩选项发生变化的 level

在 MyTopling 中，因为有分布式 Compact 加持，我们并没有针对顺序写去做优化，我们不允许把 SST 从 L0 trivial move 到 L1，也不允许从 L1 trivial move 到更下层，而是：
1. 使用较大的 MemTable，生成较大的 L0 file
2. 设置较小的 L1 SST file size
   1. 在 L0->L1 compact 时，少数 L0 SST 文件会 Compact 成很多个 L1 SST
   2. 在 L1->L2 compact 时，因为 L1 SST 文件数量众多，可以启动多个 compact 线程，利用分布式 compact，充分并行化

我们的原则是“充分并行化”，从而“为顺序写入而优化”就没有那么重要了，因为在实际业务中，绝大部分写都是随机写入！
