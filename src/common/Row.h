#ifndef ROW_H
#define ROW_H

#include "Value.h"
#include "ArrayList.h"

namespace minidb {

/**
 * @brief 表中的一行数据
 * 
 * 是 Value 的容器，每一行的列数应与表定义一致。
 */
class Row {
public:
    ArrayList<Value> values;

    Row() = default;
    Row(const Row& other) : values(other.values) {}
    Row& operator=(const Row& other) {
        if (this != &other) {
            values = other.values;
        }
        return *this;
    }

    size_t size() const { return values.size(); }

    void addValue(const Value& val) {
        values.push_back(val);
    }

    Value& operator[](size_t index) {
        return values[index];
    }

    const Value& operator[](size_t index) const {
        return values[index];
    }

    std::string toString() const {
        std::string result = "(";
        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) result += ", ";
            result += values[i].toString();
        }
        result += ")";
        return result;
    }
};

} // namespace minidb

#endif // ROW_H
