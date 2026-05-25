#ifndef SOCKET_H
#define SOCKET_H

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

namespace minidb {

/**
 * @brief TCP Socket 封装
 * 
 * 基于 POSIX Socket API 的简单封装，支持服务端和客户端。
 */
class Socket {
private:
    int sockfd_;
    bool isClosed_;

public:
    Socket() : sockfd_(-1), isClosed_(true) {}

    explicit Socket(int fd) : sockfd_(fd), isClosed_(false) {}

    ~Socket() {
        close();
    }

    // 创建服务端 Socket
    bool createServer(int port) {
        sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_ < 0) {
            return false;
        }

        // 允许地址重用
        int opt = 1;
        if (setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            close();
            return false;
        }

        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(sockfd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close();
            return false;
        }

        if (listen(sockfd_, 5) < 0) {
            close();
            return false;
        }

        isClosed_ = false;
        return true;
    }

    // 创建客户端 Socket 并连接
    bool connectToServer(const std::string& ip, int port) {
        sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_ < 0) {
            return false;
        }

        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
            close();
            return false;
        }

        if (connect(sockfd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close();
            return false;
        }

        isClosed_ = false;
        return true;
    }

    // 接受客户端连接（服务端用）
    Socket accept() {
        struct sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);
        int clientFd = ::accept(sockfd_, (struct sockaddr*)&clientAddr, &addrLen);
        if (clientFd < 0) {
            return Socket(-1);
        }
        return Socket(clientFd);
    }

    // 发送数据
    bool send(const std::string& data) {
        if (sockfd_ < 0) return false;

        std::string msg = data + "\n";  // 用换行符作为消息结束标志
        size_t totalSent = 0;
        while (totalSent < msg.size()) {
            ssize_t n = ::send(sockfd_, msg.c_str() + totalSent, 
                              msg.size() - totalSent, 0);
            if (n < 0) return false;
            totalSent += n;
        }
        return true;
    }

    // 接收数据（直到换行符）
    std::string receive() {
        if (sockfd_ < 0) return "";

        std::string result;
        char buf;
        while (true) {
            ssize_t n = ::recv(sockfd_, &buf, 1, 0);
            if (n <= 0) {
                if (result.empty()) return "";
                break;
            }
            if (buf == '\n') break;
            result += buf;
        }
        return result;
    }

    void close() {
        if (!isClosed_ && sockfd_ >= 0) {
            ::close(sockfd_);
            sockfd_ = -1;
            isClosed_ = true;
        }
    }

    bool isValid() const { return sockfd_ >= 0 && !isClosed_; }
    int getFd() const { return sockfd_; }
};

} // namespace minidb

#endif // SOCKET_H
