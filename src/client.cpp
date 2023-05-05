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
    user.set_current_dir(2);

    FileSystem fs("myDisk.img");
    fs.set_u(&user);


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
