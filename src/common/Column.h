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

/**
 * @brief 外键定义
 * 
 * 描述一个外键约束：子表列名、父表表名、父表列名、级联行为。
 */
struct ForeignKeyDef {
    std::string column;         // 子表中的外键列名
    std::string refTable;       // 被引用的父表表名
    std::string refColumn;      // 被引用的父表列名
    bool onDeleteCascade;       // 是否 ON DELETE CASCADE

    ForeignKeyDef() : onDeleteCascade(false) {}

    ForeignKeyDef(const std::string& col, const std::string& rt,
                  const std::string& rc, bool cascade = false)
        : column(col), refTable(rt), refColumn(rc), onDeleteCascade(cascade) {}
};

} // namespace minidb

#endif // COLUMN_H
