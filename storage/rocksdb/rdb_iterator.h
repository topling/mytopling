/*
   Copyright (c) 2020, Facebook, Inc.

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

#include <map>

// MySQL header files
#include "sql/debug_sync.h"
#include "sql/handler.h"

// MyRocks header files
#include "./ha_rocksdb.h"
#include "./ha_rocksdb_proto.h"
#include "./rdb_converter.h"
#include "./rdb_datadic.h"

namespace myrocks {

class Rdb_iterator {
 public:
  virtual ~Rdb_iterator() = 0;

  /*
    find_flag has the same semantics as the SQL layer and current accepts 6
    different values.
                   direction
      seek method  ASC                   DESC
      prefix       HA_READ_KEY_EXACT     HA_READ_PREFIX_LAST
      inclusive    HA_READ_KEY_OR_NEXT   HA_READ_PREFIX_LAST_OR_PREV
      exclusive    HA_READ_AFTER_KEY     HA_READ_BEFORE_KEY

    - Inclusive vs exclusive seek means exactly that, whether the seek key is
       included or not in the resultset.
    - Prefix seek is essentially the same as inclusive seek, except that the
      iterator will return EOF once outside the prefix, whereas inclusive seek
      will continue iterating until end of the table.
    - For each flag, there is a corresponding ascending vs descending version.
      In addition to the seek direction, it also specifies whether to start
      from the upper or lower end of a prefix. For descending scans, we seek
      to the upper end and vice versa for ascending scans.

    full_key_match indicates whether the seek key is a full key or not, and is
    needed for now to do the correct seek internally. Ideally, we wouldn't
    need this flag, and once rocksdb supports prefix seeks, the API can be
    simplified to remove this parameter.
  */
  virtual int seek(enum ha_rkey_function find_flag,
                   const rocksdb::Slice start_key, bool full_key_match,
                   const rocksdb::Slice end_key, bool read_current = false) = 0;
  virtual int get(const rocksdb::Slice *key, rocksdb::PinnableSlice *value,
                  Rdb_lock_type type, bool skip_ttl_check = false,
                  bool skip_wait = false) = 0;
  virtual int next() = 0;
  virtual int prev() = 0;
  virtual rocksdb::Slice key() = 0;
  virtual rocksdb::Slice value() = 0;
  virtual void reset() = 0;
  virtual bool is_valid() = 0;
  virtual void release_snapshot() = 0;
};

class Rdb_iterator_base : public Rdb_iterator {
 private:
  int read_before_key(const bool full_key_match,
                      const rocksdb::Slice &key_slice);
  int read_after_key(const rocksdb::Slice &key_slice);
  void release_scan_iterator();
  void setup_scan_iterator(const rocksdb::Slice *const slice,
                           const uint eq_cond_len, bool read_current);
  int calc_eq_cond_len(enum ha_rkey_function find_flag,
                       const rocksdb::Slice &start_key,
                       const int bytes_changed_by_succ,
                       const rocksdb::Slice &end_key);
  int next_with_direction(bool move_forward, bool skip_next);

 public:
  Rdb_iterator_base(THD *thd, ha_rocksdb *rocksdb_handler,
                    const std::shared_ptr<Rdb_key_def> kd,
                    const std::shared_ptr<Rdb_key_def> pkd,
                    const Rdb_tbl_def *tbl_def);

  ~Rdb_iterator_base() override;

  int seek(enum ha_rkey_function find_flag, const rocksdb::Slice start_key,
           bool full_key_match, const rocksdb::Slice end_key,
           bool read_current = false) override;
  int get(const rocksdb::Slice *key, rocksdb::PinnableSlice *value,
          Rdb_lock_type type, bool skip_ttl_check = false,
          bool skip_wait = false) override;

  int next() override { return next_with_direction(true, false); }

  int prev() override { return next_with_direction(false, false); }

 #if defined(_MSC_VER) || defined(__clang__)
  rocksdb::Slice key() override { return m_scan_it->key(); }
  rocksdb::Slice value() override { return m_scan_it->value(); }
 #else
  rocksdb::Slice key  () override { return m_iter_key(m_scan_it); }
  rocksdb::Slice value() override { return m_iter_val(m_scan_it); }
 #endif

  void reset() override {
    release_scan_iterator();
    m_valid = false;
  }
  void release_snapshot() override;

  void init(THD *thd, const std::shared_ptr<Rdb_key_def>& kd,
            const std::shared_ptr<Rdb_key_def>& pkd, const Rdb_tbl_def *tbl_def);

  bool is_valid() override { return m_valid; }
  void set_ignore_killed(bool flag) { m_ignore_killed = flag; }

 protected:
  friend class Rdb_iterator;

  void setup_prefix_buffer(enum ha_rkey_function find_flag,
                           const rocksdb::Slice start_key);
  std::shared_ptr<Rdb_key_def> m_kd;

  // Rdb_key_def of the primary key
  std::shared_ptr<Rdb_key_def> m_pkd;

  const Rdb_tbl_def *m_tbl_def;

  THD *m_thd;

  ha_rocksdb *m_rocksdb_handler;

  /* Iterator used for range scans and for full table/index scans */
  rocksdb::Iterator *m_scan_it;

 #if defined(_MSC_VER) || defined(__clang__)
  rocksdb::Slice InvokeRocksIter_key() const { return m_scan_iter->key(); }
  rocksdb::Slice InvokeRocksIter_val() const { return m_scan_iter->value(); }
 #else
  typedef void (*RocksIterScanFN)(rocksdb::Iterator*);
  typedef bool (*RocksIterValidFN)(const rocksdb::Iterator*);
  typedef rocksdb::Slice (*RocksIterSliceFN)(const rocksdb::Iterator*);
  RocksIterScanFN m_iter_next, m_iter_prev;
  RocksIterSliceFN m_iter_key, m_iter_val;
  RocksIterValidFN m_iter_is_valid;
  inline void rocksdb_smart_next(bool seek_backward, rocksdb::Iterator* iter) {
    if (seek_backward) {
      m_iter_prev(iter); // iter->Prev();
    } else {
      m_iter_next(iter); // iter->Next();
    }
  }
  inline void rocksdb_smart_prev(bool seek_backward, rocksdb::Iterator* iter) {
    if (seek_backward) {
      m_iter_next(iter); // iter->Next();
    } else {
      m_iter_prev(iter); // iter->Prev();
    }
  }
  inline bool is_valid_iterator(rocksdb::Iterator* scan_it) {
    if (likely(m_iter_is_valid(scan_it)))
      return true;
    else
      return is_valid_iter_err(scan_it);
  }
  rocksdb::Slice InvokeRocksIter_key() const { return m_iter_key(m_scan_it); }
  rocksdb::Slice InvokeRocksIter_val() const { return m_iter_val(m_scan_it); }
 #endif
  uint32_t m_call_cnt = 0; // for refresh_iter
  void refresh_iter();

  /* Whether m_scan_it was created with skip_bloom=true */
  bool m_scan_it_skips_bloom;
  bool m_has_been_setup = false;
  bool m_kd_is_reverse_cf = false;

  const rocksdb::Snapshot *m_scan_it_snapshot;

  /* Buffers used for upper/lower bounds for m_scan_it. */
  uchar *m_scan_it_lower_bound;
  uchar *m_scan_it_upper_bound;
  size_t m_packed_buf_len;
  rocksdb::Slice m_scan_it_lower_bound_slice;
  rocksdb::Slice m_scan_it_upper_bound_slice;

  uchar *m_prefix_buf;
  rocksdb::Slice m_prefix_tuple;
  TABLE_TYPE m_table_type;
  bool m_valid;
  bool m_check_iterate_bounds;
  bool m_ignore_killed;
  bool m_kd_has_ttl = false;
  bool m_pkd_has_ttl = false;
};

#if defined(_MSC_VER) || defined(__clang__)
#else
inline int Rdb_iterator_proxy::FatHandle::seek(
              enum ha_rkey_function find_flag,
              const rocksdb::Slice start_key, bool full_key_match,
              const rocksdb::Slice end_key, bool read_current) {
  return m_iter->seek(find_flag, start_key, full_key_match, end_key, read_current);
}
template<class LockType>
inline int Rdb_iterator_proxy::FatHandle::get(
              const rocksdb::Slice *key, rocksdb::PinnableSlice *value,
              LockType type, bool skip_ttl_check, bool skip_wait) {
  return m_iter->get(key, value, type, skip_ttl_check, skip_wait);
}
inline int Rdb_iterator_proxy::FatHandle::next() { return m_next(m_iter); }
inline int Rdb_iterator_proxy::FatHandle::prev() { return m_prev(m_iter); }
using rocksdb::Slice;
inline Slice Rdb_iterator_proxy::FatHandle::key() { return m_key(m_iter); }
inline Slice Rdb_iterator_proxy::FatHandle::value() { return m_value(m_iter); }
inline void Rdb_iterator_proxy::FatHandle::reset() { return m_iter->reset(); }
inline bool Rdb_iterator_proxy::FatHandle::is_valid() {
  // this function is rarely used, not need optimization
  // return m_is_valid(m_iter);
  return m_iter->is_valid();
}
inline void Rdb_iterator_proxy::FatHandle::release_snapshot() {
  return m_iter->release_snapshot();
}
inline Rdb_iterator_proxy::operator std::unique_ptr<Rdb_iterator_base>() && {
  std::unique_ptr<Rdb_iterator_base> tmp(m_fat.m_iter);
  m_fat.m_iter = nullptr;
  return tmp;
}
#endif

class Rdb_iterator_partial : public Rdb_iterator_base {
 private:
  TABLE *m_table;
  MEM_ROOT m_mem_root;

  Rdb_iterator_base m_iterator_pk;
  Rdb_converter m_converter;

  bool m_partial_valid;
  bool m_materialized;

  enum class Iterator_position {
    UNKNOWN,
    START_NEXT_PREFIX,
    START_CUR_PREFIX,
    END_OF_FILE
  };

  Iterator_position m_iterator_pk_position;

  const uint m_threshold;
  const uint m_prefix_keyparts;

  uchar *m_cur_prefix_key;
  uint m_cur_prefix_key_len;

  uchar *m_record_buf;
  uchar *m_pack_buffer;
  uchar *m_sk_packed_tuple;

  Rdb_string_writer m_sk_tails;

  int get_prefix_len(const rocksdb::Slice &start_key, uint *prefix_cnt,
                     uint *prefix_len);
  int get_prefix_from_start(enum ha_rkey_function find_flag,
                            const rocksdb::Slice &start_key);
  int get_next_prefix(bool direction);
  int seek_next_prefix(bool direction);
  int materialize_prefix();
  int read_prefix_from_pk();
  int next_with_direction_in_group(bool direction);
  int next_with_direction(bool direction);

  using Slice_pair = std::pair<rocksdb::Slice, rocksdb::Slice>;
  using Records = std::vector<Slice_pair>;

  struct slice_comparator {
    explicit slice_comparator(const rocksdb::Comparator *c) : m_comparator(c) {}
    const rocksdb::Comparator *const m_comparator;

    bool operator()(const rocksdb::Slice &lhs, const Slice_pair &rhs) {
      return m_comparator->Compare(lhs, rhs.first) < 0;
    }
    bool operator()(const Slice_pair &lhs, const rocksdb::Slice &rhs) {
      return m_comparator->Compare(lhs.first, rhs) < 0;
    }
    bool operator()(const Slice_pair &lhs, const Slice_pair &rhs) {
      return m_comparator->Compare(lhs.first, rhs.first) < 0;
    }
  };

  Records m_records;
  Records::iterator m_records_it;
  slice_comparator m_comparator;

 public:
  Rdb_iterator_partial(THD *thd, const std::shared_ptr<Rdb_key_def> kd,
                       const std::shared_ptr<Rdb_key_def> pkd,
                       const Rdb_tbl_def *tbl_def, TABLE *table,
                       const dd::Table *dd_table);
  ~Rdb_iterator_partial() override;

  int seek(enum ha_rkey_function find_flag, const rocksdb::Slice start_key,
           bool full_key_match, const rocksdb::Slice end_key,
           bool read_current = false) override;
  int get(const rocksdb::Slice *key, rocksdb::PinnableSlice *value,
          Rdb_lock_type type, bool skip_ttl_check = false,
          bool skip_wait = false) override;
  int next() override;
  int prev() override;
  rocksdb::Slice key() override;
  rocksdb::Slice value() override;
  void reset() override;
  bool is_valid() override {
    // This function only used for intrinsic temp tables.
    assert(false);
    return false;
  }
};

}  // namespace myrocks
