## MyTopling Document
* [Wiki Home](https://github.com/topling/mytopling/wiki) describe how to build and run MyTopling
* [Wiki Compatibility To MyRocks](https://github.com/topling/mytopling/wiki/Compatibility-To-MyRocks)
* [MyTopling README(中文版)](README.mytopling-zh_cn.md)

## MyTopling 与其它 MySQL 的性能对比（sysbench）
注：原版 sysbench 只能随机生成数据，我们[修改版的 sysbench](https://github.com/topling/sysbench) 增加了从文件加载真实数据的功能。该测试中使用 wikipedia 数据。更多信息，请参考[对比测试](https://github.com/topling/mytopling/wiki/MyTopling-Sysbench-With-Other-MySQL)。

![image](https://user-images.githubusercontent.com/1574991/210158799-ecf947e2-a058-417d-a879-79b35b55728f.png)

---

![image](https://user-images.githubusercontent.com/1574991/210158804-c6faeea7-5d8f-4834-802a-3cca0602c745.png)
---

注：MyTopling NAS after write 性能下降较多是因为 prepare 之后的 write 测试(Update & Insert) 中，MyTopling NAS 写得最快，而不同 DB 测试执行的时间相同，从而 MyTopling NAS 写入的数据最多，并且 NAS 达到了 IOPS 上限（3 万），因为每次 100 条，QPS 715 就相当于每秒访问了 71500 行数据。可以看到，相比 MyRocks NAS 和 MySQL NAS，MyTopling after write 的性能仍有压倒性的优势。

---

<!-- MyTopling 云原生架构 -->
![image](https://user-images.githubusercontent.com/1574991/210158695-03e3419d-6832-40ce-a736-67a824b7ab16.png)
