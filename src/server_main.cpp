#include <iostream>
#include <csignal>
#include "network/Server.h"

minidb::Server* g_server = nullptr;

void signalHandler(int sig) {
    std::cout << "\nShutting down server..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    int port = minidb::DEFAULT_PORT;
    if (argc > 1) {
        port = std::stoi(argv[1]);
    }

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    minidb::Server server(port);
    g_server = &server;

    if (!server.start()) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    return 0;
}
