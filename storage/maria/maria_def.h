/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   Copyright (c) 2009, 2020, MariaDB Corporation Ab

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

/* This file is included by all internal maria files */

#ifndef MARIA_DEF_INCLUDED
#define MARIA_DEF_INCLUDED

#include <my_global.h>

#ifdef EMBEDDED_LIBRARY
#undef WITH_S3_STORAGE_ENGINE
#endif

#include "maria.h"				/* Structs & some defines */
#include "ma_pagecache.h"
#include <myisampack.h>				/* packing of keys */
#include <my_tree.h>
#include <my_bitmap.h>
#include <my_pthread.h>
#include <thr_lock.h>
#include <hash.h>
#include "ma_loghandler.h"
#include "ma_control_file.h"
#include "ma_state.h"
#include <waiting_threads.h>
#include <mysql/psi/mysql_file.h>

#define MARIA_CANNOT_ROLLBACK

C_MODE_START

#ifdef _WIN32
/*
  We cannot use mmap() on Windows with Aria as mmap() can cause file
  size to increase in _ma_dynmap_file(). The extra \0 data causes
  the file to be regarded as corrupted.
*/
#undef HAVE_MMAP
#endif
/*
  Limit max keys according to HA_MAX_POSSIBLE_KEY; See myisamchk.h for details
*/

#if MAX_INDEXES > HA_MAX_POSSIBLE_KEY
#define MARIA_MAX_KEY    HA_MAX_POSSIBLE_KEY    /* Max allowed keys */
#else
#define MARIA_MAX_KEY    MAX_INDEXES            /* Max allowed keys */
#endif

#define MARIA_NAME_IEXT ".MAI"
#define MARIA_NAME_DEXT ".MAD"
/* Max extra space to use when sorting keys */
#define MARIA_MAX_TEMP_LENGTH   (2*1024L*1024L*1024L)
/* Possible values for maria_block_size (must be power of 2) */
#define MARIA_KEY_BLOCK_LENGTH  8192            /* default key block length */
#define MARIA_MIN_KEY_BLOCK_LENGTH      1024    /* Min key block length */
#define MARIA_MAX_KEY_BLOCK_LENGTH      32768
/* Minimal page cache when we only want to be able to scan a table */
#define MARIA_MIN_PAGE_CACHE_SIZE       (8192L*16L)

/*
  File align size (must be power of 2) used for pre-allocation of
  temporary table space. It is used to reduce the number of calls to
  update_tmp_file_size for static and dynamic rows.
*/
#define MARIA_TRACK_INCREMENT_SIZE	        8192

/*
  In the following macros '_keyno_' is 0 .. keys-1.
  If there can be more keys than bits in the key_map, the highest bit
  is for all upper keys. They cannot be switched individually.
  This means that clearing of high keys is ignored, setting one high key
  sets all high keys.
*/
#define MARIA_KEYMAP_BITS      (8 * SIZEOF_LONG_LONG)
#define MARIA_KEYMAP_HIGH_MASK (1ULL << (MARIA_KEYMAP_BITS - 1))
#define maria_get_mask_all_keys_active(_keys_) \
                            (((_keys_) < MARIA_KEYMAP_BITS) ? \
                             ((1ULL << (_keys_)) - 1ULL) : \
                             (~ 0ULL))
#if MARIA_MAX_KEY > MARIA_KEYMAP_BITS
#define maria_is_key_active(_keymap_,_keyno_) \
                            (((_keyno_) < MARIA_KEYMAP_BITS) ? \
                             MY_TEST((_keymap_) & (1ULL << (_keyno_))) : \
                             MY_TEST((_keymap_) & MARIA_KEYMAP_HIGH_MASK))
#define maria_set_key_active(_keymap_,_keyno_) \
                            (_keymap_)|= (((_keyno_) < MARIA_KEYMAP_BITS) ? \
                                          (1ULL << (_keyno_)) : \
                                          MARIA_KEYMAP_HIGH_MASK)
#define maria_clear_key_active(_keymap_,_keyno_) \
                            (_keymap_)&= (((_keyno_) < MARIA_KEYMAP_BITS) ? \
                                          (~ (1ULL << (_keyno_))) : \
                                          (~ (0ULL)) /*ignore*/ )
#else
#define maria_is_key_active(_keymap_,_keyno_) \
                            MY_TEST((_keymap_) & (1ULL << (_keyno_)))
#define maria_set_key_active(_keymap_,_keyno_) \
                            (_keymap_)|= (1ULL << (_keyno_))
#define maria_clear_key_active(_keymap_,_keyno_) \
                            (_keymap_)&= (~ (1ULL << (_keyno_)))
#endif
#define maria_is_any_key_active(_keymap_) \
                            MY_TEST((_keymap_))
#define maria_is_all_keys_active(_keymap_,_keys_) \
                            ((_keymap_) == maria_get_mask_all_keys_active(_keys_))
#define maria_set_all_keys_active(_keymap_,_keys_) \
                            (_keymap_)= maria_get_mask_all_keys_active(_keys_)
#define maria_clear_all_keys_active(_keymap_) \
                            (_keymap_)= 0
#define maria_intersect_keys_active(_to_,_from_) \
                            (_to_)&= (_from_)
#define maria_is_any_intersect_keys_active(_keymap1_,_keys_,_keymap2_) \
                            ((_keymap1_) & (_keymap2_) & \
                             maria_get_mask_all_keys_active(_keys_))
#define maria_copy_keys_active(_to_,_maxkeys_,_from_) \
                            (_to_)= (maria_get_mask_all_keys_active(_maxkeys_) & \
                                     (_from_))

        /* Param to/from maria_info */

typedef struct st_maria_info
{
  ha_rows records;                      /* Records in database */
  ha_rows deleted;                      /* Deleted records in database */
  MARIA_RECORD_POS recpos;              /* Pos for last used record */
  MARIA_RECORD_POS newrecpos;           /* Pos if we write new record */
  MARIA_RECORD_POS dup_key_pos;         /* Position to record with dup key */
  my_off_t data_file_length;            /* Length of data file */
  my_off_t max_data_file_length, index_file_length;
  my_off_t max_index_file_length, delete_length;
  ulonglong auto_increment;
  ulonglong key_map;                    /* Which keys are used */
  time_t create_time;                   /* When table was created */
  time_t check_time;
  time_t update_time;
  ulong record_offset;
  double *rec_per_key;                   /* for sql optimizing */
  ulong reclength;                      /* Recordlength */
  ulong mean_reclength;                 /* Mean recordlength (if packed) */
  char *data_file_name, *index_file_name;
  enum data_file_type data_file_type;
  uint keys;                            /* Number of keys in use */
  uint options;                         /* HA_OPTION_... used */
  uint reflength;
  int errkey,                           /* With key was dupplicated on err */
    sortkey;                            /* clustered by this key */
  File filenr;                          /* (uniq) filenr for datafile */
} MARIA_INFO;

struct st_maria_share;
struct st_maria_handler;                        /* For reference */
struct st_maria_keydef;

struct st_maria_key                 /* Internal info about a key */
{
  uchar *data;                              /* Data for key */
  struct st_maria_keydef *keyinfo;          /* Definition for key */
  uint data_length;                         /* Length of key data */
  uint ref_length;                          /* record ref + transid */
  uint32 flag;                               /* 0 or SEARCH_PART_KEY */
};

struct st_maria_decode_tree     /* Decode huff-table */
{
  uint16 *table;
  uint quick_table_bits;
  uchar *intervalls;
};


typedef struct s3_info S3_INFO;

extern uint maria_block_size, maria_checkpoint_frequency;
extern ulong maria_concurrent_insert;
extern my_bool maria_flush, maria_single_user, maria_page_checksums;
extern my_off_t maria_max_temp_length;
extern ulong maria_bulk_insert_tree_size, maria_data_pointer_size;
extern MY_TMPDIR *maria_tmpdir;
extern my_bool maria_encrypt_tables;

/*
  This is used to check if a symlink points into the mysql data home,
  which is normally forbidden as it can be used to get access to
  not privileged data
*/
extern int (*maria_test_invalid_symlink)(const char *filename);

        /* Prototypes for maria-functions */

extern int maria_init(void);
extern void maria_end(void);
extern my_bool maria_upgrade(void);
extern int maria_close(MARIA_HA *file);
extern int maria_delete(MARIA_HA *file, const uchar *buff);
extern MARIA_HA *maria_open(const char *name, int mode,
                            uint wait_if_locked, S3_INFO *s3);
extern void ma_change_pagecache(MARIA_HA *info);
extern int maria_panic(enum ha_panic_function function);
extern int maria_rfirst(MARIA_HA *file, uchar *buf, int inx);
extern int maria_rkey(MARIA_HA *file, uchar *buf, int inx,
                      const uchar *key, key_part_map keypart_map,
                      enum ha_rkey_function search_flag);
extern int maria_rlast(MARIA_HA *file, uchar *buf, int inx);
extern int maria_rnext(MARIA_HA *file, uchar *buf, int inx);
extern int maria_rnext_same(MARIA_HA *info, uchar *buf);
extern int maria_rprev(MARIA_HA *file, uchar *buf, int inx);
extern int maria_rrnd(MARIA_HA *file, uchar *buf,
                      MARIA_RECORD_POS pos);
extern int maria_scan_init(MARIA_HA *file);
extern int maria_scan(MARIA_HA *file, uchar *buf);
extern void maria_scan_end(MARIA_HA *file);
extern int maria_rsame(MARIA_HA *file, uchar *record, int inx);
extern int maria_rsame_with_pos(MARIA_HA *file, uchar *record,
                                int inx, MARIA_RECORD_POS pos);
extern int maria_update(MARIA_HA *file, const uchar *old,
                        const uchar *new_record);
extern int maria_write(MARIA_HA *file, const uchar *buff);
extern int maria_status(MARIA_HA *info, MARIA_INFO *x, uint flag);
extern int maria_lock_database(MARIA_HA *file, int lock_type);
extern int maria_delete_table(const char *name);
extern int maria_rename(const char *from, const char *to);
extern int maria_extra(MARIA_HA *file,
                       enum ha_extra_function function, void *extra_arg);
extern int maria_reset(MARIA_HA *file);
extern ha_rows maria_records_in_range(MARIA_HA *info, int inx,
                                      const key_range *min_key,
                                      const key_range *max_key,
                                      page_range *page);
extern int maria_is_changed(MARIA_HA *info);
extern int maria_delete_all_rows(MARIA_HA *info);
extern uint maria_get_pointer_length(ulonglong file_length, uint def);
extern int maria_commit(MARIA_HA *info);
extern int maria_begin(MARIA_HA *info);
extern void maria_disable_logging(MARIA_HA *info);
extern void maria_enable_logging(MARIA_HA *info);

#define HA_RECOVER_NONE         0       /* No automatic recover */
#define HA_RECOVER_DEFAULT      1       /* Automatic recover active */
#define HA_RECOVER_BACKUP       2       /* Make a backupfile on recover */
#define HA_RECOVER_FORCE        4       /* Recover even if we loose rows */
#define HA_RECOVER_QUICK        8       /* Don't check rows in data file */

#define HA_RECOVER_ANY (HA_RECOVER_DEFAULT | HA_RECOVER_BACKUP | HA_RECOVER_FORCE | HA_RECOVER_QUICK)

/* this is used to pass to mysql_mariachk_table */

#define MARIA_CHK_REPAIR 1              /* equivalent to mariachk -r */
#define MARIA_CHK_VERIFY 2              /* Verify, run repair if failure */

typedef uint maria_bit_type;

typedef struct st_maria_bit_buff
{                                       /* Used for packing of record */
  maria_bit_type current_byte;
  uint bits;
  uchar *pos, *end, *blob_pos, *blob_end;
  uint error;
} MARIA_BIT_BUFF;

/* functions in maria_check */
void maria_chk_init(HA_CHECK *param);
void maria_chk_init_for_check(HA_CHECK *param, MARIA_HA *info);
int maria_chk_status(HA_CHECK *param, MARIA_HA *info);
int maria_chk_del(HA_CHECK *param, MARIA_HA *info, ulonglong test_flag);
int maria_chk_size(HA_CHECK *param, MARIA_HA *info);
int maria_chk_key(HA_CHECK *param, MARIA_HA *info);
int maria_chk_data_link(HA_CHECK *param, MARIA_HA *info, my_bool extend);
int maria_repair(HA_CHECK *param, MARIA_HA *info, char * name, my_bool);
int maria_sort_index(HA_CHECK *param, MARIA_HA *info, char * name);
int maria_zerofill(HA_CHECK *param, MARIA_HA *info, const char *name);
int maria_repair_by_sort(HA_CHECK *param, MARIA_HA *info,
                         const char *name, my_bool rep_quick);
int maria_repair_parallel(HA_CHECK *param, MARIA_HA *info,
                          const char *name, my_bool rep_quick);
int maria_change_to_newfile(const char *filename, const char *old_ext,
                            const char *new_ext, time_t backup_time,
                            myf myflags);
void maria_lock_memory(HA_CHECK *param);
int maria_update_state_info(HA_CHECK *param, MARIA_HA *info, uint update);
void maria_update_key_parts(MARIA_KEYDEF *keyinfo, double *rec_per_key_part,
                            ulonglong *unique, ulonglong *notnull,
                            ulonglong records);
int maria_filecopy(HA_CHECK *param, File to, File from, my_off_t start,
                   my_off_t length, const char *type);
int maria_movepoint(MARIA_HA *info, uchar *record, my_off_t oldpos,
                    my_off_t newpos, uint prot_key);
int maria_test_if_almost_full(MARIA_HA *info);
int maria_recreate_table(HA_CHECK *param, MARIA_HA **org_info, char *filename);
int maria_disable_indexes(MARIA_HA *info);
int maria_enable_indexes(MARIA_HA *info);
int maria_indexes_are_disabled(MARIA_HA *info);
void maria_disable_indexes_for_rebuild(MARIA_HA *info, ha_rows rows,
                                       my_bool all_keys);
my_bool maria_test_if_sort_rep(MARIA_HA *info, ha_rows rows, ulonglong key_map,
                               my_bool force);

int maria_init_bulk_insert(MARIA_HA *info, size_t cache_size, ha_rows rows);
void maria_flush_bulk_insert(MARIA_HA *info, uint inx);
int maria_end_bulk_insert(MARIA_HA *info, my_bool abort);
int maria_preload(MARIA_HA *info, ulonglong key_map, my_bool ignore_leaves);
void maria_ignore_trids(MARIA_HA *info);
my_bool maria_too_big_key_for_sort(MARIA_KEYDEF *key, ha_rows rows);

/* fulltext functions */
FT_INFO *maria_ft_init_search(uint,void *, uint, uchar *, size_t,
                              CHARSET_INFO *, uchar *);

/* 'Almost-internal' Maria functions */

void _ma_update_auto_increment_key(HA_CHECK *param, MARIA_HA *info,
                                  my_bool repair);


/* Do extra sanity checking */
#define SANITY_CHECKS 1
#ifdef EXTRA_DEBUG
#define EXTRA_DEBUG_KEY_CHANGES
#endif
/*
  The following defines can be used when one has problems with redo logging
  Setting this will log the full key page which can be compared with the
  redo-changed key page. This will however make the aria log files MUCH bigger.
*/
#if defined(EXTRA_ARIA_DEBUG)
#define EXTRA_STORE_FULL_PAGE_IN_KEY_CHANGES
#endif
/* For testing recovery */
#ifdef TO_BE_REMOVED
#define IDENTICAL_PAGES_AFTER_RECOVERY 1
#endif

#define MAX_NONMAPPED_INSERTS 1000
#define MARIA_MAX_TREE_LEVELS 32
#define MARIA_MAX_RECORD_ON_STACK 16384

#define MARIA_MIN_SORT_MEMORY (16384-MALLOC_OVERHEAD)

/* maria_open() flag, specific for maria_pack */
#define HA_OPEN_IGNORE_MOVED_STATE (1U << 30)

typedef struct st_sort_key_blocks MA_SORT_KEY_BLOCKS;
typedef struct st_sort_ftbuf MA_SORT_FT_BUF;

extern PAGECACHES maria_pagecaches;
int maria_assign_to_pagecache(MARIA_HA *info, ulonglong key_map,
			      PAGECACHE *key_cache);
void maria_change_pagecache(PAGECACHE *old_key_cache,
			    PAGECACHE *new_key_cache);

typedef struct st_maria_sort_info
{
  /* sync things */
  mysql_mutex_t mutex;
  mysql_cond_t cond;
  MARIA_HA *info, *new_info;
  HA_CHECK *param;
  char *buff;
  MA_SORT_KEY_BLOCKS *key_block, *key_block_end;
  MA_SORT_FT_BUF *ft_buf;
  my_off_t filelength, dupp, buff_length;
  pgcache_page_no_t page;
  ha_rows max_records;
  uint current_key, total_keys;
  volatile uint got_error;
  uint threads_running;
  myf myf_rw;
  enum data_file_type new_data_file_type, org_data_file_type;
} MARIA_SORT_INFO;

typedef struct st_maria_sort_param
{
  pthread_t thr;
  IO_CACHE read_cache, tempfile, tempfile_for_exceptions;
  DYNAMIC_ARRAY buffpek;
  MARIA_BIT_BUFF bit_buff;               /* For parallel repair of packrec. */
  
  MARIA_KEYDEF *keyinfo;
  MARIA_SORT_INFO *sort_info;
  HA_KEYSEG *seg;
  uchar **sort_keys;
  uchar *rec_buff;
  void *wordlist, *wordptr;
  MEM_ROOT wordroot;
  uchar *record;
  MY_TMPDIR *tmpdir;
  HA_CHECK *check_param;

  /* 
    The next two are used to collect statistics, see maria_update_key_parts for
    description.
  */
  ulonglong unique[HA_MAX_KEY_SEG+1];
  ulonglong notnull[HA_MAX_KEY_SEG+1];
  ulonglong sortbuff_size;

  MARIA_RECORD_POS pos,max_pos,filepos,start_recpos, current_filepos;
  uint key, key_length,real_key_length;
  uint maxbuffers, keys, find_length, sort_keys_length;
  my_bool fix_datafile, master;
  my_bool calc_checksum;                /* calculate table checksum */
  size_t rec_buff_size;

  int (*key_cmp)(void *, const void *, const void *);
  int (*key_read)(struct st_maria_sort_param *, uchar *);
  int (*key_write)(struct st_maria_sort_param *, const uchar *);
  void (*lock_in_memory)(HA_CHECK *);
  int (*write_keys)(struct st_maria_sort_param *, uchar **,
                         ulonglong , struct st_buffpek *, IO_CACHE *);
  my_off_t (*read_to_buffer)(IO_CACHE *,struct st_buffpek *, uint);
  int (*write_key)(struct st_maria_sort_param *, IO_CACHE *,uchar *,
                   uint, ulonglong);
} MARIA_SORT_PARAM;

int maria_write_data_suffix(MARIA_SORT_INFO *sort_info, my_bool fix_datafile);

struct st_transaction;

/* undef map from my_nosys; We need test-if-disk full */
#undef my_write

#define CRC_SIZE 4

typedef struct st_maria_state_info
{
  struct
  {					/* Fileheader (24 bytes) */
    uchar file_version[4];
    uchar options[2];
    uchar header_length[2];
    uchar state_info_length[2];
    uchar base_info_length[2];
    uchar base_pos[2];
    uchar key_parts[2];			/* Key parts */
    uchar unique_key_parts[2];		/* Key parts + unique parts */
    uchar keys;				/* number of keys in file */
    uchar uniques;			/* number of UNIQUE definitions */
    uchar not_used;			/* Language for indexes */
    uchar fulltext_keys;
    uchar data_file_type;
    /* Used by mariapack to store the original data_file_type */
    uchar org_data_file_type;
  } header;

  MARIA_STATUS_INFO state;
  /* maria_ha->state points here for crash-safe but not versioned tables */
  MARIA_STATUS_INFO common;
  /* State for a versioned table that is temporary non versioned */
  MARIA_STATUS_INFO no_logging;
  ha_rows split;			/* number of split blocks */
  my_off_t dellink;			/* Link to next removed block */
  pgcache_page_no_t first_bitmap_with_space;
  ulonglong auto_increment;
  TrID create_trid;                     /* Minimum trid for file */
  TrID last_change_trn;                 /* selfdescriptive */
  ulong update_count;			/* Updated for each write lock */
  ulong status;
  double *rec_per_key_part;
  ulong *nulls_per_key_part;
  ha_checksum checksum;                 /* Table checksum */
  my_off_t *key_root;			/* Start of key trees */
  my_off_t key_del;			/* delete links for index pages */
  my_off_t records_at_analyze;		/* Rows when calculating rec_per_key */

  ulong sec_index_changed;		/* Updated when new sec_index */
  ulong sec_index_used;			/* which extra index are in use */
  ulonglong key_map;			/* Which keys are in use */
  ulong version;			/* timestamp of create */
  time_t create_time;			/* Time when created database */
  time_t recover_time;			/* Time for last recover */
  time_t check_time;			/* Time for last check */
  uint sortkey;				/* sorted by this key (not used) */
  uint open_count;
  uint changed;                         /* Changed since maria_chk */
  uint org_changed;                     /* Changed since open */
  /**
     Birthday of the table: no record in the log before this LSN should ever
     be applied to the table. Updated when created, renamed, explicitly
     repaired (REPAIR|OPTIMIZE TABLE, ALTER TABLE ENABLE KEYS, maria_chk).
  */
  LSN create_rename_lsn;
  /** @brief Log horizon when state was last updated on disk */
  TRANSLOG_ADDRESS is_of_horizon;
  /**
     REDO phase should ignore any record before this LSN. UNDO phase
     shouldn't, this is the difference with create_rename_lsn.
     skip_redo_lsn >= create_rename_lsn.
     The distinction is for these cases:
     - after a repair at end of bulk insert (enabling indices), REDO phase
     should skip the table but UNDO phase should not, so only skip_redo_lsn is
     increased, not create_rename_lsn
     - if one table is corrupted and so recovery fails, user may repair the
     table with maria_chk and let recovery restart: that recovery should then
     skip the repaired table even in the UNDO phase, so create_rename_lsn is
     increased.
  */
  LSN skip_redo_lsn;
  /* LSN when we wrote file id to the log */
  LSN logrec_file_id;

  uint8 dupp_key;                       /* Lastly processed index with    */
                                        /* violated uniqueness constraint */

  /* the following isn't saved on disk */
  uint state_diff_length;		/* Should be 0 */
  uint state_length;			/* Length of state header in file */
  ulong *key_info;
} MARIA_STATE_INFO;


/* Number of bytes written be _ma_state_info_write_sub() */
#define MARIA_STATE_INFO_SIZE	\
  (24 + 2 + LSN_STORE_SIZE*3 + 4 + 11*8 + 4*4 + 8 + 3*4 + 5*8)
#define MARIA_FILE_OPEN_COUNT_OFFSET 0
#define MARIA_FILE_CHANGED_OFFSET 2
#define MARIA_FILE_CREATE_RENAME_LSN_OFFSET 4
#define MARIA_FILE_CREATE_TRID_OFFSET (4 + LSN_STORE_SIZE*3 + 11*8)

#define MARIA_MAX_KEY_LENGTH    2300
#define MARIA_MAX_KEY_BUFF      (MARIA_MAX_KEY_LENGTH+HA_MAX_KEY_SEG*6+8+8 + \
                                 MARIA_MAX_PACK_TRANSID_SIZE)
#define MARIA_MAX_POSSIBLE_KEY_BUFF  (MARIA_MAX_KEY_LENGTH + 24+ 6+6)
#define MARIA_STATE_KEY_SIZE	(8 + 4)
#define MARIA_STATE_KEYBLOCK_SIZE  8
#define MARIA_STATE_KEYSEG_SIZE	12
#define MARIA_STATE_EXTRA_SIZE (MARIA_MAX_KEY*MARIA_STATE_KEY_SIZE + MARIA_MAX_KEY*HA_MAX_KEY_SEG*MARIA_STATE_KEYSEG_SIZE)
#define MARIA_KEYDEF_SIZE	(2+ 5*2)
#define MARIA_UNIQUEDEF_SIZE	(2+1+1)
#define HA_KEYSEG_SIZE		(6+ 2*2 + 4*2)
#define MARIA_COLUMNDEF_SIZE	(2*7+1+1+4)
#define MARIA_BASE_INFO_SIZE	(MY_UUID_SIZE + 5*8 + 6*4 + 11*2 + 6 + 5*2 + 1 + 16)
#define MARIA_INDEX_BLOCK_MARGIN 16	/* Safety margin for .MYI tables */
#define MARIA_MAX_POINTER_LENGTH 7	/* Node pointer */
/* Internal management bytes needed to store 2 transid/key on an index page */
#define MARIA_MAX_PACK_TRANSID_SIZE   (TRANSID_SIZE+1)
#define MARIA_TRANSID_PACK_OFFSET     (256- TRANSID_SIZE - 1)
#define MARIA_MIN_TRANSID_PACK_OFFSET (MARIA_TRANSID_PACK_OFFSET-TRANSID_SIZE)
#define MARIA_INDEX_OVERHEAD_SIZE     (MARIA_MAX_PACK_TRANSID_SIZE * 2 + \
                                       MARIA_MAX_POINTER_LENGTH)
#define MARIA_DELETE_KEY_NR  255	/* keynr for deleted blocks */

  /* extra options */
#define MA_EXTRA_OPTIONS_ENCRYPTED (1 << 0)
#define MA_EXTRA_OPTIONS_INSERT_ORDER (1 << 1)

#include "ma_check.h"

/*
  Basic information of the Maria table. This is stored on disk
  and not changed (unless we do DLL changes).
*/

typedef struct st_ma_base_info
{
  my_off_t keystart;                    /* Start of keys */
  my_off_t max_data_file_length;
  my_off_t max_key_file_length;
  my_off_t margin_key_file_length;
  ha_rows records, reloc;               /* Create information */
  ulong mean_row_length;                /* Create information */
  ulong reclength;                      /* length of unpacked record */
  ulong pack_reclength;                 /* Length of full packed rec */
  ulong min_pack_length;
  ulong max_pack_length;                /* Max possibly length of packed rec */
  ulong min_block_length;
  ulong s3_block_size;                  /* Block length for S3 files */
  uint fields;                          /* fields in table */
  uint fixed_not_null_fields;
  uint fixed_not_null_fields_length;
  uint max_field_lengths;
  uint pack_fields;                     /* packed fields in table */
  uint varlength_fields;                /* char/varchar/blobs */
  /* Number of bytes in the index used to refer to a row (2-8) */
  uint rec_reflength;
  /* Number of bytes in the index used to refer to another index page (2-8) */
  uint key_reflength;                   /* = 2-8 */
  uint keys;                            /* same as in state.header */
  uint auto_key;                        /* Which key-1 is a auto key */
  uint blobs;                           /* Number of blobs */
  /* Length of packed bits (when table was created first time) */
  uint pack_bytes;
  /* Length of null bits (when table was created first time) */
  uint original_null_bytes;
  uint null_bytes;                      /* Null bytes in record */
  uint field_offsets;                   /* Number of field offsets */
  uint max_key_block_length;            /* Max block length */
  uint max_key_length;                  /* Max key length */
  /* Extra allocation when using dynamic record format */
  uint extra_alloc_bytes;
  uint extra_alloc_procent;
  uint is_nulls_extended;               /* 1 if new null bytes */
  uint default_row_flag;                /* 0 or ROW_FLAG_NULLS_EXTENDED */
  uint block_size;
  /* Size of initial record buffer */
  uint default_rec_buff_size;
  /* Extra number of bytes the row format require in the record buffer */
  uint extra_rec_buff_size;
  /* Tuning flags that can be ignored by older Maria versions */
  uint extra_options;
  /* default language, not really used but displayed by maria_chk */
  uint language;
  /* Compression library used. 0 for no compression */
  uint compression_algorithm;

  /* The following are from the header */
  uint key_parts, all_key_parts;
  uchar uuid[MY_UUID_SIZE];
  /**
     @brief If false, we disable logging, versioning, transaction etc. Observe
     difference with MARIA_SHARE::now_transactional
  */
  my_bool born_transactional;
} MARIA_BASE_INFO;

uchar *_ma_base_info_read(uchar *ptr, MARIA_BASE_INFO *base);

/* Structs used intern in database */

typedef struct st_maria_blob            /* Info of record */
{
  ulong offset;                         /* Offset to blob in record */
  uint pack_length;                     /* Type of packed length */
  ulong length;                         /* Calc:ed for each record */
} MARIA_BLOB;


typedef struct st_maria_pack
{
  ulong header_length;
  uint ref_length;
  uchar version;
} MARIA_PACK;

typedef struct st_maria_file_bitmap
{
  struct st_maria_share *share;
  uchar *map;
  pgcache_page_no_t page;              /* Page number for current bitmap */
  pgcache_page_no_t last_bitmap_page; /* Last possible bitmap page */
  my_bool changed;                     /* 1 if page needs to be written */
  my_bool changed_not_flushed;         /* 1 if some bitmap is not flushed */
  my_bool return_first_match;          /* Shortcut find_head() */
  uint used_size;                      /* Size of bitmap head that is not 0 */
  uint full_head_size;                 /* Where to start search for head */
  uint full_tail_size;                 /* Where to start search for tail */
  uint flush_all_requested;            /**< If _ma_bitmap_flush_all waiting */
  uint waiting_for_flush_all_requested; /* If someone is waiting for above */
  uint non_flushable;                  /**< 0 if bitmap and log are in sync */
  uint waiting_for_non_flushable;      /* If someone is waiting for above */
  PAGECACHE_FILE file;		       /* datafile where bitmap is stored */

  mysql_mutex_t bitmap_lock;
  mysql_cond_t bitmap_cond;          /**< When bitmap becomes flushable */
  /* Constants, allocated when initiating bitmaps */
  uint sizes[8];                      /* Size per bit combination */
  uint total_size;		      /* Total usable size of bitmap page */
  uint max_total_size;                /* Max value for total_size */
  uint last_total_size;               /* Size of bitmap on last_bitmap_page */
  uint block_size;                    /* Block size of file */
  ulong pages_covered;                /* Pages covered by bitmap + 1 */
  DYNAMIC_ARRAY pinned_pages;         /**< not-yet-flushable bitmap pages */
} MARIA_FILE_BITMAP;

#define MARIA_CHECKPOINT_LOOKS_AT_ME 1
#define MARIA_CHECKPOINT_SHOULD_FREE_ME 2
#define MARIA_CHECKPOINT_SEEN_IN_LOOP 4

typedef struct st_maria_crypt_data MARIA_CRYPT_DATA;
struct ms3_st;

typedef struct st_maria_share
{					/* Shared between opens */
  MARIA_STATE_INFO state;
  MARIA_STATE_INFO checkpoint_state;   /* Copy of saved state by checkpoint */
  MARIA_BASE_INFO base;
  MARIA_STATE_HISTORY *state_history;
  MARIA_KEYDEF ft2_keyinfo;		/* Second-level ft-key definition */
  MARIA_KEYDEF *keyinfo;		/* Key definitions */
  MARIA_UNIQUEDEF *uniqueinfo;		/* unique definitions */
  HA_KEYSEG *keyparts;			/* key part info */
  MARIA_COLUMNDEF *columndef;		/* Pointer to column information */
  MARIA_PACK pack;			/* Data about packed records */
  MARIA_BLOB *blobs;			/* Pointer to blobs */
  uint16 *column_nr;			/* Original column order */
  LEX_STRING unique_file_name;		/* realpath() of index file */
  LEX_STRING data_file_name;		/* Resolved path names from symlinks */
  LEX_STRING index_file_name;
  LEX_STRING open_file_name;		/* parameter to open filename */
  uchar *file_map;			/* mem-map of file if possible */
  LIST *open_list;			/* Tables open with this share */
  PAGECACHE *pagecache;			/* ref to the current key cache */
  MARIA_DECODE_TREE *decode_trees;
  int crash_error;                      /* Reason for marked crashed */
  /*
    Previous auto-increment value. Used to verify if we can restore the
    auto-increment counter if we have to abort an insert (duplicate key).
  */
  ulonglong last_auto_increment;
  uint16 *decode_tables;
  uint16 id; /**< 2-byte id by which log records refer to the table */
  /* Called the first time the table instance is opened */
  my_bool (*once_init)(struct st_maria_share *, File);
  /* Called when the last instance of the table is closed */
  my_bool (*once_end)(struct st_maria_share *);
  /* Is called for every open of the table */
  my_bool (*init)(MARIA_HA *);
  /* Is called for every close of the table */
  void (*end)(MARIA_HA *);
  /* Called when we want to read a record from a specific position */
  int (*read_record)(MARIA_HA *, uchar *, MARIA_RECORD_POS);
  /* Initialize a scan */
  my_bool (*scan_init)(MARIA_HA *);
  /* Read next record while scanning */
  int (*scan)(MARIA_HA *, uchar *, MARIA_RECORD_POS, my_bool);
  /* End scan */
  void (*scan_end)(MARIA_HA *);
  int (*scan_remember_pos)(MARIA_HA *, MARIA_RECORD_POS*);
  int (*scan_restore_pos)(MARIA_HA *, MARIA_RECORD_POS);
  /* Pre-write of row (some handlers may do the actual write here) */
  MARIA_RECORD_POS (*write_record_init)(MARIA_HA *, const uchar *);
  /* Write record (or accept write_record_init) */
  my_bool (*write_record)(MARIA_HA *, const uchar *);
  /* Called when write failed */
  my_bool (*write_record_abort)(MARIA_HA *);
  my_bool (*update_record)(MARIA_HA *, MARIA_RECORD_POS,
                           const uchar *, const uchar *);
  my_bool (*delete_record)(MARIA_HA *, const uchar *record);
  my_bool (*compare_record)(MARIA_HA *, const uchar *);
  /* calculate checksum for a row */
  ha_checksum(*calc_checksum)(MARIA_HA *, const uchar *);
  /*
    Calculate checksum for a row during write. May be 0 if we calculate
    the checksum in write_record_init()
  */
  ha_checksum(*calc_write_checksum)(MARIA_HA *, const uchar *);
  /* calculate checksum for a row during check table */
  ha_checksum(*calc_check_checksum)(MARIA_HA *, const uchar *);
  /* Compare a row in memory with a row on disk */
  my_bool (*compare_unique)(MARIA_HA *, MARIA_UNIQUEDEF *,
                            const uchar *record, MARIA_RECORD_POS pos);
  my_off_t (*keypos_to_recpos)(struct st_maria_share *share, my_off_t pos);
  my_off_t (*recpos_to_keypos)(struct st_maria_share *share, my_off_t pos);
  my_bool (*row_is_visible)(MARIA_HA *);

  /* Mappings to read/write the data file */
  size_t (*file_read)(MARIA_HA *, uchar *, size_t, my_off_t, myf);
  size_t (*file_write)(MARIA_HA *, const uchar *, size_t, my_off_t, myf);
  /* query cache invalidator for merged tables */
  invalidator_by_filename invalidator;
  /* query cache invalidator for changing state */
  invalidator_by_filename chst_invalidator;
  my_off_t key_del_current;		/* delete links for index pages */
  ulong this_process;			/* processid */
  ulong last_process;			/* For table-change-check */
  ulong last_version;			/* Version on start */
  ulong options;			/* Options used */
  ulong min_pack_length;		/* These are used by packed data */
  ulong max_pack_length;
  ulong state_diff_length;
  uint rec_reflength;			/* rec_reflength in use now */
  /*
    Extra flag to use for my_malloc(); set to MY_THREAD_SPECIFIC for temporary
    tables whose memory allocation should be accounted to the current THD.
  */
  uint malloc_flag;
  uint keypage_header;
  uint32 ftkeys;			/* Number of distinct full-text keys
						   + 1 */
  PAGECACHE_FILE kfile;			/* Shared keyfile */
  S3_INFO *s3_path;                     /* Connection and path in s3 */
  File data_file;			/* Shared data file */
  int mode;				/* mode of file on open */
  uint reopen;				/* How many times opened */
  uint in_trans;                        /* Number of references by trn */
  uint w_locks, r_locks, tot_locks;	/* Number of read/write locks */
  uint block_size;			/* block_size of keyfile & data file*/
  uint max_index_block_size;            /* block_size - end_of_page_info */
  /* Fixed length part of a packed row in BLOCK_RECORD format */
  uint base_length;
  myf write_flag;
  enum data_file_type data_file_type;
  enum pagecache_page_type page_type;   /* value depending transactional */

  /*
    tracked will cause lost bytes (not aligned) but this is ok as it is always
    used with tmp_file_tracking if set
  */
  my_bool tracked;                      /* Tracked table (always internal) */
  struct tmp_file_tracking track_data,track_index;

  /**
     if Checkpoint looking at table; protected by close_lock or THR_LOCK_maria
  */
  uint8 in_checkpoint;
  my_bool temporary;
  /* Below flag is needed to make log tables work with concurrent insert */
  my_bool is_log_table;
  my_bool has_null_fields;
  my_bool has_varchar_fields;           /* If table has varchar fields */
  /*
    Set to 1 if open_count was wrong at open. Set to avoid asserts for
    wrong open count on close.
  */
  my_bool open_count_not_zero_on_open;

  my_bool changed,			/* If changed since lock */
    global_changed,			/* If changed since open */
    not_flushed;
  my_bool no_status_updates;            /* Set to 1 if S3 readonly table */
  my_bool internal_table;               /* Internal tmp table */
  my_bool lock_key_trees;               /* If we have to lock trees on read */
  my_bool non_transactional_concurrent_insert;
  my_bool delay_key_write;
  my_bool have_rtree;
  /**
     @brief if the table is transactional right now. It may have been created
     transactional (base.born_transactional==TRUE) but with transactionality
     (logging) temporarily disabled (now_transactional==FALSE). The opposite
     (FALSE, TRUE) is impossible.
  */
  my_bool now_transactional;
  my_bool have_versioning;
  my_bool key_del_used;                         /* != 0 if key_del is locked */
  my_bool deleting;                     /* we are going to delete this table */
  my_bool redo_error_given;             /* Used during recovery */
  my_bool silence_encryption_errors;    /* Used during recovery */
  THR_LOCK lock;
  void (*lock_restore_status)(void *);
  /**
    Protects kfile, dfile, most members of the state, state disk writes,
    versioning information (like in_trans, state_history).
    @todo find the exhaustive list.
  */
  mysql_mutex_t intern_lock;	
  mysql_mutex_t key_del_lock;
  mysql_cond_t  key_del_cond;
  /**
    _Always_ held while closing table; prevents checkpoint from looking at
    structures freed during closure (like bitmap). If you need close_lock and
    intern_lock, lock them in this order.
  */
  mysql_mutex_t close_lock;
  my_off_t mmaped_length;
  uint nonmmaped_inserts;		/* counter of writing in
						   non-mmaped area */
  MARIA_FILE_BITMAP bitmap;
  mysql_rwlock_t mmap_lock;
  LSN lsn_of_file_id; /**< LSN of its last LOGREC_FILE_ID */

  /**
     Crypt data
  */
  uint crypt_page_header_space;
  MARIA_CRYPT_DATA *crypt_data;

  /**
     Keep of track of last insert page, used to implement insert order
  */
  uint last_insert_page;
  pgcache_page_no_t last_insert_bitmap;
} MARIA_SHARE;


typedef uchar MARIA_BITMAP_BUFFER;

typedef struct st_maria_bitmap_block
{
  pgcache_page_no_t page;                       /* Page number */
  /* Number of continuous pages. TAIL_BIT is set if this is a tail page */
  uint page_count;
  uint empty_space;                     /* Set for head and tail pages */
  /*
    Number of BLOCKS for block-region (holds all non-blob-fields or one blob)
  */
  uint sub_blocks;
  /* set to <> 0 in write_record() if this block was actually used */
  uint8 used;
  uint8 org_bitmap_value;
} MARIA_BITMAP_BLOCK;


typedef struct st_maria_bitmap_blocks
{
  MARIA_BITMAP_BLOCK *block;
  uint count;
  my_bool tail_page_skipped;            /* If some tail pages was not used */
  my_bool page_skipped;                 /* If some full pages was not used */
} MARIA_BITMAP_BLOCKS;


/* Data about the currently read row */
typedef struct st_maria_row
{
  MARIA_BITMAP_BLOCKS insert_blocks;
  MARIA_BITMAP_BUFFER *extents;
  MARIA_RECORD_POS lastpos, nextpos;
  MARIA_RECORD_POS *tail_positions;
  ha_checksum checksum;
  LSN orig_undo_lsn;			/* Lsn at start of row insert */
  TrID trid;                            /* Transaction id for current row */
  uchar *empty_bits, *field_lengths;
  uint *null_field_lengths;             /* All null field lengths */
  ulong *blob_lengths;                  /* Length for each blob */
  ulong min_length, normal_length, char_length, varchar_length;
  ulong blob_length, total_length;
  size_t extents_buffer_length;         /* Size of 'extents' buffer */
  uint head_length, header_length;
  uint field_lengths_length;            /* Length of data in field_lengths */
  uint extents_count;                   /* number of extents in 'extents' */
  uint full_page_count, tail_count;     /* For maria_chk */
  uint space_on_head_page;
} MARIA_ROW;

/* Data to scan row in blocked format */
typedef struct st_maria_block_scan
{
  uchar *bitmap_buff, *bitmap_pos, *bitmap_end, *page_buff;
  uchar *dir, *dir_end;
  pgcache_page_no_t bitmap_page, max_page;
  ulonglong bits;
  uint number_of_rows, bit_pos;
  MARIA_RECORD_POS row_base_page;
  ulonglong row_changes;
} MARIA_BLOCK_SCAN;


struct st_maria_handler
{
  MARIA_SHARE *s;			/* Shared between open:s */
  struct st_ma_transaction *trn;        /* Pointer to active transaction */
  struct st_maria_handler *trn_next,**trn_prev;
  MARIA_STATUS_INFO *state, state_save;
  MARIA_STATUS_INFO *state_start;       /* State at start of transaction */
  MARIA_USED_TABLES *used_tables;
  struct ms3_st *s3;
  void **stack_end_ptr;
  MARIA_ROW cur_row;                    /* The active row that we just read */
  MARIA_ROW new_row;			/* Storage for a row during update */
  MARIA_KEY last_key;                   /* Last found key */
  MARIA_BLOCK_SCAN scan, *scan_save;
  MARIA_BLOB *blobs;			/* Pointer to blobs */
  MARIA_BIT_BUFF bit_buff;
  DYNAMIC_ARRAY bitmap_blocks;
  DYNAMIC_ARRAY pinned_pages;
  /* accumulate indexfile changes between write's */
  TREE *bulk_insert;
  LEX_CUSTRING *log_row_parts;		/* For logging */
  DYNAMIC_ARRAY *ft1_to_ft2;		/* used only in ft1->ft2 conversion */
  MEM_ROOT      ft_memroot;             /* used by the parser               */
  MYSQL_FTPARSER_PARAM *ftparser_param;	/* share info between init/deinit */
  void *external_ref;			/* For MariaDB TABLE */
  uchar *buff;				/* page buffer */
  uchar *keyread_buff;                   /* Buffer for last key read */
  uchar *lastkey_buff;			/* Last used search key */
  uchar *lastkey_buff2;
  uchar *first_mbr_key;			/* Searhed spatial key */
  uchar *rec_buff;			/* Temp buffer for recordpack */
  uchar *blob_buff;                     /* Temp buffer for blobs */
  uchar *int_keypos;			/* Save position for next/previous */
  uchar *int_maxpos;			/* -""- */
  uint keypos_offset;                   /* Tmp storage for offset int_keypos */
  uint maxpos_offset;          		/* Tmp storage for offset int_maxpos */
  uchar *update_field_data;		/* Used by update in rows-in-block */
  uint int_nod_flag;			/* -""- */
  uint32 int_keytree_version;		/* -""- */
  int (*read_record)(MARIA_HA *, uchar*, MARIA_RECORD_POS);
  invalidator_by_filename invalidator;	/* query cache invalidator */
  ulonglong last_auto_increment;        /* auto value at start of statement */
  ulonglong row_changes;                /* Incremented for each change */
  ulonglong start_row_changes;          /* Row changes since start trans */
  ulong this_unique;			/* uniq filenumber or thread */
  ulong last_unique;			/* last unique number */
  ulong this_loop;			/* counter for this open */
  ulong last_loop;			/* last used counter */
  MARIA_RECORD_POS save_lastpos;
  MARIA_RECORD_POS dup_key_pos;
  TrID             dup_key_trid;
  my_off_t pos;				/* Intern variable */
  my_off_t last_keypage;		/* Last key page read */
  my_off_t last_search_keypage;		/* Last keypage when searching */

  /*
    QQ: the following two xxx_length fields should be removed,
     as they are not compatible with parallel repair
  */
  ulong packed_length, blob_length;	/* Length of found, packed record */
  size_t rec_buff_size, blob_buff_size;
  PAGECACHE_FILE dfile;			/* The datafile */
  IO_CACHE rec_cache;			/* When cacheing records */
  LIST open_list;
  LIST share_list;
  MY_BITMAP changed_fields;
  ulong row_base_length;                /* Length of row header */
  uint row_flag;                        /* Flag to store in row header */
  uint opt_flag;			/* Optim. for space/speed */
  uint open_flags;                      /* Flags used in open() */
  uint update;				/* If file changed since open */
  uint error_count;                     /* Incremented for each error given */
  int lastinx;				/* Last used index */
  uint last_rkey_length;		/* Last length in maria_rkey() */
  uint *last_rtree_keypos;              /* Last key positions for rtrees */
  uint bulk_insert_ref_length;          /* Length of row ref during bi */
  uint non_flushable_state;
  enum ha_rkey_function last_key_func;	/* CONTAIN, OVERLAP, etc */
  uint save_lastkey_data_length;
  uint save_lastkey_ref_length;
  uint pack_key_length;			/* For MARIA_MRG */
  myf lock_wait;			/* is 0 or MY_SHORT_WAIT */
  int errkey;				/* Got last error on this key */
  int lock_type;			/* How database was locked */
  int tmp_lock_type;			/* When locked by readinfo */
  uint data_changed;			/* Somebody has changed data */
  uint save_update;			/* When using KEY_READ */
  int save_lastinx;
  uint preload_buff_size;		/* When preloading indexes */
  uint16 last_used_keyseg;              /* For MARIAMRG */
  uint8 key_del_used;                   /* != 0 if key_del is used */
  my_bool was_locked;			/* Was locked in panic */
  my_bool intern_lock_locked;           /* locked in ma_extra() */
  my_bool append_insert_at_end;		/* Set if concurrent insert */
  my_bool quick_mode;
  my_bool in_check_table;                /* We are running check tables */
  /* Marker if key_del_changed */
  /* If info->keyread_buff can't be used for rnext */
  my_bool page_changed;
  /* If info->keyread_buff has to be re-read for rnext */
  my_bool keyread_buff_used;
  my_bool once_flags;			/* For MARIA_MRG */
  /* For bulk insert enable/disable transactions control */
  my_bool switched_transactional;
  /* If transaction will autocommit */
  my_bool autocommit;
  my_bool has_cond_pushdown;
#ifdef _WIN32
  my_bool owned_by_merge;               /* This Maria table is part of a merge union */
#endif
  THR_LOCK_DATA lock;
  uchar *maria_rtree_recursion_state;	/* For RTREE */
  uchar length_buff[5];			/* temp buff to store blob lengths */
  int maria_rtree_recursion_depth;

  my_bool create_unique_index_by_sort;
  index_cond_func_t index_cond_func;   /* Index condition function */
  void *index_cond_func_arg;           /* parameter for the func */
  rowid_filter_func_t rowid_filter_func;   /* rowid filter check function */
  void *rowid_filter_func_arg;         /* parameter for the func */
};

/* Table options for the Aria and S3 storage engine */

struct ha_table_option_struct
{
  ulonglong s3_block_size;
  uint compression_algorithm;
};

/* Some defines used by maria-functions */

#define USE_WHOLE_KEY	65535         /* Use whole key in _search() */
#define F_EXTRA_LCK	-1

/* bits in opt_flag */
#define MEMMAP_USED	32U
#define REMEMBER_OLD_POS 64U

#define WRITEINFO_UPDATE_KEYFILE	1U
#define WRITEINFO_NO_UNLOCK		2U

/* once_flags */
#define USE_PACKED_KEYS         1U
#define RRND_PRESERVE_LASTINX   2U

/* bits in state.changed */

#define STATE_CHANGED		 1U
#define STATE_CRASHED		 2U
#define STATE_CRASHED_ON_REPAIR  4U
#define STATE_NOT_ANALYZED	 8U
#define STATE_NOT_OPTIMIZED_KEYS 16U
#define STATE_NOT_SORTED_PAGES	 32U
#define STATE_NOT_OPTIMIZED_ROWS 64U
#define STATE_NOT_ZEROFILLED     128U
#define STATE_NOT_MOVABLE        256U
#define STATE_MOVED              512U /* set if base->uuid != maria_uuid */
#define STATE_IN_REPAIR  	 1024U /* We are running repair on table */
#define STATE_CRASHED_PRINTED	 2048U
#define STATE_DATA_FILE_FULL     4096U
#define STATE_HAS_LSN            8192U /* Some page still has LSN */

#define STATE_CRASHED_FLAGS (STATE_CRASHED | STATE_CRASHED_ON_REPAIR | STATE_CRASHED_PRINTED)

/* options to maria_read_cache */

#define READING_NEXT	1U
#define READING_HEADER	2U

/* Number of bytes on key pages to indicate used size */
#define KEYPAGE_USED_SIZE  2U
#define KEYPAGE_KEYID_SIZE 1U
#define KEYPAGE_FLAG_SIZE  1U
#define KEYPAGE_KEY_VERSION_SIZE 4U /* encryption */
#define KEYPAGE_CHECKSUM_SIZE 4U
#define MAX_KEYPAGE_HEADER_SIZE (LSN_STORE_SIZE + KEYPAGE_USED_SIZE + \
                                 KEYPAGE_KEYID_SIZE + KEYPAGE_FLAG_SIZE + \
                                 TRANSID_SIZE + KEYPAGE_KEY_VERSION_SIZE)
#define KEYPAGE_FLAG_ISNOD      1U
#define KEYPAGE_FLAG_HAS_TRANSID 2U

#define _ma_get_page_used(share,x) \
  ((uint) mi_uint2korr((x) + (share)->keypage_header - KEYPAGE_USED_SIZE))
#define _ma_store_page_used(share,x,y) \
  mi_int2store((x) + (share)->keypage_header - KEYPAGE_USED_SIZE, (y))
#define _ma_get_keypage_flag(share,x) x[(share)->keypage_header - KEYPAGE_USED_SIZE - KEYPAGE_FLAG_SIZE]
#define _ma_test_if_nod(share,x) \
  ((_ma_get_keypage_flag(share,x) & KEYPAGE_FLAG_ISNOD) ? (share)->base.key_reflength : 0)

#define _ma_store_keynr(share, x, nr) x[(share)->keypage_header - KEYPAGE_KEYID_SIZE - KEYPAGE_FLAG_SIZE - KEYPAGE_USED_SIZE]= (nr)
#define _ma_get_keynr(share, x) ((uchar) x[(share)->keypage_header - KEYPAGE_KEYID_SIZE - KEYPAGE_FLAG_SIZE - KEYPAGE_USED_SIZE])
#define _ma_store_transid(buff, transid) \
  transid_store((buff) + LSN_STORE_SIZE, (transid))
#define _ma_korr_transid(buff) \
  transid_korr((buff) + LSN_STORE_SIZE)
#define _ma_store_keypage_flag(share,x,flag) x[(share)->keypage_header - KEYPAGE_USED_SIZE - KEYPAGE_FLAG_SIZE]= (flag)
#define _ma_mark_page_with_transid(share, page) \
  do { (page)->flag|= KEYPAGE_FLAG_HAS_TRANSID;                        \
    (page)->buff[(share)->keypage_header - KEYPAGE_USED_SIZE - KEYPAGE_FLAG_SIZE]= (page)->flag; } while (0)

#define KEYPAGE_KEY_VERSION(share, x) ((x) + \
                                       (share)->keypage_header -        \
                                       (KEYPAGE_USED_SIZE +             \
                                        KEYPAGE_FLAG_SIZE +             \
                                        KEYPAGE_KEYID_SIZE +            \
                                        KEYPAGE_KEY_VERSION_SIZE))

#define _ma_get_key_version(share,x) \
  ((uint) uint4korr(KEYPAGE_KEY_VERSION((share), (x))))

#define _ma_store_key_version(share,x,kv) \
  int4store(KEYPAGE_KEY_VERSION((share), (x)), (kv))

/*
  TODO: write int4store_aligned as *((uint32 *) (T))= (uint32) (A) for
  architectures where it is possible
*/
#define int4store_aligned(A,B) int4store((A),(B))

#define maria_mark_crashed(x) do{(x)->s->state.changed|= STATE_CRASHED; \
    (x)->s->crash_error= my_errno;                                      \
    DBUG_PRINT("error", ("Marked table crashed"));                      \
  }while(0)
#define maria_mark_crashed_share(x)                                     \
  do{(x)->state.changed|= STATE_CRASHED;                                \
    (x)->crash_error= my_errno;                                         \
    DBUG_PRINT("error", ("Marked table crashed"));                      \
  }while(0)
#define maria_mark_crashed_on_repair(x) do{(x)->s->state.changed|=      \
      STATE_CRASHED|STATE_CRASHED_ON_REPAIR;                            \
    (x)->update|= HA_STATE_CHANGED;                                     \
    (x)->s->crash_error= my_errno;                                      \
    DBUG_PRINT("error", ("Marked table crashed on repair"));            \
  }while(0)
#define maria_mark_in_repair(x) do{(x)->s->state.changed|=      \
      STATE_CRASHED | STATE_IN_REPAIR;                          \
    (x)->s->crash_error= my_errno;                              \
    (x)->update|= HA_STATE_CHANGED;                             \
    DBUG_PRINT("error", ("Marked table crashed for repair"));   \
  }while(0)
#define maria_is_crashed(x) ((x)->s->state.changed & STATE_CRASHED)
#define maria_is_crashed_on_repair(x) ((x)->s->state.changed & STATE_CRASHED_ON_REPAIR)
#define maria_in_repair(x) ((x)->s->state.changed & STATE_IN_REPAIR)

#define DBUG_DUMP_KEY(name, key) DBUG_DUMP(name, (key)->data, (key)->data_length + (key)->ref_length)

/* Functions to store length of space packed keys, VARCHAR or BLOB keys */

#define store_key_length(key,length) \
{ if ((length) < 255) \
  { *(key)=(length); } \
  else \
  { *(key)=255; mi_int2store((key)+1,(length)); } \
}

#define get_key_full_length(length,key) \
  { if (*(const uchar*) (key) != 255)            \
    length= ((uint) *(const uchar*) ((key)++))+1; \
  else \
  { length=mi_uint2korr((key)+1)+3; (key)+=3; } \
}

#define get_key_full_length_rdonly(length,key) \
{ if (*(const uchar*) (key) != 255) \
    length= ((uint) *(const uchar*) ((key)))+1; \
  else \
  { length=mi_uint2korr((key)+1)+3; } \
}

#define _ma_max_key_length() ((maria_block_size - MAX_KEYPAGE_HEADER_SIZE)/3 - MARIA_INDEX_OVERHEAD_SIZE)
#define get_pack_length(length) ((length) >= 255 ? 3 : 1)
#define _ma_have_versioning(info) ((info)->row_flag & ROW_FLAG_TRANSID)

#define MARIA_MIN_BLOCK_LENGTH	20		/* Because of delete-link */
/* Don't use to small record-blocks */
#define MARIA_EXTEND_BLOCK_LENGTH	20
#define MARIA_SPLIT_LENGTH	((MARIA_EXTEND_BLOCK_LENGTH+4)*2)
	/* Max prefix of record-block */
#define MARIA_MAX_DYN_BLOCK_HEADER	20
#define MARIA_BLOCK_INFO_HEADER_LENGTH 20
#define MARIA_DYN_DELETE_BLOCK_HEADER 20    /* length of delete-block-header */
#define MARIA_DYN_MAX_BLOCK_LENGTH	((1L << 24)-4L)
#define MARIA_DYN_MAX_ROW_LENGTH	(MARIA_DYN_MAX_BLOCK_LENGTH - MARIA_SPLIT_LENGTH)
#define MARIA_DYN_ALIGN_SIZE	  4	/* Align blocks on this */
#define MARIA_MAX_DYN_HEADER_BYTE 13	/* max header uchar for dynamic rows */
#define MARIA_MAX_BLOCK_LENGTH	((((ulong) 1 << 24)-1) & (~ (ulong) (MARIA_DYN_ALIGN_SIZE-1)))
#define MARIA_REC_BUFF_OFFSET      ALIGN_SIZE(MARIA_DYN_DELETE_BLOCK_HEADER+sizeof(uint32))

#define MEMMAP_EXTRA_MARGIN	7	/* Write this as a suffix for file */

#define PACK_TYPE_SELECTED	1U	/* Bits in field->pack_type */
#define PACK_TYPE_SPACE_FIELDS	2U
#define PACK_TYPE_ZERO_FILL	4U

#define MARIA_FOUND_WRONG_KEY INT_MAX32	/* Impossible value from ha_key_cmp */

#define MARIA_BLOCK_SIZE(key_length,data_pointer,key_pointer,block_size)  (((((key_length)+(data_pointer)+(key_pointer))*4+(key_pointer)+2)/(block_size)+1)*(block_size))
#define MARIA_MAX_KEYPTR_SIZE	5	/* For calculating block lengths */

/* Marker for impossible delete link */
#define IMPOSSIBLE_PAGE_NO 0xFFFFFFFFFFLL

/* The UNIQUE check is done with a hashed long key */

#define MARIA_UNIQUE_HASH_TYPE	HA_KEYTYPE_ULONG_INT
#define maria_unique_store(A,B)    mi_int4store((A),(B))

extern mysql_mutex_t THR_LOCK_maria;
#ifdef DONT_USE_RW_LOCKS
#define mysql_rwlock_wrlock(A) {}
#define mysql_rwlock_rdlock(A) {}
#define mysql_rwlock_unlock(A) {}
#endif

/* Some tuning parameters */
#define MARIA_MIN_KEYBLOCK_LENGTH 50	/* When to split delete blocks */
#define MARIA_MIN_SIZE_BULK_INSERT_TREE 16384U	/* this is per key */
#define MARIA_MIN_ROWS_TO_USE_BULK_INSERT 100
#define MARIA_MIN_ROWS_TO_DISABLE_INDEXES 100
#define MARIA_MIN_ROWS_TO_USE_WRITE_CACHE 10
/* Keep a small buffer for tables only using small blobs */
#define MARIA_SMALL_BLOB_BUFFER 1024U
#define MARIA_MAX_CONTROL_FILE_LOCK_RETRY 30     /* Retry this many times */

/* Some extern variables */
extern LIST *maria_open_list;
extern uchar maria_file_magic[], maria_pack_file_magic[];
extern uchar maria_uuid[MY_UUID_SIZE];
extern uint32 maria_read_vec[], maria_readnext_vec[];
extern uint maria_quick_table_bits;
extern const char *maria_data_root;
extern uchar maria_zero_string[];
extern my_bool maria_inited, maria_in_ha_maria, maria_recovery_changed_data;
extern my_bool maria_recovery_verbose, maria_checkpoint_disabled;
extern my_bool maria_assert_if_crashed_table, aria_readonly;
extern uint maria_checkpoint_min_log_activity;
extern HASH maria_stored_state;
extern int (*maria_create_trn_hook)(MARIA_HA *);
extern my_bool (*ma_killed)(MARIA_HA *);
extern void (*ma_debug_crash_here)(const char *keyword);

#ifdef HAVE_PSI_INTERFACE
extern PSI_mutex_key key_SHARE_BITMAP_lock, key_SORT_INFO_mutex,
                     key_THR_LOCK_maria, key_TRANSLOG_BUFFER_mutex,
                     key_LOCK_soft_sync,
                     key_TRANSLOG_DESCRIPTOR_dirty_buffer_mask_lock,
                     key_TRANSLOG_DESCRIPTOR_sent_to_disk_lock,
                     key_TRANSLOG_DESCRIPTOR_log_flush_lock,
                     key_TRANSLOG_DESCRIPTOR_file_header_lock,
                     key_TRANSLOG_DESCRIPTOR_unfinished_files_lock,
                     key_TRANSLOG_DESCRIPTOR_purger_lock,
                     key_SHARE_intern_lock, key_SHARE_key_del_lock,
                     key_SHARE_close_lock,
                     key_SERVICE_THREAD_CONTROL_lock,
                     key_PAGECACHE_cache_lock;

extern PSI_mutex_key key_CRYPT_DATA_lock;

extern PSI_cond_key key_SHARE_key_del_cond, key_SERVICE_THREAD_CONTROL_cond,
                    key_SORT_INFO_cond, key_SHARE_BITMAP_cond,
                    key_COND_soft_sync, key_TRANSLOG_BUFFER_waiting_filling_buffer,
                    key_TRANSLOG_BUFFER_prev_sent_to_disk_cond,
                    key_TRANSLOG_DESCRIPTOR_log_flush_cond,
                    key_TRANSLOG_DESCRIPTOR_new_goal_cond;

extern PSI_rwlock_key key_KEYINFO_root_lock, key_SHARE_mmap_lock,
                      key_TRANSLOG_DESCRIPTOR_open_files_lock;

extern PSI_thread_key key_thread_checkpoint, key_thread_find_all_keys,
                      key_thread_soft_sync;

extern PSI_file_key key_file_translog, key_file_kfile, key_file_dfile,
                    key_file_control, key_file_tmp;

#endif

/* Note that PSI_stage_info globals must always be declared. */
extern PSI_stage_info stage_waiting_for_a_resource;

/* This is used by _ma_calc_xxx_key_length och _ma_store_key */
typedef struct st_maria_s_param
{
  const uchar *key;
  uchar *prev_key, *next_key_pos;
  uchar *key_pos;                               /* For balance page */
  uint ref_length, key_length, n_ref_length;
  uint n_length, totlength, part_of_prev_key, prev_length, pack_marker;
  uint changed_length;
  int move_length;                              /* For balance_page */
  my_bool store_not_null;
} MARIA_KEY_PARAM;


/* Used to store reference to pinned page */
typedef struct st_pinned_page
{
  PAGECACHE_BLOCK_LINK *link;
  enum pagecache_page_lock unlock, write_lock;
  my_bool changed;
} MARIA_PINNED_PAGE;


/* Keeps all information about a page and related to a page */
typedef struct st_maria_page
{
  MARIA_HA *info;
  const MARIA_KEYDEF *keyinfo;
  uchar *buff;				/* Data for page */
  my_off_t pos;                         /* Disk address to page */
  uint     size;                        /* Size of data on page */
  uint     org_size;                    /* Size of page at read or after log */
  uint     node;      			/* 0 or share->base.key_reflength */
  uint     flag;			/* Page flag */
  uint     link_offset;
} MARIA_PAGE;


/* Prototypes for intern functions */
extern int _ma_read_dynamic_record(MARIA_HA *, uchar *, MARIA_RECORD_POS);
extern int _ma_read_rnd_dynamic_record(MARIA_HA *, uchar *, MARIA_RECORD_POS,
                                       my_bool);
extern my_bool _ma_write_dynamic_record(MARIA_HA *, const uchar *);
extern my_bool _ma_update_dynamic_record(MARIA_HA *, MARIA_RECORD_POS,
                                         const uchar *, const uchar *);
extern my_bool _ma_delete_dynamic_record(MARIA_HA *info, const uchar *record);
extern my_bool _ma_cmp_dynamic_record(MARIA_HA *info, const uchar *record);
extern my_bool _ma_write_blob_record(MARIA_HA *, const uchar *);
extern my_bool _ma_update_blob_record(MARIA_HA *, MARIA_RECORD_POS,
                                      const uchar *, const uchar *);
extern int _ma_read_static_record(MARIA_HA *info, uchar *, MARIA_RECORD_POS);
extern int _ma_read_rnd_static_record(MARIA_HA *, uchar *, MARIA_RECORD_POS,
                                      my_bool);
extern my_bool _ma_write_static_record(MARIA_HA *, const uchar *);
extern my_bool _ma_update_static_record(MARIA_HA *, MARIA_RECORD_POS,
                                        const uchar *, const uchar *);
extern my_bool _ma_delete_static_record(MARIA_HA *info, const uchar *record);
extern my_bool _ma_cmp_static_record(MARIA_HA *info, const uchar *record);

extern my_bool _ma_write_no_record(MARIA_HA *info, const uchar *record);
extern my_bool _ma_update_no_record(MARIA_HA *info, MARIA_RECORD_POS pos,
                                    const uchar *oldrec, const uchar *record);
extern my_bool _ma_delete_no_record(MARIA_HA *info, const uchar *record);
extern int _ma_read_no_record(MARIA_HA *info, uchar *record,
                              MARIA_RECORD_POS pos);
extern int _ma_read_rnd_no_record(MARIA_HA *info, uchar *buf,
                                  MARIA_RECORD_POS filepos,
                                  my_bool skip_deleted_blocks);
my_off_t _ma_no_keypos_to_recpos(MARIA_SHARE *share, my_off_t pos);
/* Get position to last record */
static inline MARIA_RECORD_POS maria_position(MARIA_HA *info)
{
  return info->cur_row.lastpos;
}
extern my_bool _ma_ck_write(MARIA_HA *info, MARIA_KEY *key);
extern my_bool _ma_enlarge_root(MARIA_HA *info, MARIA_KEY *key,
                                MARIA_RECORD_POS *root);
int _ma_insert(MARIA_HA *info, MARIA_KEY *key,
               MARIA_PAGE *anc_page, uchar *key_pos, uchar *key_buff,
               MARIA_PAGE *father_page, uchar *father_key_pos,
               my_bool insert_last);
extern my_bool _ma_ck_real_write_btree(MARIA_HA *info, MARIA_KEY *key,
                                   MARIA_RECORD_POS *root, uint32 comp_flag);
extern int _ma_split_page(MARIA_HA *info, MARIA_KEY *key,
                          MARIA_PAGE *split_page,
                          uint org_split_length,
                          uchar *inserted_key_pos, uint changed_length,
                          int move_length,
                          uchar *key_buff, my_bool insert_last_key);
extern uchar *_ma_find_half_pos(MARIA_KEY *key, MARIA_PAGE *page,
                                uchar ** after_key);
extern int _ma_calc_static_key_length(const MARIA_KEY *key, uint nod_flag,
                                      uchar *key_pos, uchar *org_key,
                                      uchar *key_buff,
                                      MARIA_KEY_PARAM *s_temp);
extern int _ma_calc_var_key_length(const MARIA_KEY *key, uint nod_flag,
                                   uchar *key_pos, uchar *org_key,
                                   uchar *key_buff,
                                   MARIA_KEY_PARAM *s_temp);
extern int _ma_calc_var_pack_key_length(const MARIA_KEY *key,
                                        uint nod_flag, uchar *next_key,
                                        uchar *org_key, uchar *prev_key,
                                        MARIA_KEY_PARAM *s_temp);
extern int _ma_calc_bin_pack_key_length(const MARIA_KEY *key,
                                        uint nod_flag, uchar *next_key,
                                        uchar *org_key, uchar *prev_key,
                                        MARIA_KEY_PARAM *s_temp);
extern void _ma_store_static_key(MARIA_KEYDEF *keyinfo, uchar *key_pos,
                                 MARIA_KEY_PARAM *s_temp);
extern void _ma_store_var_pack_key(MARIA_KEYDEF *keyinfo, uchar *key_pos,
                                   MARIA_KEY_PARAM *s_temp);
#ifdef NOT_USED
extern void _ma_store_pack_key(MARIA_KEYDEF *keyinfo, uchar *key_pos,
                               MARIA_KEY_PARAM *s_temp);
#endif
extern void _ma_store_bin_pack_key(MARIA_KEYDEF *keyinfo, uchar *key_pos,
                                   MARIA_KEY_PARAM *s_temp);

extern my_bool _ma_ck_delete(MARIA_HA *info, MARIA_KEY *key);
extern my_bool _ma_ck_real_delete(MARIA_HA *info, MARIA_KEY *key,
                                  my_off_t *root);
extern int _ma_readinfo(MARIA_HA *info, int lock_flag, int check_keybuffer);
extern int _ma_writeinfo(MARIA_HA *info, uint options);
extern int _ma_test_if_changed(MARIA_HA *info);
extern int _ma_mark_file_changed(MARIA_SHARE *info);
extern int _ma_mark_file_changed_now(MARIA_SHARE *info);
extern void _ma_mark_file_crashed(MARIA_SHARE *share);
extern void _ma_set_fatal_error(MARIA_HA *share, int error);
extern void _ma_set_fatal_error_with_share(MARIA_SHARE *share, int error);
extern my_bool _ma_set_uuid(MARIA_SHARE *info, my_bool reset_uuid);
extern my_bool _ma_check_if_zero(uchar *pos, size_t size);
extern int _ma_decrement_open_count(MARIA_HA *info, my_bool lock_table);
extern int _ma_check_index(MARIA_HA *info, int inx);
extern int _ma_search(MARIA_HA *info, MARIA_KEY *key, uint32 nextflag,
                      my_off_t pos);
extern int _ma_bin_search(const MARIA_KEY *key, const MARIA_PAGE *page,
                          uint32 comp_flag, uchar **ret_pos, uchar *buff,
                          my_bool *was_last_key);
extern int _ma_seq_search(const MARIA_KEY *key, const MARIA_PAGE *page,
                          uint comp_flag, uchar ** ret_pos, uchar *buff,
                          my_bool *was_last_key);
extern int _ma_prefix_search(const MARIA_KEY *key, const MARIA_PAGE *page,
                             uint32 comp_flag, uchar ** ret_pos, uchar *buff,
                             my_bool *was_last_key);
extern my_off_t _ma_kpos(uint nod_flag, const uchar *after_key);
extern void _ma_kpointer(MARIA_HA *info, uchar *buff, my_off_t pos);
MARIA_RECORD_POS _ma_row_pos_from_key(const MARIA_KEY *key);
TrID _ma_trid_from_key(const MARIA_KEY *key);
extern MARIA_RECORD_POS _ma_rec_pos(MARIA_SHARE *share, uchar *ptr);
extern void _ma_dpointer(MARIA_SHARE *share, uchar *buff,
                         MARIA_RECORD_POS pos);
extern uint _ma_get_static_key(MARIA_KEY *key, uint page_flag, uint nod_flag,
                               uchar **page);
extern uchar *_ma_skip_static_key(MARIA_KEY *key, uint page_flag,
                           uint nod_flag, uchar *page);
extern uint _ma_get_pack_key(MARIA_KEY *key, uint page_flag, uint nod_flag,
                             uchar **page);
extern uchar *_ma_skip_pack_key(MARIA_KEY *key, uint page_flag,
                                uint nod_flag, uchar *page);
extern uint _ma_get_binary_pack_key(MARIA_KEY *key, uint page_flag,
                                    uint nod_flag, uchar **page_pos);
uchar *_ma_skip_binary_pack_key(MARIA_KEY *key, uint page_flag,
                                uint nod_flag, uchar *page);
extern uchar *_ma_get_last_key(MARIA_KEY *key, MARIA_PAGE *page,
                               uchar *endpos);
extern uchar *_ma_get_key(MARIA_KEY *key, MARIA_PAGE *page, uchar *keypos);
extern uint _ma_keylength(MARIA_KEYDEF *keyinfo, const uchar *key);
extern uint _ma_keylength_part(MARIA_KEYDEF *keyinfo, const uchar *key,
                               HA_KEYSEG *end);
extern int _ma_search_next(MARIA_HA *info, MARIA_KEY *key,
                           uint32 nextflag, my_off_t pos);
extern int _ma_search_first(MARIA_HA *info, MARIA_KEYDEF *keyinfo,
                            my_off_t pos);
extern int _ma_search_last(MARIA_HA *info, MARIA_KEYDEF *keyinfo,
                           my_off_t pos);
extern my_off_t _ma_static_keypos_to_recpos(MARIA_SHARE *share, my_off_t pos);
extern my_off_t _ma_static_recpos_to_keypos(MARIA_SHARE *share, my_off_t pos);
extern my_off_t _ma_transparent_recpos(MARIA_SHARE *share, my_off_t pos);
extern my_off_t _ma_transaction_keypos_to_recpos(MARIA_SHARE *, my_off_t pos);
extern my_off_t _ma_transaction_recpos_to_keypos(MARIA_SHARE *, my_off_t pos);

extern void _ma_page_setup(MARIA_PAGE *page, MARIA_HA *info,
                           const MARIA_KEYDEF *keyinfo, my_off_t pos,
                           uchar *buff);
extern my_bool _ma_fetch_keypage(MARIA_PAGE *page, MARIA_HA *info,
                                 const MARIA_KEYDEF *keyinfo,
                                 my_off_t pos, enum pagecache_page_lock lock,
                                 int level, uchar *buff,
                                 my_bool return_buffer);
extern my_bool _ma_write_keypage(MARIA_PAGE *page,
                                 enum pagecache_page_lock lock, int level);
extern int _ma_dispose(MARIA_HA *info, my_off_t pos, my_bool page_not_read);
extern my_off_t _ma_new(MARIA_HA *info, int level,
                        MARIA_PINNED_PAGE **page_link);
extern my_bool _ma_compact_keypage(MARIA_PAGE *page, TrID min_read_from);
extern uint transid_store_packed(MARIA_HA *info, uchar *to, ulonglong trid);
extern ulonglong transid_get_packed(MARIA_SHARE *share, const uchar *from);
#define transid_packed_length(data) \
  ((data)[0] < MARIA_MIN_TRANSID_PACK_OFFSET ? 1 : \
   (uint) ((uchar) (data)[0]) - (MARIA_TRANSID_PACK_OFFSET - 1))
#define key_has_transid(key) (*(key) & 1)

#define page_mark_changed(info, page) \
  dynamic_element(&(info)->pinned_pages, (page)->link_offset,            \
                  MARIA_PINNED_PAGE*)->changed= 1;
#define page_store_size(share, page)                           \
  _ma_store_page_used((share), (page)->buff, (page)->size);
#define page_store_info(share, page)                           \
  _ma_store_keypage_flag((share), (page)->buff, (page)->flag); \
  _ma_store_page_used((share), (page)->buff, (page)->size);
#ifdef IDENTICAL_PAGES_AFTER_RECOVERY
void page_cleanup(MARIA_SHARE *share, MARIA_PAGE *page)
#else
#define page_cleanup(A,B) do { } while (0)
#endif

extern MARIA_KEY *_ma_make_key(MARIA_HA *info, MARIA_KEY *int_key, uint keynr,
                               uchar *key, const uchar *record,
                               MARIA_RECORD_POS filepos, ulonglong trid);
extern MARIA_KEY *_ma_pack_key(MARIA_HA *info, MARIA_KEY *int_key,
                               uint keynr, uchar *key,
                               const uchar *old, key_part_map keypart_map,
                               HA_KEYSEG ** last_used_keyseg);
extern void _ma_copy_key(MARIA_KEY *to, const MARIA_KEY *from);
extern int _ma_read_key_record(MARIA_HA *info, uchar *buf, MARIA_RECORD_POS);
extern my_bool _ma_read_cache(MARIA_HA *, IO_CACHE *info, uchar *buff,
                              MARIA_RECORD_POS pos, size_t length,
                              uint re_read_if_possibly);
extern ulonglong ma_retrieve_auto_increment(const uchar *key, uint8 key_type);
extern my_bool _ma_alloc_buffer(uchar **old_addr, size_t *old_size,
                                size_t new_size, myf flag);
extern size_t _ma_rec_unpack(MARIA_HA *info, uchar *to, uchar *from,
                            size_t reclength);
extern my_bool _ma_rec_check(MARIA_HA *info, const uchar *record,
                             uchar *packpos, ulong packed_length,
                             my_bool with_checkum, ha_checksum checksum);
extern int _ma_write_part_record(MARIA_HA *info, my_off_t filepos,
                                 ulong length, my_off_t next_filepos,
                                 uchar ** record, ulong *reclength,
                                 int *flag);
extern void _ma_print_key(FILE *stream, MARIA_KEY *key);
extern void _ma_print_keydata(FILE *stream, HA_KEYSEG *keyseg,
                              const uchar *key, uint length);
extern my_bool _ma_once_init_pack_row(MARIA_SHARE *share, File dfile);
extern my_bool _ma_once_end_pack_row(MARIA_SHARE *share);
extern int _ma_read_pack_record(MARIA_HA *info, uchar *buf,
                                MARIA_RECORD_POS filepos);
extern int _ma_read_rnd_pack_record(MARIA_HA *, uchar *, MARIA_RECORD_POS,
                                    my_bool);
extern int _ma_pack_rec_unpack(MARIA_HA *info, MARIA_BIT_BUFF *bit_buff,
                               uchar *to, uchar *from, ulong reclength);
extern ulonglong _ma_safe_mul(ulonglong a, ulonglong b);
extern int _ma_ft_update(MARIA_HA *info, uint keynr, uchar *keybuf,
                         const uchar *oldrec, const uchar *newrec,
                         my_off_t pos);

/*
  Parameter to _ma_get_block_info
  The dynamic row header is read into this struct. For an explanation of
  the fields, look at the function _ma_get_block_info().
*/

typedef struct st_maria_block_info
{
  uchar header[MARIA_BLOCK_INFO_HEADER_LENGTH];
  ulong rec_len;
  ulong data_len;
  ulong block_len;
  ulong blob_len;
  MARIA_RECORD_POS filepos;
  MARIA_RECORD_POS next_filepos;
  MARIA_RECORD_POS prev_filepos;
  uint second_read;
  uint offset;
} MARIA_BLOCK_INFO;


/* bits in return from _ma_get_block_info */

#define BLOCK_FIRST	1U
#define BLOCK_LAST	2U
#define BLOCK_DELETED	4U
#define BLOCK_ERROR	8U			/* Wrong data */
#define BLOCK_SYNC_ERROR 16U			/* Right data at wrong place */
#define BLOCK_FATAL_ERROR 32U			/* hardware-error */

#define NEED_MEM	((uint) 10*4*(IO_SIZE+32)+32) /* Nead for recursion */
#define MAXERR			20
#define BUFFERS_WHEN_SORTING	16		/* Alloc for sort-key-tree */
#define WRITE_COUNT		MY_HOW_OFTEN_TO_WRITE
#define INDEX_TMP_EXT		".TMM"
#define DATA_TMP_EXT		".TMD"

#define UPDATE_TIME		1U
#define UPDATE_STAT		2U
#define UPDATE_SORT		4U
#define UPDATE_AUTO_INC		8U
#define UPDATE_OPEN_COUNT	16U

/* We use MY_ALIGN_DOWN here mainly to ensure that we get stable values for mysqld --help ) */
#define PAGE_BUFFER_INIT	MY_ALIGN_DOWN(1024L*1024L*256L-MALLOC_OVERHEAD, 8192)
#define READ_BUFFER_INIT	MY_ALIGN_DOWN(1024L*256L-MALLOC_OVERHEAD, 1024)
#define SORT_BUFFER_INIT	MY_ALIGN_DOWN(1024L*1024L*256L-MALLOC_OVERHEAD, 1024)

#define fast_ma_writeinfo(INFO) if (!(INFO)->s->tot_locks) (void) _ma_writeinfo((INFO),0)
#define fast_ma_readinfo(INFO) ((INFO)->lock_type == F_UNLCK) && _ma_readinfo((INFO),F_RDLCK,1)

extern uint _ma_get_block_info(MARIA_HA *, MARIA_BLOCK_INFO *, File, my_off_t);
extern uint _ma_rec_pack(MARIA_HA *info, uchar *to, const uchar *from);
extern uint _ma_pack_get_block_info(MARIA_HA *maria, MARIA_BIT_BUFF *bit_buff,
                                    MARIA_BLOCK_INFO *info, uchar **rec_buff_p,
                                    size_t *rec_buff_size,
                                    File file, my_off_t filepos);
extern void _ma_store_blob_length(uchar *pos, uint pack_length, uint length);
extern void _ma_report_error(int errcode, const LEX_STRING *file_name,
                             myf flags);
extern void _ma_print_error(MARIA_HA *info, int error, my_bool write_to_log);
extern my_bool _ma_memmap_file(MARIA_HA *info);
extern void _ma_unmap_file(MARIA_HA *info);
extern uint _ma_save_pack_length(uint version, uchar * block_buff,
                                 ulong length);
extern uint _ma_calc_pack_length(uint version, ulong length);
extern ulong _ma_calc_blob_length(uint length, const uchar *pos);
extern size_t _ma_mmap_pread(MARIA_HA *info, uchar *Buffer,
			     size_t Count, my_off_t offset, myf MyFlags);
extern size_t _ma_mmap_pwrite(MARIA_HA *info, const uchar *Buffer,
			      size_t Count, my_off_t offset, myf MyFlags);
extern size_t _ma_nommap_pread(MARIA_HA *info, uchar *Buffer,
			       size_t Count, my_off_t offset, myf MyFlags);
extern size_t _ma_nommap_pwrite(MARIA_HA *info, const uchar *Buffer,
				size_t Count, my_off_t offset, myf MyFlags);

/* my_pwrite instead of my_write used */
#define MA_STATE_INFO_WRITE_DONT_MOVE_OFFSET 1
/* info should be written */
#define MA_STATE_INFO_WRITE_FULL_INFO        2
/* intern_lock taking is needed */
#define MA_STATE_INFO_WRITE_LOCK             4
uint _ma_state_info_write(MARIA_SHARE *share, uint pWrite)__attribute__((visibility("default"))) ;
uint _ma_state_info_write_sub(File file, MARIA_STATE_INFO *state, uint pWrite);
uint _ma_state_info_read_dsk(File file, MARIA_STATE_INFO *state);
uint _ma_base_info_write(File file, MARIA_BASE_INFO *base);
my_bool _ma_keyseg_write(File file, const HA_KEYSEG *keyseg);
uchar *_ma_keyseg_read(uchar *ptr, HA_KEYSEG *keyseg);
my_bool _ma_keydef_write(File file, MARIA_KEYDEF *keydef);
uchar *_ma_keydef_read(uchar *ptr, MARIA_KEYDEF *keydef);
my_bool _ma_uniquedef_write(File file, MARIA_UNIQUEDEF *keydef);
uchar *_ma_uniquedef_read(uchar *ptr, MARIA_UNIQUEDEF *keydef);
my_bool _ma_columndef_write(File file, MARIA_COLUMNDEF *columndef);
uchar *_ma_columndef_read(uchar *ptr, MARIA_COLUMNDEF *columndef);
my_bool _ma_column_nr_write(File file, uint16 *offsets, uint columns);
uchar *_ma_column_nr_read(uchar *ptr, uint16 *offsets, uint columns);
ulong _ma_calc_total_blob_length(MARIA_HA *info, const uchar *record);
ha_checksum _ma_checksum(MARIA_HA *info, const uchar *buf);
ha_checksum _ma_static_checksum(MARIA_HA *info, const uchar *buf);
my_bool _ma_check_unique(MARIA_HA *info, MARIA_UNIQUEDEF *def,
                         const uchar *record, ha_checksum unique_hash,
                         MARIA_RECORD_POS pos);
ha_checksum _ma_unique_hash(MARIA_UNIQUEDEF *def, const uchar *buf);
my_bool _ma_cmp_static_unique(MARIA_HA *info, MARIA_UNIQUEDEF *def,
                              const uchar *record, MARIA_RECORD_POS pos);
my_bool _ma_cmp_dynamic_unique(MARIA_HA *info, MARIA_UNIQUEDEF *def,
                               const uchar *record, MARIA_RECORD_POS pos);
my_bool _ma_unique_comp(MARIA_UNIQUEDEF *def, const uchar *a, const uchar *b,
                        my_bool null_are_equal);
void _ma_reset_status(MARIA_HA *maria);
int _ma_def_scan_remember_pos(MARIA_HA *info, MARIA_RECORD_POS *lastpos);
int _ma_def_scan_restore_pos(MARIA_HA *info, MARIA_RECORD_POS lastpos);

#include "ma_commit.h"

extern MARIA_HA *_ma_test_if_reopen(const char *filename);
my_bool _ma_check_table_is_closed(const char *name, const char *where);
int _ma_open_datafile(MARIA_HA *info, MARIA_SHARE *share);
int _ma_open_keyfile(MARIA_SHARE *share);
void _ma_setup_functions(MARIA_SHARE *share);
my_bool _ma_dynmap_file(MARIA_HA *info, my_off_t size);
void _ma_remap_file(MARIA_HA *info, my_off_t size);

MARIA_RECORD_POS _ma_write_init_default(MARIA_HA *info, const uchar *record);
my_bool _ma_write_abort_default(MARIA_HA *info);
int maria_delete_table_files(const char *name, my_bool temporary,
                             myf flags)__attribute__((visibility("default"))) ;

#define MARIA_FLUSH_DATA  1
#define MARIA_FLUSH_INDEX 2
int _ma_flush_table_files(MARIA_HA *info, uint flush_data_or_index,
                          enum flush_type flush_type_for_data,
                          enum flush_type flush_type_for_index);
/*
  Functions needed by _ma_check (are overridden in MySQL/ha_maria.cc).
  See ma_check_standalone.h .
*/
int _ma_killed_ptr(HA_CHECK *param);
void _ma_report_progress(HA_CHECK *param, ulonglong progress,
                         ulonglong max_progress);
void _ma_check_print_error(HA_CHECK *param, const char *fmt, ...)
  ATTRIBUTE_FORMAT(printf, 2, 3);
void _ma_check_print_warning(HA_CHECK *param, const char *fmt, ...)
  ATTRIBUTE_FORMAT(printf, 2, 3);
void _ma_check_print_info(HA_CHECK *param, const char *fmt, ...)
  ATTRIBUTE_FORMAT(printf, 2, 3);
my_bool write_log_record_for_repair(const HA_CHECK *param, MARIA_HA *info);

int _ma_flush_pending_blocks(MARIA_SORT_PARAM *param);
int _ma_sort_ft_buf_flush(MARIA_SORT_PARAM *sort_param);
int _ma_thr_write_keys(MARIA_SORT_PARAM *sort_param);
pthread_handler_t _ma_thr_find_all_keys(void *arg);

int _ma_sort_write_record(MARIA_SORT_PARAM *sort_param);
int _ma_create_index_by_sort(MARIA_SORT_PARAM *info, my_bool no_messages,
                             size_t);
int _ma_sync_table_files(const MARIA_HA *info);
int _ma_initialize_data_file(MARIA_SHARE *share, File dfile);
int _ma_update_state_lsns(MARIA_SHARE *share,
                          LSN lsn, TrID create_trid, my_bool do_sync,
                          my_bool update_create_rename_lsn);
int _ma_update_state_lsns_sub(MARIA_SHARE *share, LSN lsn,
                              TrID create_trid, my_bool do_sync,
                              my_bool update_create_rename_lsn);
void _ma_set_data_pagecache_callbacks(PAGECACHE_FILE *file,
                                      MARIA_SHARE *share);
void _ma_set_index_pagecache_callbacks(PAGECACHE_FILE *file,
                                       MARIA_SHARE *share);
void _ma_tmp_disable_logging_for_table(MARIA_HA *info,
                                       my_bool log_incomplete);
my_bool _ma_reenable_logging_for_table(MARIA_HA *info, my_bool flush_pages);
my_bool write_log_record_for_bulk_insert(MARIA_HA *info);
void _ma_unpin_all_pages(MARIA_HA *info, LSN undo_lsn);

#define MARIA_NO_CRC_NORMAL_PAGE 0xffffffff
#define MARIA_NO_CRC_BITMAP_PAGE 0xfffffffe
extern my_bool maria_page_crc_set_index(PAGECACHE_IO_HOOK_ARGS *args);
extern my_bool maria_page_crc_set_normal(PAGECACHE_IO_HOOK_ARGS *args);
extern my_bool maria_page_crc_check_bitmap(int, PAGECACHE_IO_HOOK_ARGS *args);
extern my_bool maria_page_crc_check_data(int, PAGECACHE_IO_HOOK_ARGS *args);
extern my_bool maria_page_crc_check_index(int, PAGECACHE_IO_HOOK_ARGS *args);
extern my_bool maria_page_crc_check_none(int, PAGECACHE_IO_HOOK_ARGS *args);
extern my_bool maria_page_crc_check(uchar *page, pgcache_page_no_t page_no,
                                    MARIA_SHARE *share, uint32 no_crc_val,
                                    int data_length);
extern my_bool maria_page_filler_set_bitmap(PAGECACHE_IO_HOOK_ARGS *args);
extern my_bool maria_page_filler_set_normal(PAGECACHE_IO_HOOK_ARGS *args);
extern my_bool maria_page_filler_set_none(PAGECACHE_IO_HOOK_ARGS *args);
extern void maria_page_write_failure(int error, PAGECACHE_IO_HOOK_ARGS *args);
extern my_bool maria_flush_log_for_page(PAGECACHE_IO_HOOK_ARGS *args);
extern my_bool maria_flush_log_for_page_none(PAGECACHE_IO_HOOK_ARGS *args);

extern PAGECACHE *maria_log_pagecache;
extern void ma_set_index_cond_func(MARIA_HA *info, index_cond_func_t func,
                                   void *func_arg);
extern void ma_set_rowid_filter_func(MARIA_HA *info,
                                     rowid_filter_func_t check_func,
                                     void *func_arg);
static inline void ma_reset_index_filter_functions(MARIA_HA *info)
{
  info->index_cond_func= NULL;
  info->rowid_filter_func= NULL;
  info->has_cond_pushdown= 0;
}
check_result_t ma_check_index_cond_real(MARIA_HA *info, uint keynr,
                                        uchar *record);
static inline check_result_t ma_check_index_cond(MARIA_HA *info, uint keynr,
                                                 uchar *record)
{
  if (!info->has_cond_pushdown)
    return CHECK_POS;
  return ma_check_index_cond_real(info, keynr, record);
}

extern void ma_update_pagecache_stats();
extern my_bool ma_yield_and_check_if_killed(MARIA_HA *info, int inx);
extern my_bool ma_killed_standalone(MARIA_HA *);

extern uint _ma_file_callback_to_id(void *callback_data);
extern uint _ma_write_flags_callback(void *callback_data, myf flags);
extern void free_maria_share(MARIA_SHARE *share);
extern int _ma_update_tmp_file_size(struct tmp_file_tracking *track,
                                    ulonglong file_size);

static inline void unmap_file(MARIA_HA *info __attribute__((unused)))
{
#ifdef HAVE_MMAP
  if (info->s->file_map)
    _ma_unmap_file(info);
#endif
}

static inline void decrement_share_in_trans(MARIA_SHARE *share)
{
  /* Internal tables doesn't have transactions */
  DBUG_ASSERT(!share->internal_table);
  if (!--share->in_trans)
    free_maria_share(share);
  else
    mysql_mutex_unlock(&share->intern_lock);
}

C_MODE_END
#endif

#define CRASH_IF_S3_TABLE(share) DBUG_ASSERT(!share->no_status_updates)
