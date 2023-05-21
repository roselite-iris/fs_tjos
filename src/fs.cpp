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

using namespace std;

time_t get_cur_time() {
    return chrono::system_clock::to_time_t(chrono::system_clock::now());
}

int end_block(unsigned int size) {
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

int FileSystem::alloc_inode() {


    
    if(sb.s_ninode <= 1)
        return 0;
    int ino = sb.s_inode[--sb.s_ninode];
    inodes[ino].d_nlink = 1;
    inodes[ino].d_uid = user_->uid;
    inodes[ino].d_gid = user_->group;
    

    // 获取当前时间的Unix时间戳
    inodes[ino].d_atime = inodes[ino].d_mtime = get_cur_time();

    // 将Int型的时间变为可读
    //char* str_time = ctime((const time_t *)&d_atime);
    //cout << "allocate new inode "<< ino <<", uid="<< d_uid <<", gid="<< d_gid <<", time="/*<< str_time*/ << endl;

    return ino;
}

int FileSystem::alloc_block() {
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

bool FileSystem::read_block(int blkno, buffer* buf) {
    // 暂未实现缓存
    //cout << "read " << blkno << endl;
    disk_.seekg(OFFSET_DATA + blkno*BLOCK_SIZE, std::ios::beg);
    disk_.read(buf, BLOCK_SIZE);
    return true;
}

bool FileSystem::write_block(int blkno, buffer* buf) {
    cout << "wrire " << blkno << endl;
    disk_.seekg(OFFSET_DATA + blkno*BLOCK_SIZE, std::ios::beg);
    disk_.write(buf, BLOCK_SIZE);
    return true;
}

int FileSystem::find_from_path(const string& path) {
    int ino;    // 起始查询的目录INode号
    if(path.empty()){
        cerr << "error path!" << endl;
        return FAIL;
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
        auto entrys = inodes[ino].get_entry();
        bool found = false;
        // 遍历所有目录项
        for (auto& entry : entrys) {
            if (entry.m_ino && strcmp(entry.m_name, token.c_str()) == 0) {
                ino = entry.m_ino;
                found = true;
                break;
            }
        }
        if (!found) return FAIL;
    }

    return ino;
}

/**
* 从外部文件读取
* 写入TJ_FS
*/
bool FileSystem::saveFile(const std::string& src, const std::string& filename) {
    // 找到目标文件所在目录的inode编号
    int dir;
    if(filename.rfind('/') == -1)
        dir = user_->current_dir_;
    else {
        dir = find_from_path(filename.substr(0, filename.rfind('/')));
        if (dir == FAIL) {
            std::cerr << "Failed to find directory: " << filename.substr(0, filename.rfind('/')) << std::endl;
            return false;
        }
    }

    // 在目标目录下创建新文件
    string name = filename.substr(filename.rfind('/') + 1);
    int ino = inodes[dir].create_file(name, false);
    if (ino == FAIL) {
        std::cerr << "Failed to create file: " << filename << std::endl;
        return false;
    }

    // 获取inode
    DiskInode& inode = inodes[ino];

    // 从文件读取并写入inode
    std::ifstream infile(src, std::ios::binary | std::ios::in);
    if (!infile) {
        std::cerr << "Failed to open file: " << src << std::endl;
        return false;
    }
    // 获取文件大小
    infile.seekg(0, std::ios::end);
    size_t size = infile.tellg();
    infile.seekg(0, std::ios::beg);

    // 一次性读取文件数据
    std::vector<char> data(size);
    infile.read(data.data(), size);

    // 写入inode
    if (!inode.write_at(0, data.data(), size)) {
        std::cerr << "Failed to write data to inode" << std::endl;
        return false;
    }

    // 更新inode信息
    inode.d_size = size;
    inode.d_mtime = get_cur_time();
    inode.d_atime = get_cur_time();
    inode.d_nlink = 1;

    infile.close();

    return true;
}

DiskInode &FileSystem::_get_root() {
    return inodes[ROOT_INO];
}

void FileSystem::_init_root(){
    auto root_ = _get_root();
    root_.init_as_dir(ROOT_INO, ROOT_INO);
}

bool FileSystem::initialize_from_external_directory(const string& path, const int root_no)
{
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
                    ino = inodes[root_no].create_file(Name, true);
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
    int path_no;
    if(path.empty())
        path_no = user_->current_dir_;
    else
        path_no = find_from_path(path);

    if (path_no == -1) {
        std::cerr << "ls: cannot access '" << path << "': No such file or directory" << std::endl;
        return false;
    }

    DiskInode inode = inodes[path_no];

    auto entries = inode.get_entry();
    for (auto& entry : entries) {
        if (entry.m_ino) {
            DiskInode child_inode = inodes[entry.m_ino];
            string name(entry.m_name);
            cout << name;
            if (entry.m_type == DirectoryEntry::FileType::Directory) {
                cout << "/";
            }
            cout << endl;
        }
    }

    return true;
}
