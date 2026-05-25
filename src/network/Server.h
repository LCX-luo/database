#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <thread>
#include "Socket.h"
#include "../common/Common.h"
#include "../common/ResultSet.h"
#include "../parser/SQLParser.h"
#include "../executor/Executor.h"

namespace minidb {

/**
 * @brief 服务端
 * 
 * 监听 TCP 连接，接收客户端发送的 SQL 语句，
 * 解析并执行，将结果以 JSON 格式返回给客户端。
 */
class Server {
private:
    Socket serverSocket_;
    int port_;
    StorageEngine storage_;
    SQLParser parser_;
    Executor executor_;
    bool running_;

public:
    explicit Server(int port = DEFAULT_PORT)
        : port_(port), executor_(storage_), running_(false) {}

    ~Server() {
        stop();
    }

    bool start() {
        if (!serverSocket_.createServer(port_)) {
            std::cerr << "Failed to start server on port " << port_ << std::endl;
            return false;
        }
        running_ = true;
        std::cout << "Server started on port " << port_ << std::endl;
        std::cout << "Waiting for connections..." << std::endl;

        while (running_) {
            Socket clientSocket = serverSocket_.accept();
            if (!clientSocket.isValid()) {
                if (running_) {
                    std::cerr << "Failed to accept connection" << std::endl;
                }
                continue;
            }

            // 处理客户端连接
            handleClient(std::move(clientSocket));
        }

        return true;
    }

    void stop() {
        running_ = false;
        serverSocket_.close();
    }

private:
    void handleClient(Socket clientSocket) {
        std::cout << "New client connected" << std::endl;

        while (true) {
            std::string request = clientSocket.receive();
            if (request.empty()) {
                std::cout << "Client disconnected" << std::endl;
                break;
            }

            // 解析 JSON 请求
            std::string sql;
            size_t sqlPos = request.find("\"sql\":\"");
            if (sqlPos != std::string::npos) {
                sqlPos += 7;  // skip "sql":"
                size_t sqlEnd = request.find('"', sqlPos);
                if (sqlEnd != std::string::npos) {
                    sql = request.substr(sqlPos, sqlEnd - sqlPos);
                }
            }

            if (sql.empty()) {
                // 直接使用请求内容作为 SQL
                sql = request;
            }

            std::cout << "Execute: " << sql << std::endl;

            // 检查 exit
            if (toLower(trim(sql)) == "exit") {
                ResultSet rs(SUCCESS, "Bye");
                clientSocket.send(rs.toJson());
                break;
            }

            // 解析和执行
            ResultSet result = executeSQL(sql);

            // 返回结果
            clientSocket.send(result.toJson());
        }
    }

public:
    ResultSet executeSQL(const std::string& sql) {
        std::unique_ptr<ASTNode> ast = parser_.parse(sql);
        if (!ast) {
            return ResultSet(ERR_SYNTAX, "Syntax error: unable to parse statement");
        }
        return executor_.execute(*ast);
    }
};

} // namespace minidb

#endif // SERVER_H
