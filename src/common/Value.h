#ifndef VALUE_H
#define VALUE_H

#include <string>
#include <cstdint>
#include <cstring>
#include <sstream>
#include "Common.h"

namespace minidb {

/**
 * @brief 数据值类型，支持 int 和 string
 * 
 * 对标MySQL的字段值，每个Value可以是整数或定长字符串。
 * string 类型最长 256 字符，UTF-8 编码。
 */
class Value {
public:
    DataType type;
    int intValue;
    char strValue[MAX_STRING_LEN + 1]; // 定长字符串，+1 for null terminator

    Value() : type(DataType::INT), intValue(0) {
        strValue[0] = '\0';
    }

    explicit Value(int val) : type(DataType::INT), intValue(val) {
        strValue[0] = '\0';
    }

    explicit Value(const std::string& val) : type(DataType::STRING), intValue(0) {
        if (val.size() > MAX_STRING_LEN) {
            std::strncpy(strValue, val.c_str(), MAX_STRING_LEN);
            strValue[MAX_STRING_LEN] = '\0';
        } else {
            std::strcpy(strValue, val.c_str());
        }
    }

    Value(const Value& other) : type(other.type), intValue(other.intValue) {
        std::strcpy(strValue, other.strValue);
    }

    Value& operator=(const Value& other) {
        if (this != &other) {
            type = other.type;
            intValue = other.intValue;
            std::strcpy(strValue, other.strValue);
        }
        return *this;
    }

    bool operator==(const Value& other) const {
        if (type != other.type) return false;
        if (type == DataType::INT) return intValue == other.intValue;
        return std::strcmp(strValue, other.strValue) == 0;
    }

    bool operator<(const Value& other) const {
        if (type != other.type) return false;
        if (type == DataType::INT) return intValue < other.intValue;
        return std::strcmp(strValue, other.strValue) < 0;
    }

    bool operator>(const Value& other) const {
        if (type != other.type) return false;
        if (type == DataType::INT) return intValue > other.intValue;
        return std::strcmp(strValue, other.strValue) > 0;
    }

    std::string toString() const {
        if (type == DataType::INT) {
            return std::to_string(intValue);
        } else {
            return std::string(strValue);
        }
    }

    // 序列化为二进制
    void serialize(std::ostream& os) const {
        os.write(reinterpret_cast<const char*>(&type), sizeof(DataType));
        if (type == DataType::INT) {
            os.write(reinterpret_cast<const char*>(&intValue), sizeof(int));
        } else {
            os.write(strValue, MAX_STRING_LEN + 1);
        }
    }

    // 反序列化
    void deserialize(std::istream& is) {
        is.read(reinterpret_cast<char*>(&type), sizeof(DataType));
        if (type == DataType::INT) {
            is.read(reinterpret_cast<char*>(&intValue), sizeof(int));
        } else {
            is.read(strValue, MAX_STRING_LEN + 1);
        }
    }

    // 获取序列化大小
    size_t serializedSize() const {
        size_t sz = sizeof(DataType);
        sz += (type == DataType::INT) ? sizeof(int) : (MAX_STRING_LEN + 1);
        return sz;
    }
};

} // namespace minidb

#endif // VALUE_H
