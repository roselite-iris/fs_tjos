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
    return chrono::system_clockto_time_t(chrono::system_clock::now());
}

FileSystem::FileSystem(const std::string& diskfile) {
    // r+w �򿪴����ļ�
    std::fstream disk(diskfile, std::ios::in | std::ios::out | std::ios::binary);

    if (!disk) {
        std::cerr << "Failed to open disk file " << diskfile << std::endl;
        exit(EXIT_FAILURE);
    }

    // ��ȡsuperBlock
    disk.seekg(0, std::ios::beg);
    disk.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

    // ��ȡInode 
    disk.seekg(OFFSET_INODE, std::ios::beg);
    disk.read(reinterpret_cast<char*>(&inodes[0]), sizeof(DiskInode)*INODE_NUM);

    // ����
    diskfile_ = diskfile;
    disk_ = std::move(disk);
}

FileSystem::~FileSystem() {
    // ��������
    disk_.seekp(0, std::ios::beg);
    disk_.write(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

    disk_.seekp(OFFSET_INODE, std::ios::beg);
    disk_.write(reinterpret_cast<char*>(&inodes[0]), sizeof(DiskInode)*INODE_NUM);

    // �ر��ļ�
    disk_.close();
}

node_num FileSystem::alloc_inode() {
    if(sb.s_ninode <= 1)
        return 0;
    int ino = sb.s_inode[--sb.s_ninode];
    inodes[ino].d_uid = user_->uid;
    inodes[ino].d_gid = user_->group;
    inodes[ino].d_nlink = 1;

    // ��ȡ��ǰʱ���Unixʱ���
    inodes[ino].d_atime = inodes[ino].d_mtime = get_cur_time();
    
    
    // ��Int�͵�ʱ���Ϊ�ɶ�
    char* str_time = ctime((const time_t *)&inodes[ino].d_atime);
    cout << "�����µ�INode "<< ino <<", uid="<< inodes[ino].d_uid <<", gid="<< inodes[ino].d_gid <<", ����ʱ��"<< str_time << endl;

    return ino;
}

block_num FileSystem::alloc_block() {
    int blkno;
    if(sb.s_nfree <= 1) // free list ����
    {
        // ��һ���±�
        buffer buf = read_block(sb.s_free[0]);
        int *table = reinterpret_cast<int *>(buf);
        sb.s_nfree = table[0];
        for(int i=0;i<sb.s_nfree;i++)
            sb.s_free[i] = table[i+1];
    }
    // ȡһ����
    blkno = sb.s_free[--sb.s_nfree];
    if (blkno == 0) {
        cout << "error : super_block empty" << endl;
        return -1;
    }
    return blkno;
}

buffer* FileSystem::read_block(block_num blkno) {
    // ��δʵ�ֻ���
    disk_.seekg(blkno*BLOCK_SIZE, std::ios::beg);
    disk_.read(inner_buf, sizeof(BLOCK_SIZE));
    return inner_buf;
}

bool FileSystem::write_block(block_num blkno, buffer* buf) {
    disk_.seekg(blkno*BLOCK_SIZE, std::ios::beg);
    disk_.write(buf, sizeof(BLOCK_SIZE));
}

block_num FileSystem::translate_block(const DiskInode& inode, uint no) {
    // ��� block_num �����ļ���С���߳����ļ�ϵͳ��Χ������ -1
    if (no >= inode.d_size/BLOCK_SIZE || no >= (6 + 2*128 + 2*128*128)) {
        return -1;
    }

    /* ֱ������ */
    if (no < 6) {           //ֱ�ӷ��ض�Ӧλ�õ�blkno
        return inode.d_addr[no];
    }

    /* һ������ */
    no -= 6;        
    if (no < 128 * 2) {     // d_add[6 or 7], 512/sizeof(uint)=128
        return ((block_num *)read_block(inode.d_addr[6 + (no/128)]))[no%128];
    }

    /* �������� */
    no -= 128 * 2;
    block_num first_level_no = (no % (128*128)) / 128;
    block_num second_block_no = ((block_num *)read_block(inode.d_addr[8 + (no/128*128)]))[first_level_no];
    block_num second_level_no = no % 128;
    return ((block_num *)read_block(second_block_no))[second_level_no];
}

block_num FileSystem::file_add_block(DiskInode& inode) {
    int block_idx = inode.d_size / BLOCK_SIZE; // ��һ�������Ӧ�����ĸ�λ��
    int second_blocks = 128 * 128 * 2;

    /* ֱ������ */
    if (block_idx < 6) {
        block_num blkno = alloc_block();
        inode.d_addr[block_idx] = blkno;
        return blkno;
    }

    /* һ������ */
    block_idx -= 6;
    if (block_idx < 128 * 2) {
        // �Ƿ���Ҫ�ȷ���һ��������������
        if (block_idx % 128 == 0) {
            block_num blkno = alloc_block();
            inode.d_addr[6 + (block_idx / 128)] = blkno;
        }

        // ����һ��������
        block_num *first_level_table = (block_num *)read_block(inode.d_addr[6 + (block_idx / 128)]);
        int offset = block_idx % 128;
        block_num blkno = alloc_block();
        first_level_table[offset] = blkno;
        write_block(inode.d_addr[6 + (block_idx / 128)], (buffer *)first_level_table);
        return blkno;
    }

    /* �������� */
    block_idx -= 128 * 2;
    if (block_idx < 128 * 128 * 2) {
        // �������������������
        if (block_idx % (128 * 128) == 0) {
            block_num blkno = alloc_block();
            inode.d_addr[8 + (block_idx / (128 * 128))] = blkno;
        }

        // ���¶���������
        block_num first_level_no = (block_idx % (128*128)) / 128;
        block_num *first_level_table = (block_num *)read_block(inode.d_addr[8 + (block_idx / (128 * 128))]);
        block_num second_block = first_level_table[first_level_no];
        if (second_block == 0) {
            // ����һ����������ָ�����������������
            block_num blkno = alloc_block();
            first_level_table[first_level_no] = blkno;
            write_block(inode.d_addr[8 + (block_idx / (128 * 128))], first_level_table );
            second_block = blkno;
        }
        block_num *second_level_table = (block_num *)read_block(second_block);
        int offset = block_idx % 128;
        block_num blkno = alloc_block();
        second_level_table[offset] = blkno;
        write_block( second_block, (buffer *)second_level_table);
        return blkno;
    }

    // ������Χ������-1
    return -1;
}

uint FileSystem::read(const DiskInode& inode, buffer* buf, uint size, uint offset) {
    if (offset >= inode.d_size) {
        return 0;
    }

    if (offset + size > inode.d_size) {
        size = inode.d_size - offset;
    }

    uint read_size = 0;

    for (uint pos = offset; pos < offset + size;) {
        uint no = pos / BLOCK_SIZE;             //����inode�еĿ���
        uint block_offset = pos % BLOCK_SIZE;   //����ƫ��
        block_num blkno = translate_block(inode, no);

        if (blkno < 0)
            break;

        /* ���� */
        buffer *block = read_block(blkno);
        /* ��ȡ���ܵ���󲿷� */
        uint block_read_size = std::min<uint>(BLOCK_SIZE - block_offset, size - read_size);
        memcpy(buf + read_size, inner_buf + block_offset, block_read_size);
        read_size += block_read_size;
        pos += block_read_size;
    }

    return read_size;
}

uint FileSystem::write(const DiskInode& inode, const buffer* buf, uint size, uint offset) {
    if (offset + size > MAX_FILE_SIZE) {
        return 0;
    }

    uint written_size = 0;

    for (uint pos = offset; pos < offset + size;) {
        uint no = pos / BLOCK_SIZE;             //����inode�еĿ���
        uint block_offset = pos % BLOCK_SIZE;   //����ƫ��
        block_num blkno = translate_block(inode, no);

        if (blkno < 0) {
            // ����鲻���ڣ���ҪΪ�ļ����һ���¿�
            blkno = file_add_block(inode);
            if (blkno < 0) {
                // ����¿�ʧ�ܣ������Ѿ�д����ֽ���
                return written_size;
            }
        }

        /* д�� */
        buffer *block = read_block(blkno);
        /* д����ܵ���󲿷� */
        uint block_write_size = std::min<uint>(BLOCK_SIZE - block_offset, size - written_size);
        memcpy(inner_buf + block_offset, buf + written_size, block_write_size);
        write_block(blkno, block);
        written_size += block_write_size;
        pos += block_write_size;
    }

    // ����inode���ļ���С������޸�ʱ��
    if (offset + written_size > inode.d_size) {
        inode.d_size = offset + written_size;
    }
    inode.d_mtime = time(NULL);

    return written_size;
}

node_num FileSystem::createFile(const node_num dir, const std::string& filename, DirectoryEntry::FileType type=DirectoryEntry::FileType::RegularFile) {
    // ���ٿռ䣬����ȡ����Ŀ¼�����Ԥ�������һ�����������пռ�
    uint blocks = (inodes[dir].d_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    // ת����һ��������vector
    DirectoryEntry *entry_table = (DirectoryEntry *)malloc(blocks * BLOCK_SIZE);

    if(entry_table == nullptr || read(inodes[dir], (buffer *)entry_table, inodes[dir].d_size, 0) == false) {
        std::cerr << "createFile: read dirextory entries failed." << std::endl;
        free(entry_table);
        return -1;
    }

    // ��������Ŀ¼��
    int entry_no, entry_num_max = blocks * ENTRYS_PER_BLOCK;
    for (entry_no = 0; entry_no < entry_num_max; entry_no++) {
        // ��ͬ��
        if (entry_table[entry_no].m_ino && strcmp(entry_table[entry_no].m_name, filename.c_str()) == 0) {
            std::cerr << "createFile: File already exists." << std::endl;
            free(entry_table);
            return -1;
        }

        // �������β������ѭ��
        if (entry_table[entry_no].m_ino == 0) 
            break;
    }

    // Ŀ¼�ļ���������Ҫ�����µ����ݿ�
    // ԭ�������entry_num_maxָ���ǵ�ǰ�����������������ܷ��µ����entry��
    if (entry_no == entry_num_max) {
        blocks++;

        block_num newblkno = file_add_block(inodes[dir]);
        entry_no = 0;
        entry_table = reinterpret_cast<DirectoryEntry*>(read_block(newblkno));
    }

    // �ҵ����е�inode�����ݿ�
    node_num ino = alloc_inode();
    if (ino == 0) {
        std::cerr << "createFile: No free inode" << std::endl;
        return -1;
    }


    // ������entry λ����Զ��table[no]
    entry_table[entry_no].m_ino = ino;
    strncpy(entry_table[entry_no].m_name, filename.c_str(), DirectoryEntry::DIRSIZ);
    entry_table[entry_no].m_type = type;
    // д��ֻ��д���һ��
    if(entry_no == 0)
        write_block(translate_block(inodes[dir], blocks), (buffer *)entry_table); // д��Ŀ¼�ļ�����
    else 
        write_block(translate_block(inodes[dir], blocks), (buffer *)entry_table + (blocks-1)*BLOCK_SIZE); // д��Ŀ¼�ļ�����

    // ����Ŀ¼�ļ�inode
    inodes[dir].d_size += ENTRY_SIZE;
    inodes[dir].d_mtime = time(nullptr);

    free(entry_table);
    return ino;
}

bool FileSystem::createDir(const node_num dir, const std::string& dirname) {
    // �ȴ���һ���ļ��� ��������ΪĿ¼
    node_num ino = createFile(dir, dirname, DirectoryEntry::FileType::Directory);
    if(ino == -1) {
        std::cerr << "createdir : dir file creating failed" << std::endl;
        return false;
    }

    // Ŀ¼�ļ�������Ҫ�Դ� . �� ..
    DirectoryEntry dotEntry(ino, ".", DirectoryEntry::FileType::Directory);
    DirectoryEntry dotDotEntry(dir, "..", DirectoryEntry::FileType::Directory);
    
    block_num blkno = alloc_block();
    buffer* blockBuf = read_block(blkno);
    if (blockBuf == nullptr) {
        std::cerr << "Error: Failed to read block." << std::endl;
        return false;
    }
    memcpy(blockBuf, &dotEntry, ENTRY_SIZE);
    memcpy(blockBuf + ENTRY_SIZE, &dotDotEntry, ENTRY_SIZE);
    if (!write_block(blkno, blockBuf)) {
        std::cerr << "Error: Failed to write block." << std::endl;
        return false;
    }

    // ����inode��superblock
    inodes[ino].d_mtime = get_cur_time();
    sb.s_time = get_cur_time();

    return true;
}

node_num FileSystem::find_from_rootpath(const string& path) {
    // ���½���Path
    std::vector<std::string> tokens;
    std::istringstream iss(path);
    std::string token;
    while (getline(iss, token, '/')) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }

    // �Ӹ�Ŀ¼��ʼ����
    int ino = ROOT_INO;

    // ���β���ÿһ��Ŀ¼���ļ�
    for (const auto& token : tokens) {
        DiskInode inode = inodes[ino];

        DirectoryEntry *entry_table = (DirectoryEntry *)malloc(inode.d_size);

        if(entry_table == nullptr || read(inode, (buffer *)entry_table, inode.d_size, 0) == false) {
            std::cerr << "createFile: read dirextory entries failed." << std::endl;
            return -1;
        }

        // ��������Ŀ¼��
        bool found = false;
        int entry_no, entry_num = inode.d_size/ENTRY_SIZE;
        for (entry_no = 0; entry_no < entry_num; entry_no++) {
            // ��ͬ��
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

    // �ҵ�Ŀ��Ŀ¼��inode���
    node_num dir = find_from_rootpath(filename.substr(0, filename.rfind('/')));
    if (dir == -1) {
        std::cerr << "Failed to find directory: " << filename.substr(0, filename.rfind('/')) << std::endl;
        return false;
    }

    // ��Ŀ��Ŀ¼�´������ļ�
    node_num ino = createFile(dir, filename.substr(filename.rfind('/') + 1));
    if (ino == -1) {
        std::cerr << "Failed to create file: " << filename << std::endl;
        return false;
    }

    // ��ȡinode
    DiskInode& inode = inodes[ino];

    // ���ļ���ȡ��д��inode
    uint offset = 0;
    char buf[BLOCK_SIZE];
    while (infile) {
        infile.read(buf, BLOCK_SIZE);
        uint count = infile.gcount();

        while (count > 0) {
            // �ҵ���ǰƫ��������Ӧ��������
            uint blockno = offset / BLOCK_SIZE;
            block_num blkno = translate_block(inode, blockno);

            // �������鲻���ڣ������һ��
            if (blkno == 0) {
                blkno = alloc_block();
                if (blkno == 0) {
                    std::cerr << "Failed to allocate block for file: " << filename << std::endl;
                    return false;
                }
                inode.d_addr[blockno] = blkno;
            }

            // ��ȡ���������
            char* block_buf = read_block(blkno);

            // д������
            uint pos = offset % BLOCK_SIZE;
            uint n = std::min(BLOCK_SIZE - pos, (long)count);
            memcpy(block_buf + pos, buf + (infile.gcount() - count), n);
            count -= n;
            offset += n;

            // д�������
            if (!write_block(blkno, block_buf)) {
                std::cerr << "Failed to write block for file: " << filename << std::endl;
                return false;
            }
        }
    }

    // ����inode��Ϣ
    inode.d_size = offset;
    inode.d_mtime = get_cur_time();
    inode.d_atime = get_cur_time();
    inode.d_nlink = 1;

    return true;
}


// �����ڲ��ļ�ϵͳ��Ŀ¼��·�����ⲿ�ļ�ϵͳĿ¼��·��
const std::string internal_root_path = "/root";

bool FileSystem::initialize_from_external_directory(string external_root_path)
{
    // ���ȼ���ⲿ�ļ�ϵͳĿ¼�Ƿ����
    struct stat st;
    if (stat(external_root_path.c_str(), &st) != 0)
    {
        return false;
    }

    // Ȼ������ⲿ�ļ�ϵͳĿ¼�е������ļ�����Ŀ¼�������ڲ��ļ�ϵͳ�д�����Ӧ���ļ���Ŀ¼
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(external_root_path.c_str())) != NULL)
    {
        while ((ent = readdir(dir)) != NULL)
        {
            // ��������Ŀ¼��"."��".."��
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            {
                continue;
            }

            // �����ⲿ�ļ�ϵͳ���ļ���Ŀ¼������·��
            std::string external_file_path = external_root_path + "/" + ent->d_name;

            // ����ļ���Ŀ¼�Ƿ����
            struct stat st;
            if (stat(external_file_path.c_str(), &st) != 0)
            {
                continue;
            }

            // �����һ��Ŀ¼�������ڲ��ļ�ϵͳ�д�����Ӧ��Ŀ¼
            if (S_ISDIR(st.st_mode))
            {
                std::string internal_dir_path = internal_root_path + "/" + ent->d_name;
                create_directory(internal_dir_path);
            }
            // �����һ����ͨ�ļ��������ڲ��ļ�ϵͳ�д�����Ӧ���ļ��������ⲿ�ļ��е����ݸ��Ƶ��ڲ��ļ���
            else if (S_ISREG(st.st_mode))
            {
                std::string internal_file_path = internal_root_path + "/" + ent->d_name;
                std::ifstream external_file(external_file_path);
                std::ofstream internal_file(internal_file_path);
                internal_file << external_file.rdbuf();
            }
        }
        closedir(dir);
    }
    return true;
}
