# MyTopling
MyTopling is a MySQL compatible database developed by Topling([topling.cn](https://topling.cn)).

MyTopling is a fork from facebook's [myrocks-8.0.28](https://github.com/facebook/mysql-5.6/tree/fb-mysql-8.0.28) with major modifications:
1. Replace storage engine from RocksDB to ToplingDB
1. Fixed MyRocks' bugs
   * https://github.com/facebook/mysql-5.6/issues/1211
   * https://github.com/facebook/mysql-5.6/pull/1213
   * https://github.com/facebook/mysql-5.6/pull/1218
1. Deeply optimized the access layer of myrocks
1. Deeply optimized ToplingDB Transaction
   * Although the Transaction code is in ToplingDB/RocksDB, it mainly serves MyTopling/MyRocks
1. Support ToplingDB's [Distributed Compaction](https://github.com/topling/rockside/wiki/Distributed-Compaction)
1. Support ToplingDB's [CSPP\_WBWI](https://github.com/topling/cspp-wbwi)(CSPP WriteBatchWithIndex)
1. Support ToplingDB's [CSPP MemTable](https://github.com/topling/rockside/wiki/ToplingCSPPMemTab)
1. Support ToplingDB's [ToplingFastTable](https://github.com/topling/rockside/wiki/ToplingFastTable)
1. Support ToplingDB's [ToplingZipTable](https://github.com/topling/rockside/wiki/ToplingZipTable)
1. Support ToplingDB's [引擎级监控（grafana）](https://github.com/topling/rockside/wiki/grafana%E5%B1%95%E7%A4%BAtoplingdb%E8%BF%90%E8%A1%8C%E6%8C%87%E6%A0%87-%E6%89%8B%E5%8A%A8%E9%85%8D%E7%BD%AE)
1. Support ToplingDB's [Web View](https://github.com/topling/rockside/wiki/WebView)

# (1). MyTopling's index creation and bulk\_load acceleration
In MyRocks, bulk\_load and index creation follow the same set of logic, and the processing method is roughly as follows:
1. If the input data is ordered, directly use RocksDB's SstWriter to create the SST file, and then inject the created SST file into the LSM Tree
1. If the input data is unordered, first sort the data (inner sort + outer sort), and then perform the steps in 1

In the sysbench performance test, we found that the speed of index creation in the prepare phase is very slow, and further confirmed that the sorting in memory is very slow, when the sorting method is very primitive and rude. `std::set` is used, and where you should specifie a custom KeyComparator for `std::set`, it put the Comparator pointer in RocksDB into each set element. So, even using the most common optimizations can improve performance and reduce memory usage. But we ended up with a very thorough optimization:
1. In ToplingDB SstWriter, out-of-order input is allowed
1. Implement AutoSortTable to support out-of-order input
1. Modify MyRocks related code to support out-of-order input

The core of this is VecAutoSortTable. We know that RocksDB's SST is a **S**tatic **S**orted **T**able (also known as a Sorted String Table). When creating an SST, the input data must be ordered by Key. Supporting out-of-order input, the essence is to move the sorting from the outside of the SST to the inside, which seems to be nothing remarkable.

However, the core of the core here is: VecAutoSortTable, which collects data at the fastest speed, and then sorts it in `TableBuilder::Finish`, using devirtualization technology during sorting, which greatly optimizes performance.
> For example, for the create index in sysbench, the physical storage of its secondary key is tuple(index_id, k, id) three 32-bit integers, for a single index, the index_id of all tuples are the same, so we put the following Put together (k, id) into an array, after conversion, sort as an array of 64-bit integers, which not only reduces memory usage, but also avoids indirect access through pointers in String (the general type of Key) The overhead of memory greatly improves the speed of sorting.

> Earlier, AutoSortTable used CSPP Trie, inserting Keys into CSPP Trie one by one is faster than sorting in memory, and CSPP Trie only uses a whole piece of memory, which means that the created CSPP Trie data structure can be passed once The write operation writes to the SST file.  
> However, the storage order of KeyValue in CSPP Trie-based AutoSortTable is disordered, so when compacting multiple AutoSortTables, all these AutoSortTable SSTs need to be loaded into memory, which will cause OOM for a large Index.  

The net effect is that after optimization, the overall speed of index creation is increased by 5 times
> Among them, the performance of the key links we optimized has increased by more than 30 times, and the overall performance has been dragged down by the rest of the links!

# (2). Transaction optimization of ToplingDB
1. In the Transaction of MyRocks, the original performance hotspot was SkipList (SkipList MemTable and SkipList WBWI). After we replaced both of them with CSPP Trie, this performance hotspot disappeared.
1. After the SkipList performance hotspot is eliminated, Lock management, which was not a performance hotspot, has become a new performance hotspot. This part is mainly solved by topling's hash_strmap and gold_hash_map, and the std::string on the call chain is changed to no memory. Copy Slice

# (3). MyTopling's Distributed Compaction
The core of Distributed Compact is to transfer the data dictionary required by CompactionFilter and TablePropertiesCollector to the Compact Worker node. For this, we only need to study the relevant code carefully and serialize/deserialize the data dictionary. Moreover, ToplingDB provides Perfecting the SidePlugin mechanism makes it relatively easy to achieve this.

# (4). Multi-process Compaction Worker
CompactionFilter and TablePropertiesCollector in MyRocks use some global variables, and our Compact Worker is multi-threaded to serve multiple DB instances, and global variables are shared among multiple threads, which will lead to confusion of data from multiple DBs. Therefore, we changed the Compact Worker to multi-threaded, and put the code that executes the Compact Job into the new process that forked out. Regarding this, we have [an answer on Zhihu](https://www.zhihu.com/question/25390536/answer/2510470886)

# (5). Pull Request to upstream RocksDB
Among our many modifications, some codes use topling-core, and the changes to RocksDB are also relatively large, but there are still some modifications that have no external dependencies. We will not have codes that have external dependencies, and we have initiated [Pull Requests](https://github.com/facebook/rocksdb/pulls/rockeet) to the upstream official RocksDB

# (6). For performance test, see: [MyTopling sysbench test report](https://zhuanlan.zhihu.com/p/573217285)

# (7). MyTopling is more optimized for random write
This sysbench command is actually more suitable for InnoDB and the original MyRocks, because prepare writes data in the order of auto-increment id!
1. For InnoDB, sequential writing is a very friendly operation for BTree. Sequential writing for each of the 1000 tables is not really sequential writing on the surface, but in fact, when it reaches the lower layer of BTree, it is equivalent to 1000 Write in the order of range
2. If the number of tables is 1, for the original version of MyRocks, data is written sequentially, after SST flush, only trivial move is required until the level where the compression option changes

In MyTopling, because of the support of distributed compaction, we do not optimize for sequential writing. We do not allow SST from L0 trivial move to L1, nor from L1 trivial move to lower layers, but:
1. Use larger MemTable, generate larger L0 file
2. Set smaller L1 SST file size
   1. During L0->L1 compact, a small number of L0 SST files will be compacted into many L1 SST files
   2. During L1->L2 compact, because of the large number of L1 SST files, multiple compact threads can be started, and distributed compact can be used to fully parallelize

Our principle is "full parallelization", so "optimizing for sequential writing" is not so important, because in actual business, most of the writing is random writing!
