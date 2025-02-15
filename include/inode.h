#include "parameter.h"
#include "directory.h"
#include <string>
#include <vector>

class Inode
{
public:
	enum INodeFlag {
		ILOCK = 0x1,		/* 索引节点上锁 */
		IUPD  = 0x2,		/* 内存inode被修改过，需要更新相应外存inode */
		IACC  = 0x4,		/* 内存inode被访问过，需要修改最近一次访问时间 */
		IMOUNT = 0x8,		/* 内存inode用于挂载子文件系统 */
		IWANT = 0x10,		/* 有进程正在等待该内存inode被解锁，清ILOCK标志时，要唤醒这种进程 */
		ITEXT = 0x20		/* 内存inode对应进程图像的正文段 */
	};
	
	enum FileType {
        Unknown = 0x40,
        RegularFile = 0x80,
        Directory = 0x100,
        Link = 0x200
    };

	Inode() {;};
	~Inode() {;};

	int get_block_id(int inner_id);
	int resize(int size);
	int decrease_size();
	int push_back_block();
	int pop_back_block();
	
	int read_at(int offset, char *buf, int size);
	int write_at(int offset, const char *buf, int size);

	int clear();
	int copy_from(Inode &src);
	
	/* dir inode only */
	int init_as_dir(int ino, int fa_ino);
	int find_file(const std::string &name);
	int create_file(const std::string &name, bool is_dir);
	int delete_file_entry(const std::string &name);   // 删除目录项，返回Inode号，但是没有删除Inode(调用者保证删除目录时子文件的处理)

	std::vector<DirectoryEntry> get_entry();
	int set_entry(std::vector<DirectoryEntry>& entrys);

public:
	/*-------------------------内存Inode独有成员------------------------------*/
	int		i_ino;			/* inode在内存inode数组中的下标 */
	int		i_lock;			/* 该内存inode的锁 */

	/*---------------------以下是DiskInode成员，属于磁盘部分--------------------------*/
	unsigned int d_mode;	/* 状态的标志位，定义见enum INodeFlag enum FileType*/
	int		d_nlink;		/* 文件联结计数，即该文件在目录树中不同路径名的数量 */
	
	short	d_uid;			/* 文件所有者的用户标识数 */
	short	d_gid;			/* 文件所有者的组标识数 */
	
	int		d_size;			/* 文件大小，字节为单位 */
	int		d_addr[10];		/* 用于文件逻辑块号和物理块号转换的基本索引表 */
	
	int		d_atime;		/* 最后访问时间 */
	int		d_mtime;		/* 最后修改时间 */
};



class DiskInode
{
public:
	enum INodeFlag
	{
		ILOCK = 0x1,		/* 索引节点上锁 */
		IUPD  = 0x2,		/* 内存inode被修改过，需要更新相应外存inode */
		IACC  = 0x4,		/* 内存inode被访问过，需要修改最近一次访问时间 */
		IMOUNT = 0x8,		/* 内存inode用于挂载子文件系统 */
		IWANT = 0x10,		/* 有进程正在等待该内存inode被解锁，清ILOCK标志时，要唤醒这种进程 */
		ITEXT = 0x20		/* 内存inode对应进程图像的正文段 */
	};
	
	DiskInode() {;};
	~DiskInode() {;};

public:
	unsigned int d_mode;	/* 状态的标志位，定义见enum INodeFlag */
	int		d_nlink;		/* 文件联结计数，即该文件在目录树中不同路径名的数量 ？？？*/
	
	short	d_uid;			/* 文件所有者的用户标识数 */
	short	d_gid;			/* 文件所有者的组标识数 */
	
	int		d_size;			/* 文件大小，字节为单位 */
	int		d_addr[10];		/* 用于文件逻辑块号和物理块号转换的基本索引表 ？？？*/
	
	int		d_atime;		/* 最后访问时间 */
	int		d_mtime;		/* 最后修改时间 */
};