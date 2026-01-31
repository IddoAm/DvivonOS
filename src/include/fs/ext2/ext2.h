#ifndef FS_EXT2_EXT2_H
#define FS_EXT2_EXT2_H

#include <stdint.h>
#include "fs/types.h"
#include "fs/vfs/superblock.h"
#include "fs/vfs/inode.h"
#include <drivers/block_device.h>

/* Ext2 Constants */
#define EXT2_SUPERBLOCK_OFFSET    1024
#define EXT2_SUPERBLOCK_SIZE      1024
#define EXT2_MAGIC                0xEF53
#define EXT2_ROOT_INO             2

/* Block sizes */
#define EXT2_MIN_BLOCK_SIZE       1024
#define EXT2_MAX_BLOCK_SIZE       4096

/* Revision levels */
#define EXT2_GOOD_OLD_REV         0
#define EXT2_DYNAMIC_REV          1

/* Feature flags */
#define EXT2_FEATURE_COMPAT_DIR_PREALLOC  0x0001
#define EXT2_FEATURE_COMPAT_IMAGIC_INODES 0x0002
#define EXT2_FEATURE_COMPAT_HAS_JOURNAL   0x0004
#define EXT2_FEATURE_COMPAT_EXT_ATTR      0x0008
#define EXT2_FEATURE_COMPAT_RESIZE_INO    0x0010
#define EXT2_FEATURE_COMPAT_DIR_INDEX     0x0020

/* Inode flags */
#define EXT2_SECRM_FL             0x00000001    /* Secure deletion */
#define EXT2_UNRM_FL              0x00000002    /* Undelete */
#define EXT2_COMPR_FL             0x00000004    /* Compress file */
#define EXT2_SYNC_FL              0x00000008    /* Synchronous updates */
#define EXT2_IMMUTABLE_FL         0x00000010    /* Immutable file */
#define EXT2_APPEND_FL            0x00000020    /* Append only */
#define EXT2_NODUMP_FL            0x00000040    /* Do not dump file */
#define EXT2_NOATIME_FL           0x00000080    /* Do not update access time */

/* Directory entry types */
#define EXT2_FT_UNKNOWN           0     /* Unknown file type */
#define EXT2_FT_REG_FILE          1     /* Regular file */
#define EXT2_FT_DIR               2     /* Directory */
#define EXT2_FT_CHRDEV            3     /* Character device */
#define EXT2_FT_BLKDEV            4     /* Block device */
#define EXT2_FT_FIFO              5     /* FIFO */
#define EXT2_FT_SOCK              6     /* Socket */
#define EXT2_FT_SYMLINK           7     /* Symbolic link */

/* Ext2 Superblock Structure (on-disk format) */
typedef struct ext2_superblock {
    uint32_t s_inodes_count;           /* Total number of inodes */
    uint32_t s_blocks_count;           /* Total number of blocks */
    uint32_t s_r_blocks_count;         /* Number of reserved blocks */
    uint32_t s_free_blocks_count;      /* Number of free blocks */
    uint32_t s_free_inodes_count;      /* Number of free inodes */
    uint32_t s_first_data_block;       /* First data block */
    uint32_t s_log_block_size;         /* Block size = 1024 << s_log_block_size */
    uint32_t s_log_frag_size;          /* Fragment size */
    uint32_t s_blocks_per_group;       /* Blocks per group */
    uint32_t s_frags_per_group;        /* Fragments per group */
    uint32_t s_inodes_per_group;       /* Inodes per group */
    uint32_t s_mtime;                  /* Mount time */
    uint32_t s_wtime;                  /* Write time */
    uint16_t s_mnt_count;              /* Mount count */
    uint16_t s_max_mnt_count;          /* Maximum mount count */
    uint16_t s_magic;                  /* Magic signature */
    uint16_t s_state;                  /* File system state */
    uint16_t s_errors;                 /* Behaviour when detecting errors */
    uint16_t s_minor_rev_level;        /* Minor revision level */
    uint32_t s_lastcheck;              /* Time of last check */
    uint32_t s_checkinterval;          /* Maximum time between checks */
    uint32_t s_creator_os;             /* OS that created the filesystem */
    uint32_t s_rev_level;              /* Revision level */
    uint16_t s_def_resuid;             /* Default uid for reserved blocks */
    uint16_t s_def_resgid;             /* Default gid for reserved blocks */
    uint32_t s_first_ino;              /* First non-reserved inode */
    uint16_t s_inode_size;             /* Size of inode structure */
    uint16_t s_block_group_nr;         /* Block group number of this superblock */
    uint32_t s_feature_compat;         /* Compatible feature set */
    uint32_t s_feature_incompat;       /* Incompatible feature set */
    uint32_t s_feature_ro_compat;      /* Readonly-compatible feature set */
    uint8_t  s_uuid[16];               /* 128-bit uuid for volume */
    char     s_volume_name[16];        /* Volume name */
    char     s_last_mounted[64];       /* Directory where last mounted */
    uint32_t s_algorithm_usage_bitmap; /* For compression */
    uint8_t  s_prealloc_blocks;        /* Number of blocks to try to preallocate */
    uint8_t  s_prealloc_dir_blocks;    /* Number to preallocate for dirs */
    uint16_t s_padding1;
    uint8_t  s_journal_uuid[16];       /* uuid of journal superblock */
    uint32_t s_journal_inum;           /* inode number of journal file */
    uint32_t s_journal_dev;            /* device number of journal file */
    uint32_t s_last_orphan;            /* start of list of inodes to delete */
    uint32_t s_hash_seed[4];           /* HTREE hash seed */
    uint8_t  s_def_hash_version;       /* Default hash version to use */
    uint8_t  s_reserved_char_pad;
    uint16_t s_reserved_word_pad;
    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;          /* First metablock block group */
    uint32_t s_reserved[190];          /* Padding to the end of the block */
} __attribute__((packed)) ext2_superblock_t;

/* Ext2 Block Group Descriptor */
typedef struct ext2_group_desc {
    uint32_t bg_block_bitmap;          /* Block bitmap block */
    uint32_t bg_inode_bitmap;          /* Inode bitmap block */
    uint32_t bg_inode_table;           /* Inode table block */
    uint16_t bg_free_blocks_count;     /* Free blocks count */
    uint16_t bg_free_inodes_count;     /* Free inodes count */
    uint16_t bg_used_dirs_count;       /* Directories count */
    uint16_t bg_pad;
    uint32_t bg_reserved[3];
} __attribute__((packed)) ext2_group_desc_t;

/* Ext2 Inode Structure (on-disk format) */
typedef struct ext2_inode {
    uint16_t i_mode;                   /* File mode */
    uint16_t i_uid;                    /* Low 16 bits of Owner Uid */
    uint32_t i_size;                   /* Size in bytes */
    uint32_t i_atime;                  /* Access time */
    uint32_t i_ctime;                  /* Creation time */
    uint32_t i_mtime;                  /* Modification time */
    uint32_t i_dtime;                  /* Deletion Time */
    uint16_t i_gid;                    /* Low 16 bits of Group Id */
    uint16_t i_links_count;            /* Links count */
    uint32_t i_blocks;                 /* Blocks count */
    uint32_t i_flags;                  /* File flags */
    uint32_t i_osd1;                   /* OS dependent 1 */
    uint32_t i_block[15];              /* Pointers to blocks */
    uint32_t i_generation;             /* File version (for NFS) */
    uint32_t i_file_acl;               /* File ACL */
    uint32_t i_dir_acl;                /* Directory ACL */
    uint32_t i_faddr;                  /* Fragment address */
    uint8_t  i_osd2[12];               /* OS dependent 2 */
} __attribute__((packed)) ext2_inode_t;

/* Ext2 Directory Entry Structure */
typedef struct ext2_dir_entry {
    uint32_t inode;                    /* Inode number */
    uint16_t rec_len;                  /* Directory entry length */
    uint8_t  name_len;                 /* Name length */
    uint8_t  file_type;                /* File type */
    char     name[];                   /* File name (variable length) */
} __attribute__((packed)) ext2_dir_entry_t;

/* Ext2 Private Data Structures (in-memory) */

/* Ext2 filesystem private data */
typedef struct ext2_fs_data {
    ext2_superblock_t *sb;             /* Copy of superblock */
    ext2_group_desc_t *group_desc;     /* Block group descriptors */
    uint32_t groups_count;             /* Number of block groups */
    uint32_t block_size;               /* Block size in bytes */
    uint32_t inodes_per_block;         /* Inodes per block */
    uint32_t itable_blocks;            /* Inode table blocks per group */
} ext2_fs_data_t;

/* Ext2 inode private data */
typedef struct ext2_inode_data {
    ext2_inode_t disk_inode;           /* On-disk inode data */
    uint32_t block_group;              /* Block group number */
    uint32_t index_in_group;           /* Index within block group */
} ext2_inode_data_t;

#endif /* FS_EXT2_EXT2_H */