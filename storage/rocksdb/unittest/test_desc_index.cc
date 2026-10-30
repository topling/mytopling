/*
   Copyright (c) 2026, MyTopling.

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

/*
  A DESC key part is stored as the ASC mem-comparable image with every byte
  complemented, the NULL marker and the varlength cell flags included. These
  tests drive the real packing and walking code with such images and check the
  three properties the engine relies on:

    - the image is the bytewise complement of the ASC image,
    - walking it consumes exactly as many bytes as the ASC image does,
    - byte order of the images is the reverse of the value order.
*/

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "../rdb_datadic.h"

namespace {

using myrocks::Rdb_field_packing;
using myrocks::Rdb_key_def;
using myrocks::Rdb_string_reader;

int g_failures = 0;

#define CHECK(cond)                                                  \
  do {                                                               \
    if (!(cond)) {                                                   \
      std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__,     \
                   __LINE__, #cond);                                 \
      ++g_failures;                                                  \
    }                                                                \
  } while (0)

#define CHECK_EQ(a, b)                                              \
  do {                                                              \
    if (!((a) == (b))) {                                            \
      std::fprintf(stderr, "%s:%d: CHECK_EQ failed: %s != %s\n",     \
                   __FILE__, __LINE__, #a, #b);                      \
      ++g_failures;                                                  \
    }                                                               \
  } while (0)

/* A key def with the bookkeeping a real one gets from Rdb_key_def::setup(). */
class Test_key_def : public Rdb_key_def {
 public:
  explicit Test_key_def(const uint maxlength) {
    m_maxlength = maxlength;
    m_index_type = INDEX_TYPE_SECONDARY;
  }
  /* A one part key def, once the part has been set up. */
  void bind(Rdb_field_packing *const fpi) {
    m_key_parts = 1;
    m_pack_info = fpi;
  }
  /* m_pack_info points at a stack object here, so the base must not free it. */
  ~Test_key_def() { m_pack_info = nullptr; }
};

/* A record buffer plus the TABLE the Fields point into. */
struct Record_ctx {
  Record_ctx() {
    std::memset(record, 0, sizeof(record));
    table.record[0] = record;
    table.record[1] = record;
    table.field = field_slot;
  }
  /* Make the field findable through TABLE::field[field_index()]. */
  void attach(Field *const field) {
    field->set_field_index(0);
    field_slot[0] = field;
  }
  uchar record[64];
  TABLE table;
  Field *field_slot[1] = {nullptr};
};

std::string to_image(const std::string &s) {
  std::string out(s);
  for (char &c : out) c = static_cast<char>(~static_cast<uchar>(c));
  return out;
}

/* Unsigned byte order, which is all RocksDB's comparator knows about. */
int cmp_images(const std::string &a, const std::string &b) {
  const size_t n = std::min(a.size(), b.size());
  const int c = n ? std::memcmp(a.data(), b.data(), n) : 0;
  if (c != 0) return c < 0 ? -1 : 1;
  if (a.size() == b.size()) return 0;
  return a.size() < b.size() ? -1 : 1;
}

std::string as_string(const uchar *const b, const uchar *const e) {
  return std::string(reinterpret_cast<const char *>(b), e - b);
}

/* Store a value into a Field_varstring's record image. */
void set_varstring(uchar *const rec, const char *const s) {
  const size_t len = std::strlen(s);
  rec[0] = static_cast<uchar>(len);
  std::memcpy(rec + 1, s, len);
}

/* Pack one key part the way a real index would, DESC or not. */
std::string pack_part(Rdb_key_def *const kd, Rdb_field_packing *const fpi,
                      Field *const field, const bool desc) {
  fpi->m_is_desc = desc;
  uchar tuple[512];
  uchar pack_buffer[512];
  uchar *const end =
      kd->pack_field(field, fpi, tuple, tuple, pack_buffer, nullptr, nullptr);
  return as_string(tuple, end);
}

/*
  Walk one key part the way a real read does, and return the bytes eaten.
  A negative return means the walk failed.
*/
int skip_part(const Rdb_key_def *const kd, const std::string &img) {
  Rdb_string_reader reader(reinterpret_cast<const uchar *>(img.data()),
                           img.size());
  const int rc = kd->read_memcmp_key_part(&reader, 0);
  if (rc > 0) return -1; /* -1 means the part is NULL, which is not a failure */
  return static_cast<int>(img.size() - reader.remaining_bytes());
}

/* Unpack the value of one (non NULL) key part out of a DESC image. */
bool unpack_part(Rdb_field_packing *const fpi, const std::string &img,
                 uchar *const out) {
  Rdb_string_reader reader(reinterpret_cast<const uchar *>(img.data()),
                           img.size());
  const std::string empty;
  Rdb_string_reader unpack_reader(empty);
  myrocks::Rdb_unpack_func_context ctx{nullptr};
  return fpi->m_unpack_func(fpi, &ctx, out, &reader, &unpack_reader) == 0;
}

/*
  Signed integers: the image is the complement of the ASC image, the walker
  eats the same number of bytes, and byte order is reversed.
*/
void test_integer_desc() {
  Record_ctx ctx;
  Field_long field(ctx.record, 4, nullptr, 0, Field::NONE, "c", false, false);
  field.table = &ctx.table;
  ctx.attach(&field);

  Test_key_def kd(512);
  Rdb_field_packing fpi;
  CHECK(fpi.setup(&kd, &field, 0, 0, 4));
  kd.bind(&fpi);
  CHECK(fpi.m_pack_func == &Rdb_key_def::pack_integer<4>);

  const long values[] = {-2147483647L - 1, -1000, -1, 0, 1, 42, 2147483647L};
  const size_t n_values = sizeof(values) / sizeof(values[0]);
  std::string asc[n_values];
  std::string desc[n_values];

  for (size_t i = 0; i < n_values; ++i) {
    std::memcpy(ctx.record, &values[i], 4);
    asc[i] = pack_part(&kd, &fpi, &field, false);
    desc[i] = pack_part(&kd, &fpi, &field, true);
    CHECK_EQ(desc[i], to_image(asc[i]));
    CHECK_EQ(asc[i].size(), (size_t)4);
  }

  for (size_t i = 0; i < n_values; ++i) {
    CHECK_EQ(skip_part(&kd, desc[i]), 4);

    uchar out[4];
    std::memset(out, 0, sizeof(out));
    CHECK(unpack_part(&fpi, desc[i], out));
    int32_t round_trip = 0;
    std::memcpy(&round_trip, out, 4);
    CHECK_EQ(round_trip, values[i]);

    for (size_t j = 0; j < n_values; ++j) {
      if (i < j) {
        CHECK(cmp_images(asc[i], asc[j]) < 0);
        CHECK(cmp_images(desc[i], desc[j]) > 0);
      }
    }
  }
}

/*
  The NULL marker takes part in the complement, so a NULL sorts after every
  non NULL value in a DESC part.
*/
void test_null_marker_desc() {
  Record_ctx ctx;
  Field_long field(ctx.record, 4, ctx.record + 8, 1, Field::NONE, "c", false, false);
  field.table = &ctx.table;
  ctx.attach(&field);

  Test_key_def kd(512);
  Rdb_field_packing fpi;
  CHECK(fpi.setup(&kd, &field, 0, 0, 4));
  kd.bind(&fpi);
  CHECK(fpi.m_field_is_nullable);

  ctx.record[0] = 7;
  ctx.record[1] = 0;
  ctx.record[2] = 0;
  ctx.record[3] = 0;
  const std::string value_asc = pack_part(&kd, &fpi, &field, false);
  const std::string value_desc = pack_part(&kd, &fpi, &field, true);
  CHECK_EQ(value_asc[0], '\1');
  CHECK_EQ(value_desc[0], '\xfe');
  CHECK_EQ(value_desc, to_image(value_asc));

  ctx.record[8] = 1; /* the null bit */
  const std::string null_asc = pack_part(&kd, &fpi, &field, false);
  const std::string null_desc = pack_part(&kd, &fpi, &field, true);
  CHECK_EQ(null_asc, std::string("\0", 1));
  CHECK_EQ(null_desc, std::string("\xff", 1));
  CHECK(cmp_images(null_desc, value_desc) > 0);

  CHECK_EQ(skip_part(&kd, null_desc), 1);
  CHECK_EQ(skip_part(&kd, value_desc), 1 + 4);

  /* 0x00 complemented is 0xff: neither NULL nor "not NULL". */
  const std::string bad_desc("\0\xfe\xff\xff\xff", 5);
  CHECK_EQ(skip_part(&kd, bad_desc), -1);
}

/* Doubles go through a different unpack path (byte order swapping). */
void test_double_desc() {
  Record_ctx ctx;
  Field_double field(ctx.record, 8, nullptr, 0, Field::NONE, "c", 0, false, false);
  field.table = &ctx.table;
  ctx.attach(&field);

  Test_key_def kd(512);
  Rdb_field_packing fpi;
  CHECK(fpi.setup(&kd, &field, 0, 0, 8));
  kd.bind(&fpi);

  const double values[] = {-1e300, -1.5, -0.0, 0.0, 1.5, 1e300};
  const size_t n_values = sizeof(values) / sizeof(values[0]);
  std::string desc[n_values];

  for (size_t i = 0; i < n_values; ++i) {
    std::memcpy(ctx.record, &values[i], 8);
    const std::string asc = pack_part(&kd, &fpi, &field, false);
    desc[i] = pack_part(&kd, &fpi, &field, true);
    CHECK_EQ(desc[i], to_image(asc));
  }

  for (size_t i = 0; i < n_values; ++i) {
    CHECK_EQ(skip_part(&kd, desc[i]), 8);

    uchar out[8];
    std::memset(out, 0, sizeof(out));
    CHECK(unpack_part(&fpi, desc[i], out));
    double round_trip;
    std::memcpy(&round_trip, out, 8);
    CHECK_EQ(round_trip, values[i]);
  }
}

/* VARBINARY: the 9 byte cell format with the flag byte complemented. */
void test_varbinary_desc() {
  Record_ctx ctx;
  Field_varstring field(ctx.record, 32, 1, nullptr, 0, Field::NONE, "c", nullptr,
                        &my_charset_bin);
  field.table = &ctx.table;
  ctx.attach(&field);

  Test_key_def kd(512);
  Rdb_field_packing fpi;
  CHECK(fpi.setup(&kd, &field, 0, 0, 32));
  kd.bind(&fpi);
  CHECK(fpi.m_skip_func == &Rdb_key_def::skip_variable_length_encoding);

  /* in ascending order, which is what the check below relies on */
  const char *const values[] = {"",
                                "0123456789abcdef0123456789abcdef",
                                "a",
                                "abcdefg",
                                "abcdefgh",
                                "abcdefghabcdefgh",
                                "abcdefghi"};
  const size_t n_values = sizeof(values) / sizeof(values[0]);
  std::string asc[n_values];
  std::string desc[n_values];

  for (size_t i = 0; i < n_values; ++i) {
    set_varstring(ctx.record, values[i]);

    asc[i] = pack_part(&kd, &fpi, &field, false);
    desc[i] = pack_part(&kd, &fpi, &field, true);
    CHECK_EQ(desc[i], to_image(asc[i]));

    CHECK_EQ(skip_part(&kd, desc[i]), (int)asc[i].size());
  }

  /*
    "abcdefgh" and "abcdefghabcdefgh" are a prefix pair whose lengths are
    both an exact multiple of the 8 byte cell payload, so the shorter one
    ends on a cell boundary instead of on a short cell.
  */
  /* 8 payload bytes plus the flag byte per cell, see pack_variable_format */
  CHECK_EQ(asc[4].size(), (size_t)9);
  CHECK_EQ(asc[5].size(), (size_t)18);
  CHECK(cmp_images(asc[4], asc[5]) < 0);
  CHECK(cmp_images(desc[4], desc[5]) > 0);

  for (size_t i = 0; i < n_values; ++i) {
    for (size_t j = 0; j < n_values; ++j) {
      if (i < j) CHECK(cmp_images(asc[i], asc[j]) < 0);
    }
  }
}

/* UTF8MB4_BIN VARCHAR: the space padded chunks with the markers complemented. */
void test_varchar_space_pad_desc() {
  Record_ctx ctx;
  /* 80 bytes is 20 characters, enough for the longest value below */
  Field_varstring field(ctx.record, 80, 1, nullptr, 0, Field::NONE, "c", nullptr,
                        &my_charset_utf8mb4_bin);
  field.table = &ctx.table;
  ctx.attach(&field);

  Test_key_def kd(1024);
  Rdb_field_packing fpi;
  CHECK(fpi.setup(&kd, &field, 0, 0, 80));
  kd.bind(&fpi);
  CHECK(fpi.m_skip_func == &Rdb_key_def::skip_variable_space_pad);
  CHECK(fpi.m_pack_func == &Rdb_key_def::pack_with_varlength_space_pad);

  CHECK_EQ(fpi.m_segment_size, (size_t)10); /* 3 characters, 3 bytes each + 1 */

  /* non-descending under this collation, which pads with spaces */
  const char *const values[] = {"",         "a",        "ab",       "ab ",
                                "ab   ",    "abc",      "abcd efgh ijkl",
                                "abcdefgh", "abcdefghi", "abcdefghijklmnopqr"};
  const size_t n_values = sizeof(values) / sizeof(values[0]);
  std::string asc[n_values];
  std::string desc[n_values];

  for (size_t i = 0; i < n_values; ++i) {
    set_varstring(ctx.record, values[i]);

    asc[i] = pack_part(&kd, &fpi, &field, false);
    desc[i] = pack_part(&kd, &fpi, &field, true);
    CHECK_EQ(desc[i], to_image(asc[i]));

    CHECK_EQ(skip_part(&kd, desc[i]), (int)asc[i].size());
  }

  /*
    "abcdefghi" and "abcdefghijklmnopqr" are a prefix pair whose lengths
    (27 and 54 bytes of transformed data) are both an exact multiple of the
    9 byte segment payload, so the shorter one ends on a segment boundary
    instead of on a padded last segment. The lead byte and one terminator
    per segment are on top of that.
  */
  CHECK_EQ(asc[8].size(), (size_t)(1 + 3 * 10));
  CHECK_EQ(asc[9].size(), (size_t)(1 + 6 * 10));
  CHECK(cmp_images(asc[8], asc[9]) < 0);
  CHECK(cmp_images(desc[8], desc[9]) > 0);
}

/*
  NO PAD collations use the other variable length format: cells of 8 payload
  bytes and a flag byte. A value whose transformed length is an exact
  multiple of 8 ends on a full cell with no padding and with flag 8 in the
  last cell.
*/
void test_varchar_nopad_desc() {
  Record_ctx ctx;
  /* 64 bytes is 16 characters, enough for the longest value below */
  Field_varstring field(ctx.record, 64, 1, nullptr, 0, Field::NONE, "c", nullptr,
                        &my_charset_utf8mb4_0900_ai_ci);
  field.table = &ctx.table;
  ctx.attach(&field);

  Test_key_def kd(1024);
  Rdb_field_packing fpi;
  CHECK(fpi.setup(&kd, &field, 0, 0, 64));
  kd.bind(&fpi);
  CHECK(fpi.m_pack_func == &Rdb_key_def::pack_with_varlength_encoding);
  CHECK(fpi.m_skip_func == &Rdb_key_def::skip_variable_length_encoding);

  const char *const values[] = {"", "a", "abcdefgh", "abcdefghijklmnop"};
  const size_t n_values = sizeof(values) / sizeof(values[0]);
  std::string asc[n_values];
  std::string desc[n_values];

  for (size_t i = 0; i < n_values; ++i) {
    set_varstring(ctx.record, values[i]);

    asc[i] = pack_part(&kd, &fpi, &field, false);
    desc[i] = pack_part(&kd, &fpi, &field, true);
    CHECK_EQ(desc[i], to_image(asc[i]));

    CHECK_EQ(skip_part(&kd, desc[i]), (int)asc[i].size());
  }

  for (size_t i = 0; i < n_values; ++i) {
    for (size_t j = 0; j < n_values; ++j) {
      if (i < j) CHECK(cmp_images(asc[i], asc[j]) < 0);
    }
  }

  /*
    Both of these are a prefix of the other and neither of them needs a
    partial last cell, whatever the weight width of this collation is.
  */
  CHECK_EQ(asc[2].back(), (char)8);
  CHECK_EQ(asc[3].back(), (char)8);
  CHECK_EQ(asc[2].size() % 9, (size_t)0);
  CHECK_EQ(asc[3].size() % 9, (size_t)0);
  CHECK(cmp_images(asc[2], asc[3]) < 0);
  CHECK(cmp_images(desc[2], desc[3]) > 0);
}

/* A composite key: each part keeps its own direction. */
void test_composite_desc() {
  Test_key_def kd(1024);
  Record_ctx ctx;

  Field_long int_field(ctx.record, 4, nullptr, 0, Field::NONE, "i", false, false);
  int_field.table = &ctx.table;
  ctx.attach(&int_field);
  Field_varstring str_field(ctx.record + 8, 16, 1, nullptr, 0, Field::NONE, "s",
                            nullptr, &my_charset_bin);
  str_field.table = &ctx.table;
  ctx.field_slot[0] = &str_field;
  str_field.set_field_index(0);

  Rdb_field_packing int_fpi;
  Rdb_field_packing str_fpi;
  CHECK(int_fpi.setup(&kd, &int_field, 0, 0, 4));
  CHECK(str_fpi.setup(&kd, &str_field, 0, 1, 16));

  auto pack_both = [&](long iv, const char *sv, bool int_desc, bool str_desc) {
    std::memcpy(ctx.record, &iv, 4);
    const size_t len = std::strlen(sv);
    ctx.record[8] = static_cast<uchar>(len);
    std::memcpy(ctx.record + 9, sv, len);

    int_fpi.m_is_desc = int_desc;
    str_fpi.m_is_desc = str_desc;
    uchar tuple[512];
    uchar pack_buffer[512];
    uchar *p = kd.pack_field(&int_field, &int_fpi, tuple, tuple, pack_buffer,
                             nullptr, nullptr);
    p = kd.pack_field(&str_field, &str_fpi, p, tuple, pack_buffer, nullptr,
                      nullptr);
    return as_string(tuple, p);
  };

  /* Part 1 ASC, part 2 DESC: part 1 dominates, part 2 is reversed. */
  CHECK(cmp_images(pack_both(1, "a", false, true),
                   pack_both(2, "a", false, true)) < 0);
  CHECK(cmp_images(pack_both(1, "a", false, true),
                   pack_both(1, "b", false, true)) > 0);
  /*
    Both parts DESC: the whole key is the complement of the whole ASC key, so
    byte order is exactly reversed.
  */
  CHECK(cmp_images(pack_both(1, "a", true, true),
                   pack_both(2, "a", true, true)) > 0);
  CHECK(cmp_images(pack_both(1, "a", true, true),
                   pack_both(1, "b", true, true)) > 0);
}

}  // namespace

int main() {
  test_integer_desc();
  test_null_marker_desc();
  test_double_desc();
  test_varbinary_desc();
  test_varchar_space_pad_desc();
  test_varchar_nopad_desc();
  test_composite_desc();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
  }
  std::fprintf(stderr, "all checks passed\n");
  return 0;
}
