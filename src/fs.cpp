#include "../include/fs.h"
#include <iostream>
using namespace std;



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
    for (int i = 0; i < sb.s_ninode; i++) {
        disk.seekg(OFFSET_INODE, std::ios::beg);
        disk.read(reinterpret_cast<char*>(&inodes[i]), sizeof(DiskInode));
    }

    // ����
    diskfile_ = diskfile;
    disk_ = std::move(disk);
}

FileSystem::~FileSystem() {
    // ��������
    disk_.seekp(0, std::ios::beg);
    disk_.write(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

    for (int i = 0; i < sb.s_ninode; i++) {
        disk_.seekp(OFFSET_INODE, std::ios::beg);
        disk_.write(reinterpret_cast<char*>(&inodes[i]), sizeof(DiskInode));
    }

    // �ر��ļ�
    disk_.close();
}

