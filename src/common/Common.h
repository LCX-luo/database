#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <memory>
#include <utility>
#include <ctime>

namespace minidb {

// 数据类型枚举
enum class DataType {
    INT,
    STRING
};

// 操作符枚举
enum class Operator {
    EQ,  // =
    LT,  // <
    GT,  // >
    NONE // 无条件
};

// 语句类型枚举
enum class StatementType {
    CREATE_DATABASE,
    DROP_DATABASE,
    USE_DATABASE,
    CREATE_TABLE,
    DROP_TABLE,
    CREATE_VIEW,
    DROP_VIEW,
    INSERT,
    SELECT,
    UPDATE,
    DELETE,
    EXIT,
    UNKNOWN
};

// 错误码
constexpr int SUCCESS = 0;
constexpr int ERR_GENERAL = -1;
constexpr int ERR_NOT_FOUND = -2;
constexpr int ERR_EXISTS = -3;
constexpr int ERR_SYNTAX = -4;
constexpr int ERR_TYPE = -5;
constexpr int ERR_NOT_IMPLEMENTED = -6;

// 字符串最大长度
constexpr int MAX_STRING_LEN = 256;
// B+树阶数
constexpr int BPLUS_ORDER = 4;
// 默认服务器端口
constexpr int DEFAULT_PORT = 23333;

// 获取当前时间戳字符串 [HH:MM:SS]
inline std::string timestamp() {
    time_t now = time(nullptr);
    struct tm* tm = localtime(&now);
    char buf[16];
    strftime(buf, sizeof(buf), "%H:%M:%S", tm);
    return "[" + std::string(buf) + "]";
}

// 将字符串转为小写
inline std::string toLower(const std::string& str) {
    std::string result = str;
    for (char& c : result) {
        if (c >= 'A' && c <= 'Z') {
            c += 32;
        }
    }
    return result;
}

// 去除字符串两端空白
inline std::string trim(const std::string& str) {
    if (str.empty()) return str;
    size_t start = 0;
    while (start < str.size() && (str[start] == ' ' || str[start] == '\t' || str[start] == '\n' || str[start] == '\r')) {
        start++;
    }
    size_t end = str.size();
    while (end > start && (str[end-1] == ' ' || str[end-1] == '\t' || str[end-1] == '\n' || str[end-1] == '\r')) {
        end--;
    }
    return str.substr(start, end - start);
}

} // namespace minidb

#endif // COMMON_H
