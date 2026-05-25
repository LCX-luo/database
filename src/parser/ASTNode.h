#ifndef AST_NODE_H
#define AST_NODE_H

#include <string>
#include "Common.h"
#include "ArrayList.h"

namespace minidb {

/**
 * @brief 抽象语法树节点基类
 * 
 * SQL解析结果以AST表示，每个语句类型对应一个派生类。
 */

// 条件表达式
struct Condition {
    std::string column;     // 列名
    std::string op;         // 操作符：=, <, >
    std::string value;      // 常量值（字符串形式）
    bool hasCondition;      // 是否有条件

    Condition() : hasCondition(false) {}
};

// AST节点基类
struct ASTNode {
    StatementType type;

    explicit ASTNode(StatementType t) : type(t) {}
    virtual ~ASTNode() = default;
};

// CREATE DATABASE
struct CreateDatabaseNode : ASTNode {
    std::string dbName;
    CreateDatabaseNode() : ASTNode(StatementType::CREATE_DATABASE) {}
};

// DROP DATABASE
struct DropDatabaseNode : ASTNode {
    std::string dbName;
    DropDatabaseNode() : ASTNode(StatementType::DROP_DATABASE) {}
};

// USE DATABASE
struct UseDatabaseNode : ASTNode {
    std::string dbName;
    UseDatabaseNode() : ASTNode(StatementType::USE_DATABASE) {}
};

// 列定义
struct ColumnDef {
    std::string name;
    std::string type;   // "int" or "string"
    bool isPrimaryKey;

    ColumnDef() : isPrimaryKey(false) {}
};

// CREATE TABLE
struct CreateTableNode : ASTNode {
    std::string tableName;
    ArrayList<ColumnDef> columns;
    CreateTableNode() : ASTNode(StatementType::CREATE_TABLE) {}
};

// DROP TABLE
struct DropTableNode : ASTNode {
    std::string tableName;
    DropTableNode() : ASTNode(StatementType::DROP_TABLE) {}
};

// INSERT
struct InsertNode : ASTNode {
    std::string tableName;
    ArrayList<std::string> values;  // 字符串形式的常量值
    InsertNode() : ASTNode(StatementType::INSERT) {}
};

// SELECT
struct SelectNode : ASTNode {
    ArrayList<std::string> columns;  // 列名列表，或 ["*"]
    std::string tableName;
    Condition condition;
    SelectNode() : ASTNode(StatementType::SELECT) {}
};

// UPDATE
struct UpdateNode : ASTNode {
    std::string tableName;
    std::string setColumn;
    std::string setValue;
    Condition condition;
    UpdateNode() : ASTNode(StatementType::UPDATE) {}
};

// DELETE
struct DeleteNode : ASTNode {
    std::string tableName;
    Condition condition;
    DeleteNode() : ASTNode(StatementType::DELETE) {}
};

// EXIT
struct ExitNode : ASTNode {
    ExitNode() : ASTNode(StatementType::EXIT) {}
};

} // namespace minidb

#endif // AST_NODE_H
