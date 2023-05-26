/*
   Portions Copyright (c) 2016-Present, Facebook, Inc.
   Portions Copyright (c) 2012,2013 Monty Program Ab

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

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#ifdef __APPLE__

#include <libkern/OSByteOrder.h>

#define be16toh(x) OSSwapBigToHostInt16(x)
#define htobe16(x) OSSwapHostToBigInt16(x)

#define be32toh(x) OSSwapBigToHostInt32(x)
#define htobe32(x) OSSwapHostToBigInt32(x)

#define be64toh(x) OSSwapBigToHostInt64(x)
#define htobe64(x) OSSwapHostToBigInt64(x)

#endif  // __APPLE__

/* MySQL header files */

/* MyRocks header files */
#include "./rdb_global.h"
#include "./rdb_utils.h"

/* RocksDB header files */
#include "rocksdb/slice.h"
#include "rocksdb/status.h"

#include <terark/io/MemStream.hpp>

namespace myrocks {

/*
  Basic composition functions for a network buffer presented as a MySQL String
  ("netstr") which stores data in Network Byte Order (Big Endian).
*/

inline void rdb_netstr_append_uint64(my_core::String *const out_netstr,
                                     const uint64 val) {
  assert(out_netstr != nullptr);

  // Convert from host machine byte order (usually Little Endian) to network
  // byte order (Big Endian).
  uint64 net_val = htobe64(val);
  out_netstr->append(reinterpret_cast<char *>(&net_val), sizeof(net_val));
}

inline void rdb_netstr_append_uint32(my_core::String *const out_netstr,
                                     const uint32 val) {
  assert(out_netstr != nullptr);

  // Convert from host machine byte order (usually Little Endian) to network
  // byte order (Big Endian).
  uint32 net_val = htobe32(val);
  out_netstr->append(reinterpret_cast<char *>(&net_val), sizeof(net_val));
}

inline void rdb_netstr_append_uint16(my_core::String *const out_netstr,
                                     const uint16 val) {
  assert(out_netstr != nullptr);

  // Convert from host machine byte order (usually Little Endian) to network
  // byte order (Big Endian).
  uint16 net_val = htobe16(val);
  out_netstr->append(reinterpret_cast<char *>(&net_val), sizeof(net_val));
}

/*
  Basic network buffer ("netbuf") write helper functions.
*/

inline void rdb_netbuf_store_uint64(uchar *const dst_netbuf, const uint64 n) {
  assert(dst_netbuf != nullptr);

  // Convert from host byte order (usually Little Endian) to network byte order
  // (Big Endian).
  uint64 net_val = htobe64(n);
  memcpy(dst_netbuf, &net_val, sizeof(net_val));
}

inline void rdb_netbuf_store_uint32(uchar *const dst_netbuf, const uint32 n) {
  assert(dst_netbuf != nullptr);

  // Convert from host byte order (usually Little Endian) to network byte order
  // (Big Endian).
  uint32 net_val = htobe32(n);
  memcpy(dst_netbuf, &net_val, sizeof(net_val));
}

inline void rdb_netbuf_store_uint16(uchar *const dst_netbuf, const uint16 n) {
  assert(dst_netbuf != nullptr);

  // Convert from host byte order (usually Little Endian) to network byte order
  // (Big Endian).
  uint16 net_val = htobe16(n);
  memcpy(dst_netbuf, &net_val, sizeof(net_val));
}

inline void rdb_netbuf_store_byte(uchar *const dst_netbuf, const uchar c) {
  assert(dst_netbuf != nullptr);

  *dst_netbuf = c;
}

inline void rdb_netbuf_store_index(uchar *const dst_netbuf,
                                   const uint32 number) {
  assert(dst_netbuf != nullptr);

  rdb_netbuf_store_uint32(dst_netbuf, number);
}

/*
  Basic conversion helper functions from network byte order (Big Endian) to host
  machine byte order (usually Little Endian).
*/

inline uint64 rdb_netbuf_to_uint64(const uchar *const netbuf) {
  assert(netbuf != nullptr);

  uint64 net_val;
  memcpy(&net_val, netbuf, sizeof(net_val));

  // Convert from network byte order (Big Endian) to host machine byte order
  // (usually Little Endian).
  return be64toh(net_val);
}

inline uint32 rdb_netbuf_to_uint32(const uchar *const netbuf) {
  assert(netbuf != nullptr);

  uint32 net_val;
  memcpy(&net_val, netbuf, sizeof(net_val));

  // Convert from network byte order (Big Endian) to host machine byte order
  // (usually Little Endian).
  return be32toh(net_val);
}

inline uint16 rdb_netbuf_to_uint16(const uchar *const netbuf) {
  assert(netbuf != nullptr);

  uint16 net_val;
  memcpy(&net_val, netbuf, sizeof(net_val));

  // Convert from network byte order (Big Endian) to host machine byte order
  // (usually Little Endian).
  return be16toh(net_val);
}

inline uchar rdb_netbuf_to_byte(const uchar *const netbuf) {
  assert(netbuf != nullptr);

  return (uchar)netbuf[0];
}

/*
  Basic network buffer ("netbuf") read helper functions.
  Network buffer stores data in Network Byte Order (Big Endian).
  NB: The netbuf is passed as an input/output param, hence after reading,
      the netbuf pointer gets advanced to the following byte.
*/

inline uint64 rdb_netbuf_read_uint64(const uchar **netbuf_ptr) {
  assert(netbuf_ptr != nullptr);

  // Convert from network byte order (Big Endian) to host machine byte order
  // (usually Little Endian).
  const uint64 host_val = rdb_netbuf_to_uint64(*netbuf_ptr);

  // Advance pointer.
  *netbuf_ptr += sizeof(host_val);

  return host_val;
}

inline uint32 rdb_netbuf_read_uint32(const uchar **netbuf_ptr) {
  assert(netbuf_ptr != nullptr);

  // Convert from network byte order (Big Endian) to host machine byte order
  // (usually Little Endian).
  const uint32 host_val = rdb_netbuf_to_uint32(*netbuf_ptr);

  // Advance pointer.
  *netbuf_ptr += sizeof(host_val);

  return host_val;
}

inline uint16 rdb_netbuf_read_uint16(const uchar **netbuf_ptr) {
  assert(netbuf_ptr != nullptr);

  // Convert from network byte order (Big Endian) to host machine byte order
  // (usually Little Endian).
  const uint16 host_val = rdb_netbuf_to_uint16(*netbuf_ptr);

  // Advance pointer.
  *netbuf_ptr += sizeof(host_val);

  return host_val;
}

inline void rdb_netbuf_read_gl_index(const uchar **netbuf_ptr,
                                     GL_INDEX_ID *const gl_index_id) {
  assert(gl_index_id != nullptr);
  assert(netbuf_ptr != nullptr);

  gl_index_id->cf_id = rdb_netbuf_read_uint32(netbuf_ptr);
  gl_index_id->index_id = rdb_netbuf_read_uint32(netbuf_ptr);
}

/*
  A simple string reader:
  - it keeps position within the string that we read from
  - it prevents one from reading beyond the end of the string.
*/

class Rdb_string_reader {
  const uchar *m_ptr;
  const uchar *m_end;

 private:
  Rdb_string_reader &operator=(const Rdb_string_reader &) = default;

 public:
  Rdb_string_reader(const Rdb_string_reader &) = default;
  /* named constructor */
  static Rdb_string_reader read_or_empty(const rocksdb::Slice *const slice) {
    if (!slice) {
      return Rdb_string_reader("");
    } else {
      return Rdb_string_reader(slice);
    }
  }

  explicit Rdb_string_reader(const std::string &str) {
    if (!str.empty()) {
      m_ptr = (const uchar*)str.data();
      m_end = (const uchar*)str.data() + str.size();
    } else {
      /*
        One can a create a Rdb_string_reader for reading from an empty string
        (although attempts to read anything will fail).
        We must not access str.at(0), since len==0, we can set ptr to any
        value.
      */
      m_ptr = nullptr;
      m_end = nullptr;
    }
  }

  explicit Rdb_string_reader(const rocksdb::Slice *const slice) {
    m_ptr = (const uchar*)slice->data();
    m_end = (const uchar*)slice->end();
  }

  Rdb_string_reader(const uchar *buf, uint buf_len) noexcept
      : m_ptr{buf}, m_end{buf + buf_len} {}

  /*
    Read the next @param size bytes. Returns pointer to the bytes read, or
    nullptr if the remaining string doesn't have that many bytes.
  */
  const char *read(const uint size) {
    const char *res;
    if (unlikely(m_ptr + size > m_end)) {
      res = nullptr;
    } else {
      res = (const char*)m_ptr;
      m_ptr += size;
    }
    return res;
  }

  bool read_uint8(uint *const res) {
    if (unlikely(m_ptr >= m_end)) {
      return true;  // error
    } else {
      *res = *m_ptr++;
      return false;  // Ok
    }
  }

  bool read_uint16(uint *const res) {
    if (unlikely(m_ptr + 2 > m_end)) {
      return true;  // error
    } else {
      *res = rdb_netbuf_to_uint16(m_ptr);
      m_ptr += 2;
      return false;  // Ok
    }
  }

  [[nodiscard]] bool read_uint32(uint32 *const res) {
    if (unlikely(m_ptr + 4 > m_end)) {
      return true;  // error
    } else {
      *res = rdb_netbuf_to_uint32(m_ptr);
      m_ptr += 4;
      return false;  // Ok
    }
  }

  bool read_uint64(uint64 *const res) {
    if (unlikely(m_ptr + 8 > m_end)) {
      return true;  // error
    } else {
      *res = rdb_netbuf_to_uint64(m_ptr);
      m_ptr += 8;
      return false;  // Ok
    }
  }

  uint remaining_bytes() const { return m_end - m_ptr; }

  /*
    Return pointer to data that will be read by next read() call (if there is
    nothing left to read, returns pointer to beyond the end of previous read()
    call)
  */
  const char *get_current_ptr() const { return (const char*)m_ptr; }
};

/*
  @brief
  A buffer one can write the data to.

  @detail
  Suggested usage pattern:

    writer->clear();
    writer->write_XXX(...);
    ...
    // Ok, writer->ptr() points to the data written so far,
    // and writer->get_current_pos() is the length of the data

*/

class Rdb_string_writer {
  terark::AutoGrownMemIO m_data;

 public:
  Rdb_string_writer(const Rdb_string_writer &rhs) {
    m_data.clone(rhs.m_data);
  }

  Rdb_string_writer &operator=(const Rdb_string_writer &rhs) {
    m_data.clone(rhs.m_data);
    return *this;
  }

  Rdb_string_writer(Rdb_string_writer &&rhs) noexcept {
    static_cast<terark::SeekableMemIO&>(m_data) =
    static_cast<terark::SeekableMemIO&>(rhs.m_data);
    rhs.m_data.risk_release_ownership();
  }

  Rdb_string_writer &operator=(Rdb_string_writer &&rhs) noexcept {
    if (m_data.begin()) ::free(m_data.begin());
    static_cast<terark::SeekableMemIO&>(m_data) =
    static_cast<terark::SeekableMemIO&>(rhs.m_data);
    rhs.m_data.risk_release_ownership();
    return *this;
  }
  Rdb_string_writer() {}

  void reserve(size_t size) { m_data.resize(size); }
  void alloc(size_t size) {
    size_t pos = m_data.tell();
    if (m_data.size() < pos + size) {
      m_data.resize(pos + size);
    }
    m_data.skip_unsafe(size);
  }

  void clear() { m_data.rewind(); }
  void write_uint8(const uint val) { m_data.writeByte(val); }
  void write_uint16(const uint16 val) {
    uint16 be = htobe16(val);
    m_data.ensureWrite(&be, 2);
  }

  void write_uint32(const uint32 val) {
    uint32 be = htobe32(val);
    m_data.ensureWrite(&be, 4);
  }

  void write_uint64(const uint64 val) {
    uint64 be = htobe64(val);
    m_data.ensureWrite(&be, 8);
  }

  void write(const uchar *const new_data, const size_t len) {
    m_data.ensureWrite(new_data, len);
  }

  uchar *ptr() { return m_data.begin(); }
  const uchar *ptr() const { return m_data.begin(); }
  uchar *end() { return m_data.current(); }
  const uchar *end() const { return m_data.current(); }
  size_t get_current_pos() const { return m_data.tell(); }
  bool is_empty() const { return m_data.current() == m_data.begin(); }

  void write_uint8_at(const size_t pos, const uint new_val) {
    // This function will only overwrite what was written
    assert(pos < get_current_pos());
    m_data.begin()[pos] = new_val;
  }

  void write_uint16_at(const size_t pos, const uint new_val) {
    // This function will only overwrite what was written
    assert(pos < get_current_pos() && (pos + 1) < get_current_pos());
    rdb_netbuf_store_uint16(m_data.begin() + pos, new_val);
  }

  void truncate(const size_t pos) {
    assert(pos < m_data.size());
    m_data.seek_unsafe(pos);
  }

  void alloc_init(const size_t len, const uchar val = 0) {
    alloc(len);
    memset(m_data.current() - len, val, len);
  }

  /*
    An awful hack to deallocate the buffer without relying on the deconstructor.
    This is needed to suppress valgrind errors in rocksdb.partition
  */
  void free() { m_data.clear(); }

  rocksdb::Slice to_slice() {
    return rocksdb::Slice(reinterpret_cast<char *>(m_data.begin()), m_data.tell());
  }

  bool operator==(const Rdb_string_writer &rhs) const {
    if (m_data.tell() == rhs.m_data.tell()) {
      if (this->is_empty()) {
        // Both empty
        return true;
      }
      return memcmp(ptr(), rhs.ptr(), m_data.tell()) == 0;
    }

    return false;
  }

  bool operator>=(const Rdb_string_writer &rhs) const {
    return cmp(*this, rhs) >= 0;
  }
  bool operator<=(const Rdb_string_writer &rhs) const {
    return cmp(*this, rhs) <= 0;
  }
  bool operator>(const Rdb_string_writer &rhs) const {
    return cmp(*this, rhs) > 0;
  }
  bool operator<(const Rdb_string_writer &rhs) const {
    return cmp(*this, rhs) < 0;
  }

  static int cmp(const Rdb_string_writer &lhs, const Rdb_string_writer &rhs) {
    size_t l_size = lhs.get_current_pos();
    size_t r_size = rhs.get_current_pos();
    size_t size = std::min(l_size, r_size);

    // Only compare if both of them are not empty
    if (size > 0) {
      int diff = memcmp(lhs.ptr(), rhs.ptr(), size);
      if (diff) {
        return diff;
      }
    }

    // The first min(l_size, r_size) bytes are equal, compare length
    if (l_size > r_size) {
      return 1;
    } else if (l_size < r_size) {
      return -1;
    } else {
      return 0;
    }
  }
};

/*
   A helper class for writing bits into Rdb_string_writer.

   The class assumes (but doesn't check) that nobody tries to write
   anything to the Rdb_string_writer that it is writing to.
*/
class Rdb_bit_writer {
  Rdb_string_writer *m_writer;
  uchar m_offset;

 public:
  Rdb_bit_writer(const Rdb_bit_writer &) = delete;
  Rdb_bit_writer &operator=(const Rdb_bit_writer &) = delete;

  explicit Rdb_bit_writer(Rdb_string_writer *writer_arg)
      : m_writer(writer_arg), m_offset(0) {}

  void write(uint size, const uint value) {
    assert((value & ((1 << size) - 1)) == value);

    while (size > 0) {
      if (m_offset == 0) {
        m_writer->write_uint8(0);
      }
      // number of bits to put in this byte
      const uint bits = std::min(size, (uint)(8 - m_offset));
      uchar *const last_byte =
          m_writer->ptr() + m_writer->get_current_pos() - 1;
      *last_byte |= (uchar)((value >> (size - bits)) & ((1 << bits) - 1))
                    << m_offset;
      size -= bits;
      m_offset = (m_offset + bits) & 0x7;
    }
  }
};

class Rdb_bit_reader {
  const uchar *m_cur;
  uchar m_offset;
  uint m_ret;
  Rdb_string_reader *const m_reader;

 public:
  Rdb_bit_reader(const Rdb_bit_reader &) = delete;
  Rdb_bit_reader &operator=(const Rdb_bit_reader &) = delete;

  explicit Rdb_bit_reader(Rdb_string_reader *const reader)
      : m_cur(nullptr), m_offset(0), m_reader(reader) {}

  // Returns a pointer to an uint containing the bits read. On subsequent
  // reads, the value being pointed to will be overwritten.  Returns nullptr
  // on failure.
  uint *read(uint size) {
    m_ret = 0;
    assert(size <= 32);

    while (size > 0) {
      if (m_offset == 0) {
        m_cur = (const uchar *)m_reader->read(1);
        if (m_cur == nullptr) {
          return nullptr;
        }
      }
      // how many bits from the current byte?
      const uint bits = std::min((uint)(8 - m_offset), size);
      m_ret <<= bits;
      m_ret |= (*m_cur >> m_offset) & ((1 << bits) - 1);
      size -= bits;
      m_offset = (m_offset + bits) & 0x7;
    }

    return &m_ret;
  }
};

template <size_t buf_length>
class Rdb_buf_writer {
 public:
  Rdb_buf_writer(const Rdb_buf_writer &) = delete;
  Rdb_buf_writer &operator=(const Rdb_buf_writer &) = delete;
  Rdb_buf_writer() { reset(); }

  void write_uint32(const uint32 n) {
    assert(m_ptr + sizeof(n) <= m_buf.data() + buf_length);
    rdb_netbuf_store_uint32(m_ptr, n);
    m_ptr += sizeof(n);
  }

  void write_uint64(const uint64 n) {
    assert(m_ptr + sizeof(n) <= m_buf.data() + buf_length);
    rdb_netbuf_store_uint64(m_ptr, n);
    m_ptr += sizeof(n);
  }

  void write_uint16(const uint16 n) {
    assert(m_ptr + sizeof(n) <= m_buf.data() + buf_length);
    rdb_netbuf_store_uint16(m_ptr, n);
    m_ptr += sizeof(n);
  }

  void write_byte(const uchar c) {
    assert(m_ptr + sizeof(c) <= m_buf.data() + buf_length);
    rdb_netbuf_store_byte(m_ptr, c);
    m_ptr += sizeof(c);
  }

  void write_index(const uint32 n) { write_uint32(n); }

  void write(const char *buf, const size_t size) {
    assert(m_ptr + size <= m_buf.data() + buf_length);
    memcpy(m_ptr, buf, size);
    m_ptr += size;
  }

  void write(const uchar *buf, const size_t size) {
    assert(m_ptr + size <= m_buf.data() + buf_length);
    memcpy(m_ptr, buf, size);
    m_ptr += size;
  }

  void reset() { m_ptr = m_buf.data(); }

  const char *data() const {
    return reinterpret_cast<const char *>(m_buf.data());
  }

  size_t capacity() { return buf_length; }

  /** Returns actual size of the buffer that has data */
  size_t size() { return m_ptr - m_buf.data(); }

  rocksdb::Slice to_slice() { return rocksdb::Slice(data(), size()); }

 private:
  std::array<uchar, buf_length> m_buf;
  uchar *m_ptr;
};

}  // namespace myrocks
