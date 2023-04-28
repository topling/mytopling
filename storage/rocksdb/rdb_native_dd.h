/*
   Copyright (c) 2023 Meta, Inc

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

/* C++ standard header files */
#include <unordered_set>

/* MySQL header files */
#include "sql/dd/object_id.h"

namespace dd {
class Table;
}

namespace myrocks {

void rocksdb_dict_register_dd_table_id(dd::Object_id dd_table_id);

class native_dd {
 private:
  /* Set of ids of DD tables */
  static std::unordered_set<dd::Object_id> s_dd_table_ids;

 public:
  static bool is_dd_table_id(dd::Object_id id);

  static void insert_dd_table_ids(dd::Object_id dd_table_id);

  static void clear_dd_table_ids();

  static int reject_if_dd_table(const dd::Table *table_def);
};

}  // namespace myrocks
