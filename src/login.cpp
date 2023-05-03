#include "../include/login.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
using namespace std;

// ��֤�û����������Ƿ���ȷ
User verifyLogin(const string& username, const string& password) {
    vector<User> users;

    // �򿪴洢�û�����������ļ�
    ifstream file("users.txt");
    if (!file.is_open()) {
        return User(); // ����һ���յ� User ����
    }

    // ��ȡ�ļ��е��û���Ϣ
    while (!file.eof()) {
        User user;
        file >> user.username >> user.password >> user.group;
        users.push_back(user);
    }
    file.close();

    // �����û��б�����ƥ����û�
    for (int i = 0; i < users.size(); i++) {
        if (users[i].username == username) {
            // ��֤�����Ƿ���ȷ
            if (users[i].password == password) {
                return users[i]; // ���ذ����û���Ϣ�� User ����
            }
            else {
                return User(); // ����һ���յ� User �����ʾ�������
            }
        }
    }
    return User(); // ����һ���յ� User �����ʾ�û���������
}

User login(){
    string username, password;

    while(true){
        // ��ʾ�û������û���������
        cout << "Username: ";
        getline(cin, username);
        cout << "Password: ";
        getline(cin, password);

        // ��֤��¼��Ϣ
        User user = verifyLogin(username, password);
        if (user.username != "") {
            cout << "Login successful" << endl;
            return user;
        }
        else {
            cout << "Incorrect username or password" << endl;
        }
    }
}
