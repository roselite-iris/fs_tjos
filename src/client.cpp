#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iterator>
#include "../include/user.h"
#include "../include/command.h"
#include "../include/fs.h"

using namespace std;

extern User login();

Command commands[] = {
    //{"ls", &listFiles},
    //{"mkdir", &makeDirectory},
    //{"cd", &changeDirectory},
    // ��������������������
};

int main() {
    User user = login();

    FileSystem fs("myDisk.img");
    cout<<"inum="<<fs.sb.s_ninode<<endl;




    while (true) {
        // ��ȡ�û����������
        string input;
        cout << ">> ";
        std::getline(std::cin, input);

        // ��������
        std::istringstream iss(input);
        string s;
        vector<std::string> tokens;
        while(iss){
            iss >> s;
            tokens.emplace_back(s);
            cout<<s;
        }
        if (tokens.empty()) {
            continue;
        }

        // �������б��в��Ҷ�Ӧ������
        bool found = false;
        for (const auto& command : commands) {
            if (command.name == tokens[0]) {
                // ִ������
                command.handler();
                found = true;
                break;
            }
        }

        if (!found) {
            std::cout << "Invalid command." << std::endl;
        }
    }


    fs.~FileSystem();
    return 0;
}
