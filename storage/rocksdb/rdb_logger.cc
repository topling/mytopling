// Created by leipeng 2024-01-04 12:05
#include "logger.h"

/* MySQL header files */
#include <sql/log.h>

namespace myrocks {

void Rdb_logger::Logv(const rocksdb::InfoLogLevel log_level, const char *format,
                      va_list ap) {
  assert(format != nullptr);

  if (m_logger) {
    m_logger->Logv(log_level, format, ap);
  }

  if (log_level < m_mysql_log_level) {
    return;
  }

  enum loglevel mysql_log_level;
  if (log_level >= rocksdb::InfoLogLevel::FATAL_LEVEL) {
    mysql_log_level = loglevel::SYSTEM_LEVEL;
  } else if (log_level >= rocksdb::InfoLogLevel::ERROR_LEVEL) {
    mysql_log_level = loglevel::ERROR_LEVEL;
  } else if (log_level >= rocksdb::InfoLogLevel::WARN_LEVEL) {
    mysql_log_level = loglevel::WARNING_LEVEL;
  } else if (log_level >= rocksdb::InfoLogLevel::INFO_LEVEL) {
    mysql_log_level = loglevel::INFORMATION_LEVEL;
  } else if (log_level >= rocksdb::InfoLogLevel::DEBUG_LEVEL) {
    mysql_log_level = loglevel::DEBUG_LEVEL;
  }

  // log to MySQL
  char* f = (char*)alloca(strlen("LibRocksDB:") + strlen(format) + 1);
  strcpy(f, "LibRocksDB:"); // compiler optimize to mov
  strcat(f, format); // optimize to memcpy, strlen(format) result is reused

  // Given that we are working with a va_list, we can't pass it down
  // to log_errlog_formatted and need to format the message ourselves
  char msg[LOG_BUFF_MAX];
  vsnprintf(msg, sizeof(msg) - 1, f, ap);

  log_errlog_formatted(mysql_log_level, msg);
}

void Rdb_logger::Logv(const char *format, va_list ap) {
  assert(format != nullptr);
  // If no level is specified, it is by default at information level
  Logv(rocksdb::InfoLogLevel::INFO_LEVEL, format, ap);
}

void Rdb_logger::SetInfoLogLevel(const rocksdb::InfoLogLevel log_level) {
  // The InfoLogLevel for the logger is used by rocksdb to filter
  // messages, so it needs to be the lower of the two loggers
  rocksdb::InfoLogLevel base_level = log_level;

  if (m_logger && m_logger->GetInfoLogLevel() < base_level) {
    base_level = m_logger->GetInfoLogLevel();
  }
  rocksdb::Logger::SetInfoLogLevel(base_level);
  m_mysql_log_level = log_level;
}

rocksdb::Status Rdb_logger::CloseImpl() {
  if (m_logger) return m_logger->Close();
  else return rocksdb::Status::OK();
}

} // namespace myrocks
