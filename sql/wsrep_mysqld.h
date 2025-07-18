/* Copyright 2008-2023 Codership Oy <http://www.codership.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335 USA */

#ifndef WSREP_MYSQLD_H
#define WSREP_MYSQLD_H

#include <wsrep.h>

#ifdef WITH_WSREP

#include <mysql/plugin.h>
#include "mysql/service_wsrep.h"

#include <my_global.h>
#include <my_pthread.h>
#include "log.h"
#include "mysqld.h"

typedef struct st_mysql_show_var SHOW_VAR;
#include <sql_priv.h>
#include "mdl.h"
#include "sql_table.h"
#include "wsrep_mysqld_c.h"

#include "wsrep/provider.hpp"
#include "wsrep/streaming_context.hpp"
#include "wsrep_api.h"
#include <map>

#define WSREP_UNDEFINED_TRX_ID ULONGLONG_MAX

class THD;

// Global wsrep parameters

// MySQL wsrep options
extern const char* wsrep_provider;
extern const char* wsrep_provider_options;
extern const char* wsrep_cluster_name;
extern const char* wsrep_cluster_address;
extern const char* wsrep_node_name;
extern const char* wsrep_node_address;
extern const char* wsrep_node_incoming_address;
extern const char* wsrep_data_home_dir;
extern const char* wsrep_dbug_option;
extern long        wsrep_slave_threads;
extern int         wsrep_slave_count_change;
extern ulong       wsrep_debug;
extern my_bool     wsrep_convert_LOCK_to_trx;
extern ulong       wsrep_retry_autocommit;
extern my_bool     wsrep_auto_increment_control;
extern my_bool     wsrep_drupal_282555_workaround;
extern my_bool     wsrep_incremental_data_collection;
extern const char* wsrep_start_position;
extern ulong       wsrep_max_ws_size;
extern ulong       wsrep_max_ws_rows;
extern const char* wsrep_notify_cmd;
extern const char* wsrep_status_file;
extern const char* wsrep_allowlist;
extern my_bool     wsrep_certify_nonPK;
extern long int    wsrep_protocol_version;
extern my_bool     wsrep_desync;
extern ulong       wsrep_reject_queries;
extern my_bool     wsrep_recovery;
extern my_bool     wsrep_log_conflicts;
extern ulong       wsrep_mysql_replication_bundle;
extern my_bool     wsrep_load_data_splitting;
extern my_bool     wsrep_restart_slave;
extern my_bool     wsrep_restart_slave_activated;
extern my_bool     wsrep_slave_FK_checks;
extern my_bool     wsrep_slave_UK_checks;
extern ulong       wsrep_trx_fragment_unit;
extern ulong       wsrep_SR_store_type;
extern uint        wsrep_ignore_apply_errors;
extern ulong       wsrep_running_threads;
extern ulong       wsrep_running_applier_threads;
extern ulong       wsrep_running_rollbacker_threads;
extern bool        wsrep_new_cluster;
extern bool        wsrep_gtid_mode;
extern uint32      wsrep_gtid_domain_id;
extern std::atomic <bool > wsrep_thread_create_failed;
extern ulonglong   wsrep_mode;

enum enum_wsrep_reject_types {
  WSREP_REJECT_NONE,    /* nothing rejected */
  WSREP_REJECT_ALL,     /* reject all queries, with UNKNOWN_COMMAND error */
  WSREP_REJECT_ALL_KILL /* kill existing connections and reject all queries*/
};

enum enum_wsrep_OSU_method {
    WSREP_OSU_TOI,
    WSREP_OSU_RSU,
    WSREP_OSU_NONE,
};

enum enum_wsrep_sync_wait {
    WSREP_SYNC_WAIT_NONE= 0x0,
    // select, begin
    WSREP_SYNC_WAIT_BEFORE_READ= 0x1,
    WSREP_SYNC_WAIT_BEFORE_UPDATE_DELETE= 0x2,
    WSREP_SYNC_WAIT_BEFORE_INSERT_REPLACE= 0x4,
    WSREP_SYNC_WAIT_BEFORE_SHOW= 0x8,
    WSREP_SYNC_WAIT_MAX= 0xF
};

enum enum_wsrep_ignore_apply_error {
    WSREP_IGNORE_ERRORS_NONE= 0x0,
    WSREP_IGNORE_ERRORS_ON_RECONCILING_DDL= 0x1,
    WSREP_IGNORE_ERRORS_ON_RECONCILING_DML= 0x2,
    WSREP_IGNORE_ERRORS_ON_DDL= 0x4,
    WSREP_IGNORE_ERRORS_MAX= 0x7
};

/* wsrep_mode features */
enum enum_wsrep_mode {
  WSREP_MODE_STRICT_REPLICATION= (1ULL << 0),
  WSREP_MODE_BINLOG_ROW_FORMAT_ONLY= (1ULL << 1),
  WSREP_MODE_REQUIRED_PRIMARY_KEY= (1ULL << 2),
  WSREP_MODE_REPLICATE_MYISAM= (1ULL << 3),
  WSREP_MODE_REPLICATE_ARIA= (1ULL << 4),
  WSREP_MODE_DISALLOW_LOCAL_GTID= (1ULL << 5),
  WSREP_MODE_BF_MARIABACKUP= (1ULL << 6),
  WSREP_MODE_APPLIER_SKIP_FK_CHECKS_IN_IST= (1ULL << 7)
};

// Streaming Replication
#define WSREP_FRAG_BYTES      0
#define WSREP_FRAG_ROWS       1
#define WSREP_FRAG_STATEMENTS 2

#define WSREP_SR_STORE_NONE   0
#define WSREP_SR_STORE_TABLE  1

extern const char *wsrep_fragment_units[];
extern const char *wsrep_SR_store_types[];

// MySQL status variables
extern my_bool     wsrep_connected;
extern const char* wsrep_cluster_state_uuid;
extern long long   wsrep_cluster_conf_id;
extern const char* wsrep_cluster_status;
extern long        wsrep_cluster_size;
extern long        wsrep_local_index;
extern long long   wsrep_local_bf_aborts;
extern const char* wsrep_provider_name;
extern const char* wsrep_provider_version;
extern const char* wsrep_provider_vendor;
extern char*       wsrep_provider_capabilities;
extern char*       wsrep_cluster_capabilities;

int  wsrep_show_status(THD *thd, SHOW_VAR *var, void *buff,
                       system_status_var *status_var, enum_var_type scope);
int  wsrep_show_ready(THD *thd, SHOW_VAR *var, void *buff,
                      system_status_var *, enum_var_type);
void wsrep_free_status(THD *thd);
void wsrep_update_cluster_state_uuid(const char* str);

/* Filters out --wsrep-new-cluster option from argv[]
 * should be called in the very beginning of main() */
void wsrep_filter_new_cluster (int* argc, char* argv[]);

int  wsrep_init();
void wsrep_deinit(bool free_options);

/* Initialize wsrep thread LOCKs and CONDs */
void wsrep_thr_init();
/* Destroy wsrep thread LOCKs and CONDs */
void wsrep_thr_deinit();

void wsrep_recover();
bool wsrep_before_SE(); // initialize wsrep before storage
                        // engines (true) or after (false)
/* wsrep initialization sequence at startup
 * @param before wsrep_before_SE() value */
void wsrep_init_startup(bool before);

/* Recover streaming transactions from fragment storage */
void wsrep_recover_sr_from_storage(THD *);

// Other wsrep global variables
extern my_bool     wsrep_inited; // whether wsrep is initialized ?
extern bool        wsrep_service_started;

extern "C" void wsrep_fire_rollbacker(THD *thd);
extern "C" uint32 wsrep_thd_wsrep_rand(THD *thd);
extern "C" time_t wsrep_thd_query_start(THD *thd);
extern void wsrep_close_client_connections(my_bool wait_to_end,
                                           THD *except_caller_thd= NULL);
extern "C" query_id_t wsrep_thd_wsrep_last_query_id(THD *thd);
extern "C" void wsrep_thd_set_wsrep_last_query_id(THD *thd, query_id_t id);

extern int  wsrep_wait_committing_connections_close(int wait_time);
extern void wsrep_close_applier(THD *thd);
extern void wsrep_wait_appliers_close(THD *thd);
extern void wsrep_close_applier_threads(int count);


/* new defines */
extern void wsrep_stop_replication(THD *thd);
extern bool wsrep_start_replication(const char *wsrep_cluster_address);
extern void wsrep_shutdown_replication();
extern bool wsrep_check_mode (enum_wsrep_mode mask);
extern bool wsrep_check_mode_after_open_table (THD *thd, const handlerton *hton,
                                               TABLE_LIST *tables);
extern bool wsrep_check_mode_before_cmd_execute (THD *thd);
extern bool wsrep_must_sync_wait (THD* thd, uint mask= WSREP_SYNC_WAIT_BEFORE_READ);
extern bool wsrep_sync_wait (THD* thd, uint mask= WSREP_SYNC_WAIT_BEFORE_READ);
extern bool wsrep_sync_wait (THD* thd, enum enum_sql_command command);
extern enum wsrep::provider::status
wsrep_sync_wait_upto (THD* thd, wsrep_gtid_t* upto, int timeout);
extern int  wsrep_check_opts();
extern void wsrep_prepend_PATH (const char* path);
extern bool wsrep_append_fk_parent_table(THD* thd, TABLE_LIST* table, wsrep::key_array* keys);
extern bool wsrep_reload_ssl();
extern bool wsrep_split_allowlist(std::vector<std::string>& allowlist);

/* Other global variables */
extern wsrep_seqno_t wsrep_locked_seqno;

/* A wrapper function for MySQL log functions. The call will prefix
   the log message with WSREP and forward the result buffer to fun. */
void WSREP_LOG(void (*fun)(const char* fmt, ...), const char* fmt, ...);

#define WSREP_SYNC_WAIT(thd_, before_)                                  \
    { if (WSREP_CLIENT(thd_) &&                                         \
          wsrep_sync_wait(thd_, before_)) goto wsrep_error_label; }

#define WSREP_MYSQL_DB (char *)"mysql"

#define WSREP_TO_ISOLATION_BEGIN(db_, table_, table_list_)              \
  if (WSREP_ON && WSREP(thd) && wsrep_to_isolation_begin(thd, db_, table_, table_list_)) \
    goto wsrep_error_label;

#define WSREP_TO_ISOLATION_BEGIN_CREATE(db_, table_, table_list_, create_info_)	\
  if (WSREP_ON && WSREP(thd) &&                                         \
      wsrep_to_isolation_begin(thd, db_, table_,                        \
                               table_list_, nullptr, nullptr, create_info_))\
    goto wsrep_error_label;

#define WSREP_TO_ISOLATION_BEGIN_ALTER(db_, table_, table_list_, alter_info_, fk_tables_, create_info_) \
  if (WSREP(thd) && wsrep_thd_is_local(thd) &&                          \
      wsrep_to_isolation_begin(thd, db_, table_,                        \
                               table_list_, alter_info_, fk_tables_, create_info_))

#define WSREP_TO_ISOLATION_END                                          \
  if ((WSREP(thd) && wsrep_thd_is_local_toi(thd)) ||                    \
      wsrep_thd_is_in_rsu(thd))                                         \
    wsrep_to_isolation_end(thd);

/*
  Checks if lex->no_write_to_binlog is set for statements that use LOCAL or
  NO_WRITE_TO_BINLOG.
*/
#define WSREP_TO_ISOLATION_BEGIN_WRTCHK(db_, table_, table_list_)       \
  if (WSREP(thd) && !thd->lex->no_write_to_binlog                       \
      && wsrep_to_isolation_begin(thd, db_, table_, table_list_))       \
    goto wsrep_error_label;


#define WSREP_PROVIDER_EXISTS (WSREP_PROVIDER_EXISTS_)

static inline bool wsrep_cluster_address_exists()
{
  if (mysqld_server_started)
    mysql_mutex_assert_owner(&LOCK_global_system_variables);
  return wsrep_cluster_address && wsrep_cluster_address[0];
}

extern my_bool wsrep_ready_get();
extern void wsrep_ready_wait();

extern mysql_mutex_t LOCK_wsrep_ready;
extern mysql_cond_t  COND_wsrep_ready;
extern mysql_mutex_t LOCK_wsrep_sst;
extern mysql_cond_t  COND_wsrep_sst;
extern mysql_mutex_t LOCK_wsrep_sst_init;
extern mysql_cond_t  COND_wsrep_sst_init;
extern int wsrep_replaying;
extern mysql_mutex_t LOCK_wsrep_replaying;
extern mysql_cond_t  COND_wsrep_replaying;
extern mysql_mutex_t LOCK_wsrep_slave_threads;
extern mysql_cond_t  COND_wsrep_slave_threads;
extern mysql_mutex_t LOCK_wsrep_gtid_wait_upto;
extern mysql_mutex_t LOCK_wsrep_cluster_config;
extern mysql_mutex_t LOCK_wsrep_desync;
extern mysql_mutex_t LOCK_wsrep_SR_pool;
extern mysql_mutex_t LOCK_wsrep_SR_store;
extern mysql_mutex_t LOCK_wsrep_config_state;
extern mysql_mutex_t LOCK_wsrep_group_commit;
extern mysql_mutex_t LOCK_wsrep_joiner_monitor;
extern mysql_mutex_t LOCK_wsrep_donor_monitor;
extern mysql_cond_t  COND_wsrep_joiner_monitor;
extern mysql_cond_t  COND_wsrep_donor_monitor;

extern int           wsrep_to_isolation;
#ifdef GTID_SUPPORT
extern rpl_sidno     wsrep_sidno;
#endif /* GTID_SUPPORT */
extern my_bool       wsrep_preordered_opt;

#ifdef HAVE_PSI_INTERFACE

extern PSI_cond_key  key_COND_wsrep_thd;

extern PSI_mutex_key key_LOCK_wsrep_ready;
extern PSI_mutex_key key_COND_wsrep_ready;
extern PSI_mutex_key key_LOCK_wsrep_sst;
extern PSI_cond_key  key_COND_wsrep_sst;
extern PSI_mutex_key key_LOCK_wsrep_sst_init;
extern PSI_cond_key  key_COND_wsrep_sst_init;
extern PSI_mutex_key key_LOCK_wsrep_sst_thread;
extern PSI_cond_key  key_COND_wsrep_sst_thread;
extern PSI_mutex_key key_LOCK_wsrep_replaying;
extern PSI_cond_key  key_COND_wsrep_replaying;
extern PSI_mutex_key key_LOCK_wsrep_slave_threads;
extern PSI_cond_key  key_COND_wsrep_slave_threads;
extern PSI_mutex_key key_LOCK_wsrep_gtid_wait_upto;
extern PSI_cond_key  key_COND_wsrep_gtid_wait_upto;
extern PSI_mutex_key key_LOCK_wsrep_cluster_config;
extern PSI_mutex_key key_LOCK_wsrep_desync;
extern PSI_mutex_key key_LOCK_wsrep_SR_pool;
extern PSI_mutex_key key_LOCK_wsrep_SR_store;
extern PSI_mutex_key key_LOCK_wsrep_global_seqno;
extern PSI_mutex_key key_LOCK_wsrep_thd_queue;
extern PSI_cond_key  key_COND_wsrep_thd_queue;
extern PSI_mutex_key key_LOCK_wsrep_joiner_monitor;
extern PSI_mutex_key key_LOCK_wsrep_donor_monitor;

extern PSI_file_key key_file_wsrep_gra_log;

extern PSI_thread_key key_wsrep_sst_joiner;
extern PSI_thread_key key_wsrep_sst_donor;
extern PSI_thread_key key_wsrep_rollbacker;
extern PSI_thread_key key_wsrep_applier;
extern PSI_thread_key key_wsrep_sst_joiner_monitor;
extern PSI_thread_key key_wsrep_sst_donor_monitor;
#endif /* HAVE_PSI_INTERFACE */


struct TABLE_LIST;
class Alter_info;
int wsrep_to_isolation_begin(THD *thd, const char *db_, const char *table_,
                             const TABLE_LIST* table_list,
                             const Alter_info* alter_info= nullptr,
                             const wsrep::key_array *fk_tables= nullptr,
                             const HA_CREATE_INFO* create_info= nullptr);

bool wsrep_should_replicate_ddl(THD* thd, const handlerton *hton);
bool wsrep_should_replicate_ddl_iterate(THD* thd, const TABLE_LIST* table_list);

void wsrep_to_isolation_end(THD *thd);

bool wsrep_append_SR_keys(THD *thd);
int wsrep_to_buf_helper(
  THD* thd, const char *query, uint query_len, uchar** buf, size_t* buf_len);
int wsrep_create_trigger_query(THD *thd, uchar** buf, size_t* buf_len);
int wsrep_create_event_query(THD *thd, uchar** buf, size_t* buf_len);

void wsrep_init_sidno(const wsrep_uuid_t&);
bool wsrep_node_is_donor();
bool wsrep_node_is_synced();

void wsrep_init_SR();
void wsrep_verify_SE_checkpoint(const wsrep_uuid_t& uuid, wsrep_seqno_t seqno);
int wsrep_replay_from_SR_store(THD*, const wsrep_trx_meta_t&);

class Log_event;
int wsrep_ignored_error_code(Log_event* ev, int error);
int wsrep_must_ignore_error(THD* thd);

struct wsrep_server_gtid_t
{
  uint32 domain_id;
  uint32 server_id;
  uint64 seqno;
};
class Wsrep_gtid_server
{
public:
  uint32 domain_id;
  uint32 server_id;
  Wsrep_gtid_server()
    : m_force_signal(false)
    , m_seqno(0)
    , m_committed_seqno(0)
  { }
  void gtid(const wsrep_server_gtid_t& gtid)
  {
    domain_id=  gtid.domain_id;
    server_id=  gtid.server_id;
    m_seqno=    gtid.seqno;
  }
  wsrep_server_gtid_t gtid()
  {
    wsrep_server_gtid_t gtid;
    gtid.domain_id= domain_id;
    gtid.server_id= server_id;
    gtid.seqno=     m_seqno;
    return gtid;
  }
  void seqno(const uint64 seqno) { m_seqno= seqno; }
  uint64 seqno() const { return m_seqno; }
  uint64 seqno_committed() const { return m_committed_seqno; }
  uint64 seqno_inc()
  {
    m_seqno++;
    return m_seqno;
  }
  const wsrep_server_gtid_t& undefined()
  {
    return m_undefined;
  }
  int wait_gtid_upto(const uint64_t seqno, uint timeout)
  {
    int wait_result= 0;
    struct timespec wait_time;
    int ret= 0;
    mysql_cond_t wait_cond;
    mysql_cond_init(key_COND_wsrep_gtid_wait_upto, &wait_cond, NULL);
    set_timespec(wait_time, timeout);
    mysql_mutex_lock(&LOCK_wsrep_gtid_wait_upto);
    std::multimap<uint64, mysql_cond_t*>::iterator it;
    if (seqno > m_seqno)
    {
      try
      {
        it= m_wait_map.insert(std::make_pair(seqno, &wait_cond));
      } 
      catch (std::bad_alloc& e)
      {
         ret= ENOMEM;
      }
      while (!ret && (m_committed_seqno < seqno) && !m_force_signal)
      {
        wait_result= mysql_cond_timedwait(&wait_cond,
                                          &LOCK_wsrep_gtid_wait_upto,
                                          &wait_time);
        if (wait_result == ETIMEDOUT || wait_result == ETIME)
        {
          ret= wait_result;
          break;
        }
      }
      if (ret != ENOMEM)
      {
        m_wait_map.erase(it);
      }
    }
    mysql_mutex_unlock(&LOCK_wsrep_gtid_wait_upto);
    mysql_cond_destroy(&wait_cond);
    return ret;
  }
  void signal_waiters(uint64 seqno, bool signal_all)
  {
    mysql_mutex_lock(&LOCK_wsrep_gtid_wait_upto);
    if (!signal_all && (m_committed_seqno >= seqno))
    {
      mysql_mutex_unlock(&LOCK_wsrep_gtid_wait_upto);
      return;
    }
    m_force_signal= true;
    std::multimap<uint64, mysql_cond_t*>::iterator it_end;
    std::multimap<uint64, mysql_cond_t*>::iterator it_begin;
    if (signal_all)
    {
      it_end= m_wait_map.end();
    }
    else
    {
      it_end= m_wait_map.upper_bound(seqno);
    }
    if (m_committed_seqno < seqno)
    {
      m_committed_seqno= seqno;
    }
    for (it_begin = m_wait_map.begin(); it_begin != it_end; ++it_begin)
    {
      mysql_cond_signal(it_begin->second);
    }
    m_force_signal= false;
    mysql_mutex_unlock(&LOCK_wsrep_gtid_wait_upto);
  }
private:
  const wsrep_server_gtid_t m_undefined= {0,0,0};
  std::multimap<uint64, mysql_cond_t*> m_wait_map;
  bool m_force_signal;
  Atomic_counter<uint64_t> m_seqno;
  Atomic_counter<uint64_t> m_committed_seqno;
};
extern Wsrep_gtid_server wsrep_gtid_server;
void wsrep_init_gtid();
bool wsrep_check_gtid_seqno(const uint32&, const uint32&, uint64&);
bool wsrep_get_binlog_gtid_seqno(wsrep_server_gtid_t&);

int wsrep_append_table_keys(THD* thd,
                            TABLE_LIST* first_table,
                            TABLE_LIST* table_list,
                            Wsrep_service_key_type key_type);

extern void
wsrep_handle_mdl_conflict(MDL_context *requestor_ctx,
                          const MDL_ticket *ticket,
                          const MDL_key *key);

enum wsrep_thread_type {
  WSREP_APPLIER_THREAD=1,
  WSREP_ROLLBACKER_THREAD=2
};

typedef void (*wsrep_thd_processor_fun)(THD*, void *);
class Wsrep_thd_args
{
 public:
 Wsrep_thd_args(wsrep_thd_processor_fun fun,
                wsrep_thread_type thread_type,
                pthread_t thread_id)
   :
  fun_ (fun),
  thread_type_ (thread_type),
  thread_id_ (thread_id)
  { }

  wsrep_thd_processor_fun fun() { return fun_; }
  pthread_t* thread_id() {return &thread_id_; }
  enum wsrep_thread_type thread_type() {return thread_type_;}

 private:

  Wsrep_thd_args(const Wsrep_thd_args&);
  Wsrep_thd_args& operator=(const Wsrep_thd_args&);

  wsrep_thd_processor_fun fun_;
  enum wsrep_thread_type  thread_type_;
  pthread_t thread_id_;
};

void* start_wsrep_THD(void*);

void wsrep_close_threads(THD *thd);
bool wsrep_is_show_query(enum enum_sql_command command);
void wsrep_replay_transaction(THD *thd);
bool wsrep_create_like_table(THD* thd, TABLE_LIST* table,
                             TABLE_LIST* src_table,
                             HA_CREATE_INFO *create_info);
bool wsrep_node_is_donor();
bool wsrep_node_is_synced();

/**
 * Check if the wsrep provider (ie the Galera library) is capable of
 * doing streaming replication.
 * @return true if SR capable
 */
bool wsrep_provider_is_SR_capable();

/**
 * Initialize WSREP server instance.
 *
 * @return Zero on success, non-zero on error.
 */
int wsrep_init_server();

/**
 * Initialize WSREP globals. This should be done after server initialization
 * is complete and the server has joined to the cluster.
 *
 */
void wsrep_init_globals();

/**
 * Deinit and release WSREP resources.
 */
void wsrep_deinit_server();

/**
 * Convert streaming fragment unit (WSREP_FRAG_BYTES, WSREP_FRAG_ROWS...)
 * to corresponding wsrep-lib fragment_unit
 */
enum wsrep::streaming_context::fragment_unit wsrep_fragment_unit(ulong unit);

wsrep::key wsrep_prepare_key_for_toi(const char* db, const char* table,
                                     enum wsrep::key::type type);

void wsrep_wait_ready(THD *thd);
void wsrep_ready_set(bool ready_value);

/**
 * Returns true if the given list of tables contains at least one
 * non-temporary table.
 */
bool wsrep_table_list_has_non_temp_tables(THD *thd, TABLE_LIST *tables);

/**
 * Append foreign key to wsrep.
 *
 * @param thd           Thread object
 * @param fk            Foreign Key Info
 *
 * @return true if error, otherwise false.
 */
bool wsrep_foreign_key_append(THD *thd, FOREIGN_KEY_INFO *fk);

#else /* !WITH_WSREP */

/* These macros are needed to compile MariaDB without WSREP support
 * (e.g. embedded) */

#define WSREP_PROVIDER_EXISTS (0)
#define wsrep_emulate_bin_log (0)
#define wsrep_to_isolation (0)
#define wsrep_before_SE() (0)
#define wsrep_init_startup(X)
#define wsrep_check_opts() (0)
#define wsrep_thr_init() do {} while(0)
#define wsrep_thr_deinit() do {} while(0)
#define wsrep_init_globals() do {} while(0)
#define wsrep_create_appliers(X) do {} while(0)
#define wsrep_cluster_address_exists() (false)
#define WSREP_MYSQL_DB (0)
#define WSREP_TO_ISOLATION_BEGIN(db_, table_, table_list_) do { } while(0)
#define WSREP_TO_ISOLATION_BEGIN_ALTER(db_, table_, table_list_, alter_info_, fk_tables_)
#define WSREP_TO_ISOLATION_END
#define WSREP_TO_ISOLATION_BEGIN_WRTCHK(db_, table_, table_list_)
#define WSREP_SYNC_WAIT(thd_, before_)

#endif /* WITH_WSREP */

#endif /* WSREP_MYSQLD_H */
