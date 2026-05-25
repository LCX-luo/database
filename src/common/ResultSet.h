#ifndef RESULT_SET_H
#define RESULT_SET_H

#include <string>
#include <sstream>
#include "Column.h"
#include "Row.h"
#include "ArrayList.h"

namespace minidb {

/**
 * @brief 查询结果集
 * 
 * 包含列信息和多行数据，用于 select 语句的返回结果，
 * 也可作为 insert/update/delete 的受影响行数反馈。
 */
class ResultSet {
public:
    ArrayList<Column> columns;
    ArrayList<Row> rows;
    std::string message;
    int code;       // 0=成功, 负值=错误
    int affectedRows; // 受影响行数

    ResultSet() : code(SUCCESS), affectedRows(0) {
        message = "Success";
    }

    explicit ResultSet(int c, const std::string& msg = "")
        : code(c), affectedRows(0) {
        if (msg.empty()) {
            message = (c == SUCCESS) ? "Success" : "Error";
        } else {
            message = msg;
        }
    }

    void setError(const std::string& msg) {
        code = ERR_GENERAL;
        message = msg;
    }

    void setSuccess(const std::string& msg = "") {
        code = SUCCESS;
        message = msg.empty() ? "Query OK" : msg;
    }

    // 格式化输出为表格
    std::string format() const {
        if (code != SUCCESS) {
            return "ERROR: " + message;
        }

        std::ostringstream oss;

        // 对于 DDL/DML 的非查询语句，显示消息内容
        if (columns.empty() && rows.empty()) {
            if (message.find("row") != std::string::npos) {
                // 消息中已有具体行数描述（如 "1 row inserted" / "2 rows deleted"），直接显示
                oss << message;
            } else if (affectedRows > 0) {
                // 通用格式
                oss << "Query OK, " << affectedRows << " rows affected";
            } else {
                oss << message;
            }
            return oss.str();
        }

        // 对于 select 查询，显示表格

        // 计算每列宽度
        ArrayList<size_t> widths;
        for (size_t i = 0; i < columns.size(); ++i) {
            size_t w = columns[i].name.size();
            for (size_t j = 0; j < rows.size(); ++j) {
                size_t val_len = rows[j][i].toString().size();
                if (val_len > w) w = val_len;
            }
            widths.push_back(w + 2); // +2 for padding
        }

        // 画分隔线
        auto drawLine = [&]() {
            for (size_t i = 0; i < widths.size(); ++i) {
                oss << "+";
                for (size_t j = 0; j < widths[i]; ++j) oss << "-";
            }
            oss << "+" << "\n";
        };

        // 画表头
        drawLine();
        for (size_t i = 0; i < columns.size(); ++i) {
            oss << "| " << columns[i].name;
            for (size_t j = columns[i].name.size() + 1; j < widths[i]; ++j) oss << " ";
        }
        oss << "|\n";
        drawLine();

        // 画数据行
        for (size_t i = 0; i < rows.size(); ++i) {
            for (size_t j = 0; j < columns.size(); ++j) {
                std::string val = rows[i][j].toString();
                oss << "| " << val;
                for (size_t k = val.size() + 1; k < widths[j]; ++k) oss << " ";
            }
            oss << "|\n";
        }
        drawLine();

        oss << rows.size() << (rows.size() == 1 ? " row" : " rows") << " in set\n";
        return oss.str();
    }

    // 序列化为 JSON 字符串（用于网络传输）
    std::string toJson() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"code\":" << code << ",";
        oss << "\"message\":\"" << escapeJson(message) << "\",";
        oss << "\"affectedRows\":" << affectedRows << ",";

        oss << "\"columns\":[";
        for (size_t i = 0; i < columns.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "{";
            oss << "\"name\":\"" << columns[i].name << "\",";
            oss << "\"type\":\"" << (columns[i].type == DataType::INT ? "int" : "string") << "\",";
            oss << "\"primary\":" << (columns[i].isPrimaryKey ? "true" : "false");
            oss << "}";
        }
        oss << "],";

        oss << "\"rows\":[";
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "[";
            for (size_t j = 0; j < rows[i].size(); ++j) {
                if (j > 0) oss << ",";
                const Value& v = rows[i][j];
                if (v.type == DataType::INT) {
                    oss << v.intValue;
                } else {
                    oss << "\"" << escapeJson(v.toString()) << "\"";
                }
            }
            oss << "]";
        }
        oss << "]";
        oss << "}";
        return oss.str();
    }

    // 从 JSON 反序列化（简化：从字符串解析）
    static ResultSet fromJson(const std::string& json) {
        ResultSet rs;
        // 简单解析，仅提取 code 和 message
        auto findVal = [](const std::string& s, const std::string& key) -> std::string {
            size_t pos = s.find("\"" + key + "\":");
            if (pos == std::string::npos) return "";
            pos = s.find(':', pos) + 1;
            // skip whitespace
            while (pos < s.size() && s[pos] == ' ') pos++;
            if (pos >= s.size()) return "";
            if (s[pos] == '"') {
                pos++;
                size_t end = s.find('"', pos);
                if (end == std::string::npos) return "";
                return s.substr(pos, end - pos);
            } else {
                size_t end = s.find_first_of(",}]", pos);
                if (end == std::string::npos) return "";
                return s.substr(pos, end - pos);
            }
        };
        std::string codeStr = findVal(json, "code");
        if (!codeStr.empty()) rs.code = std::stoi(codeStr);
        rs.message = findVal(json, "message");

        std::string affectedStr = findVal(json, "affectedRows");
        if (!affectedStr.empty()) rs.affectedRows = std::stoi(affectedStr);

        // 解析 columns 数组
        size_t colsStart = json.find("\"columns\":[");
        if (colsStart != std::string::npos) {
            colsStart += 10; // skip "columns":[
            size_t colsEnd = json.find(']', colsStart);
            if (colsEnd != std::string::npos) {
                std::string colsStr = json.substr(colsStart, colsEnd - colsStart);
                // 解析每个 {name, type, primary}
                size_t cur = 0;
                while (cur < colsStr.size()) {
                    size_t braceStart = colsStr.find('{', cur);
                    if (braceStart == std::string::npos) break;
                    size_t braceEnd = colsStr.find('}', braceStart);
                    if (braceEnd == std::string::npos) break;
                    std::string colJson = colsStr.substr(braceStart, braceEnd - braceStart + 1);
                    Column col;
                    std::string colName = findVal(colJson, "name");
                    if (!colName.empty()) col.name = colName;
                    std::string colType = findVal(colJson, "type");
                    col.type = (colType == "int") ? DataType::INT : DataType::STRING;
                    std::string colPrimary = findVal(colJson, "primary");
                    col.isPrimaryKey = (colPrimary == "true");
                    rs.columns.push_back(col);
                    cur = braceEnd + 1;
                }
            }
        }

        // 解析 rows 数组
        // JSON 格式示例: "rows":[[2024001,"张三",20],[2024002,"李四",21]]
        size_t rowsStart = json.find("\"rows\":[");
        if (rowsStart != std::string::npos) {
            rowsStart += 7; // skip past "rows":[
            // 找到匹配的闭合 ]
            int depth = 0;
            size_t rowsEnd = std::string::npos;
            for (size_t i = rowsStart; i < json.size(); ++i) {
                if (json[i] == '[') depth++;
                else if (json[i] == ']') {
                    depth--;
                    if (depth == 0) { rowsEnd = i; break; }
                }
            }
            if (rowsEnd != std::string::npos) {
                std::string rowsStr = json.substr(rowsStart, rowsEnd - rowsStart);
                // rowsStr = [[2024001,"张三",20],[2024002,"李四",21],[2024003,"王五",19]]
                // 跳过外层 [，从位置 1 开始
                size_t cur = 1;
                while (cur < rowsStr.size()) {
                    // 每个内层行数组以 [ 开头
                    size_t arrStart = rowsStr.find('[', cur);
                    if (arrStart == std::string::npos) break;
                    // 找到匹配的 ]
                    size_t arrEnd = rowsStr.find(']', arrStart + 1);
                    if (arrEnd == std::string::npos) break;
                    // 提取行内容（不含 []）
                    std::string rowStr = rowsStr.substr(arrStart + 1, arrEnd - arrStart - 1);

                    Row row;
                    // 解析逗号分隔的值
                    std::stringstream valSs(rowStr);
                    std::string val;
                    while (std::getline(valSs, val, ',')) {
                        val = trim(val);
                        if (val.empty()) continue;
                        if (val.front() == '"') {
                            // 去掉字符串值的引号
                            std::string strVal = val.substr(1, val.size() - 2);
                            row.addValue(Value(strVal));
                        } else {
                            row.addValue(Value(std::stoi(val)));
                        }
                    }
                    rs.rows.push_back(row);
                    cur = arrEnd + 1;
                }
            }
        }

        return rs;
    }

private:
    static std::string escapeJson(const std::string& s) {
        std::string result;
        for (char c : s) {
            if (c == '"') result += "\\\"";
            else if (c == '\\') result += "\\\\";
            else if (c == '\n') result += "\\n";
            else if (c == '\r') result += "\\r";
            else if (c == '\t') result += "\\t";
            else result += c;
        }
        return result;
    }
};

} // namespace minidb

#endif // RESULT_SET_H
