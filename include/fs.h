#include "sb.h"
#include "dinode.h"
#include "directory.h"
#include "user.h"
#include <string>
#include <fstream>

typedef unsigned int uint;

class FileSystem {

private:
    User *user_;
    std::fstream disk_;     // Disk file stream
    std::string diskfile_;  // Disk file name
    SuperBlock sb;          // Super block
    DiskInode inodes[INODE_NUM];  // Inode table


private:
    /*
    * --------- ����� ------------
    */

    /* ��ȡһ������inode����������ʼ�� */
    node_num alloc_inode();

    /* ��ȡһ����������� */
    block_num alloc_block();

    /* ��ȡһ���������������ݣ�����ָ����Ƭ�����buffer(char *)���� */
    buffer* read_block(block_num blkno);
    
    /* д��һ�������(ȫ����) */
    bool write_block(block_num blkno, buffer* buf);





    /*
    * --------- �ļ�(inode)�� ------------
    */

    /* ����һ���ļ�����Ѱ�������ĵ�no�����ݿ��blkno */
    block_num translate_block(const DiskInode& inode, uint no);

	/* ����һ���ļ��������Ľ�β����µĿ�������飬�����µ������� */
    block_num file_add_block(DiskInode& inode);

    /* ϵͳ�ڽӿڣ�����ļ��Ķ�����ƫ��������ȡsize��С�����ݣ����ض�ȡ���� */
    uint read(const DiskInode& inode, buffer* buf, uint size, uint offset);

    /* ϵͳ�ڽӿڣ�����ļ���д����ƫ������д��size��С�����ݣ�����д���� */
    uint write(DiskInode& inode, const buffer* buf, uint size, uint offset);

    /*
    * --------- Ŀ¼�� ------------
    */
   
    /* ����ļ�·���Ĳ��ң��Ӹ���·�����Ҷ�Ӧ�ļ���inode��� ���򷵻�-1 */
    node_num find_from_rootpath(const std::string& path);
   
    

public:
    FileSystem() {};
    FileSystem(const std::string& diskfile);
    ~FileSystem();

    void set_u(User *u) {user_ = u;};
    
    /* ��һ���ⲿ�ļ�ϵͳĿ¼��Ϊ�ڲ��ļ�ϵͳ�ĸ�Ŀ¼����ʼ���ļ�ϵͳ��Ŀ¼���ļ� */
    bool initialize_from_external_directory(const std::string& external_root_path, const node_num root_no = 1);
    
    node_num createFile(const node_num dir, const std::string& filename, DirectoryEntry::FileType type);
    //��dirĿ¼�´���һ�����ļ�

    bool createDir(const node_num dir, const std::string& dirname);
    //��dirĿ¼�´���һ����Ŀ¼

    bool loadFile(const std::string& filename, const std::string& dst);
    //���ļ�ϵͳ�м���һ���ļ������临�Ƶ�����Ŀ¼��

    bool saveFile(const std::string& src, const std::string& filename);
    //�������ļ����Ƶ��ļ�ϵͳ��

    bool deleteDir(const std::string& dirname);
    //ɾ����ǰĿ¼�µ�ָ��Ŀ¼

    bool deleteFile(const std::string& filename);
    //ɾ����ǰĿ¼�µ�ָ���ļ�

    bool openFile(const std::string& filename);
    //���û����ļ��������һ���ļ���׼�����ж�д����

    bool closeFile(const std::string& filename);
    //���û����ļ������Ƴ�ָ���ļ�

    bool readFile(const std::string& filename, char* buffer, int size, int offset);
    //��ָ���ļ��ж�ȡ���ݵ���������

    bool writeFile(const std::string& filename, const char* buffer, int size, int offset);
    //���������е�����д��ָ���ļ���

    bool moveFile(const std::string& src, const std::string& dst);
    //��һ���ļ���һ��Ŀ¼�ƶ�����һ��Ŀ¼

    bool renameFile(const std::string& filename, const std::string& newname);
    //�޸��ļ���

    bool changeDir(const std::string& dirname);
    //���ĵ�ǰĿ¼��ָ��Ŀ¼

    //bool listDir(std::vector<std::string>& files);
    //��ȡ��ǰĿ¼�µ������ļ���Ŀ¼

    bool getFileInfo(const std::string& filename, DiskInode& ino);
    //��ȡָ���ļ�����ϸ��Ϣ�����ļ���С������ʱ���



};
