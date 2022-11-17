#include <topling/side_plugin_repo.h>
#include <topling/side_plugin_factory.h>
#include <rocksdb/db.h>
#include "sql/mysql_githash.h"
#include <iostream>


#pragma GCC diagnostic ignored "-Wcast-qual"
extern const char* rocksdb_build_git_sha  __attribute__((weak));
extern const char* rocksdb_build_git_date __attribute__((weak));
const char* rocksdb_git_hash = rocksdb_build_git_sha  ? strchr(rocksdb_build_git_sha , ':') + 1 : "none";
const char* rocksdb_git_date = rocksdb_build_git_date ? strchr(rocksdb_build_git_date, ':') + 1 : "none";

const char* git_version_hash_info_cspp_memtable();
const char* git_version_hash_info_cspp_wbwi();
const char* git_version_hash_info_topling_sst();
const char* git_version_hash_info_zbs();
const char* git_version_hash_info_core();
const char* git_version_hash_info_fsa();
__attribute__((weak)) const char* git_version_hash_info_topling_rocks();


namespace rocksdb {

void JS_TopTable_AddVersion(json& djs, bool html);
__attribute__((weak)) void JS_ZipTable_AddVersion(json& djs, bool html);

class GitInfoShower : public AnyPlugin {
public:
  const char* Name() const final { return "GitInfoShower"; }
  void Update(const json&, const json&, const SidePluginRepo &) {}
  std::string ToString(const json& dump_options, const SidePluginRepo&) const {
    json js;
    bool html = JsonSmartBool(dump_options, "html", true);

    std::string mysql_git_hash(MYSQL_GIT_HASH);
    js["mysql"]["git_sha"] = html ? "<a target=\"_blank\" href=\"https://github.com/topling/mytopling/commit/" + mysql_git_hash + "\">" + mysql_git_hash + "</a>" : mysql_git_hash;
    js["mysql"]["git_date"] = MYSQL_GIT_DATE;

    // std::string rocksdb_git_hash2(rocksdb_git_hash);
    // js["ToplingDB"]["Git commit"] = "<a target=\"_blank\" href=\"https://github.com/topling/toplingdb/commit/" + rocksdb_git_hash2 + "\">" + rocksdb_git_hash2 + "</a>";
    // js["ToplingDB"]["Git date"] = rocksdb_git_date;

    std::string git_cspp_mem(git_version_hash_info_cspp_memtable());
    std::string git_cspp_wbwi(git_version_hash_info_cspp_wbwi());

    if (html) {
      js["cspp memtable"] = insert_html_link_and_symbol(git_cspp_mem, "https://github.com/topling/cspp-memtable/commit/", 33);
      js["cspp wbwi"] = insert_html_link_and_symbol(git_cspp_wbwi, "https://github.com/topling/cspp-wbwi/commit/", 33);
    } else {
      js["cspp memtable"] = git_cspp_mem;
      js["cspp wbwi"] = git_cspp_wbwi;
    }
    
    if (git_version_hash_info_topling_rocks) {
        std::string git_topling_rocks(git_version_hash_info_topling_rocks());
        if (html) {
          js["topling rocks"] = insert_html_link_and_symbol(git_topling_rocks, "https://github.com/rockeet/topling-rocks/commit/", 33);
        } else {
          js["topling rocks"] = git_topling_rocks;
        }
    }

    // std::string git_topling_sst(git_version_hash_info_topling_sst());
    // js["Topling SST"] = insert_html_link_and_symbol(git_topling_sst, "https://github.com/topling/topling-sst/commit/", 33);

    // std::string git_topling_zip_zbs(git_version_hash_info_zbs());
    // js["Topling Zip"] = insert_html_link_and_symbol(git_topling_zip_zbs, "https://github.com/topling/topling-zip/commit/", 25);

    json jtop, jzip;
    if (JS_ZipTable_AddVersion) {
      JS_ZipTable_AddVersion(jzip, html);
    }
    JS_TopTable_AddVersion(jtop, html);
    js.merge_patch(jtop["version"]);

    return JsonToString(js, dump_options);
  }

private:
    // ids is the first pos of the git hash
    std::string& insert_html_link_and_symbol(std::string& str, const std::string link, const uint idx) const {
        str.append("</pre>");
        str.insert(idx + 40, "</a>");
        str.insert(idx, "<a target=\"_blank\" href=\"" + link + str.substr(idx, 40) +"\">");
        str.insert(0, "<pre>");
        return str;
    }
};

ROCKSDB_REG_DEFAULT_CONS(GitInfoShower, AnyPlugin);
ROCKSDB_REG_AnyPluginManip("GitInfoShower");

} // namespace rocksdb