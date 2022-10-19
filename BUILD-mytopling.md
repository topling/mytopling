# 1. First build ToplingDB
```bash
  cd /path/to/topling # you should create this dir
  git clone https://github.com/topling/toplingdb.git
  cd toplingdb
  make -j`nproc`
```

# 2. Build MyTopling
```bash
  cd /path/to/topling
  git clone https://github.com/topling/mytopling.git
  cd mytopling
  bash build.sh -DCMAKE_INSTALL_PREFIX=/path/to/install/dir
  cd build-rls
  make -j`nproc`
  sudo make install
```

# 3. Enjoy MyTopling
Now you can run MyTopling Server: [start\_mysqld.sh](storage/rocksdb/start_mysqld.sh)

To explore more feature of MyTopling Server, read and modify [mytopling.json](storage/rocksdb/mytopling.json).

Reference: [ToplingDB SidePlugin Manual](https://github.com/topling/rockside/wiki)

