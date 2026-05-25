#ifndef SQL_PARSER_H
#define SQL_PARSER_H

#include <string>
#include <sstream>
#include <memory>
#include "Common.h"
#include "ASTNode.h"
#include "ArrayList.h"

namespace minidb {

/**
 * @brief SQL 解析器
 * 
 * 将 SQL 文本解析为 AST（抽象语法树）。
 * 支持词法分析和语法分析。
 * 
 * 支持的语句：
 * - create database <dbname>
 * - drop database <dbname>
 * - use <dbname>
 * - create table <name> (<col> <type> [primary], ...)
 * - drop table <name>
 * - create view <name> as select <cols> from <table> [where <cond>]
 * - drop view <name>
 * - insert <table> values(<val>, ...)
 * - select <col>|* from <table> [where <col> <op> <val>]
 * - update <table> set <col>=<val> [where <cond>]
 * - delete <table> [where <cond>]
 * - exit
 */
class SQLParser {
public:
    SQLParser() = default;

    // 解析单条 SQL 语句
    std::unique_ptr<ASTNode> parse(const std::string& sql) {
        std::string trimmed = trim(sql);
        if (trimmed.empty()) {
            return nullptr;
        }

        std::string lower = toLower(trimmed);
        std::stringstream ss(lower);
        std::string firstWord;
        ss >> firstWord;

        if (firstWord == "exit") {
            return std::make_unique<ExitNode>();
        }

        if (firstWord == "create") {
            return parseCreate(trimmed, lower, ss);
        }
        if (firstWord == "drop") {
            return parseDrop(trimmed, lower, ss);
        }
        if (firstWord == "use") {
            return parseUse(trimmed, lower, ss);
        }
        if (firstWord == "insert") {
            return parseInsert(trimmed, lower, ss);
        }
        if (firstWord == "select") {
            return parseSelect(trimmed, lower, ss);
        }
        if (firstWord == "update") {
            return parseUpdate(trimmed, lower, ss);
        }
        if (firstWord == "delete") {
            return parseDelete(trimmed, lower, ss);
        }

        // 未知语句
        return nullptr;
    }

private:
    // 解析 create database / create table / create view
    std::unique_ptr<ASTNode> parseCreate(const std::string& original, const std::string& lower, std::stringstream& ss) {
        std::string keyword;
        ss >> keyword;  // "database" or "table" or "view"

        if (keyword == "database") {
            std::string dbName;
            ss >> dbName;
            if (dbName.empty()) return nullptr;

            auto node = std::make_unique<CreateDatabaseNode>();
            node->dbName = dbName;
            return node;
        }

        if (keyword == "view") {
            return parseCreateView(original, lower, ss);
        }

        if (keyword == "table") {
            std::string tableName;
            ss >> tableName;
            if (tableName.empty()) return nullptr;

            // 读取列定义部分：找 '(' 和 ')'
            auto node = std::make_unique<CreateTableNode>();
            node->tableName = tableName;

            // 找括号
            size_t parenStart = original.find('(');
            size_t parenEnd = original.find(')');
            if (parenStart == std::string::npos || parenEnd == std::string::npos) {
                // 没有括号，尝试用 lower 找
                parenStart = lower.find('(');
                parenEnd = lower.find(')');
            }

            if (parenStart != std::string::npos && parenEnd != std::string::npos) {
                std::string colsStr = original.substr(parenStart + 1, parenEnd - parenStart - 1);
                std::stringstream colSs(colsStr);
                std::string colDef;

                while (std::getline(colSs, colDef, ',')) {
                    colDef = trim(colDef);
                    if (colDef.empty()) continue;

                    std::stringstream defSs(colDef);
                    ColumnDef col;
                    defSs >> col.name;
                    defSs >> col.type;

                    // 检查是否有 primary 关键字
                    std::string extra;
                    if (defSs >> extra) {
                        std::string extraLower = toLower(extra);
                        if (extraLower == "primary" || extraLower == "primarykey" || extraLower == "primary_key") {
                            col.isPrimaryKey = true;
                        }
                    }

                    node->columns.push_back(col);
                }
            }

            return node;
        }

        return nullptr;
    }

    // 解析 drop database / drop table / drop view
    std::unique_ptr<ASTNode> parseDrop(const std::string& original, const std::string& lower, std::stringstream& ss) {
        std::string keyword;
        ss >> keyword;  // "database" or "table" or "view"

        if (keyword == "database") {
            std::string dbName;
            ss >> dbName;
            if (dbName.empty()) return nullptr;

            auto node = std::make_unique<DropDatabaseNode>();
            node->dbName = dbName;
            return node;
        }

        if (keyword == "view") {
            std::string viewName;
            ss >> viewName;
            if (viewName.empty()) return nullptr;

            auto node = std::make_unique<DropViewNode>();
            node->viewName = viewName;
            return node;
        }

        if (keyword == "table") {
            std::string tableName;
            ss >> tableName;
            if (tableName.empty()) return nullptr;

            auto node = std::make_unique<DropTableNode>();
            node->tableName = tableName;
            return node;
        }

        return nullptr;
    }

    // 解析 use
    std::unique_ptr<ASTNode> parseUse(const std::string& original, const std::string& lower, std::stringstream& ss) {
        std::string dbName;
        ss >> dbName;
        if (dbName.empty()) return nullptr;

        auto node = std::make_unique<UseDatabaseNode>();
        node->dbName = dbName;
        return node;
    }

    // 解析 insert
    std::unique_ptr<ASTNode> parseInsert(const std::string& original, const std::string& lower, std::stringstream& ss) {
        std::string tableName;
        ss >> tableName;
        if (tableName.empty()) return nullptr;

        // 接下来是 "values(...)" 关键字
        std::string valuesKw;
        ss >> valuesKw;

        auto node = std::make_unique<InsertNode>();
        node->tableName = tableName;

        // 用原始字符串提取括号中的值
        size_t parenStart = original.find('(');
        size_t parenEnd = original.rfind(')');

        if (parenStart != std::string::npos && parenEnd != std::string::npos && parenEnd > parenStart) {
            std::string valuesStr = original.substr(parenStart + 1, parenEnd - parenStart - 1);
            std::stringstream valSs(valuesStr);
            std::string val;

            while (std::getline(valSs, val, ',')) {
                val = trim(val);
                if (!val.empty()) {
                    node->values.push_back(val);
                }
            }
        }

        return node;
    }

    // 解析 select
    std::unique_ptr<ASTNode> parseSelect(const std::string& original, const std::string& lower, std::stringstream& ss) {
        auto node = std::make_unique<SelectNode>();

        // 读取列名（直到 "from"）
        std::string token;
        bool readingColumns = true;
        while (ss >> token) {
            if (token == "from") {
                readingColumns = false;
                continue;
            }
            if (readingColumns) {
                // 去除逗号
                if (!token.empty() && token.back() == ',') {
                    token.pop_back();
                }
                node->columns.push_back(token);
            } else {
                node->tableName = token;
                break;
            }
        }

        // 如果表名后还有内容，尝试用原始字符串解析 where 条件
        size_t wherePos = toLower(original).find(" where ");
        if (wherePos != std::string::npos) {
            std::string conditionStr = original.substr(wherePos + 7);
            conditionStr = trim(conditionStr);
            parseCondition(conditionStr, node->condition);
        }

        return node;
    }

    // 解析 update
    std::unique_ptr<ASTNode> parseUpdate(const std::string& original, const std::string& lower, std::stringstream& ss) {
        auto node = std::make_unique<UpdateNode>();

        ss >> node->tableName;

        // 读取 "set"
        std::string setKw;
        ss >> setKw;

        // 用原始字符串解析 set 表达式
        // 格式: update <table> set <column> = <value> [where ...]
        size_t setPos = toLower(original).find(" set ");
        if (setPos != std::string::npos) {
            std::string afterSet = original.substr(setPos + 5);  // 跳过 " set "
            afterSet = trim(afterSet);

            // 查找 where 分隔位置
            size_t wherePos = toLower(afterSet).find(" where ");
            std::string setPart;
            if (wherePos != std::string::npos) {
                setPart = afterSet.substr(0, wherePos);
            } else {
                setPart = afterSet;
            }

            // 解析 setPart 格式为: <column> = <value>
            size_t eqPos = setPart.find('=');
            if (eqPos != std::string::npos) {
                node->setColumn = trim(setPart.substr(0, eqPos));
                node->setValue = trim(setPart.substr(eqPos + 1));
            }

            // 解析 where 条件
            if (wherePos != std::string::npos) {
                std::string conditionStr = afterSet.substr(wherePos + 7);
                conditionStr = trim(conditionStr);
                parseCondition(conditionStr, node->condition);
            }
        }

        return node;
    }

    // 解析 delete
    std::unique_ptr<ASTNode> parseDelete(const std::string& original, const std::string& lower, std::stringstream& ss) {
        auto node = std::make_unique<DeleteNode>();

        ss >> node->tableName;

        // 解析 where 条件
        size_t wherePos = toLower(original).find(" where ");
        if (wherePos != std::string::npos) {
            std::string conditionStr = original.substr(wherePos + 7);
            conditionStr = trim(conditionStr);
            parseCondition(conditionStr, node->condition);
        }

        return node;
    }

    // 解析条件表达式 <col> <op> <val>
    void parseCondition(const std::string& condStr, Condition& cond) {
        std::string trimmed = trim(condStr);
        if (trimmed.empty()) return;

        // 支持 =, <, > 操作符
        size_t opPos = std::string::npos;
        char opChar = 0;

        for (size_t i = 0; i < trimmed.size(); ++i) {
            if (trimmed[i] == '=' || trimmed[i] == '<' || trimmed[i] == '>') {
                opPos = i;
                opChar = trimmed[i];
                break;
            }
        }

        if (opPos == std::string::npos) return;

        cond.column = trim(trimmed.substr(0, opPos));
        cond.op = std::string(1, opChar);
        cond.value = trim(trimmed.substr(opPos + 1));
        cond.hasCondition = true;
    }

    // 解析 create view <name> as select <cols> from <table> [where <cond>]
    std::unique_ptr<ASTNode> parseCreateView(const std::string& original, const std::string& lower, std::stringstream& ss) {
        std::string viewName;
        ss >> viewName;
        if (viewName.empty()) return nullptr;

        // 读取 "as"
        std::string asKw;
        ss >> asKw;
        if (asKw != "as") return nullptr;

        // 读取 "select"
        std::string selectKw;
        ss >> selectKw;
        if (selectKw != "select") return nullptr;

        auto node = std::make_unique<CreateViewNode>();
        node->viewName = viewName;

        // 解析 select 列（直到 "from"）
        std::string token;
        bool readingColumns = true;
        while (ss >> token) {
            if (token == "from") {
                readingColumns = false;
                continue;
            }
            if (readingColumns) {
                if (!token.empty() && token.back() == ',') {
                    token.pop_back();
                }
                node->selectColumns.push_back(token);
            } else {
                node->sourceTable = token;
                break;
            }
        }

        // 解析 where 条件（使用 original 保留大小写）
        size_t wherePos = toLower(original).find(" where ");
        if (wherePos != std::string::npos) {
            std::string conditionStr = original.substr(wherePos + 7);
            conditionStr = trim(conditionStr);
            parseCondition(conditionStr, node->condition);
        }

        return node;
    }
};

} // namespace minidb

#endif // SQL_PARSER_H
