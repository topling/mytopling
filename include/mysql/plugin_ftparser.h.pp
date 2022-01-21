#include "plugin.h"
#include "status_var.h"
enum enum_mysql_show_type {
  SHOW_UNDEF,
  SHOW_BOOL,
  SHOW_INT,
  SHOW_LONG,
  SHOW_LONGLONG,
  SHOW_CHAR,
  SHOW_CHAR_PTR,
  SHOW_ARRAY,
  SHOW_FUNC,
  SHOW_DOUBLE,
  SHOW_KEY_CACHE_LONG,
  SHOW_KEY_CACHE_LONGLONG,
  SHOW_LONG_STATUS,
  SHOW_DOUBLE_STATUS,
  SHOW_HAVE,
  SHOW_MY_BOOL,
  SHOW_HA_ROWS,
  SHOW_SYS,
  SHOW_LONG_NOFLUSH,
  SHOW_LONGLONG_STATUS,
  SHOW_LEX_STRING,
  SHOW_SIGNED_INT,
  SHOW_SIGNED_LONG,
  SHOW_SIGNED_LONGLONG,
  SHOW_TIMER,
  SHOW_TIMER_STATUS
};
enum enum_mysql_show_scope {
  SHOW_SCOPE_UNDEF,
  SHOW_SCOPE_GLOBAL,
  SHOW_SCOPE_SESSION,
  SHOW_SCOPE_ALL
};
struct SHOW_VAR {
  const char *name;
  char *value;
  enum enum_mysql_show_type type;
  enum enum_mysql_show_scope scope;
};
typedef int (*mysql_show_var_func)(void *, SHOW_VAR *, char *);
typedef void *MYSQL_PLUGIN;
struct MYSQL_XID {
  long formatID;
  long gtrid_length;
  long bqual_length;
  char data[128];
};
struct SYS_VAR;
struct st_mysql_value;
typedef int (*mysql_var_check_func)(void * thd, SYS_VAR *var, void *save,
                                    struct st_mysql_value *value);
typedef void (*mysql_var_update_func)(void * thd, SYS_VAR *var,
                                      void *var_ptr, const void *save);
struct st_mysql_plugin {
  int type;
  void *info;
  const char *name;
  const char *author;
  const char *descr;
  int license;
  int (*init)(MYSQL_PLUGIN);
  int (*check_uninstall)(MYSQL_PLUGIN);
  int (*deinit)(MYSQL_PLUGIN);
  unsigned int version;
  SHOW_VAR *status_vars;
  SYS_VAR **system_vars;
  void *__reserved1;
  unsigned long flags;
};
struct st_mysql_daemon {
  int interface_version;
};
struct st_mysql_information_schema {
  int interface_version;
};
struct st_mysql_storage_engine {
  int interface_version;
};
struct handlerton;
struct Mysql_replication {
  int interface_version;
};
struct st_mysql_value {
  int (*value_type)(struct st_mysql_value *);
  const char *(*val_str)(struct st_mysql_value *, char *buffer, int *length);
  int (*val_real)(struct st_mysql_value *, double *realbuf);
  int (*val_int)(struct st_mysql_value *, long long *intbuf);
  int (*is_unsigned)(struct st_mysql_value *);
};
int thd_in_lock_tables(const void * thd);
int thd_tablespace_op(const void * thd);
long long thd_test_options(const void * thd, long long test_options);
int thd_sql_command(const void * thd);
const char *set_thd_proc_info(void * thd, const char *info,
                              const char *calling_func,
                              const char *calling_file,
                              const unsigned int calling_line);
void **thd_ha_data(const void * thd, const struct handlerton *hton);
void thd_storage_lock_wait(void * thd, long long value);
int thd_tx_isolation(const void * thd);
int thd_tx_is_read_only(const void * thd);
void * thd_tx_arbitrate(void * requestor, void * holder);
int thd_tx_priority(const void * thd);
int thd_tx_is_dd_trx(const void * thd);
char *thd_security_context(void * thd, char *buffer, size_t length,
                           size_t max_query_len);
void thd_inc_row_count(void * thd);
int thd_allow_batch(void * thd);
void thd_mark_transaction_to_rollback(void * thd, int all);
int mysql_tmpfile(const char *prefix);
int thd_killed(const void *v_thd);
void thd_set_kill_status(const void * thd);
void thd_binlog_pos(const void * thd, const char **file_var,
                    unsigned long long *pos_var, const char **gtid_var,
					const char **max_gtid_var);
unsigned long thd_get_thread_id(const void * thd);
void thd_get_xid(const void * thd, MYSQL_XID *xid);
void *thd_get_ha_data(const void * thd, const struct handlerton *hton);
void thd_set_ha_data(void * thd, const struct handlerton *hton,
                     const void *ha_data);
void remove_ssl_err_thread_state();
unsigned int thd_get_num_vcpus();
char mysql_bin_log_is_open(void);
void mysql_bin_log_lock_commits(void);
void mysql_bin_log_unlock_commits(char *binlog_file,
                                  unsigned long long *binlog_pos,
                                  char **gtid_executed,
                                  int *gtid_executed_length,
                                  unsigned long long *snapshot_hlc);
enum enum_ftparser_mode {
  MYSQL_FTPARSER_SIMPLE_MODE = 0,
  MYSQL_FTPARSER_WITH_STOPWORDS = 1,
  MYSQL_FTPARSER_FULL_BOOLEAN_INFO = 2
};
enum enum_ft_token_type {
  FT_TOKEN_EOF = 0,
  FT_TOKEN_WORD = 1,
  FT_TOKEN_LEFT_PAREN = 2,
  FT_TOKEN_RIGHT_PAREN = 3,
  FT_TOKEN_STOPWORD = 4
};
struct MYSQL_FTPARSER_BOOLEAN_INFO {
  enum enum_ft_token_type type;
  int yesno;
  int weight_adjust;
  char wasign;
  char trunc;
  int position;
  char prev;
  char *quot;
};
struct MYSQL_FTPARSER_PARAM {
  int (*mysql_parse)(MYSQL_FTPARSER_PARAM *, char *doc, int doc_len);
  int (*mysql_add_word)(MYSQL_FTPARSER_PARAM *, char *word, int word_len,
                        MYSQL_FTPARSER_BOOLEAN_INFO *boolean_info);
  void *ftparser_state;
  void *mysql_ftparam;
  const CHARSET_INFO *cs;
  char *doc;
  int length;
  int flags;
  enum enum_ftparser_mode mode;
};
struct st_mysql_ftparser {
  int interface_version;
  int (*parse)(MYSQL_FTPARSER_PARAM *param);
  int (*init)(MYSQL_FTPARSER_PARAM *param);
  int (*deinit)(MYSQL_FTPARSER_PARAM *param);
};
