#ifndef COLUMN_H
#define COLUMN_H

#include <string>
#include "Common.h"

namespace minidb {

/**
 * @brief 表的列定义
 * 
 * 描述一个列的属性：名称、数据类型、是否主键。
 */
class Column {
public:
    std::string name;
    DataType type;
    bool isPrimaryKey;

    Column() : type(DataType::INT), isPrimaryKey(false) {}

    Column(const std::string& n, DataType t, bool pk = false)
        : name(n), type(t), isPrimaryKey(pk) {}

    Column(const Column& other)
        : name(other.name), type(other.type), isPrimaryKey(other.isPrimaryKey) {}

    Column& operator=(const Column& other) {
        if (this != &other) {
            name = other.name;
            type = other.type;
            isPrimaryKey = other.isPrimaryKey;
        }
        return *this;
    }

    bool operator==(const Column& other) const {
        return name == other.name;
    }

    std::string toString() const {
        std::string result = name + " " + (type == DataType::INT ? "int" : "string");
        if (isPrimaryKey) result += " primary";
        return result;
    }
};

} // namespace minidb

#endif // COLUMN_H
