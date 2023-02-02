### [中文版 (Chinese)](./README-zh_cn.md)

## MyTopling Document
* Wiki: [Home](https://github.com/topling/mytopling/wiki) describe how to build and run MyTopling
* Wiki: [Compatibility To MyRocks](https://github.com/topling/mytopling/wiki/Compatibility-To-MyRocks)
* [MyTopling README](./README.mytopling.md)

## Performance comparison between MyTopling and other MySQL (sysbench)
Note: The original sysbench can only generate data randomly, our [modified version of sysbench](https://github.com/topling/sysbench) adds the function of loading real data from files. Wikipedia data is used in this test. For more information, see [Comparison Test](https://github.com/topling/mytopling/wiki/MyTopling-Sysbench-With-Other-MySQL).

![image](https://user-images.githubusercontent.com/1574991/210158799-ecf947e2-a058-417d-a879-79b35b55728f.png)

---

![image](https://user-images.githubusercontent.com/1574991/210158804-c6faeea7-5d8f-4834-802a-3cca0602c745.png)

---

Note: MyTopling NAS after write performance drops more because in the write test (Update & Insert) after prepare, MyTopling NAS writes the fastest, and the execution time of different DB tests is the same, so MyTopling NAS writes the most data, and The NAS has reached the upper limit of IOPS (30,000), because each time 100, QPS 715 is equivalent to accessing 71,500 rows of data per second. It can be seen that compared with MyRocks NAS and MySQL NAS, the performance of MyTopling after write still has an overwhelming advantage.

---

<!-- MyTopling cloud-native architecture -->
![image](https://user-images.githubusercontent.com/1574991/210158695-03e3419d-6832-40ce-a736-67a824b7ab16.png)
