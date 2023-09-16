### [中文版 (Chinese)](./README-zh_cn.md)

## MyTopling Document
* Wiki: [Home](https://github.com/topling/mytopling-wiki-en/wiki) describe how to build and run MyTopling
* Wiki: [Compatibility To MyRocks](https://github.com/topling/mytopling-wiki-en/wiki/Compatibility-To-MyRocks)
* [MyTopling README](./README.mytopling.md)

## Performance comparison between MyTopling and other MySQL (sysbench)
Note: The original sysbench can only generate data randomly, our [modified version of sysbench](https://github.com/topling/sysbench) adds the function of loading real data from files. Wikipedia data is used in this test. For more information, see [Comparison Test](https://github.com/topling/mytopling-wiki-en/wiki/MyTopling-Sysbench-With-Other-MySQL).

![image](https://user-images.githubusercontent.com/54310035/218080504-c77b152c-72eb-4560-8e83-08a2eb3c4646.png)

---

![image](https://user-images.githubusercontent.com/54310035/218080865-ce2106fe-b508-4523-8e60-1d7a5a3a7cf3.png)

---

Note: MyTopling NAS after write performance drops more because in the write test (Update & Insert) after prepare, MyTopling NAS writes the fastest, and the execution time of different DB tests is the same, so MyTopling NAS writes the most data, and The NAS has reached the upper limit of IOPS (30,000), because each time 100, QPS 715 is equivalent to accessing 71,500 rows of data per second. It can be seen that compared with MyRocks NAS and MySQL NAS, the performance of MyTopling after write still has an overwhelming advantage.

---

<!-- MyTopling cloud-native architecture -->
![image](https://user-images.githubusercontent.com/54310035/217221215-c23775ae-9277-4405-bce7-c18f61ecf9c0.png)
