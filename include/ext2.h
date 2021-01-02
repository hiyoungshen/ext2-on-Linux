// 移植完毕
#include <stdlib.h>
#define BLOCK_SIZE_EXT2 1024 //BYTE
#define INODE_SIZE_EXT2 128	//BYTE
#define GDESC_SIZE_EXT2 32
#define N_BLOCKS_EXT2 15
#define NAME_LEN_EXT2 255
#define INODES_PER_BLOCK_EXT2 (BLOCK_SIZE_EXT2 / INODE_SIZE_EXT2)
#define DIR_DEEP 10
#define PATH_LEN 256

#define TRUE 1	//是
#define FALSE 0 //否

#define OK 1				//正常返回
#define SYSERROR 2			//系统错误
#define VDISKERROR 3		//虚拟磁盘错误
#define INSUFFICIENTSPACE 4 //虚拟磁盘空间不足
#define WRONGPATH 5			//路径有误
#define NAMEEXIST 6			//文件或目录名已存在
#define ACCESSDENIED 7		//读写权限不对拒绝访问

#define R 5 //读
#define W 6 //写
#define F 1 //文件
#define D 2 //目录

typedef int STATE;
typedef unsigned char BYTE;	 //字节
typedef unsigned short WORD; //双字节
typedef unsigned long DWORD; //四字节
typedef unsigned int UINT;	 //无符号整型
typedef char CHAR;			 //字符类型

typedef unsigned char *PBYTE;
typedef unsigned short *PWORD;
typedef unsigned long *PDWORD; //四字节指针
typedef unsigned int *PUINT;   //无符号整型指针
typedef char *PCHAR;		   //字符指针

//
#ifndef TYPEFILE
#define TYPEFILE
typedef struct
{					  //文件类型
	CHAR parent[256]; //父路径
	CHAR name[256];	  //文件名
	DWORD start;	  //起始地址
	DWORD off;		  //总偏移量,以字节为单位
	DWORD size;		  //文件的大小，以字节为单位
	unsigned int inode;
} File, *PFile;
#endif

#ifndef DARRELEM
#define DARRELEM
typedef struct
{						//动态数组元素类型，用于存储文件或目录的基本信息
	CHAR fullpath[256]; //绝对路径
	CHAR name[256];		//文件名或目录名
	UINT tag;			//1表示文件，2表示目录
} DArrayElem;
#endif
#ifndef DARRAY
#define DARRAY
typedef struct
{					  //动态数组类型
	DArrayElem *base; //数组基地址
	UINT offset;	  //读取数组时的偏移量
	UINT used;		  //数组当前已使用的容量
	UINT capacity;	  //数组的总容量
	UINT increment;	  //当数组容量不足时，动态增长的步长
} DArray;
#endif

#ifndef PRO
#define PRO
typedef struct
{							   //文件或目录属性类型
	BYTE type;				   //0x10表示目录，否则表示文件
	CHAR name[256];			   //文件或目录名称
	CHAR location[256];		   //文件或目录的位置，绝对路径
	DWORD size;				   //文件的大小或整个目录中(包括子目录中)的文件总大小
	CHAR createTime[20];	   //创建时间型如：yyyy-MM-dd hh:mm:ss类型
	CHAR lastModifiedTime[20]; //最后修改时间
	union
	{
		CHAR lastAccessDate[11]; //最后访问时间，当type为文件值有效
		UINT contain[2];		 //目录中的文件个数和子目录的个数，当type为目录时有效
	} share;					 //
} Properties;
#endif

// shy2
#ifndef EXT2_SUPER_BLOCK
#define EXT2_SUPER_BLOCK
typedef struct
{
	unsigned int s_inodes_count;	  /* Inodes count */
	
	unsigned int s_blocks_count;	  /* Blocks count */
	unsigned int s_r_blocks_count;	  /* Reserved blocks count */
	unsigned int s_free_blocks_count; /* Free blocks count */
	unsigned int s_free_inodes_count; /* Free inodes count */
	unsigned int s_first_data_block;  /* First Data Block */
	unsigned int s_log_BLOCK_SIZE_EXT2;	  /* Block size */
	unsigned int s_log_frag_size;	  /* Fragment size */
	unsigned int s_blocks_per_group;  /* # Blocks per group */
	unsigned int s_frags_per_group;	  /* # Fragments per group */
	unsigned int s_inodes_per_group;  /* # Inodes per group */
	unsigned int s_mtime;			  /* Mount time */
	unsigned int s_wtime;			  /* Write time */
	// 13*4
	unsigned short s_mnt_count;		  /* Mount count */
	unsigned short s_max_mnt_count;	  /* Maximal mount count */
	unsigned short s_magic;			  /* Magic signature */
	unsigned short s_state;			  /* File system state */
	unsigned short s_errors;		  /* Behaviour when detecting errors */
	unsigned short s_minor_rev_level; /* minor revision level */
	// 6*2
	unsigned int s_lastcheck;	  /* time of last check */
	unsigned int s_checkinterval; /* max. time between checks */
	unsigned int s_creator_os;	  /* OS */
	unsigned int s_rev_level;	  /* Revision level */
	// 4*4
	unsigned short s_def_resuid; /* Default uid for reserved blocks */
	unsigned short s_def_resgid; /* Default gid for reserved blocks */
	// 2*2
	unsigned int s_first_ino; /* First non-reserved inode */
	// 1*4
	unsigned short s_INODE_SIZE_EXT2;	 /* size of inode structure */
	unsigned short s_block_group_nr; /* block group # of this superblock */
	// 2*2
	unsigned int s_feature_compat;	  /* compatible feature set */
	unsigned int s_feature_incompat;  /* incompatible feature set */
	unsigned int s_feature_ro_compat; /* readonly-compatible feature set */
	// 3*4
	unsigned char s_uuid[16]; /* 128-bit uuid for volume */
	char s_volume_name[16];	  /* volume name */
	char s_last_mounted[64];  /* directory where last mounted */
	// (16+16+64)*1
	unsigned int s_algorithm_usage_bitmap; /* For compression */
	// 1*4
	unsigned char s_prealloc_blocks;	 /* Nr of blocks to try to preallocate*/
	unsigned char s_prealloc_dir_blocks; /* Nr to preallocate for dirs */
	// 2*1
	unsigned short s_padding1;
	// 1*2
	unsigned char s_journal_uuid[16]; /* uuid of journal superblock */
	// 16*1
	unsigned int s_journal_inum; /* inode number of journal file */
	unsigned int s_journal_dev;	 /* device number of journal file */
	unsigned int s_last_orphan;	 /* start of list of inodes to delete */
	unsigned int s_hash_seed[4]; /* HTREE hash seed */
	// 3*4+4*4
	unsigned char s_def_hash_version; /* Default hash version to use */
	unsigned char s_reserved_char_pad;
	// 2*1
	unsigned short s_reserved_word_pad;
	// 1*2
	unsigned int s_default_mount_opts;
	unsigned int s_first_meta_bg; /* First metablock block group */
	unsigned int s_reserved[190]; /* Padding to the end of the block */
								  // 2*4+190*4
} ext2_super_block;
#endif

#ifndef EXT2_GROUP_DESC
#define EXT2_GROUP_DESC
typedef struct
{
	unsigned int bg_block_bitmap; /* Blocks bitmap block */
	unsigned int bg_inode_bitmap; /* Inodes bitmap block */
	unsigned int bg_inode_table;  /* Inodes table block */
	// 3*4

	unsigned short bg_free_blocks_count; /* Free blocks count */
	unsigned short bg_free_inodes_count; /* Free inodes count */
	unsigned short bg_used_dirs_count;	 /* Directories count */
	unsigned short bg_pad;
	// 2*4

	unsigned int bg_reserved[3];
	// 3*4
} ext2_group_desc;
// 3*4+2*4+3*4=12+8+12=32
#endif

#ifndef EXT2_INODE
#define EXT2_INODE
typedef struct
{
	unsigned int inode;			  /*这个inode节点号*/
	unsigned short i_mode;		  /* File mode */
	unsigned short i_uid;		  /* Low 16 bits of Owner Uid */
	unsigned int i_size;		  /* Size in bytes */
	unsigned int i_atime;		  /* Access time */
	unsigned int i_ctime;		  /* Creation time */
	unsigned int i_mtime;		  /* Modification time */
	unsigned int i_dtime;		  /* Deletion Time */
	unsigned short i_gid;		  /* Low 16 bits of Group Id */
	unsigned short i_links_count; /* Links count */
	unsigned int i_blocks;		  /* Blocks count */
	unsigned int i_flags;		  /* File flags */
	union
	{
		struct
		{
			unsigned int l_i_reserved1;
		} linux1;
		struct
		{
			unsigned int h_i_translator;
		} hurd1;
		struct
		{
			unsigned int m_i_reserved1;
		} masix1;
	} osd1;							/* OS dependent 1 */
	unsigned int i_block[N_BLOCKS_EXT2]; /* Pointers to blocks */
	unsigned int i_generation;		/* File version (for NFS) */
	unsigned int i_file_acl;		/* File ACL */
	unsigned int i_dir_acl;			/* Directory ACL */
	unsigned int i_faddr;			/* Fragment address */
	union
	{
		struct
		{
			unsigned char l_i_frag;	 /* Fragment number */
			unsigned char l_i_fsize; /* Fragment size */
			unsigned short i_pad1;
			unsigned short l_i_uid_high; /* these 2 fields    */
			unsigned short l_i_gid_high; /* were reserved2[0] */
			unsigned int l_i_reserved2;
		} linux2;
		struct
		{
			unsigned char h_i_frag;	 /* Fragment number */
			unsigned char h_i_fsize; /* Fragment size */
			unsigned short h_i_mode_high;
			unsigned short h_i_uid_high;
			unsigned short h_i_gid_high;
			unsigned int h_i_author;
		} hurd2;
		struct
		{
			unsigned char m_i_frag;	 /* Fragment number */
			unsigned char m_i_fsize; /* Fragment size */
			unsigned short m_pad1;
			unsigned int m_i_reserved2[2];
		} masix2;
	} osd2; /* OS dependent 2 */
} ext2_inode;
#endif

#ifndef DENTRY
#define DENTRY
typedef struct
{
	unsigned int inode;		 /* Inode number */
	unsigned short rec_len;	 /* Directory entry length */
	unsigned char name_len;	 /* Name length */
	unsigned char file_type; /*存储：文件为1，目录为2*/
	char name[NAME_LEN_EXT2];	 /* File name */
} ext2_dir_entry;
#endif

#ifndef INODEOPS
#define INODEOPS
typedef struct
{
	int (*create)(struct inode *, struct dentry *);
	struct ext2_dir_entry *(*lookup)(struct inode *, struct dentry *, struct nameidata *);
	int (*mkdir)(struct inode *, struct dentry *, int);
	int (*rmdir)(struct inode *, struct dentry *);
	int (*mknod)(struct inode *, struct dentry *, int);
	int (*rename)(struct inode *, struct dentry *,
				  struct inode *, struct dentry *);
	long (*fallocate)(struct inode *inode, int mode);
	int (*fiemap)(struct inode *);
} inode_operations;
#endif

#ifndef SBOPS
#define SBOPS
typedef struct
{
	struct ext2_inode *(*alloc_inode)();
	void (*destroy_inode)();

	void (*dirty_inode)();
	int (*write_inode)();
	void (*write_super)();
} super_operations;
#endif

#ifndef FILEOPS
#define FILEOPS
typedef struct
{
	STATE(*read)
	();
	STATE(*write)
	();
	int (*readdir)();
	int (*open)();
	int (*flush)();
	int (*release)();
} file_operations;
#endif

#ifndef CUR
#define CUR
typedef struct
{
	ext2_dir_entry *root;
	ext2_dir_entry *current;
	int num;
	ext2_dir_entry *path[DIR_DEEP];
} cpath;
#endif

/*全局变量*/

FILE *ext2_fp;
unsigned int groups_counts_ext2;
unsigned int gdesc_blocks_ext2;
unsigned int inode_blocks_per_group_ext2;
unsigned int ext2_size;
ext2_group_desc *gdesc_ext2;
ext2_super_block sb_ext2;
ext2_inode *root_inode_ext2;
cpath current_path_ext2;
