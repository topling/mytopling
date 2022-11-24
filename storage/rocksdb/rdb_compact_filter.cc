//
// Created by leipeng on 2019-11-27.
//

#include "rdb_compact_filter.h"

/* MyRocks includes */
#include "rdb_perf_context.h"
#include "rdb_datadic.h"
#include "ha_rocksdb_proto.h"
#include "rocksdb/listener.h"

/* C++ system header files */
#include <string>
#include <time.h>

#include <boost/range/algorithm/binary_search.hpp>
#include <terark/util/function.hpp>
#include <terark/valvec.hpp>
#include <terark/io/FileStream.hpp>
#include <terark/io/DataIO.hpp>
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

namespace myrocks {

using namespace terark;

//using namespace rocksdb;
// rocksdb::ERROR_LEVEL confilict with mysql's

using rocksdb::Slice;
using rocksdb::CompactionFilter;
using rocksdb::CompactionFilterFactory;
using rocksdb::CompactionFilterContext;
using rocksdb::InfoLogLevel;

using rocksdb::IsCompactionWorker;
using rocksdb::CompactionParams;
using rocksdb::JS_CompactionParamsDecodePtr;
using rocksdb::SidePluginRepo;
using rocksdb::SerDeFunc;
using rocksdb::json;

class Rdb_compact_filter : public rocksdb::CompactionFilter {
  Rdb_compact_filter(const Rdb_compact_filter &) = default;
  Rdb_compact_filter &operator=(const Rdb_compact_filter &) = delete;
public:

  explicit Rdb_compact_filter(uint32_t _cf_id) : m_cf_id(_cf_id) {
  }
  ~Rdb_compact_filter();

  Decision FilterV2(int /*level*/, const Slice& key, ValueType value_type,
                    const Slice &existing_value,
                    std::string* /*new_value*/,
                    std::string* /*skip_until*/) const override {
    assert(key.size() >= sizeof(uint32));
    if (value_type != CompactionFilter::ValueType::kValue) {
      // since we has not override FiterMerge...,
      // just keep merge type value
      return CompactionFilter::Decision::kKeep;
    }

    GL_INDEX_ID gl_index_id;
    gl_index_id.cf_id = m_cf_id;
    gl_index_id.index_id = rdb_netbuf_to_uint32((const uchar *)key.data());
    assert(gl_index_id.index_id >= 1);

    if (gl_index_id != m_prev_index) {
      m_should_delete = boost::binary_search(m_wire->m_del_vec, gl_index_id);
      if (!m_should_delete) {
        get_ttl_duration_and_offset(gl_index_id);
      }
      m_prev_index = gl_index_id;
    }

    if (m_should_delete) {
      m_num_deleted++;
      return rocksdb::CompactionFilter::Decision::kRemove;
    }

    if (m_ttl_duration > 0) {
      if (should_filter_ttl_rec(key, existing_value)) {
        m_num_expired++;
        return ::rocksdb::CompactionFilter::Decision::kRemove;
      }
    }
    return rocksdb::CompactionFilter::Decision::kKeep;
  }

  bool IgnoreSnapshots() const override { return true; }

  const char *Name() const override { return "Rdb_compact_filter"; }

  void get_ttl_duration_and_offset(const GL_INDEX_ID gl_index_id) const {
    m_ttl_duration = 0;
    //m_ttl_offset = INT32_MAX;
    if (m_wire->m_idx_vec.empty()) {
      return;
    }
    auto beg = m_wire->m_idx_vec.begin();
    auto end = m_wire->m_idx_vec.end();
    auto lo = lower_bound_ex(beg,end,gl_index_id,TERARK_FIELD(m_gl_index_id));
    if (lo < end && lo->m_gl_index_id == gl_index_id) {
      m_ttl_duration = lo->m_ttl_duration;
      //m_ttl_offset = 0;
      assert(0 == Rdb_key_def::calculate_index_flag_offset(
          lo->m_index_flags, Rdb_key_def::TTL_FLAG));
      return;
    }

#if !defined(NDEBUG)
    if (rdb_dbug_set_ttl_ignore_pk() &&
        lo->m_index_type == Rdb_key_def::INDEX_TYPE_PRIMARY) {
      m_ttl_duration = 0;
    }
#endif  // !defined(NDEBUG)

  }

  bool should_filter_ttl_rec(const rocksdb::Slice& /*key*/,
                             const rocksdb::Slice& existing_value) const {
    if (!m_is_compact_worker) {
      // Case: TTL filtering is paused or expiration ts is 0 (happens on server
      // restart until the next compaction ts is calculated)
      if (rdb_is_ttl_compaction_filter_paused() || m_wire->m_snapshot_timestamp == 0) {
        return false;
      }
    }
    uint64 ttl_timestamp;
    if (existing_value.size() < sizeof(uint64)) {
      std::string buf;
      buf = rdb_hexdump(existing_value.data(), existing_value.size(),
                        RDB_MAX_HEXDUMP_LEN);
      // NO_LINT_DEBUG
      sql_print_error("Decoding ttl from PK value failed in compaction filter, "
                      "for index (%u,%u), val: %s",
                      m_prev_index.cf_id, m_prev_index.index_id, buf.c_str());
      abort();
    }
    ttl_timestamp = rdb_netbuf_to_uint64((const uchar*)existing_value.data());

    /*
      Filter out the record only if it is older than the oldest snapshot
      timestamp.  This prevents any rows from expiring in the middle of
      long-running transactions.
    */
    return ttl_timestamp + m_ttl_duration <= m_wire->m_snapshot_timestamp;
  }

  struct WireData {
    void InitFromDB(uint32_t cf_id);
    // Oldest snapshot timestamp at the time a TTL index is discovered
    mutable uint64_t m_snapshot_timestamp = 0;

    std::vector<GL_INDEX_ID> m_del_vec;
    std::vector<Rdb_index_info> m_idx_vec;
  };

  // Column family for this compaction filter
  uint32_t m_cf_id;
  const bool m_is_compact_worker = rocksdb::IsCompactionWorker();
  // Current index id should be deleted or not (should be deleted if true)
  mutable bool m_should_delete = false;
  // Index id of the previous record
  mutable GL_INDEX_ID m_prev_index = {0, 0};
  // Number of rows deleted for the same index id
  mutable uint64 m_num_deleted = 0;
  // Number of rows expired for the TTL index
  mutable uint64 m_num_expired = 0;
  // TTL duration for the current index if TTL is enabled
  mutable uint64 m_ttl_duration = 0;
  // TTL offset for all records in the current index
  // mutable uint32 m_ttl_offset = 0; // it is always 0

  std::shared_ptr<WireData> m_wire;
  class Rdb_compact_filter_factory* m_fac;
};

class Rdb_compact_filter_factory : public CompactionFilterFactory {
public:
  const char *Name() const override { return "Rdb_compact_filter_factory"; }

  std::unique_ptr<CompactionFilter>
  CreateCompactionFilter(const CompactionFilterContext& context)
  override {
    std::unique_ptr<Rdb_compact_filter>
      cf(new Rdb_compact_filter(context.column_family_id));
    if (IsCompactionWorker()) {
      cf->m_wire = m_wire; // from deserialized data
    }
    else {
      // master node, create fresh WireData on demmand
      cf->m_wire = std::make_shared<Rdb_compact_filter::WireData>();
      cf->m_wire->InitFromDB(context.column_family_id);
    }
    cf->m_fac = this;
    return std::unique_ptr<rocksdb::CompactionFilter>(cf.release());
  }

  // for compact worker
  mutable uint64 m_num_deleted = 0;
  mutable uint64 m_num_expired = 0;
  mutable const CompactionParams* m_cp = nullptr;
  mutable std::shared_ptr<Rdb_compact_filter::WireData> m_wire;
};

Rdb_compact_filter::~Rdb_compact_filter() {
  if (m_num_expired) {
    m_fac->m_num_expired += m_num_expired;
    if (!IsCompactionWorker()) {
      // Increment stats by num expired at the end of compaction
      rdb_update_global_stats(ROWS_EXPIRED, m_num_expired);
    }
  }
  if (m_num_deleted) {
    m_fac->m_num_deleted += m_num_deleted;
  }
}

void Rdb_compact_filter::WireData::InitFromDB(uint32_t cf_id) {
  m_snapshot_timestamp = 0;
  if (rocksdb::DB* rdb = rdb_get_rocksdb_db()) {
    if (!rdb->GetIntProperty(rocksdb::DB::Properties::kOldestSnapshotTime,
                            &m_snapshot_timestamp) ||
        m_snapshot_timestamp == 0) {
      m_snapshot_timestamp = static_cast<uint64_t>(std::time(nullptr));
    }
  }
  if (rdb_is_binlog_ttl_enabled()) {
    m_snapshot_timestamp =
        std::min(rocksdb_binlog_ttl_compaction_timestamp.load(),
                 m_snapshot_timestamp);
  }
  auto dman = rdb_get_dict_manager()->get_dict_manager_selector_const(cf_id);
  dman->get_ongoing_drop_indexes(&m_del_vec);
  if (dman->get_system_cf() && rdb_is_ttl_enabled()) {
    dman->get_all_index_info(&m_idx_vec);
    auto sys_cf_id = dman->get_system_cf()->GetID();
    auto endrm = std::remove_if(m_idx_vec.begin(), m_idx_vec.end(),
      [sys_cf_id](const Rdb_index_info& x) {
        if (x.m_gl_index_id.cf_id == sys_cf_id) {
          /*
            If key is part of system column family, it's definitely not a TTL key.
          */
          return true; // not ttl, remove
        }
        if (!Rdb_key_def::has_index_flag(x.m_index_flags, Rdb_key_def::TTL_FLAG)) {
          return true; // no ttl, remove
        }
        return false; // is ttl, keep
      });
    m_idx_vec.erase(endrm, m_idx_vec.end());
    m_idx_vec.shrink_to_fit();
  }
}

DATA_IO_DUMP_RAW_MEM_E(GL_INDEX_ID)
DATA_IO_DUMP_RAW_MEM_E(Rdb_index_info)

struct Rdb_compact_filter_factory_SerDe : SerDeFunc<CompactionFilterFactory> {
  const CompactionParams* m_cp;
  rocksdb::Logger* info_log;
  int job_id;
  size_t rawzip[2];

  Rdb_compact_filter_factory_SerDe(const json& js, const SidePluginRepo&) {
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
    TRAC("Rdb_compact_filter_factory_SerDe: job_id = %d, smallest_user_key = %s, largest_user_key = %s, job raw = %.3f GB, zip = %.3f GB",
        cp->job_id, smallest_user_key.c_str(), largest_user_key.c_str(), rawzip[0]/1e9, rawzip[1]/1e9);
  }
  void Serialize(FILE* output, const CompactionFilterFactory& base)
  const override {
    auto& fac = dynamic_cast<const Rdb_compact_filter_factory&>(base);
    LittleEndianDataOutput<NonOwnerFileStream> dio(output);
    if (IsCompactionWorker()) {
      dio << fac.m_num_expired;
    }
    else {
      auto wire = std::make_shared<Rdb_compact_filter::WireData>();
      wire->InitFromDB(m_cp->cf_id);
      dio << wire->m_snapshot_timestamp;
      dio << wire->m_del_vec;
      dio << wire->m_idx_vec;
      DEBG("job-%05d: Rdb_compact_filter_factory_SerDe::Serialize: job raw = %.3f GB, zip = %.3f GB, smallest_seqno = %lld",
            job_id, rawzip[0]/1e9, rawzip[1]/1e9, (llong)m_cp->smallest_seqno);
    }
  }
  void DeSerialize(FILE* reader, CompactionFilterFactory* base)
  const override {
    auto fac = dynamic_cast<Rdb_compact_filter_factory*>(base);
    LittleEndianDataInput<NonOwnerFileStream> dio(reader);
    if (IsCompactionWorker()) {
      //ROCKSDB_VERIFY(terark::getEnvBool("MULTI_PROCESS"));
      auto wire = std::make_shared<Rdb_compact_filter::WireData>();
      dio >> wire->m_snapshot_timestamp;
      dio >> wire->m_del_vec;
      dio >> wire->m_idx_vec;
      fac->m_cp = this->m_cp;
      fac->m_wire = wire;
      DEBG("job-%05d: Rdb_compact_filter_factory_SerDe::DeSerialize: job raw = %.3f GB, zip = %.3f GB, smallest_seqno = %lld",
            job_id, rawzip[0]/1e9, rawzip[1]/1e9, (llong)m_cp->smallest_seqno);
    }
    else {
      uint64_t m_num_expired = 0;
      dio >> m_num_expired;
      if (m_num_expired) {
        // Increment stats by num expired at the end of compaction
        rdb_update_global_stats(ROWS_EXPIRED, (uint)m_num_expired);
      }
    }
  }
};

std::shared_ptr<CompactionFilterFactory> New_Rdb_compact_filter_factory() {
  return std::make_shared<Rdb_compact_filter_factory>();
}

using namespace rocksdb;
struct Compaction_Filter_Stat_Manip : PluginManipFunc<CompactionFilterFactory> {
  void Update(CompactionFilterFactory*, const json&, const json&,
              const SidePluginRepo &repo) const {}
  std::string ToString(const CompactionFilterFactory &fac,
                       const json &dump_options,
                       const SidePluginRepo &) const {
    if (auto f = dynamic_cast<const Rdb_compact_filter_factory *>(&fac)) {
      json js;
      js["Class"] = "Rdb_compact_filter_factory";
      js["NumExpired"] = f->m_num_expired;
      js["NumDeleted"] = f->m_num_deleted;

      return JsonToString(js, dump_options);
    }
    THROW_InvalidArgument("Unknow CompactionFilterFactory");
  }
};

ROCKSDB_REG_PluginManip("Rdb_compact_filter_factory", Compaction_Filter_Stat_Manip);

namespace detail {
using namespace rocksdb;
ROCKSDB_REG_PluginSerDe(Rdb_compact_filter_factory);
}

} // namespace myrocks
