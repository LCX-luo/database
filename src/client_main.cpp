#include <iostream>
#include <string>
#include "network/Client.h"

int main(int argc, char* argv[]) {
    std::string ip = "127.0.0.1";
    int port = minidb::DEFAULT_PORT;

    if (argc > 1) {
        ip = argv[1];
    }
    if (argc > 2) {
        port = std::stoi(argv[2]);
    }

    minidb::Client client(ip, port);
    client.run();

    return 0;
}
