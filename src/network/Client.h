#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <iostream>
#include <sstream>
#include <readline/readline.h>
#include <readline/history.h>
#include "Socket.h"
#include "../common/Common.h"
#include "../common/ResultSet.h"

namespace minidb {

/**
 * @brief 客户端
 * 
 * 提供交互式 CLI 界面，支持：
 * - 多行输入（以 `;` 结束语句）
 * - 多条语句（用 `;` 分隔，逐条执行）
 * - 命令历史（↑/↓ 箭头键浏览）
 * - TCP 发送 SQL 到服务端，接收并显示结果
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

        // 直接发送原始 SQL 语句
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

    // 运行交互式 CLI（支持多行输入 / 多条语句 / 命令历史）
    void run() {
        if (!connected_) {
            if (!connect()) return;
        }

        std::cout << std::endl;
        std::cout << "MiniDB Client" << std::endl;
        std::cout << "Type SQL statements or 'exit' to quit" << std::endl;
        std::cout << "Use ';' to end a statement (supports multi-line input)" << std::endl;
        std::cout << "Use " << (char)0xE2 << (char)0x86 << (char)0x91 
                  << "/" << (char)0xE2 << (char)0x86 << (char)0x93 
                  << " arrow keys for command history" << std::endl;
        std::cout << std::endl;

        std::string inputBuffer;
        std::string prompt = "minidb> ";

        while (true) {
            char* rawLine = readline(prompt.c_str());
            if (!rawLine) { // EOF (Ctrl+D)
                std::cout << std::endl;
                break;
            }

            std::string line(rawLine);
            std::free(rawLine);

            std::string trimmed = trim(line);

            // 空行：重置提示符
            if (trimmed.empty()) {
                if (inputBuffer.empty()) {
                    prompt = "minidb> ";
                }
                continue;
            }

            // 检查本地退出（仅在一行输入、无缓冲区时）
            if (inputBuffer.empty() && toLower(trimmed) == "exit") {
                std::cout << "Bye" << std::endl;
                break;
            }

            // 追加到输入缓冲区
            if (!inputBuffer.empty()) {
                inputBuffer += " ";
            }
            inputBuffer += trimmed;

            // 检查是否包含分号
            size_t semiPos = inputBuffer.find(';');
            if (semiPos == std::string::npos) {
                // 没有分号 → 多行续入模式
                prompt = "-> ";
                continue;
            }

            // --- 有分号，分割并逐条执行 ---
            bool endsWithSemi = (inputBuffer.back() == ';');

            size_t start = 0;
            size_t end;
            while ((end = inputBuffer.find(';', start)) != std::string::npos) {
                std::string stmt = trim(inputBuffer.substr(start, end - start));
                start = end + 1;
                if (stmt.empty()) continue;

                // 检查内联 exit
                if (toLower(stmt) == "exit") {
                    std::cout << "Bye" << std::endl;
                    disconnect();
                    return;
                }

                // 加入命令历史
                add_history(stmt.c_str());

                // 发送并显示结果
                ResultSet rs = executeSQL(stmt);
                std::cout << rs.format() << std::endl;
            }

            // 处理分号后的剩余文本
            std::string rest = trim(inputBuffer.substr(start));
            if (endsWithSemi) {
                // 末尾有分号 → 全部已执行，清空缓冲区
                inputBuffer.clear();
                prompt = "minidb> ";
            } else if (!rest.empty()) {
                // 末尾无分号 → 剩余文本作为续入
                inputBuffer = rest;
                prompt = "-> ";
            } else {
                inputBuffer.clear();
                prompt = "minidb> ";
            }
        }

        disconnect();
    }
};

} // namespace minidb

#endif // CLIENT_H
