/*
   Copyright (c) 2015, Facebook, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */
#pragma once

#include <rocksdb/env.h> // for rocksdb::Logger

namespace myrocks {

class Rdb_logger : public rocksdb::Logger {
 public:
  explicit Rdb_logger(const rocksdb::InfoLogLevel log_level =
                          rocksdb::InfoLogLevel::ERROR_LEVEL)
      : m_mysql_log_level(log_level) {}

  void Logv(const rocksdb::InfoLogLevel log_level, const char *format,
            va_list ap) override;

  void Logv(const char *format, va_list ap) override;

  void SetRocksDBLogger(const std::shared_ptr<rocksdb::Logger> logger) {
    m_logger = logger;
  }

  void SetInfoLogLevel(const rocksdb::InfoLogLevel log_level) override;

 private:
  rocksdb::Status CloseImpl() override;

  std::shared_ptr<rocksdb::Logger> m_logger;
  rocksdb::InfoLogLevel m_mysql_log_level;
};

}  // namespace myrocks
