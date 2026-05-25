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
        std::cout << timestamp() << " Connected to MiniDB server at "
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

        // 判断一个词是否为"新语句起始关键字"（select 除外，因为它可能出现在 CREATE VIEW ... AS 之后）
        auto isNewStmtKeyword = [](const std::string& w) -> bool {
            return w == "create" || w == "drop" || w == "use"
                || w == "insert" || w == "update" || w == "delete"
                || w == "exit";
        };

        // 提取一行文本的第一个词的 lower 版
        auto firstWordLower = [](const std::string& s) -> std::string {
            std::stringstream ss(s);
            std::string w;
            ss >> w;
            return toLower(w);
        };

        // 执行单条 SQL 并输出结果；返回 true = exit 信号
        auto execAndPrint = [&](const std::string& sql) -> bool {
            if (sql.empty()) return false;
            std::string lower = toLower(sql);
            if (lower == "exit") {
                std::cout << "Bye" << std::endl;
                disconnect();
                return true;
            }
            // ★ FIX: 历史记录带 ; 存储，↑ 调出后可直接回车执行
            add_history((sql + ";").c_str());
            ResultSet rs = executeSQL(sql);
            std::cout << timestamp() << " " << rs.format() << std::endl;
            return false;
        };

        // 将整个缓冲区按 ; 分割执行，处理残余文本
        auto flushBuffer = [&]() -> bool {
            if (inputBuffer.empty()) return false;

            size_t start = 0;
            size_t end;
            while ((end = inputBuffer.find(';', start)) != std::string::npos) {
                std::string stmt = trim(inputBuffer.substr(start, end - start));
                start = end + 1;
                if (stmt.empty()) continue;
                if (execAndPrint(stmt)) return true;
            }
            // 最后一个 ; 后的残余文本
            std::string rest = trim(inputBuffer.substr(start));
            if (!rest.empty()) {
                if (execAndPrint(rest)) return true;
            }
            inputBuffer.clear();
            prompt = "minidb> ";
            return false;
        };

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

            std::string firstLower = firstWordLower(trimmed);

            // ★ 核心修复：当缓冲区非空且新行以新语句关键字开头时，
            //   先将缓冲区中的内容作为完整语句执行（追加虚拟 ; 触发分割）。
            //   这样 "use school"（无 ;）+ 换行 + "drop school" 可以正确拆分。
            //   select 不在关键字列表中，所以 CREATE VIEW ... AS 换行后的 SELECT 不会误触发。
            if (!inputBuffer.empty() && isNewStmtKeyword(firstLower)) {
                inputBuffer += ";";
                if (flushBuffer()) return;
            }

            // 追加到缓冲区
            if (!inputBuffer.empty()) {
                inputBuffer += " ";
            }
            inputBuffer += trimmed;

            // 当前行包含分号？
            bool currentLineHasSemi = (trimmed.find(';') != std::string::npos);

            // 本地退出（仅单行、无缓冲区、无分号时）
            if (inputBuffer.empty() && !currentLineHasSemi && firstLower == "exit") {
                std::cout << "Bye" << std::endl;
                break;
            }

            if (!currentLineHasSemi) {
                // 无分号 → 多行续入模式
                prompt = "-> ";
                continue;
            }

            // 有分号 → 分割执行
            if (flushBuffer()) return;
        }

        disconnect();
    }
};

} // namespace minidb

#endif // CLIENT_H
