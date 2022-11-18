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

/* This C++ file's header file */
#include "./properties_collector.h"

/* Standard C++ header files */
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <terark/valvec.hpp>
#include <terark/io/FileStream.hpp>
#include <terark/io/DataIO.hpp>

/* MySQL header files */
#include "./my_stacktrace.h"
#include "sql/log.h"
#include "sql/sql_array.h"

/* MyRocks header files */
#include "./rdb_datadic.h"
#include "./rdb_utils.h"
#include "ha_rocksdb_proto.h"

#include <topling/side_plugin_factory.h>
#include <db/compaction/compaction_executor.h>
#include <logging/logging.h>

#if 0
#define DoPrintLog(...) fprintf(stderr, __VA_ARGS__)
#else
#define DoPrintLog(...) \
    info_log ? ROCKS_LOG_INFO(info_log, __VA_ARGS__) \
             : (void)fprintf(stderr, __VA_ARGS__)
#endif

#define PrintLog(level, fmt, ...) \
  do { if (SidePluginRepo::DebugLevel() >= level) \
    DoPrintLog("%s: " fmt "\n", \
            TERARK_PP_SmartForPrintf(rocksdb::StrDateTimeNow(), ## __VA_ARGS__)); \
  } while (0)
#define TRAC(...) PrintLog(4, "TRAC: " __VA_ARGS__)
#define DEBG(...) PrintLog(3, "DEBG: " __VA_ARGS__)
#define INFO(...) PrintLog(2, "INFO: " __VA_ARGS__)
#define WARN(...) PrintLog(1, "WARN: " __VA_ARGS__)
#define ERR(...)  PrintLog(1, "ERROR: " __VA_ARGS__)

namespace std {
  DATA_IO_DUMP_RAW_MEM_E(atomic<uint64_t>)
}

namespace myrocks {

using namespace terark;

std::atomic<uint64_t> rocksdb_num_sst_entry_put(0);
std::atomic<uint64_t> rocksdb_num_sst_entry_delete(0);
std::atomic<uint64_t> rocksdb_num_sst_entry_singledelete(0);
std::atomic<uint64_t> rocksdb_num_sst_entry_merge(0);
std::atomic<uint64_t> rocksdb_num_sst_entry_other(0);
std::atomic<uint64_t> rocksdb_additional_compaction_triggers(0);
bool rocksdb_compaction_sequential_deletes_count_sd = false;

Rdb_tbl_prop_coll::Rdb_tbl_prop_coll(find_key_def_func_t find_key_def,
                                     const Rdb_compact_params &params,
                                     const uint32_t cf_id,
                                     const uint8_t table_stats_sampling_pct)
    : m_cf_id(cf_id),
      m_find_key_def(find_key_def),
      m_last_stats(nullptr),
      m_window_pos(0l),
      m_deleted_rows(0l),
      m_max_deleted_rows(0l),
      m_total_puts(0l),
      m_total_merges(0l),
      m_total_deletes(0l),
      m_total_singledeletes(0l),
      m_total_others(0l),
      m_file_size(0),
      m_params(params),
      m_cardinality_collector(table_stats_sampling_pct),
      m_recorded(false) {
  assert(find_key_def != nullptr);

  m_deleted_rows_window.resize(m_params.m_window, false);
}

/*
  This function is called by RocksDB for every key in the SST file
*/
rocksdb::Status Rdb_tbl_prop_coll::AddUserKey(
    const rocksdb::Slice &key, const rocksdb::Slice &value,
    rocksdb::EntryType type,
    rocksdb::SequenceNumber seq MY_ATTRIBUTE((__unused__)),
    uint64_t file_size) {
  if (key.size() >= 4) {
    AdjustDeletedRows(type);

    CollectStatsForRow(key, value, type, file_size);
  }

  return rocksdb::Status::OK();
}

void Rdb_tbl_prop_coll::AdjustDeletedRows(rocksdb::EntryType type) {
  if (m_params.m_window > 0) {
    // record the "is deleted" flag into the sliding window
    // the sliding window is implemented as a circular buffer
    // in m_deleted_rows_window vector
    // the current position in the circular buffer is pointed at by
    // m_rows % m_deleted_rows_window.size()
    // m_deleted_rows is the current number of 1's in the vector
    // --update the counter for the element which will be overridden
    const bool is_delete = (type == rocksdb::kEntryDelete ||
                            (type == rocksdb::kEntrySingleDelete &&
                             rocksdb_compaction_sequential_deletes_count_sd));

    // Only make changes if the value at the current position needs to change
    if (is_delete != m_deleted_rows_window[m_window_pos]) {
      // Set or clear the flag at the current position as appropriate
      m_deleted_rows_window[m_window_pos] = is_delete;
      if (!is_delete) {
        m_deleted_rows--;
      } else if (++m_deleted_rows > m_max_deleted_rows) {
        m_max_deleted_rows = m_deleted_rows;
      }
    }

    if (++m_window_pos == m_params.m_window) {
      m_window_pos = 0;
    }
  }
}

Rdb_index_stats *Rdb_tbl_prop_coll::AccessStats(const rocksdb::Slice &key) {
  GL_INDEX_ID gl_index_id{m_cf_id,
                          rdb_netbuf_to_uint32(
                                 reinterpret_cast<const uchar *>(key.data()))};

  if (m_last_stats == nullptr || m_last_stats->m_gl_index_id != gl_index_id) {
    m_keydef = nullptr;

    // starting a new table
    // add the new element into m_stats
    m_stats.emplace_back(gl_index_id);
    m_last_stats = &m_stats.back();

    if (m_find_key_def) {
      // safe_find() returns a std::shared_ptr<Rdb_key_def> with the count
      // incremented (so it can't be deleted out from under us) and with
      // the mutex locked (if setup has not occurred yet).  We must make
      // sure to free the mutex (via unblock_setup()) when we are done
      // with this object.  Currently this happens earlier in this function
      // when we are switching to a new Rdb_key_def and when this object
      // is destructed.
     #if 0
      m_keydef = m_ddl_manager->safe_find(gl_index_id);
     #else
      m_keydef = m_find_key_def(gl_index_id);
     #endif
      if (m_keydef != nullptr) {
        // resize the array to the number of columns.
        // It will be initialized with zeroes
        m_last_stats->m_distinct_keys_per_prefix.resize(
            m_keydef->get_key_parts());
        m_last_stats->m_name = m_keydef->get_name();
      }
      else {
        using namespace rocksdb;
        rocksdb::Logger* info_log = nullptr;
        WARN("not found gl_index_id{%d,%d}",
          gl_index_id.cf_id, gl_index_id.index_id);
      }
    }
    else {
      using namespace rocksdb;
      rocksdb::Logger* info_log = nullptr;
      ERR("m_find_key_def is null");
    }
    m_cardinality_collector.Reset();
  }

  return m_last_stats;
}

void Rdb_tbl_prop_coll::CollectStatsForRow(const rocksdb::Slice &key,
                                           const rocksdb::Slice &value,
                                           const rocksdb::EntryType &type,
                                           const uint64_t file_size) {
  auto stats = AccessStats(key);

  stats->m_data_size += key.size() + value.size();

  // Incrementing per-index entry-type statistics
  switch (type) {
    case rocksdb::kEntryPut:
      stats->m_rows++;
      break;
    case rocksdb::kEntryDelete:
      stats->m_entry_deletes++;
      break;
    case rocksdb::kEntrySingleDelete:
      stats->m_entry_single_deletes++;
      break;
    case rocksdb::kEntryMerge:
      stats->m_entry_merges++;
      break;
    case rocksdb::kEntryRangeDeletion:
    case rocksdb::kEntryOther:
      stats->m_entry_others++;
      break;
    default:
      rdb_fatal_error(
          "RocksDB: Unexpected entry type found: %u. "
          "This should not happen so aborting the system.",
          type);
      break;
  }

  if (file_size > m_file_size) {
    stats->m_actual_disk_size += file_size - m_file_size;
    m_file_size = file_size;
  }
  else if (file_size < m_file_size) {
    sql_print_warning("Rdb_tbl_prop_coll::CollectStatsForRow: file_size decr: old = %zd, new = %zd",
      m_file_size, file_size);
  }

  if (m_keydef != nullptr && type == rocksdb::kEntryPut) {
    m_cardinality_collector.ProcessKey(key, m_keydef.get(), stats);
  }
}

const char *Rdb_tbl_prop_coll::INDEXSTATS_KEY = "__indexstats__";

/*
  This function is called by RocksDB to compute properties to store in sst file
*/
rocksdb::Status Rdb_tbl_prop_coll::Finish(
    rocksdb::UserCollectedProperties *const properties) {
  assert(properties != nullptr);

  if (!m_recorded) {
    m_total_puts = 0;
    m_total_deletes = 0;
    m_total_singledeletes = 0;
    m_total_merges = 0;
    m_total_others = 0;
    for (auto it = m_stats.begin(); it != m_stats.end(); it++) {
      m_total_puts += it->m_rows;
      m_total_deletes += it->m_entry_deletes;
      m_total_singledeletes += it->m_entry_single_deletes;
      m_total_merges += it->m_entry_merges;
      m_total_others += it->m_entry_others;
    }
    if (m_total_puts > 0) {
      rocksdb_num_sst_entry_put += m_total_puts;
    }

    if (m_total_deletes > 0) {
      rocksdb_num_sst_entry_delete += m_total_deletes;
    }

    if (m_total_singledeletes > 0) {
      rocksdb_num_sst_entry_singledelete += m_total_singledeletes;
    }

    if (m_total_merges > 0) {
      rocksdb_num_sst_entry_merge += m_total_merges;
    }

    if (m_total_merges > 0) {
      rocksdb_num_sst_entry_other += m_total_merges;
    }

    for (Rdb_index_stats &stat : m_stats) {
      m_cardinality_collector.SetCardinality(&stat);
      m_cardinality_collector.AdjustStats(&stat);

      // Adjust cardinality down if it exceeds total number of rows.
      if (stat.m_distinct_keys_per_prefix.size()) {
        int64_t max_distinct_keys = stat.m_distinct_keys_per_prefix.back();
        if (max_distinct_keys > stat.m_rows) {
          stat.adjust_cardinality(static_cast<double>(stat.m_rows) /
                                  max_distinct_keys);
        }
      }
#ifndef NDEBUG
      for (size_t i = 0; i < stat.m_distinct_keys_per_prefix.size(); i++) {
        assert(stat.m_distinct_keys_per_prefix[i] <= stat.m_rows);
      }
#endif
    }
    m_recorded = true;
  }
  properties->insert({INDEXSTATS_KEY, Rdb_index_stats::materialize(m_stats)});
  return rocksdb::Status::OK();
}

bool Rdb_tbl_prop_coll::NeedCompact() const {
  if (m_params.m_deletes && (m_params.m_window > 0) &&
      (m_file_size > m_params.m_file_size) &&
      ((m_max_deleted_rows > m_params.m_deletes) || FilledWithDeletions())) {
    rocksdb_additional_compaction_triggers++;
    return true;
  }
  return false;
}

bool Rdb_tbl_prop_coll::FilledWithDeletions() const {
  uint64_t total_entries = m_total_puts + m_total_deletes +
                           m_total_singledeletes + m_total_merges +
                           m_total_others;
  uint64_t total_deletes = m_total_deletes;
  if (rocksdb_compaction_sequential_deletes_count_sd) {
    total_deletes += m_total_singledeletes;
  }

  if (total_entries > 0 &&
      (static_cast<double>(total_deletes / total_entries) >
       static_cast<double>(m_params.m_deletes / m_params.m_window))) {
    return true;
  }
  return false;
}

/*
  Returns the same as above, but in human-readable way for logging
*/
rocksdb::UserCollectedProperties Rdb_tbl_prop_coll::GetReadableProperties()
    const {
  std::string s;
#ifdef NDEBUG
  s.append("[...");
  s.append(std::to_string(m_stats.size()));
  s.append("  records...]");
#else
  bool first = true;
  for (const auto &it : m_stats) {
    if (first) {
      first = false;
    } else {
      s.append(",");
    }
    s.append(GetReadableStats(it));
  }
#endif
  return rocksdb::UserCollectedProperties{{INDEXSTATS_KEY, s}};
}

std::string Rdb_tbl_prop_coll::GetReadableStats(const Rdb_index_stats &it) {
  std::string s;
  s.append("(");
  s.append(std::to_string(it.m_gl_index_id.cf_id));
  s.append(", ");
  s.append(std::to_string(it.m_gl_index_id.index_id));
  s.append("):{name:");
  s.append(it.m_name);
  s.append(", size:");
  s.append(std::to_string(it.m_data_size));
  s.append(", m_rows:");
  s.append(std::to_string(it.m_rows));
  s.append(", m_actual_disk_size:");
  s.append(std::to_string(it.m_actual_disk_size));
  s.append(", deletes:");
  s.append(std::to_string(it.m_entry_deletes));
  s.append(", single_deletes:");
  s.append(std::to_string(it.m_entry_single_deletes));
  s.append(", merges:");
  s.append(std::to_string(it.m_entry_merges));
  s.append(", others:");
  s.append(std::to_string(it.m_entry_others));
  s.append(", distincts per prefix: [");
  for (auto num : it.m_distinct_keys_per_prefix) {
    s.append(std::to_string(num));
    s.append(" ");
  }
  s.append("]}");
  return s;
}

std::string Rdb_tbl_prop_coll_factory::UserPropToString
(const rocksdb::UserCollectedProperties& user_properties) const {
  std::vector<Rdb_index_stats> stats;
  using namespace rocksdb;
  json djs;
  const auto it2 = user_properties.find(std::string(Rdb_tbl_prop_coll::INDEXSTATS_KEY));
  json& statsjs = djs[Rdb_tbl_prop_coll::INDEXSTATS_KEY];
  if (it2 != user_properties.end()) {
    auto result = Rdb_index_stats::unmaterialize(it2->second, &stats);
    ROCKSDB_VERIFY_EQ(result, HA_EXIT_SUCCESS);
    for (auto& it : stats) {
      json one;
      one["index_id"] = it.m_gl_index_id.index_id;
      one["name"] = it.m_name;
      one["size"] = it.m_data_size;
      one["rows"] = it.m_rows;
      one["actual_disk_size"] = it.m_actual_disk_size;
      one["deletes"] = it.m_entry_deletes;
      one["single_deletes"] = it.m_entry_single_deletes;
      one["merges"] = it.m_entry_merges;
      one["others"] = it.m_entry_others;
      std::string distincts;
      if (it.m_distinct_keys_per_prefix.empty()) {
        distincts = "[ ]";
      }
      else {
        distincts = "[ ";
        for (auto num : it.m_distinct_keys_per_prefix) {
          distincts.append(std::to_string(num));
          distincts.append(", ");
        }
        distincts.replace(distincts.end() - 2, distincts.end(), " ]");
      }
      one["distincts per prefix"] = std::move(distincts);
      statsjs.push_back(std::move(one));
    }
  } else {
    statsjs = "EMPTY";
  }
  return JsonToString(djs, json{{"html", 1}});
}

/*
  Given the properties of an SST file, reads the stats from it and returns it.
*/

void Rdb_tbl_prop_coll::read_stats_from_tbl_props(
    const std::shared_ptr<const rocksdb::TableProperties> &table_props,
    std::vector<Rdb_index_stats> *const out_stats_vector) {
  assert(out_stats_vector != nullptr);
  const auto &user_properties = table_props->user_collected_properties;
  const auto it2 = user_properties.find(std::string(INDEXSTATS_KEY));
  if (it2 != user_properties.end()) {
    auto result MY_ATTRIBUTE((__unused__)) =
        Rdb_index_stats::unmaterialize(it2->second, out_stats_vector);
    assert(result == 0);
  }
}

/*
  Serializes an array of Rdb_index_stats into a network string.
*/
std::string Rdb_index_stats::materialize(
    const std::vector<Rdb_index_stats> &stats) {
  String ret;
  rdb_netstr_append_uint16(&ret, INDEX_STATS_VERSION_WITH_NAME);
  for (const auto &i : stats) {
    rdb_netstr_append_uint32(&ret, i.m_gl_index_id.cf_id);
    rdb_netstr_append_uint32(&ret, i.m_gl_index_id.index_id);
    rdb_netstr_append_uint64(&ret, i.m_data_size);
    rdb_netstr_append_uint64(&ret, i.m_rows);
    rdb_netstr_append_uint64(&ret, i.m_actual_disk_size);
    rdb_netstr_append_uint64(&ret, i.m_distinct_keys_per_prefix.size());
    rdb_netstr_append_uint64(&ret, i.m_entry_deletes);
    rdb_netstr_append_uint64(&ret, i.m_entry_single_deletes);
    rdb_netstr_append_uint64(&ret, i.m_entry_merges);
    rdb_netstr_append_uint64(&ret, i.m_entry_others);
    for (const auto &num_keys : i.m_distinct_keys_per_prefix) {
      rdb_netstr_append_uint64(&ret, num_keys);
    }
    rdb_netstr_append_uint16(&ret, i.m_name.size());
    ret.append(i.m_name.data(), i.m_name.size());
  }

  return std::string((char *)ret.ptr(), ret.length());
}

/**
  @brief
  Reads an array of Rdb_index_stats from a string.
  @return HA_EXIT_FAILURE if it detects any inconsistency in the input
  @return HA_EXIT_SUCCESS if completes successfully
*/
int Rdb_index_stats::unmaterialize(const std::string &s,
                                   std::vector<Rdb_index_stats> *const ret) {
  const uchar *p = rdb_std_str_to_uchar_ptr(s);
  const uchar *const p2 = p + s.size();

  assert(ret != nullptr);

  if (p + 2 > p2) {
    return HA_EXIT_FAILURE;
  }

  const int version = rdb_netbuf_read_uint16(&p);
  Rdb_index_stats stats;
  // Make sure version is within supported range.
  if (version < INDEX_STATS_VERSION_INITIAL ||
      version > INDEX_STATS_VERSION_WITH_NAME) {
    rdb_fatal_error(
        "Index stats version %d was outside of supported range. "
        "This should not happen so aborting the system.",
        version);
  }

  size_t needed = sizeof(stats.m_gl_index_id.cf_id) +
                  sizeof(stats.m_gl_index_id.index_id) +
                  sizeof(stats.m_data_size) + sizeof(stats.m_rows) +
                  sizeof(stats.m_actual_disk_size) + sizeof(uint64);
  if (version >= INDEX_STATS_VERSION_ENTRY_TYPES) {
    needed += sizeof(stats.m_entry_deletes) +
              sizeof(stats.m_entry_single_deletes) +
              sizeof(stats.m_entry_merges) + sizeof(stats.m_entry_others);
  }

  while (p < p2) {
    if (p + needed > p2) {
      return HA_EXIT_FAILURE;
    }
    rdb_netbuf_read_gl_index(&p, &stats.m_gl_index_id);
    stats.m_data_size = rdb_netbuf_read_uint64(&p);
    stats.m_rows = rdb_netbuf_read_uint64(&p);
    stats.m_actual_disk_size = rdb_netbuf_read_uint64(&p);
    stats.m_distinct_keys_per_prefix.resize(rdb_netbuf_read_uint64(&p));
    if (version >= INDEX_STATS_VERSION_ENTRY_TYPES) {
      stats.m_entry_deletes = rdb_netbuf_read_uint64(&p);
      stats.m_entry_single_deletes = rdb_netbuf_read_uint64(&p);
      stats.m_entry_merges = rdb_netbuf_read_uint64(&p);
      stats.m_entry_others = rdb_netbuf_read_uint64(&p);
    }
    if (p + stats.m_distinct_keys_per_prefix.size() *
                sizeof(stats.m_distinct_keys_per_prefix[0]) >
        p2) {
      return HA_EXIT_FAILURE;
    }
    for (std::size_t i = 0; i < stats.m_distinct_keys_per_prefix.size(); i++) {
      stats.m_distinct_keys_per_prefix[i] = rdb_netbuf_read_uint64(&p);
    }
    if (version >= INDEX_STATS_VERSION_WITH_NAME) {
      size_t namelen = rdb_netbuf_read_uint16(&p);
      stats.m_name.assign((const char*)p, namelen);
      p += namelen;
    }
    ret->push_back(stats);
  }
  return HA_EXIT_SUCCESS;
}

void Rdb_index_stats::reset_cardinality() {
  for (size_t i = 0; i < m_distinct_keys_per_prefix.size(); i++) {
    m_distinct_keys_per_prefix[i] = 0;
  }
}

/*
  Merges one Rdb_index_stats into another. Can be used to come up with the stats
  for the index based on stats for each sst
*/
void Rdb_index_stats::merge(const Rdb_index_stats &s, const bool increment,
                            const int64_t estimated_data_len) {
  std::size_t i;

  assert(estimated_data_len >= 0);

  m_gl_index_id = s.m_gl_index_id;
  if (m_distinct_keys_per_prefix.size() < s.m_distinct_keys_per_prefix.size()) {
    m_distinct_keys_per_prefix.resize(s.m_distinct_keys_per_prefix.size());
  }
  if (increment) {
    m_rows += s.m_rows;
    m_data_size += s.m_data_size;

    /*
      The Data_length and Avg_row_length are trailing statistics, meaning
      they don't get updated for the current SST until the next SST is
      written.  So, if rocksdb reports the data_length as 0,
      we make a reasoned estimate for the data_file_length for the
      index in the current SST.
    */
    m_actual_disk_size += s.m_actual_disk_size ? s.m_actual_disk_size
                                               : estimated_data_len * s.m_rows;
    m_entry_deletes += s.m_entry_deletes;
    m_entry_single_deletes += s.m_entry_single_deletes;
    m_entry_merges += s.m_entry_merges;
    m_entry_others += s.m_entry_others;
    for (i = 0; i < s.m_distinct_keys_per_prefix.size(); i++) {
      m_distinct_keys_per_prefix[i] += s.m_distinct_keys_per_prefix[i];
    }
  } else {
    m_rows -= s.m_rows;
    m_data_size -= s.m_data_size;
    m_actual_disk_size -= s.m_actual_disk_size ? s.m_actual_disk_size
                                               : estimated_data_len * s.m_rows;
    m_entry_deletes -= s.m_entry_deletes;
    m_entry_single_deletes -= s.m_entry_single_deletes;
    m_entry_merges -= s.m_entry_merges;
    m_entry_others -= s.m_entry_others;
    for (i = 0; i < s.m_distinct_keys_per_prefix.size(); i++) {
      m_distinct_keys_per_prefix[i] -= s.m_distinct_keys_per_prefix[i];
    }
  }
}

void Rdb_index_stats::adjust_cardinality(double adjustment_factor) {
  for (int64_t &num_keys : m_distinct_keys_per_prefix) {
    num_keys = std::max<int64_t>(
        1, static_cast<int64_t>(num_keys * adjustment_factor));
  }
}

Rdb_tbl_card_coll::Rdb_tbl_card_coll(const uint8_t table_stats_sampling_pct)
    : m_table_stats_sampling_pct(table_stats_sampling_pct),
      m_seed(time(nullptr)) {}

bool Rdb_tbl_card_coll::IsSampingDisabled() {
  // Zero means that we'll use all the keys to update statistics.
  return m_table_stats_sampling_pct == 0 ||
         RDB_TBL_STATS_SAMPLE_PCT_MAX == m_table_stats_sampling_pct;
}

bool Rdb_tbl_card_coll::ShouldCollectStats() {
  if (IsSampingDisabled()) {
    return true;  // collect every key
  }

  const int val = rand_r(&m_seed) % (RDB_TBL_STATS_SAMPLE_PCT_MAX -
                                     RDB_TBL_STATS_SAMPLE_PCT_MIN + 1) +
                  RDB_TBL_STATS_SAMPLE_PCT_MIN;

  assert(val >= RDB_TBL_STATS_SAMPLE_PCT_MIN);
  assert(val <= RDB_TBL_STATS_SAMPLE_PCT_MAX);

  return val <= m_table_stats_sampling_pct;
}

void Rdb_tbl_card_coll::ProcessKey(const rocksdb::Slice &key,
                                   const Rdb_key_def *keydef,
                                   Rdb_index_stats *stats) {
  if (ShouldCollectStats()) {
    std::size_t column = 0;
    bool new_key = true;

    if (!m_last_key.empty()) {
      rocksdb::Slice last(m_last_key.data(), m_last_key.size());
      new_key = (keydef->compare_keys(&last, &key, &column) == 0);
    }

    if (new_key) {
      assert(column <= stats->m_distinct_keys_per_prefix.size());

      if (column < stats->m_distinct_keys_per_prefix.size()) {
        // At this point, stats->m_distinct_keys_per_prefix does
        // not hold its final values.
        // After all keys are processed, SetCardinality() needs
        // to be called to calculate the final values.
        stats->m_distinct_keys_per_prefix[column]++;
        m_last_key.assign(key.data(), key.size());
      }
    }
  }
}

void Rdb_tbl_card_coll::Reset() { m_last_key.clear(); }

// We need to adjust the index cardinality numbers based on the sampling
// rate so that the output of "SHOW INDEX" command will reflect reality
// more closely. It will still be an approximation, just a better one.
void Rdb_tbl_card_coll::AdjustStats(Rdb_index_stats *stats) {
  if (IsSampingDisabled()) {
    // no sampling was done, return as stats is
    return;
  }
  for (int64_t &num_keys : stats->m_distinct_keys_per_prefix) {
    num_keys = num_keys * 100 / m_table_stats_sampling_pct;
  }
}

void Rdb_tbl_card_coll::SetCardinality(Rdb_index_stats *stats) {
  for (size_t i = 1; i < stats->m_distinct_keys_per_prefix.size(); i++) {
    stats->m_distinct_keys_per_prefix[i] +=
        stats->m_distinct_keys_per_prefix[i - 1];
  }
}

Rdb_tbl_prop_coll_factory::Rdb_tbl_prop_coll_factory(Rdb_ddl_manager* dm) {
  m_find_key_def = [dm](GL_INDEX_ID iid) { return dm->safe_find(iid); };
}

DATA_IO_DUMP_RAW_MEM_E(Rdb_compact_params)


namespace detail {
using namespace rocksdb;
struct Rdb_tbl_prop_coll_factory_SerDe : SerDeFunc<TablePropertiesCollectorFactory> {
  const CompactionParams* m_cp;
  rocksdb::Logger* info_log;
  int job_id;
  size_t rawzip[2];

  Rdb_tbl_prop_coll_factory_SerDe(const json& js, const SidePluginRepo&) {
    auto cp = m_cp = JS_CompactionParamsDecodePtr(js);
    info_log = cp->info_log;

    // we requires 1==max_subcompactions, this makes all things simpler
    //
    // 1==max_subcompactions is not required for Dcompact, but it is too
    // complicated to gracefully support multi sub compact in Dcompact,
    // such as this use case, it requires DB side and Worker side has same
    // sub compact boundary to reading keys by streaming.
    TERARK_VERIFY_EQ(cp->max_subcompactions, 1);

    const auto& smallest_user_key = cp->smallest_user_key;
    const auto& largest_user_key = cp->largest_user_key;
    job_id = cp->job_id;
    cp->InputBytes(rawzip);
    TRAC("Rdb_tbl_prop_coll_factory_SerDe cons: job_id = %d, smallest_user_key = %s, largest_user_key = %s, job raw = %.3f GB, zip = %.3f GB",
        cp->job_id, smallest_user_key.c_str(), largest_user_key.c_str(), rawzip[0]/1e9, rawzip[1]/1e9);
  }
  void Serialize(FILE* output, const TablePropertiesCollectorFactory& base)
  const override {
    auto& fac = dynamic_cast<const Rdb_tbl_prop_coll_factory&>(base);
    LittleEndianDataOutput<NonOwnerFileStream> dio(output);
    if (IsCompactionWorker()) {
      dio << rocksdb_num_sst_entry_put;
      dio << rocksdb_num_sst_entry_delete;
      dio << rocksdb_num_sst_entry_singledelete;
      dio << rocksdb_num_sst_entry_merge;
      dio << rocksdb_num_sst_entry_other;
      dio << rocksdb_additional_compaction_triggers;
    }
    else {
      dio << rocksdb_compaction_sequential_deletes_count_sd;
      dio << fac.m_params;
      dio << fac.m_table_stats_sampling_pct;
      rdb_get_ddl_manager()->write_key_def_rng(dio, m_cp->cf_id,
                        m_cp->smallest_user_key, m_cp->largest_user_key);
      DEBG("job-%05d: Rdb_tbl_prop_coll_factory_SerDe::Serialize: job raw = %.3f GB, zip = %.3f GB, smallest_seqno = %lld",
            job_id, rawzip[0]/1e9, rawzip[1]/1e9, (llong)m_cp->smallest_seqno);
    }
  }
  void DeSerialize(FILE* reader, TablePropertiesCollectorFactory* base)
  const override {
    auto fac = dynamic_cast<Rdb_tbl_prop_coll_factory*>(base);
    LittleEndianDataInput<NonOwnerFileStream> dio(reader);
    if (IsCompactionWorker()) {
      //ROCKSDB_VERIFY(terark::getEnvBool("MULTI_PROCESS"));
      dio >> rocksdb_compaction_sequential_deletes_count_sd;
      dio >> fac->m_params;
      dio >> fac->m_table_stats_sampling_pct;
      auto pvec = std::make_shared<std::vector<std::shared_ptr<Rdb_key_def> > >();
      rocksdb::Status s = rdb_get_ddl_manager()->read_key_def_all(dio, pvec.get());
      if (s.ok()) {
        // m_find_key_def shared own pvec
        fac->m_find_key_def = [pvec](GL_INDEX_ID x) {
          auto iter = lower_bound_ex_r(*pvec, x, TERARK_FIELD_P(get_gl_index_id()));
          std::shared_ptr<Rdb_key_def> ret;
          if (pvec->end() != iter && (*iter)->get_gl_index_id() == x) {
            ret = *iter;
          }
          return ret;
        };
      }
      else {
        ERR("read_key_def_all = %s\n", s.ToString());
      }
      DEBG("job-%05d: Rdb_tbl_prop_coll_factory_SerDe::DeSerialize: job raw = %.3f GB, zip = %.3f GB, smallest_seqno = %lld",
            job_id, rawzip[0]/1e9, rawzip[1]/1e9, (llong)m_cp->smallest_seqno);
    }
    else { // MySQL side
      uint64_t num_sst_entry_put(0);
      uint64_t num_sst_entry_delete(0);
      uint64_t num_sst_entry_singledelete(0);
      uint64_t num_sst_entry_merge(0);
      uint64_t num_sst_entry_other(0);
      uint64_t additional_compaction_triggers(0);
      dio >> num_sst_entry_put;
      dio >> num_sst_entry_delete;
      dio >> num_sst_entry_singledelete;
      dio >> num_sst_entry_merge;
      dio >> num_sst_entry_other;
      dio >> additional_compaction_triggers;
      #define UpdateNum(val) \
        if (val) rocksdb_##val.fetch_add(val, std::memory_order_relaxed)
      UpdateNum(num_sst_entry_put);
      UpdateNum(num_sst_entry_delete);
      UpdateNum(num_sst_entry_singledelete);
      UpdateNum(num_sst_entry_merge);
      UpdateNum(num_sst_entry_other);
      UpdateNum(additional_compaction_triggers);
    }
  }
};

ROCKSDB_REG_PluginSerDe(Rdb_tbl_prop_coll_factory);
} // namespace detail
}  // namespace myrocks
