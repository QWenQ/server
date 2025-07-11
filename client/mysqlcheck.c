/*
   Copyright (c) 2001, 2013, Oracle and/or its affiliates.
   Copyright (c) 2010, 2024, MariaDB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1335  USA
*/

/* By Jani Tolonen, 2001-04-20, MySQL Development Team */

#define VER "2.8"

#include "client_priv.h"
#include <m_ctype.h>
#include <mysql_version.h>
#include <mysqld_error.h>
#include <sslopt-vars.h>
#include <welcome_copyright_notice.h> /* ORACLE_WELCOME_COPYRIGHT_NOTICE */

/* Exit codes */

#define EX_USAGE 1
#define EX_MYSQLERR 2

/* ALTER instead of repair. */
#define MAX_ALTER_STR_SIZE 128 * 1024
#define KEY_PARTITIONING_CHANGED_STR "KEY () partitioning changed"

static MYSQL mysql_connection, *sock = 0;
static my_bool opt_alldbs = 0, opt_check_only_changed = 0, opt_extended = 0,
               opt_compress = 0, opt_databases = 0, opt_fast = 0,
               opt_medium_check = 0, opt_quick = 0, opt_all_in_1 = 0,
               opt_silent = 0, opt_auto_repair = 0, ignore_errors = 0,
               tty_password= 0, opt_frm= 0, debug_info_flag= 0, debug_check_flag= 0,
               opt_fix_table_names= 0, opt_fix_db_names= 0, opt_upgrade= 0,
               opt_persistent_all= 0, opt_do_tables= 1;
static my_bool opt_write_binlog= 1, opt_flush_tables= 0;
static uint verbose = 0, opt_mysql_port=0;
static int my_end_arg;
static char * opt_mysql_unix_port = 0;
static char *opt_password = 0, *current_user = 0, 
	    *default_charset= 0, *current_host= 0;
static char *opt_plugin_dir= 0, *opt_default_auth= 0;
static int first_error = 0;
static char *opt_skip_database;
DYNAMIC_ARRAY tables4repair, tables4rebuild, alter_table_cmds;
DYNAMIC_ARRAY views4repair;
static uint opt_protocol=0;

enum operations { DO_CHECK=1, DO_REPAIR, DO_ANALYZE, DO_OPTIMIZE, DO_FIX_NAMES };
const char *operation_name[]=
{
  "???", "check", "repair", "analyze", "optimize", "fix names"
};

typedef enum { DO_VIEWS_NO, DO_VIEWS_YES, DO_UPGRADE, DO_VIEWS_FROM_MYSQL } enum_do_views;
const char *do_views_opts[]= {"NO", "YES", "UPGRADE", "UPGRADE_FROM_MYSQL",
  NullS};
TYPELIB do_views_typelib= CREATE_TYPELIB_FOR(do_views_opts);
static ulong opt_do_views= DO_VIEWS_NO;

static struct my_option my_long_options[] =
{
  {"all-databases", 'A',
   "Check all the databases. This is the same as --databases with all databases selected.",
   &opt_alldbs, &opt_alldbs, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"analyze", 'a', "Analyze given tables.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"all-in-1", '1',
   "Instead of issuing one query for each table, use one query per database, naming all tables in the database in a comma-separated list.",
   &opt_all_in_1, &opt_all_in_1, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"auto-repair", 0,
   "If a checked table is corrupted, automatically fix it. Repairing will be done after all tables have been checked, if corrupted ones were found.",
   &opt_auto_repair, &opt_auto_repair, 0, GET_BOOL, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"character-sets-dir", 0,
   "Directory for character set files.", (char**) &charsets_dir,
   (char**) &charsets_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"check", 'c', "Check table for errors.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"check-only-changed", 'C',
   "Check only tables that have changed since last check or haven't been closed properly.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"check-upgrade", 'g',
   "Check tables for version-dependent changes. May be used with --auto-repair to correct tables requiring version-dependent updates.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"compress", 0, "Use compression in server/client protocol.",
   &opt_compress, &opt_compress, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"databases", 'B',
   "Check several databases. Note the difference in usage; in this case no tables are given. All name arguments are regarded as database names.",
   &opt_databases, &opt_databases, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
#ifdef DBUG_OFF
  {"debug", '#', "This is a non-debug version. Catch this and exit.",
   0, 0, 0, GET_DISABLED, OPT_ARG, 0, 0, 0, 0, 0, 0},
#else
  {"debug", '#', "Output debug log. Often this is 'd:t:o,filename'.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"debug-check", 0, "Check memory and open file usage at exit.",
   &debug_check_flag, &debug_check_flag, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"debug-info", 0, "Print some debug info at exit.",
   &debug_info_flag, &debug_info_flag,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"default-character-set", 0,
   "Set the default character set.", &default_charset,
   &default_charset, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"default_auth", 0,
   "Default authentication client-side plugin to use.",
   &opt_default_auth, &opt_default_auth, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"fast",'F', "Check only tables that haven't been closed properly.",
   &opt_fast, &opt_fast, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
  {"fix-db-names", OPT_FIX_DB_NAMES, "Fix database names.",
    &opt_fix_db_names, &opt_fix_db_names,
    0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"fix-table-names", OPT_FIX_TABLE_NAMES, "Fix table names.",
    &opt_fix_table_names, &opt_fix_table_names,
    0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"force", 'f', "Continue even if we get an SQL error.",
   &ignore_errors, &ignore_errors, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"extended", 'e',
   "If you are using this option with CHECK TABLE, it will ensure that the table is 100 percent consistent, but will take a long time. If you are using this option with REPAIR TABLE, it will force using old slow repair with keycache method, instead of much faster repair by sorting.",
   &opt_extended, &opt_extended, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"flush", 0, "Flush each table after check. This is useful if you don't want to have the checked tables take up space in the caches after the check",
   &opt_flush_tables, &opt_flush_tables, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0 },
  {"help", '?', "Display this help message and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"host",'h', "Connect to host. Defaults in the following order: "
  "$MARIADB_HOST, and then localhost",
   &current_host, &current_host, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"medium-check", 'm',
   "Faster than extended-check, but only finds 99.99 percent of all errors. Should be good enough for most cases.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"write-binlog", 0,
   "Log ANALYZE, OPTIMIZE and REPAIR TABLE commands. Use --skip-write-binlog "
   "when commands should not be sent to replication slaves.",
   &opt_write_binlog, &opt_write_binlog, 0, GET_BOOL, NO_ARG,
   1, 0, 0, 0, 0, 0},
  {"optimize", 'o', "Optimize table.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"password", 'p',
   "Password to use when connecting to server. If password is not given, it's solicited on the tty.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"persistent", 'Z',
   "When using ANALYZE TABLE use the PERSISTENT FOR ALL option.",
   &opt_persistent_all, &opt_persistent_all, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
#ifdef _WIN32
  {"pipe", 'W', "Use named pipes to connect to server.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"plugin_dir", 0, "Directory for client-side plugins.",
   &opt_plugin_dir, &opt_plugin_dir, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', "Port number to use for connection or 0 for default to, in "
   "order of preference, my.cnf, $MYSQL_TCP_PORT, "
#if MYSQL_PORT_DEFAULT == 0
   "/etc/services, "
#endif
   "built-in default (" STRINGIFY_ARG(MYSQL_PORT) ").",
   &opt_mysql_port, &opt_mysql_port, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0,
   0},
  {"protocol", OPT_MYSQL_PROTOCOL, "The protocol to use for connection (tcp, socket, pipe).",
   0, 0, 0, GET_STR,  REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"quick", 'q',
   "If you are using this option with CHECK TABLE, it prevents the check from scanning the rows to check for wrong links. This is the fastest check. If you are using this option with REPAIR TABLE, it will try to repair only the index tree. This is the fastest repair method for a table.",
   &opt_quick, &opt_quick, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
  {"repair", 'r',
   "Can fix almost anything except unique keys that aren't unique.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"silent", 's', "Print only error messages.", &opt_silent,
   &opt_silent, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip_database", 0, "Don't process the database specified as argument", 
   &opt_skip_database, &opt_skip_database, 0, GET_STR, REQUIRED_ARG, 
   0, 0, 0, 0, 0, 0},
  {"socket", 'S', "The socket file to use for connection.",
   &opt_mysql_unix_port, &opt_mysql_unix_port, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#include <sslopt-longopts.h>
  {"tables", OPT_TABLES, "Overrides option --databases (-B).", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"use-frm", 0,
   "When used with REPAIR, get table structure from .frm file, so the table can be repaired even if .MYI header is corrupted.",
   &opt_frm, &opt_frm, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
#ifndef DONT_ALLOW_USER_CHANGE
  {"user", 'u', "User for login if not current user.", &current_user,
   &current_user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"verbose", 'v', "Print info about the various stages; Using it 3 times will print out all CHECK, RENAME and ALTER TABLE during the check phase.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"process-views", 0,
   "Perform the requested operation (check or repair) on views. "
   "One of: NO, YES (correct the checksum, if necessary, add the "
   "mariadb-version field), UPGRADE (run from mariadb-upgrade), "
   "UPGRADE_FROM_MYSQL (same as YES and toggle the algorithm "
   "MERGE<->TEMPTABLE.", &opt_do_views, &opt_do_views,
   &do_views_typelib, GET_ENUM, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"process-tables", 0, "Perform the requested operation on tables.",
   &opt_do_tables, &opt_do_tables, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static const char *load_default_groups[]=
{ "mysqlcheck", "mariadb-check", "client", "client-server", "client-mariadb",
  0 };


static void usage(void);
static int get_options(int *argc, char ***argv);
static int process_all_databases();
static int process_databases(char **db_names);
static int process_selected_tables(char *db, char **table_names, int tables);
static int process_all_tables_in_db(char *database);
static int process_one_db(char *database);
static int use_db(char *database);
static int handle_request_for_tables(char *, size_t, my_bool, my_bool);
static int dbConnect(char *host, char *user,char *passwd);
static void dbDisconnect(char *host);
static void DBerror(MYSQL *mysql, const char *when);
static void safe_exit(int error);
static void print_result();
static size_t fixed_name_length(const char *name);
static char *fix_table_name(char *dest, char *src);
int what_to_do = 0;


static inline int cmp_database(const char *a, const char *b)
{
  return my_strcasecmp_latin1(a, b);
}


static void usage(void)
{
  DBUG_ENTER("usage");
  print_version();
  puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2000"));
  puts("This program can be used to CHECK (-c, -m, -C), REPAIR (-r), ANALYZE (-a),");
  puts("or OPTIMIZE (-o) tables. Some of the options (like -e or -q) can be");
  puts("used at the same time. Not all options are supported by all storage engines.");
  puts("The options -c, -r, -a, and -o are exclusive to each other, which");
  puts("means that the last option will be used, if several was specified.\n");
  puts("The option -c (--check) will be used by default, if none was specified.");
  puts("You can change the default behavior by making a symbolic link, or");
  puts("copying this file somewhere with another name, the alternatives are:");
  puts("mysqlrepair:   The default option will be -r");
  puts("mysqlanalyze:  The default option will be -a");
  puts("mysqloptimize: The default option will be -o\n");
  printf("Usage: %s [OPTIONS] database [tables]\n", my_progname);
  printf("OR     %s [OPTIONS] --databases DB1 [DB2 DB3...]\n",
	 my_progname);
  puts("Please consult the MariaDB Knowledge Base at");
  puts("https://mariadb.com/kb/en/mysqlcheck for latest information about");
  puts("this program.");
  print_defaults("my", load_default_groups);
  puts("");
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
  DBUG_VOID_RETURN;
} /* usage */


static my_bool
get_one_option(const struct my_option *opt,
	       const char *argument,
               const char *filename)
{
  int orig_what_to_do= what_to_do;
  DBUG_ENTER("get_one_option");

  switch(opt->id) {
  case 'a':
    what_to_do = DO_ANALYZE;
    break;
  case 'c':
    what_to_do = DO_CHECK;
    break;
  case 'C':
    what_to_do = DO_CHECK;
    opt_check_only_changed = 1;
    break;
  case 'I': /* Fall through */
  case '?':
    usage();
    exit(0);
  case 'm':
    what_to_do = DO_CHECK;
    opt_medium_check = 1;
    break;
  case 'o':
    what_to_do = DO_OPTIMIZE;
    break;
  case OPT_FIX_DB_NAMES:
    what_to_do= DO_FIX_NAMES;
    opt_databases= 1;
    break;
  case OPT_FIX_TABLE_NAMES:
    what_to_do= DO_FIX_NAMES;
    break;
  case 'p':
    if (argument == disabled_my_option)
      argument= (char*) "";			/* Don't require password */
    if (argument)
    {
      /*
        One should not really change the argument, but we make an
        exception for passwords
      */
      char *start= (char*) argument;
      my_free(opt_password);
      opt_password = my_strdup(PSI_NOT_INSTRUMENTED, argument, MYF(MY_FAE));
      while (*argument)
        *(char*) argument++= 'x';		/* Destroy argument */
      if (*start)
	start[1] = 0;                             /* Cut length of argument */
      tty_password= 0;
    }
    else
      tty_password = 1;
    break;
  case 'r':
    what_to_do = DO_REPAIR;
    break;
  case 'g':
    what_to_do= DO_CHECK;
    opt_upgrade= 1;
    break;
  case 'W':
#ifdef _WIN32
    opt_protocol = MYSQL_PROTOCOL_PIPE;
#endif
    break;
  case '#':
    DBUG_PUSH(argument ? argument : "d:t:o");
    debug_check_flag= 1;
    break;
#include <sslopt-case.h>
  case OPT_TABLES:
    opt_databases = 0;
    break;
  case 'v':
    verbose++;
    break;
  case 'V':
    print_version(); exit(0);
    break;
  case OPT_MYSQL_PROTOCOL:
    if ((opt_protocol= find_type_with_warning(argument, &sql_protocol_typelib,
                                              opt->name)) <= 0)
    {
      sf_leaking_memory= 1; /* no memory leak reports here */
      exit(1);
    }
    break;
  case 'P':
    if (filename[0] == '\0')
    {
      /* Port given on command line, switch protocol to use TCP */
      opt_protocol= MYSQL_PROTOCOL_TCP;
    }
    break;
  case 'S':
    if (filename[0] == '\0')
    {
      /*
        Socket given on command line, switch protocol to use SOCKETSt
        Except on Windows if 'protocol= pipe' has been provided in
        the config file or command line.
      */
      if (opt_protocol != MYSQL_PROTOCOL_PIPE)
      {
        opt_protocol= MYSQL_PROTOCOL_SOCKET;
      }
    }
    break;
  }

  if (orig_what_to_do && (what_to_do != orig_what_to_do))
  {
    fprintf(stderr, "Error: %s doesn't support multiple contradicting commands.\n",
            my_progname);
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


static int get_options(int *argc, char ***argv)
{
  int ho_error;
  DBUG_ENTER("get_options");

  if (current_host == NULL)
    current_host= getenv("MARIADB_HOST");

  if (*argc == 1)
  {
    usage();
    exit(0);
  }

  if ((ho_error=handle_options(argc, argv, my_long_options, get_one_option)))
    exit(ho_error);

  if (what_to_do == DO_REPAIR && !opt_do_views && !opt_do_tables)
  {
    fprintf(stderr, "Error: Nothing to repair when both "
            "--process-tables=NO and --process-views=NO\n");
    exit(1);
  }
  if (!what_to_do)
  {
    size_t pnlen= strlen(my_progname);

    if (pnlen < 6) /* name too short */
      what_to_do = DO_CHECK;
    else if (!strcmp("repair", my_progname + pnlen - 6))
      what_to_do = DO_REPAIR;
    else if (!strcmp("analyze", my_progname + pnlen - 7))
      what_to_do = DO_ANALYZE;
    else if  (!strcmp("optimize", my_progname + pnlen - 8))
      what_to_do = DO_OPTIMIZE;
    else
      what_to_do = DO_CHECK;
  }

  if (opt_do_views && what_to_do != DO_REPAIR && what_to_do != DO_CHECK)
  {
    fprintf(stderr, "Error: %s doesn't support %s for views.\n",
            my_progname, operation_name[what_to_do]);
    exit(1);
  }

  /*
    If there's no --default-character-set option given with
    --fix-table-name or --fix-db-name set the default character set to "utf8".
  */
  if (!default_charset)
  {
    if (opt_fix_db_names || opt_fix_table_names)
      default_charset= (char*) "utf8";
    else
      default_charset= (char*) MYSQL_AUTODETECT_CHARSET_NAME;
  }
  if (!strcmp(default_charset, MYSQL_AUTODETECT_CHARSET_NAME))
    default_charset= (char *)my_default_csname();

  if (!get_charset_by_csname(default_charset, MY_CS_PRIMARY,
                             MYF(MY_UTF8_IS_UTF8MB3 | MY_WME)))
  {
    printf("Unsupported character set: %s\n", default_charset);
    DBUG_RETURN(1);
  }
  my_set_console_cp(default_charset);
  if (*argc > 0 && opt_alldbs)
  {
    printf("You should give only options, no arguments at all, with option\n");
    printf("--all-databases. Please see %s --help for more information.\n",
	   my_progname);
    DBUG_RETURN(1);
  }
  if (*argc < 1 && !opt_alldbs)
  {
    printf("You forgot to give the arguments! Please see %s --help\n",
	   my_progname);
    printf("for more information.\n");
    DBUG_RETURN(1);
  }
  if (tty_password)
    opt_password = my_get_tty_password(NullS);
  if (debug_info_flag)
    my_end_arg= MY_CHECK_ERROR | MY_GIVE_INFO;
  if (debug_check_flag)
    my_end_arg= MY_CHECK_ERROR;
  DBUG_RETURN((0));
} /* get_options */


static int process_all_databases()
{
  MYSQL_ROW row;
  MYSQL_RES *tableres;
  int result = 0;
  DBUG_ENTER("process_all_databases");

  if (mysql_query(sock, "SHOW DATABASES") ||
      !(tableres = mysql_store_result(sock)))
  {
    my_printf_error(0, "Error: Couldn't execute 'SHOW DATABASES': %s",
		    MYF(0), mysql_error(sock));
    DBUG_RETURN(1);
  }
  if (verbose)
    printf("Processing databases\n");
  while ((row = mysql_fetch_row(tableres)))
  {
    if (process_one_db(row[0]))
      result = 1;
  }
  mysql_free_result(tableres);
  DBUG_RETURN(result);
}
/* process_all_databases */


static int process_databases(char **db_names)
{
  int result = 0;
  DBUG_ENTER("process_databases");

  if (verbose)
    printf("Processing databases\n");
  for ( ; *db_names ; db_names++)
  {
    if (process_one_db(*db_names))
      result = 1;
  }
  DBUG_RETURN(result);
} /* process_databases */


/* returns: -1 for error, 1 for view, 0 for table */
static int is_view(const char *table)
{
  char query[1024];
  MYSQL_RES *res;
  MYSQL_FIELD *field;
  int view;
  DBUG_ENTER("is_view");

  my_snprintf(query, sizeof(query), "SHOW CREATE TABLE %sQ", table);
  if (mysql_query(sock, query))
  {
    fprintf(stderr, "Failed to %s\n", query);
    fprintf(stderr, "Error: %s\n", mysql_error(sock));
    DBUG_RETURN(-1);
  }
  res= mysql_store_result(sock);
  field= mysql_fetch_field(res);
  view= (strcmp(field->name,"View") == 0) ? 1 : 0;
  mysql_free_result(res);

  DBUG_RETURN(view);
}

static int process_selected_tables(char *db, char **table_names, int tables)
{
  int view;
  char *table;
  size_t table_len;
  DBUG_ENTER("process_selected_tables");

  if (use_db(db))
    DBUG_RETURN(1);
  if (opt_all_in_1 && what_to_do != DO_FIX_NAMES)
  {
    /* 
      We need table list in form `a`, `b`, `c`
      that's why we need 2 more chars added to to each table name
      space is for more readable output in logs and in case of error
    */	  
    char *table_names_comma_sep, *end;
    size_t tot_length= 0;
    int             i= 0;

    if (opt_do_tables && opt_do_views)
    {
      fprintf(stderr, "Error: %s cannot process both tables and views "
                      "in one command (--process-tables=YES "
                      "--process-views=YES --all-in-1).\n",
              my_progname);
      DBUG_RETURN(1);
    }

    for (i = 0; i < tables; i++)
      tot_length+= fixed_name_length(*(table_names + i)) + 2;

    if (!(table_names_comma_sep = (char *)
	  my_malloc(PSI_NOT_INSTRUMENTED, tot_length + 4, MYF(MY_WME))))
      DBUG_RETURN(1);

    for (end = table_names_comma_sep + 1; tables > 0;
	 tables--, table_names++)
    {
      end= fix_table_name(end, *table_names);
      *end++= ',';
    }
    *--end = 0;
    handle_request_for_tables(table_names_comma_sep + 1, tot_length - 1,
                              opt_do_views != 0, opt_all_in_1);
    my_free(table_names_comma_sep);
  }
  else
  {
    for (; tables > 0; tables--, table_names++)
    {
      table= *table_names;
      table_len= fixed_name_length(*table_names);
      view= is_view(table);
      if (view < 0)
        continue;
      handle_request_for_tables(table, table_len, view == 1, opt_all_in_1);
    }
  }
  DBUG_RETURN(0);
} /* process_selected_tables */


static size_t fixed_name_length(const char *name)
{
  const char *p;
  size_t extra_length= 2;  /* count the first/last backticks */
  DBUG_ENTER("fixed_name_length");

  for (p= name; *p; p++)
  {
    if (*p == '`')
      extra_length++;
  }
  DBUG_RETURN((size_t) ((p - name) + extra_length));
}


static char *fix_table_name(char *dest, char *src)
{
  DBUG_ENTER("fix_table_name");

  *dest++= '`';
  for (; *src; src++)
  {
    if (*src == '`')
      *dest++= '`';
    *dest++= *src;
  }
  *dest++= '`';

  DBUG_RETURN(dest);
}


static int process_all_tables_in_db(char *database)
{
  MYSQL_RES *UNINIT_VAR(res);
  MYSQL_ROW row;
  uint num_columns;
  my_bool system_database= 0;
  my_bool view= FALSE;
  DBUG_ENTER("process_all_tables_in_db");

  if (use_db(database))
    DBUG_RETURN(1);
  if ((mysql_query(sock, "SHOW /*!50002 FULL*/ TABLES") &&
       mysql_query(sock, "SHOW TABLES")) ||
      !(res= mysql_store_result(sock)))
  {
    my_printf_error(0, "Error: Couldn't get table list for database %s: %s",
		    MYF(0), database, mysql_error(sock));
    DBUG_RETURN(1);
  }

  if (!strcmp(database, "mysql") || !strcmp(database, "MYSQL"))
    system_database= 1;

  num_columns= mysql_num_fields(res);

  if (opt_all_in_1 && what_to_do != DO_FIX_NAMES)
  {
    /*
      We need table list in form `a`, `b`, `c`
      that's why we need 2 more chars added to to each table name
      space is for more readable output in logs and in case of error
     */

    char *tables, *end;
    size_t tot_length = 0;

    char *views, *views_end;
    size_t tot_views_length = 0;

    while ((row = mysql_fetch_row(res)))
    {
      if ((num_columns == 2) && (strcmp(row[1], "VIEW") == 0) &&
          opt_do_views)
        tot_views_length+= fixed_name_length(row[0]) + 2;
      else if (opt_do_tables)
        tot_length+= fixed_name_length(row[0]) + 2;
    }
    mysql_data_seek(res, 0);

    if (!(tables=(char *) my_malloc(PSI_NOT_INSTRUMENTED, tot_length+4, MYF(MY_WME))))
    {
      mysql_free_result(res);
      DBUG_RETURN(1);
    }
    if (!(views=(char *) my_malloc(PSI_NOT_INSTRUMENTED, tot_views_length+4, MYF(MY_WME))))
    {
      my_free(tables);
      mysql_free_result(res);
      DBUG_RETURN(1);
    }

    for (end = tables + 1, views_end= views + 1; (row = mysql_fetch_row(res)) ;)
    {
      if ((num_columns == 2) && (strcmp(row[1], "VIEW") == 0))
      {
        if (!opt_do_views)
          continue;
        views_end= fix_table_name(views_end, row[0]);
        *views_end++= ',';
      }
      else
      {
        if (!opt_do_tables)
          continue;
        end= fix_table_name(end, row[0]);
        *end++= ',';
      }
    }
    *--end = 0;
    *--views_end = 0;
    if (tot_length)
      handle_request_for_tables(tables + 1, tot_length - 1, FALSE, opt_all_in_1);
    if (tot_views_length)
      handle_request_for_tables(views + 1, tot_views_length - 1, TRUE, opt_all_in_1);
    my_free(tables);
    my_free(views);
  }
  else
  {
    while ((row = mysql_fetch_row(res)))
    {
      /* Skip views if we don't perform renaming. */
      if ((what_to_do != DO_FIX_NAMES) && (num_columns == 2) && (strcmp(row[1], "VIEW") == 0))
      {
        if (!opt_do_views)
          continue;
        view= TRUE;
      }
      else
      {
        if (!opt_do_tables)
          continue;
        view= FALSE;
      }
      if (system_database &&
          (!strcmp(row[0], "general_log") ||
           !strcmp(row[0], "slow_log")))
        continue;                               /* Skip logging tables */

      handle_request_for_tables(row[0], fixed_name_length(row[0]), view, opt_all_in_1);
    }
  }
  mysql_free_result(res);
  DBUG_RETURN(0);
} /* process_all_tables_in_db */


static int run_query(const char *query, my_bool log_query)
{
  if (verbose >=3 && log_query)
    puts(query);
  if (mysql_query(sock, query))
  {
    fprintf(stderr, "Failed to %s\n", query);
    fprintf(stderr, "Error: %s\n", mysql_error(sock));
    return 1;
  }
  return 0;
}


static int fix_table_storage_name(const char *name)
{
  char qbuf[100 + NAME_LEN*4];
  int rc= 0;
  DBUG_ENTER("fix_table_storage_name");

  if (strncmp(name, "#mysql50#", 9))
    DBUG_RETURN(1);
  my_snprintf(qbuf, sizeof(qbuf), "RENAME TABLE %sQ TO %sQ",
              name, name + 9);

  rc= run_query(qbuf, 1);
  if (!opt_silent)
    printf("%-50s %s\n", name, rc ? "FAILED" : "OK");
  DBUG_RETURN(rc);
}

static int fix_database_storage_name(const char *name)
{
  char qbuf[100 + NAME_LEN*4];
  int rc= 0;
  DBUG_ENTER("fix_database_storage_name");

  if (strncmp(name, "#mysql50#", 9))
    DBUG_RETURN(1);
  my_snprintf(qbuf, sizeof(qbuf), "ALTER DATABASE %sQ UPGRADE DATA DIRECTORY "
              "NAME", name);
  rc= run_query(qbuf, 1);
  if (!opt_silent)
    printf("%-50s %s\n", name, rc ? "FAILED" : "OK");
  DBUG_RETURN(rc);
}

static int rebuild_table(char *name)
{
  char *query, *ptr;
  int rc= 0;
  DBUG_ENTER("rebuild_table");

  query= (char*)my_malloc(PSI_NOT_INSTRUMENTED, 12+strlen(name)+6+1, MYF(MY_WME));
  if (!query)
    DBUG_RETURN(1);
  ptr= strxmov(query, "ALTER TABLE ", name, " FORCE", NullS);
  if (verbose >= 3)
    puts(query);
  if (mysql_real_query(sock, query, (ulong)(ptr - query)))
  {
    fprintf(stderr, "Failed to %s\n", query);
    fprintf(stderr, "Error: %s\n", mysql_error(sock));
    rc= 1;
  }
  if (!opt_silent)
    printf("%-50s %s\n", name, rc ? "FAILED" : "OK");
  my_free(query);
  DBUG_RETURN(rc);
}

static int process_one_db(char *database)
{
  DBUG_ENTER("process_one_db");

  if (opt_skip_database && !strcmp(database, opt_skip_database))
    DBUG_RETURN(0);

  if (verbose)
    puts(database);
  if (what_to_do == DO_FIX_NAMES)
  {
    int rc= 0;
    if (opt_fix_db_names && !strncmp(database,"#mysql50#", 9))
    {
      rc= fix_database_storage_name(database);
      database+= 9;
    }
    if (rc || !opt_fix_table_names)
      DBUG_RETURN(rc);
  }
  DBUG_RETURN(process_all_tables_in_db(database));
}


static int use_db(char *database)
{
  DBUG_ENTER("use_db");

  if (mysql_get_server_version(sock) >= FIRST_INFORMATION_SCHEMA_VERSION &&
      !cmp_database(database, INFORMATION_SCHEMA_DB_NAME))
    DBUG_RETURN(1);
  if (mysql_get_server_version(sock) >= FIRST_PERFORMANCE_SCHEMA_VERSION &&
      !cmp_database(database, PERFORMANCE_SCHEMA_DB_NAME))
    DBUG_RETURN(1);
  if (mysql_select_db(sock, database))
  {
    DBerror(sock, "when selecting the database");
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
} /* use_db */

/* Do not send commands to replication slaves. */
static int disable_binlog()
{
  mysql_query(sock, "SET WSREP_ON=0"); /* ignore the error, if any */
  return run_query("SET SQL_LOG_BIN=0", 0);
}


/* Ok as mysqlcheck is not multi threaded */
PRAGMA_DISABLE_CHECK_STACK_FRAME

static int handle_request_for_tables(char *tables, size_t length,
                                     my_bool view, my_bool dont_quote)
{
  char *query, *end, options[100], message[100];
  char table_name_buff[NAME_CHAR_LEN*2*2+1], *table_name;
  size_t query_length= 0, query_size= sizeof(char)*(length+110);
  const char *op = 0;
  const char *tab_view;
  DBUG_ENTER("handle_request_for_tables");

  options[0] = 0;
  tab_view= view ? " VIEW " : " TABLE ";
  end = options;
  switch (what_to_do) {
  case DO_CHECK:
    op = "CHECK";
    if (view)
    {
      if (opt_fast || opt_check_only_changed)
        DBUG_RETURN(0);
    }
    else
    {
      if (opt_quick)              end = strmov(end, " QUICK");
      if (opt_fast)               end = strmov(end, " FAST");
      if (opt_extended)           end = strmov(end, " EXTENDED");
      if (opt_medium_check)       end = strmov(end, " MEDIUM"); /* Default */
      if (opt_check_only_changed) end = strmov(end, " CHANGED");
    }
    if (opt_upgrade)            end = strmov(end, " FOR UPGRADE");
    break;
  case DO_REPAIR:
    op= opt_write_binlog ?  "REPAIR" : "REPAIR NO_WRITE_TO_BINLOG";
    if (view)
    {
      if (opt_do_views == DO_VIEWS_FROM_MYSQL)
        end = strmov(end, " FROM MYSQL");
      else if (opt_do_views == DO_UPGRADE)
        end = strmov(end, " FOR UPGRADE");
    }
    else
    {
      if (opt_quick)    end = strmov(end, " QUICK");
      if (opt_extended) end = strmov(end, " EXTENDED");
      if (opt_frm)      end = strmov(end, " USE_FRM");
    }
    break;
  case DO_ANALYZE:
    if (view)
    {
      printf("%-50s %s\n", tables, "Can't run analyze on a view");
      DBUG_RETURN(1);
    }
    DBUG_ASSERT(!view);
    op= (opt_write_binlog) ? "ANALYZE" : "ANALYZE NO_WRITE_TO_BINLOG";
    if (opt_persistent_all) end = strmov(end, " PERSISTENT FOR ALL");
    break;
  case DO_OPTIMIZE:
    if (view)
    {
      printf("%-50s %s\n", tables, "Can't run optimize on a view");
      DBUG_RETURN(1);
    }
    op= (opt_write_binlog) ? "OPTIMIZE" : "OPTIMIZE NO_WRITE_TO_BINLOG";
    break;
  case DO_FIX_NAMES:
    if (view)
    {
      printf("%-50s %s\n", tables, "Can't run fix names on a view");
      DBUG_RETURN(1);
    }
    DBUG_RETURN(fix_table_storage_name(tables));
  }

  if (!(query =(char *) my_malloc(PSI_NOT_INSTRUMENTED, query_size, MYF(MY_WME))))
    DBUG_RETURN(1);
  if (dont_quote)
  {
    DBUG_ASSERT(op);
    DBUG_ASSERT(strlen(op)+strlen(tables)+strlen(options)+8+1 <= query_size);

    /* No backticks here as we added them before */
    query_length= sprintf(query, "%s%s%s %s", op,
                          tab_view, tables, options);
    table_name= tables;
  }
  else
  {
    char *ptr, *org;

    org= ptr= strmov(strmov(query, op), tab_view);
    ptr= fix_table_name(ptr, tables);
    strmake(table_name_buff, org, MY_MIN((int) sizeof(table_name_buff)-1,
                                         (int) (ptr - org)));
    table_name= table_name_buff;
    ptr= strxmov(ptr, " ", options, NullS);
    query_length= (size_t) (ptr - query);
  }
  if (verbose >= 3)
    puts(query);
  if (mysql_real_query(sock, query, (ulong)query_length))
  {
    my_snprintf(message, sizeof(message), "when executing '%s%s... %s'",
                op, tab_view, options);
    DBerror(sock, message);
    my_free(query);
    DBUG_RETURN(1);
  }
  print_result();
  if (opt_flush_tables)
  {
    query_length= sprintf(query, "FLUSH TABLES %s", table_name);
    if (mysql_real_query(sock, query, (ulong)query_length))
    {
      DBerror(sock, query);
      my_free(query);
      DBUG_RETURN(1);
    }
  }
  my_free(query);
  DBUG_RETURN(0);
}

static void insert_table_name(DYNAMIC_ARRAY *arr, char *in, size_t dblen)
{
  char buf[NAME_LEN*2+2];
  in[dblen]= 0;
  my_snprintf(buf, sizeof(buf), "%sQ.%sQ", in, in + dblen + 1);
  insert_dynamic(arr, (uchar*) buf);
}

static void __attribute__((noinline)) print_result()
{
  MYSQL_RES *res;
  MYSQL_ROW row;
  char prev[(NAME_LEN+9)*3+2];
  size_t length_of_db= strlen(sock->db);
  my_bool found_error=0, table_rebuild=0;
  DYNAMIC_ARRAY *array4repair= &tables4repair;
  DBUG_ENTER("print_result");

  res = mysql_use_result(sock);

  prev[0] = '\0';
  while ((row = mysql_fetch_row(res)))
  {
    int changed = strcmp(prev, row[0]);
    my_bool status = !strcmp(row[2], "status");

    if (status)
    {
      /*
        if there was an error with the table, we have --auto-repair set,
        and this isn't a repair op, then add the table to the tables4repair
        list
      */
      if (found_error && opt_auto_repair && what_to_do != DO_REPAIR &&
	  strcmp(row[3],"OK"))
      {
        if (table_rebuild)
          insert_table_name(&tables4rebuild, prev, length_of_db);
        else
          insert_table_name(array4repair, prev, length_of_db);
      }
      array4repair= &tables4repair;
      found_error=0;
      table_rebuild=0;
      if (opt_silent)
	continue;
    }
    if (status && changed)
      printf("%-50s %s", row[0], row[3]);
    else if (!status && changed)
    {
      /*
        If the error message includes REPAIR TABLE, we assume it means
        we have to run REPAIR on it. In this case we write a nicer message
        than "Please do "REPAIR TABLE""...
        If the message includes ALTER TABLE then there is something wrong
        with the table definition and we have to run ALTER TABLE to fix it.
        Write also a nice error message for this case.
      */
      if (!strcmp(row[2],"error") && strstr(row[3],"REPAIR "))
      {
        printf("%-50s %s", row[0], "Needs upgrade with REPAIR");
        array4repair= strstr(row[3], "VIEW") ? &views4repair : &tables4repair;
      }
      else if (!strcmp(row[2],"error") && strstr(row[3],"ALTER TABLE"))
      {
        printf("%-50s %s", row[0], "Needs upgrade with ALTER TABLE FORCE");
        array4repair= &tables4rebuild;
      }
      else
        printf("%s\n%-9s: %s", row[0], row[2], row[3]);
      if (strcmp(row[2],"note"))
      {
        found_error=1;
        if (strstr(row[3], "ALTER TABLE"))
          table_rebuild=1;
      }
    }
    else
      printf("%-9s: %s", row[2], row[3]);
    strmov(prev, row[0]);
    putchar('\n');
  }
  /* add the last table to be repaired to the list */
  if (found_error && opt_auto_repair && what_to_do != DO_REPAIR)
  {
    if (table_rebuild)
      insert_table_name(&tables4rebuild, prev, length_of_db);
    else
      insert_table_name(array4repair, prev, length_of_db);
  }
  mysql_free_result(res);
  DBUG_VOID_RETURN;
}
PRAGMA_REENABLE_CHECK_STACK_FRAME


static int dbConnect(char *host, char *user, char *passwd)
{
  my_bool reconnect= 1;
  DBUG_ENTER("dbConnect");
  if (verbose > 1)
  {
    fprintf(stderr, "# Connecting to %s...\n", host ? host : "localhost");
  }
  mysql_init(&mysql_connection);
  if (opt_compress)
    mysql_options(&mysql_connection, MYSQL_OPT_COMPRESS, NullS);
  SET_SSL_OPTS(&mysql_connection);
  if (opt_protocol)
    mysql_options(&mysql_connection,MYSQL_OPT_PROTOCOL,(char*)&opt_protocol);

  if (opt_plugin_dir && *opt_plugin_dir)
    mysql_options(&mysql_connection, MYSQL_PLUGIN_DIR, opt_plugin_dir);

  if (opt_default_auth && *opt_default_auth)
    mysql_options(&mysql_connection, MYSQL_DEFAULT_AUTH, opt_default_auth);

  mysql_options(&mysql_connection, MYSQL_SET_CHARSET_NAME, default_charset);
  mysql_options(&mysql_connection, MYSQL_OPT_CONNECT_ATTR_RESET, 0);
  mysql_options4(&mysql_connection, MYSQL_OPT_CONNECT_ATTR_ADD,
                 "program_name", "mysqlcheck");
  if (!(sock = mysql_real_connect(&mysql_connection, host, user, passwd,
         NULL, opt_mysql_port, opt_mysql_unix_port, 0)))
  {
    DBerror(&mysql_connection, "when trying to connect");
    DBUG_RETURN(1);
  }
  mysql_options(&mysql_connection, MYSQL_OPT_RECONNECT, &reconnect);
  DBUG_RETURN(0);
} /* dbConnect */


static void dbDisconnect(char *host)
{
  DBUG_ENTER("dbDisconnect");
  if (verbose > 1)
    fprintf(stderr, "# Disconnecting from %s...\n", host ? host : "localhost");
  mysql_close(sock);
  DBUG_VOID_RETURN;
} /* dbDisconnect */


static void DBerror(MYSQL *mysql, const char *when)
{
  DBUG_ENTER("DBerror");
  my_printf_error(0,"Got error: %d: %s %s", MYF(0),
		  mysql_errno(mysql), mysql_error(mysql), when);
  safe_exit(EX_MYSQLERR);
  DBUG_VOID_RETURN;
} /* DBerror */


static void safe_exit(int error)
{
  DBUG_ENTER("safe_exit");
  if (!first_error)
    first_error= error;
  if (ignore_errors)
    DBUG_VOID_RETURN;
  if (sock)
    mysql_close(sock);
  sf_leaking_memory= 1; /* don't check for memory leaks */
  exit(error);
  DBUG_VOID_RETURN;
}


int main(int argc, char **argv)
{
  int ret= EX_USAGE;
  char **defaults_argv;

  MY_INIT(argv[0]);
  sf_leaking_memory=1; /* don't report memory leaks on early exits */

  /* We need to know if protocol-related options originate from CLI args */
  my_defaults_mark_files = TRUE;

  /*
  ** Check out the args
  */
  load_defaults_or_exit("my", load_default_groups, &argc, &argv);
  defaults_argv= argv;
  if (get_options(&argc, &argv))
    goto end1;

  sf_leaking_memory=0; /* from now on we cleanup properly */

  ret= EX_MYSQLERR;
  if (dbConnect(current_host, current_user, opt_password))
    goto end1;

  ret= 1;
  if (!opt_write_binlog)
  {
    if (disable_binlog())
      goto end;
  }

  if (opt_auto_repair &&
      (my_init_dynamic_array(PSI_NOT_INSTRUMENTED, &tables4repair,
                             NAME_LEN*2+2, 16, 64, MYF(0)) ||
       my_init_dynamic_array(PSI_NOT_INSTRUMENTED, &views4repair,
                             NAME_LEN*2+2, 16, 64, MYF(0)) ||
       my_init_dynamic_array(PSI_NOT_INSTRUMENTED, &tables4rebuild,
                             NAME_LEN*2+2, 16, 64, MYF(0)) ||
       my_init_dynamic_array(PSI_NOT_INSTRUMENTED, &alter_table_cmds,
                             MAX_ALTER_STR_SIZE, 0, 1, MYF(0))))
    goto end;

  if (opt_alldbs)
    process_all_databases();
  /* Only one database and selected table(s) */
  else if (argc > 1 && !opt_databases)
    process_selected_tables(*argv, (argv + 1), (argc - 1));
  /* One or more databases, all tables */
  else
    process_databases(argv);
  if (opt_auto_repair)
  {
    size_t i;

    if (!opt_silent && (tables4repair.elements || tables4rebuild.elements))
      puts("\nRepairing tables");
    what_to_do = DO_REPAIR;
    for (i = 0; i < tables4repair.elements ; i++)
    {
      char *name= (char*) dynamic_array_ptr(&tables4repair, i);
      handle_request_for_tables(name, fixed_name_length(name), FALSE, TRUE);
    }
    for (i = 0; i < tables4rebuild.elements ; i++)
      rebuild_table((char*) dynamic_array_ptr(&tables4rebuild, i));
    for (i = 0; i < alter_table_cmds.elements ; i++)
      run_query((char*) dynamic_array_ptr(&alter_table_cmds, i), 1);
    if (!opt_silent && views4repair.elements)
      puts("\nRepairing views");
    for (i = 0; i < views4repair.elements ; i++)
    {
      char *name= (char*) dynamic_array_ptr(&views4repair, i);
      handle_request_for_tables(name, fixed_name_length(name), TRUE, TRUE);
    }
  }
  ret= MY_TEST(first_error);

 end:
  dbDisconnect(current_host);
  if (opt_auto_repair)
  {
    delete_dynamic(&views4repair);
    delete_dynamic(&tables4repair);
    delete_dynamic(&tables4rebuild);
    delete_dynamic(&alter_table_cmds);
  }
 end1:
  my_free(opt_password);;
  mysql_library_end();
  free_defaults(defaults_argv);
  my_end(my_end_arg);
  return ret;
} /* main */
