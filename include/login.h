#ifndef LOGIN_H
#define LOGIN_H

// �û���Ϣ
#include <string>
struct User {
    std::string username;
    std::string password;
    int group;
};

// ��֤�û����������Ƿ���ȷ
User login();
#endif // LOGIN_H
