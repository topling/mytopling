#include <topling/side_plugin_repo.h>
#include <topling/side_plugin_factory.h>
#include <rocksdb/db.h>
#include "sql/mysql_githash.h"

const char* git_version_hash_info_cspp_memtable();
const char* git_version_hash_info_cspp_wbwi();

namespace rocksdb {

void JS_TopTable_AddVersion(json& djs, bool html);
__attribute__((weak)) void JS_ZipTable_AddVersion(json& djs, bool html);

class GitInfoShower : public AnyPlugin {
public:
  const char* Name() const final { return "GitInfoShower"; }
  void Update(const json&, const json&, const SidePluginRepo&) override {}
  std::string ToString(const json& dump_options, const SidePluginRepo&)
  const override {
    json js, jtop, jzip;
    bool html = JsonSmartBool(dump_options, "html", true);

    if (JS_ZipTable_AddVersion) {
      JS_ZipTable_AddVersion(jzip, html);
    }
    JS_TopTable_AddVersion(jtop, html);

    std::string mysql_git_hash(MYSQL_GIT_HASH);
    js["mytopling"]["git_sha"] = html ?
        "<a href=\"https://github.com/topling/mytopling/commit/"
        + mysql_git_hash + "\">"
        + mysql_git_hash + "</a>"
        : mysql_git_hash;
    js["mytopling"]["git_date"] = MYSQL_GIT_DATE;

    js["toplingdb"] = jtop["version"]["rocksdb"];

    constexpr uint SHA_BEGIN_POS = 33;
    std::string git_cspp_mem =
        std::string(git_version_hash_info_cspp_memtable()).substr(SHA_BEGIN_POS);
    std::string git_cspp_wbwi =
        std::string(git_version_hash_info_cspp_wbwi()).substr(SHA_BEGIN_POS);
    if (html) {
      js["cspp-memtable"] = insert_html_link_and_symbol(
          git_cspp_mem, "https://github.com/topling/cspp-memtable/commit/");
      js["cspp-wbwi"] = insert_html_link_and_symbol(
          git_cspp_wbwi, "https://github.com/topling/cspp-wbwi/commit/");
    } else {
      js["cspp-memtable"] = git_cspp_mem;
      js["cspp-wbwi"] = git_cspp_wbwi;
    }
    js.merge_patch(jzip["version"]);
    js["topling-sst"] = jtop["version"]["topling-sst"];
    js["topling-zip"] = jtop["version"]["topling-zip"];

    return JsonToString(js, dump_options);
  }

private:
    std::string& insert_html_link_and_symbol(
        std::string& str, const std::string link) const {
        constexpr uint GIT_SHA_LEN = 40;
        str.append("</pre>");
        str.insert(GIT_SHA_LEN, "</a>");
        str.insert(0, "<pre><a href=\"" + link + str.substr(0, GIT_SHA_LEN) +"\">");
        return str;
    }
};

ROCKSDB_REG_DEFAULT_CONS(GitInfoShower, AnyPlugin);
ROCKSDB_REG_AnyPluginManip("GitInfoShower");

} // namespace rocksdb