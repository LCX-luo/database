#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <iostream>
#include "Socket.h"
#include "../common/Common.h"
#include "../common/ResultSet.h"

namespace minidb {

/**
 * @brief 客户端
 * 
 * 提供交互式 CLI 界面，用户输入 SQL 语句，
 * 通过 TCP 发送到服务端，接收并显示结果。
 */
class Client {
private:
    Socket socket_;
    std::string serverIp_;
    int port_;
    bool connected_;

public:
    explicit Client(const std::string& ip = "127.0.0.1", int port = DEFAULT_PORT)
        : serverIp_(ip), port_(port), connected_(false) {}

    ~Client() {
        disconnect();
    }

    bool connect() {
        if (!socket_.connectToServer(serverIp_, port_)) {
            std::cerr << "Failed to connect to server at " 
                      << serverIp_ << ":" << port_ << std::endl;
            return false;
        }
        connected_ = true;
        std::cout << "Connected to MiniDB server at " 
                  << serverIp_ << ":" << port_ << std::endl;
        return true;
    }

    void disconnect() {
        if (connected_) {
            socket_.send("exit");
            socket_.close();
            connected_ = false;
        }
    }

    bool isConnected() const { return connected_; }

    // 发送 SQL 并获取结果
    ResultSet executeSQL(const std::string& sql) {
        if (!connected_) {
            return ResultSet(ERR_GENERAL, "Not connected to server");
        }

        // 直接发送原始 SQL 语句（避免 SQL 中的双引号破坏 JSON 格式）
        if (!socket_.send(sql)) {
            return ResultSet(ERR_GENERAL, "Failed to send request");
        }

        // 接收响应
        std::string response = socket_.receive();
        if (response.empty()) {
            return ResultSet(ERR_GENERAL, "Server disconnected");
        }

        return ResultSet::fromJson(response);
    }

    // 运行交互式 CLI
    void run() {
        if (!connected_) {
            if (!connect()) return;
        }

        std::cout << std::endl;
        std::cout << "MiniDB Client" << std::endl;
        std::cout << "Type SQL statements or 'exit' to quit" << std::endl;
        std::cout << std::endl;

        std::string line;
        while (true) {
            std::cout << "minidb> ";
            std::getline(std::cin, line);

            std::string trimmed = trim(line);
            if (trimmed.empty()) continue;

            // 检查本地退出
            if (toLower(trimmed) == "exit") {
                std::cout << "Bye" << std::endl;
                break;
            }

            // 发送并获取结果
            ResultSet rs = executeSQL(trimmed);

            // 显示结果
            std::cout << rs.format() << std::endl;
        }

        disconnect();
    }
};

} // namespace minidb

#endif // CLIENT_H
