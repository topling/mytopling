### [中文版 (Chinese)](./README-zh_cn.md)

## MyTopling Document
* Wiki: [Home](https://github.com/topling/mytopling/wiki) describe how to build and run MyTopling
* Wiki: [Compatibility To MyRocks](https://github.com/topling/mytopling/wiki/Compatibility-To-MyRocks)
* [MyTopling README](./README.mytopling.md)

## Performance comparison between MyTopling and other MySQL (sysbench)
Note: The original sysbench can only generate data randomly, our [modified version of sysbench](https://github.com/topling/sysbench) adds the function of loading real data from files. Wikipedia data is used in this test. For more information, see [Comparison Test](https://github.com/topling/mytopling/wiki/MyTopling-Sysbench-With-Other-MySQL).

![image](https://user-images.githubusercontent.com/54310035/218071917-ab78937c-fe91-44b2-8a10-c6391266266b.png)

---

![image](https://user-images.githubusercontent.com/54310035/218071981-29e63385-6126-4f36-a73e-6659ac5c6a11.png)

---

Note: MyTopling NAS after write performance drops more because in the write test (Update & Insert) after prepare, MyTopling NAS writes the fastest, and the execution time of different DB tests is the same, so MyTopling NAS writes the most data, and The NAS has reached the upper limit of IOPS (30,000), because each time 100, QPS 715 is equivalent to accessing 71,500 rows of data per second. It can be seen that compared with MyRocks NAS and MySQL NAS, the performance of MyTopling after write still has an overwhelming advantage.

---

<!-- MyTopling cloud-native architecture -->
![image](https://user-images.githubusercontent.com/54310035/217221215-c23775ae-9277-4405-bce7-c18f61ecf9c0.png)
