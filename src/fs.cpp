#include "../include/fs.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <cstring>
#include <vector>
#include <sstream>
#include <iterator>
#include <fstream>
#include <dirent.h>
#include <sys/stat.h>

buffer inner_buf[BLOCK_SIZE];

using namespace std;

time_t get_cur_time() {
    return chrono::system_clock::to_time_t(chrono::system_clock::now());
}

block_num end_block(unsigned int size) {
    return (size + BLOCK_SIZE - 1)/BLOCK_SIZE;
}

FileSystem::FileSystem(const std::string& diskfile) {
    // r+w 打开磁盘文件
    std::fstream disk(diskfile, std::ios::in | std::ios::out | std::ios::binary);

    if (!disk) {
        std::cerr << "Failed to open disk file " << diskfile << std::endl;
        exit(EXIT_FAILURE);
    }

    // 读取superBlock
    disk.seekg(0, std::ios::beg);
    disk.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

    // 读取Inode 
    disk.seekg(OFFSET_INODE, std::ios::beg);
    disk.read(reinterpret_cast<char*>(&inodes[0]), sizeof(DiskInode)*100);

    // 保存
    diskfile_ = diskfile;
    disk_ = std::move(disk);
}

FileSystem::~FileSystem() {
    // 保存数据
    disk_.seekp(0, std::ios::beg);
    disk_.write(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

    disk_.seekp(OFFSET_INODE, std::ios::beg);
    disk_.write(reinterpret_cast<char*>(&inodes[0]), sizeof(DiskInode)*INODE_NUM);

    // 关闭文件
    disk_.close();
}

node_num FileSystem::alloc_inode() {
    if(sb.s_ninode <= 1)
        return 0;
    int ino = sb.s_inode[--sb.s_ninode];
    inodes[ino].d_uid = user_->uid;
    inodes[ino].d_gid = user_->group;
    inodes[ino].d_nlink = 1;

    // 获取当前时间的Unix时间戳
    inodes[ino].d_atime = inodes[ino].d_mtime = get_cur_time();
    
    
    // 将Int型的时间变为可读
    //char* str_time = ctime((const time_t *)&inodes[ino].d_atime);
    //cout << "allocate new inode "<< ino <<", uid="<< inodes[ino].d_uid <<", gid="<< inodes[ino].d_gid <<", time="/*<< str_time*/ << endl;

    return ino;
}

block_num FileSystem::alloc_block() {
    int blkno;
    if(sb.s_nfree <= 1) // free list 不足
    {
        // 换一张新表
        buffer buf[BLOCK_SIZE] = "";
        read_block(sb.s_free[0], buf);
        int *table = reinterpret_cast<int *>(buf);
        sb.s_nfree = table[0];
        for(int i=0;i<sb.s_nfree;i++)
            sb.s_free[i] = table[i+1];
    }
    // 取一个块
    blkno = sb.s_free[--sb.s_nfree];
    if (blkno == 0) {
        cout << "error : block list empty" << endl;
        return FAIL;
    }
    return blkno;
}

bool FileSystem::read_block(block_num blkno, buffer* buf) {
    // 暂未实现缓存
    //cout << "read " << blkno << endl;
    disk_.seekg(OFFSET_DATA + blkno*BLOCK_SIZE, std::ios::beg);
    disk_.read(buf, BLOCK_SIZE);
    return true;
}

bool FileSystem::write_block(block_num blkno, buffer* buf) {
    cout << "wrire " << blkno << endl;
    disk_.seekg(OFFSET_DATA + blkno*BLOCK_SIZE, std::ios::beg);
    disk_.write(buf, BLOCK_SIZE);
    return true;
}

block_num FileSystem::file_idx_block(DiskInode& inode, uint block_idx, bool create) {
    // 如果 block_num 大于文件大小或者超出文件系统范围，返回 -1
    if (create == false && (block_idx > inode.d_size/BLOCK_SIZE || block_idx >= (6 + 2*128 + 2*128*128))) {
        return FAIL;
    }

    /* 直接索引 */
    if (block_idx < 6) {
        if(create){  // 此处需要新增
            if(inode.d_addr[block_idx]) {  // 已有块，失败
                cout << "block in " << block_idx << "already exist! is:" << inode.d_addr[block_idx] << endl;
                //return FAIL;
            }
            else {                         // 分配
                block_num blkno = alloc_block();
                if(blkno == FAIL) return FAIL;
                inode.d_addr[block_idx] = blkno;
                cout << "file:" << (&inode - inodes)/sizeof(DiskInode) << " alloc " << blkno << endl;
            }
        }
        return inode.d_addr[block_idx];
    }

    /* 一级索引 */
    block_idx -= 6;
    if (block_idx < 128 * 2) {
        // 是否需要先分配一级索引表的物理块
        if (inode.d_addr[6 + (block_idx / 128)] == 0) {
            if(create) {
                block_num blkno = alloc_block();
                if(blkno == FAIL) return FAIL;
                inode.d_addr[6 + (block_idx / 128)] = blkno;
            }
            else
                return 0; // 试图索引不存在的块，失败
        }

        // 访问一级索引
        buffer buf_1[BLOCK_SIZE] = "";
        memset(buf_1, 1, BLOCK_SIZE);
        block_num *first_level_table = reinterpret_cast<block_num *>(buf_1);
        read_block(inode.d_addr[6 + (block_idx / 128)], buf_1);

        int idx_in_ftable = block_idx % 128;
        if( create ) {
            if(first_level_table[idx_in_ftable] == 0) {
                block_num blkno = alloc_block();
                if(blkno == FAIL) return FAIL;
                first_level_table[idx_in_ftable] = blkno;
                write_block(inode.d_addr[6 + (block_idx / 128)], buf_1);
            }
            else {
                cout << "block in " << block_idx << "already exist! is:" << first_level_table[idx_in_ftable] << endl;
            }
        }
        return first_level_table[idx_in_ftable];
    }

    /* 二级索引 */
    block_idx -= 128 * 2;
    if (block_idx < 128 * 128 * 2) {
        // 是否需要先分配一级索引表的物理块
        if (inode.d_addr[8 + (block_idx / (128 * 128))] == 0) {
            if(create) {
                block_num blkno = alloc_block();
                if(blkno == FAIL) return FAIL;
                inode.d_addr[8 + (block_idx / (128 * 128))] = blkno;
            }
            else
                return 0; // 试图索引不存在的块，失败
        }

        // 访问一级索引表
        block_num first_level_no = (block_idx % (128*128)) / 128;

        buffer buf_1[BLOCK_SIZE] = "";
        block_num *first_level_table = reinterpret_cast<block_num *>(buf_1);
        read_block(inode.d_addr[8 + (block_idx / (128 * 128))], buf_1);


        block_num second_block = first_level_table[first_level_no];
        // 一级索引表中的对应项为空
        if (second_block == 0) {
            if( create ) {
                // 分配一级索引表中指向二级索引表的物理块
                block_num blkno = alloc_block();
                if(blkno == FAIL) return FAIL;
                first_level_table[first_level_no] = blkno;
                write_block(inode.d_addr[8 + (block_idx / (128 * 128))], buf_1);
                second_block = blkno;
            }
            else 
                return 0;
        }

        // 访问二级索引表
        
        buffer buf_2[BLOCK_SIZE] = "";
        block_num *second_level_table = reinterpret_cast<block_num *>(buf_2);
        read_block(second_block, buf_2);

        int idx_in_stable = block_idx % 128;
        if( create ) {
            if(second_level_table[idx_in_stable] == 0) {
                block_num blkno = alloc_block();
                if( blkno == FAIL) return FAIL;
                second_level_table[idx_in_stable] = blkno;
                write_block( second_block, (buffer *)second_level_table);
            }
            else {
                cout << "block in " << block_idx << "already exist! is:" << second_level_table[idx_in_stable] << endl;
                //return FAIL;
            }
        }
        return second_level_table[idx_in_stable];
    }

    // 超出范围，返回FAIL
    return FAIL;
}

/*
* 使用了全局的Inner_buf
*/
uint FileSystem::read(DiskInode& inode, buffer* buf, uint size, uint offset) {
    if (offset >= inode.d_size) {
        return 0;
    }

    if (offset + size > inode.d_size) {
        size = inode.d_size - offset;
    }

    uint read_size = 0;

    for (uint pos = offset; pos < offset + size;) {
        uint no = pos / BLOCK_SIZE;             //计算inode中的块编号
        uint block_offset = pos % BLOCK_SIZE;   //块内偏移
        block_num blkno = file_idx_block(inode, no, false);

        if (blkno < 0)
            break;

        /* 读块 */
        read_block(blkno, inner_buf);
        /* 读取可能的最大部分 */
        uint block_read_size = std::min<uint>(BLOCK_SIZE - block_offset, size - read_size);
        memcpy(buf + read_size, inner_buf + block_offset, block_read_size);
        read_size += block_read_size;
        pos += block_read_size;
    }

    return read_size;
}

/*
* 使用了全局的Inner_buf
*/
uint FileSystem::write(DiskInode& inode, const buffer* buf, uint size, uint offset) {
    if (offset + size > MAX_FILE_SIZE) {
        return 0;
    }

    uint written_size = 0;

    for (uint pos = offset; pos < offset + size;) {
        uint no = pos / BLOCK_SIZE;             //计算inode中的块编号
        uint block_offset = pos % BLOCK_SIZE;   //块内偏移
        block_num blkno = file_idx_block(inode, no, true);

        if (blkno < 0) {
            // 添加新块失败，返回已经写入的字节数
            return written_size;
        }

        /* 写入可能的最大部分 */
        uint block_write_size = std::min<uint>(BLOCK_SIZE - block_offset, size - written_size);
        
        /* 是否读原本的内容 */
        if(block_write_size < BLOCK_SIZE) // 要用到原本的内容
            read_block(blkno, inner_buf);

        memcpy(inner_buf + block_offset, buf + written_size, block_write_size);
        write_block(blkno, inner_buf);
        written_size += block_write_size;
        pos += block_write_size;
    }

    // 更新inode的文件大小和最后修改时间
    if (offset + written_size > inode.d_size) {
        inode.d_size = offset + written_size;
    }
    inode.d_mtime = get_cur_time();

    return written_size;
}

node_num FileSystem::createFile(const node_num dir, const std::string& filename, DirectoryEntry::FileType type=DirectoryEntry::FileType::RegularFile) {
    // 开辟空间，并读取所有目录项，这里预读了最后一格物理块的所有空间
    uint blocks = (inodes[dir].d_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    // 转化成一个完整的vector
    DirectoryEntry *entry_table = (DirectoryEntry *)malloc(blocks * BLOCK_SIZE);

    if(entry_table == nullptr || read(inodes[dir], (buffer *)entry_table, inodes[dir].d_size, 0) == false) {
        cerr << "createFile: read directory entries failed." << endl;
        free(entry_table);
        return -1;
    }

    // 遍历所有目录项
    int entry_no, entry_num_max = blocks * ENTRYS_PER_BLOCK;
    for (entry_no = 0; entry_no < entry_num_max; entry_no++) {
        // 查同名
        string tmp = entry_table[entry_no].m_name;
        if (entry_table[entry_no].m_ino && strcmp(entry_table[entry_no].m_name, filename.c_str()) == 0) {
            std::cerr << "createFile: File already exists." << std::endl;
            free(entry_table);
            return -1;
        }

        // 如果到结尾，跳出循环
        if (entry_table[entry_no].m_ino == 0) 
            break;
    }

    // 目录文件已满，需要分配新的数据块
    // 原因，这里的entry_num_max指的是当前所有已申请的物理块能放下的最多entry数
    if (entry_no == entry_num_max) {
        blocks++;

        block_num newblkno = file_idx_block(inodes[dir], end_block(inodes[dir].d_size), true);
        entry_no = 0;
        buffer *buf = reinterpret_cast<buffer *>(entry_table);
        read_block(newblkno, buf);
    }

    // 找到空闲的inode和数据块
    node_num ino = alloc_inode();
    if (ino == 0) {
        std::cerr << "createFile: No free inode" << std::endl;
        return -1;
    }

    // 填入新entry 位置永远是table[no]
    entry_table[entry_no].m_ino = ino;
    strncpy(entry_table[entry_no].m_name, filename.c_str(), DirectoryEntry::DIRSIZ);
    entry_table[entry_no].m_type = type;
    // 写回只用写最后一块
    if(entry_no == 0)
        write_block(file_idx_block(inodes[dir], blocks - 1, false), (buffer *)entry_table); // 写回目录文件内容
    else 
        write_block(file_idx_block(inodes[dir], blocks - 1, false), (buffer *)entry_table + (blocks-1)*BLOCK_SIZE); // 写回目录文件内容

    // 更新目录文件inode
    inodes[dir].d_size += ENTRY_SIZE;
    inodes[dir].d_mtime = get_cur_time();

    free(entry_table);
    return ino;
}

node_num FileSystem::createDir(const node_num dir, const std::string& dirname) {
    // 先创建一个文件， 类型设置为目录
    node_num ino = createFile(dir, dirname, DirectoryEntry::FileType::Directory);
    if(ino == -1) {
        std::cerr << "createdir : dir file creating failed" << std::endl;
        return false;
    }

    // 目录文件创建需要自带 . 和 ..
    DirectoryEntry dotEntry(ino, ".", DirectoryEntry::FileType::Directory);
    DirectoryEntry dotDotEntry(dir, "..", DirectoryEntry::FileType::Directory);
    
    block_num blkno = file_idx_block(inodes[ino], end_block(inodes[ino].d_size), true);
    buffer blockBuf[BLOCK_SIZE] = "";

    memcpy(blockBuf, &dotEntry, ENTRY_SIZE);
    memcpy(blockBuf + ENTRY_SIZE, &dotDotEntry, ENTRY_SIZE);
    if (!write_block(blkno, blockBuf)) {
        std::cerr << "Error: Failed to write block." << std::endl;
        return false;
    }

    // 更新inode和superblock
    inodes[ino].d_mtime = get_cur_time();
    inodes[ino].d_size = ENTRY_SIZE * 2;
    sb.s_time = get_cur_time();

    return ino;
}

bool FileSystem::createRootDir() {
    // 先创建一个文件， 类型设置为目录
    node_num ino = 1;

    // 目录文件创建需要自带 . 和 ..
    DirectoryEntry dotEntry(ino, ".", DirectoryEntry::FileType::Directory);
    DirectoryEntry dotDotEntry(ino, "..", DirectoryEntry::FileType::Directory);
    
    block_num blkno = file_idx_block(inodes[ino], end_block(inodes[ino].d_size), true); 
    buffer blockBuf[BLOCK_SIZE] = "";
    memcpy(blockBuf, &dotEntry, ENTRY_SIZE);
    memcpy(blockBuf + ENTRY_SIZE, &dotDotEntry, ENTRY_SIZE);
    if (!write_block(blkno, blockBuf)) {
        std::cerr << "Error: Failed to write block." << std::endl;
        return false;
    }

    // 更新inode和superblock
    inodes[ino].d_mtime = get_cur_time();
    inodes[ino].d_size = ENTRY_SIZE * 2;
    sb.s_time = get_cur_time();

    return true;
}

node_num FileSystem::find_from_path(const string& path) {
    int ino;    // 起始查询的目录INode号
    if(path.empty()){
        cerr << "error path!" << endl;
        return -1;
    }
    else {      // 判断是相对路径还是绝对路径
        if(path[0] == '/')
            ino = ROOT_INO;         
        else
            ino = user_->current_dir_;
    }

    // 重新解析Path
    std::vector<std::string> tokens;
    std::istringstream iss(path);
    std::string token;
    while (getline(iss, token, '/')) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }

    // 依次查找每一级目录或文件
    for (const auto& token : tokens) {
        DiskInode inode = inodes[ino];

        DirectoryEntry *entry_table = (DirectoryEntry *)malloc(inode.d_size);

        if(entry_table == nullptr || read(inode, (buffer *)entry_table, inode.d_size, 0) == false) {
            std::cerr << "createFile: read directory entries failed." << std::endl;
            return -1;
        }

        // 遍历所有目录项
        bool found = false;
        int entry_no, entry_num = inode.d_size/ENTRY_SIZE;
        for (entry_no = 0; entry_no < entry_num; entry_no++) {
            // 查同名
            if (entry_table[entry_no].m_ino && strcmp(entry_table[entry_no].m_name, token.c_str()) == 0) {
                ino = entry_table[entry_no].m_ino;
                found = true;
                break;
            }
        }

        free(entry_table);
        if (!found) 
            return -1;
    }
    return ino;
}

bool FileSystem::saveFile(const std::string& src, const std::string& filename) {
    std::ifstream infile(src, std::ios::binary | std::ios::in);
    if (!infile) {
        std::cerr << "Failed to open file: " << src << std::endl;
        return false;
    }

    // 找到目标文件所在目录的inode编号
    node_num dir;
    if(filename.rfind('/') == -1)
        dir = user_->current_dir_;
    else {
        dir = find_from_path(filename.substr(0, filename.rfind('/')));
        if (dir == -1) {
            std::cerr << "Failed to find directory: " << filename.substr(0, filename.rfind('/')) << std::endl;
            return false;
        }
    }

    // 在目标目录下创建新文件
    node_num ino = createFile(dir, filename.substr(filename.rfind('/') + 1));
    if (ino == -1) {
        std::cerr << "Failed to create file: " << filename << std::endl;
        return false;
    }

    // 获取inode
    DiskInode& inode = inodes[ino];

    // 从文件读取并写入inode
    uint offset = 0;
    char buf[BLOCK_SIZE] = "";
    while (infile) {
        infile.read(buf, BLOCK_SIZE);
        uint count = infile.gcount();

        uint blockno = offset / BLOCK_SIZE;
        block_num blkno = file_idx_block(inode, blockno, true);


        // 写入数据
        uint pos = offset % BLOCK_SIZE;
        uint n = std::min(BLOCK_SIZE - pos, (long)count);
        if(n < BLOCK_SIZE)
            memset(buf+n, 0, BLOCK_SIZE - n);

        // 写入物理块
        if (!write_block(blkno, buf)) {
            std::cerr << "Failed to write block for file: " << filename << std::endl;
            return false;
        }
        
        count -= n;
        offset += n;
    }

    // 更新inode信息
    inode.d_size = offset;
    inode.d_mtime = get_cur_time();
    inode.d_atime = get_cur_time();
    inode.d_nlink = 1;

    infile.close();

    return true;
}

bool FileSystem::initialize_from_external_directory(const string& path, const node_num root_no)
{
    if(root_no == 1 && createRootDir() == false){
        cerr << "create root dir failed!" << endl;
        return false;
    }

    DIR *pDIR = opendir((path + '/').c_str());
    dirent *pdirent = NULL;
    if (pDIR != NULL)
    {
        cout << "under " << path << ":" << endl;
        while ((pdirent = readdir(pDIR)) != NULL)
        {
            string Name = pdirent->d_name;
            if (Name == "." || Name == "..")
                continue;


            struct stat buf;
            if (!lstat((path + '/' + Name).c_str(), &buf))
            {
                int ino; // 获取文件的inode号
                if (S_ISDIR(buf.st_mode))
                {
                    ino = createDir(root_no, Name);
                    cout << "make folder: " << Name << " success! inode:" << ino << endl;

                    /* 递归进入，需要递进用户目录 */
                    user_->current_dir_ = ino;
                    if(initialize_from_external_directory(path + '/' + Name, ino) == false){
                        return false;
                    }
                    user_->current_dir_ = root_no;

                }
                else if (S_ISREG(buf.st_mode))
                {
                    cout << "normal file:" << Name << endl;
                    if(saveFile(path + '/' + Name, Name) == false)
                        return false;
                }
                else
                {
                    cout << "other file" << Name << endl;
                }
            }
        }
    }
    closedir(pDIR);
    return true;
}

bool FileSystem::ls(const string& path) {
    node_num path_no;
    if(path.empty())
        path_no = user_->current_dir_;
    else
        path_no = find_from_path(path);

    if (path_no == -1) {
        std::cerr << "ls: cannot access '" << path << "': No such file or directory" << std::endl;
        return false;
    }

    DiskInode inode = inodes[path_no];

    DirectoryEntry *entry_table = (DirectoryEntry *)malloc(inode.d_size);
    if (entry_table == nullptr  || read(inode, (buffer *)entry_table, inode.d_size, 0) == false) {
        std::cerr << "ls: read directory entries failed." << std::endl;
        return false;
    }

    int entry_no, entry_num = inode.d_size / ENTRY_SIZE;
    for (entry_no = 0; entry_no < entry_num; entry_no++) {
        if (entry_table[entry_no].m_ino) {
            DiskInode child_inode = inodes[entry_table[entry_no].m_ino];
            string name(entry_table[entry_no].m_name);
            cout << name;
            if (entry_table[entry_no].m_type == DirectoryEntry::FileType::Directory) {
                cout << "/";
            }
            cout << endl;
        }
    }
    free(entry_table);
    return true;
}
