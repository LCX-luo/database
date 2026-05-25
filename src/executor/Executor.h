#ifndef EXECUTOR_H
#define EXECUTOR_H

#include <string>
#include <memory>
#include "Common.h"
#include "ASTNode.h"
#include "ResultSet.h"
#include "../storage/StorageEngine.h"
#include "../storage/Table.h"

namespace minidb {

/**
 * @brief 执行引擎
 * 
 * 接收 AST，调用存储引擎执行操作，返回结果集。
 * 负责将解析后的SQL语句转换为实际的数据库操作。
 */
class Executor {
private:
    StorageEngine& storage_;

    // 从 ColumnDef 转换为 Column
    Column convertColumn(const ColumnDef& def) {
        DataType t = (def.type == "int") ? DataType::INT : DataType::STRING;
        return Column(def.name, t, def.isPrimaryKey);
    }

    // 根据条件字符串获取操作符和值
    Operator getOperator(const std::string& opStr) {
        if (opStr == "=") return Operator::EQ;
        if (opStr == "<") return Operator::LT;
        if (opStr == ">") return Operator::GT;
        return Operator::NONE;
    }

    // 将字符串值解析为对应类型的 Value
    Value parseValue(const std::string& str, DataType type) {
        std::string trimmed = trim(str);
        if (type == DataType::INT) {
            return Value(std::stoi(trimmed));
        } else {
            // 去除可能的引号
            if (trimmed.size() >= 2 && trimmed[0] == '"' && trimmed.back() == '"') {
                trimmed = trimmed.substr(1, trimmed.size() - 2);
            }
            return Value(trimmed);
        }
    }

    // 自动推断值类型
    Value inferValue(const std::string& str) {
        std::string trimmed = trim(str);
        if (trimmed.empty()) return Value(0);

        // 如果被引号包围，是字符串
        if (trimmed.front() == '"' && trimmed.back() == '"') {
            return Value(trimmed.substr(1, trimmed.size() - 2));
        }

        // 如果是纯数字，是整数
        bool isInt = true;
        for (char c : trimmed) {
            if (c == '-' || c == '+') continue;
            if (c < '0' || c > '9') { isInt = false; break; }
        }
        if (isInt) return Value(std::stoi(trimmed));

        // 否则当作字符串
        return Value(trimmed);
    }

public:
    explicit Executor(StorageEngine& storage) : storage_(storage) {}

    ResultSet execute(const ASTNode& node) {
        switch (node.type) {
            case StatementType::CREATE_DATABASE:
                return executeCreateDatabase(static_cast<const CreateDatabaseNode&>(node));
            case StatementType::DROP_DATABASE:
                return executeDropDatabase(static_cast<const DropDatabaseNode&>(node));
            case StatementType::USE_DATABASE:
                return executeUseDatabase(static_cast<const UseDatabaseNode&>(node));
            case StatementType::CREATE_TABLE:
                return executeCreateTable(static_cast<const CreateTableNode&>(node));
            case StatementType::DROP_TABLE:
                return executeDropTable(static_cast<const DropTableNode&>(node));
            case StatementType::CREATE_VIEW:
                return executeCreateView(static_cast<const CreateViewNode&>(node));
            case StatementType::DROP_VIEW:
                return executeDropView(static_cast<const DropViewNode&>(node));
            case StatementType::INSERT:
                return executeInsert(static_cast<const InsertNode&>(node));
            case StatementType::SELECT:
                return executeSelect(static_cast<const SelectNode&>(node));
            case StatementType::UPDATE:
                return executeUpdate(static_cast<const UpdateNode&>(node));
            case StatementType::DELETE:
                return executeDelete(static_cast<const DeleteNode&>(node));
            default:
                return ResultSet(ERR_NOT_IMPLEMENTED, "Unsupported statement");
        }
    }

private:
    ResultSet executeCreateDatabase(const CreateDatabaseNode& node) {
        return storage_.createDatabase(node.dbName);
    }

    ResultSet executeDropDatabase(const DropDatabaseNode& node) {
        return storage_.dropDatabase(node.dbName);
    }

    ResultSet executeUseDatabase(const UseDatabaseNode& node) {
        return storage_.useDatabase(node.dbName);
    }

    ResultSet executeCreateTable(const CreateTableNode& node) {
        std::string dbName = storage_.getCurrentDb();
        if (dbName.empty()) {
            return ResultSet(ERR_GENERAL, "No database selected");
        }

        // 检查表是否已存在
        if (storage_.tableExists(dbName, node.tableName)) {
            return ResultSet(ERR_EXISTS, "Table '" + node.tableName + "' already exists");
        }

        // 转换列定义
        ArrayList<Column> columns;
        for (size_t i = 0; i < node.columns.size(); ++i) {
            columns.push_back(convertColumn(node.columns[i]));
        }

        // 创建表文件
        std::string dataPath = storage_.getTableDataPath(dbName, node.tableName);
        std::string idxPath = storage_.getIndexFilePath(dbName, node.tableName);

        Table table(dbName, node.tableName, columns, dataPath, idxPath);
        ResultSet rs = table.create();
        if (rs.code != SUCCESS) return rs;

        // 保存元数据
        rs = storage_.saveTableSchema(dbName, node.tableName, columns);
        return rs;
    }

    ResultSet executeDropTable(const DropTableNode& node) {
        std::string dbName = storage_.getCurrentDb();
        if (dbName.empty()) {
            return ResultSet(ERR_GENERAL, "No database selected");
        }

        if (!storage_.tableExists(dbName, node.tableName)) {
            return ResultSet(ERR_NOT_FOUND, "Table '" + node.tableName + "' not found");
        }

        // 加载表信息以获取索引
        ArrayList<Column> columns = storage_.loadTableSchema(dbName, node.tableName);
        std::string dataPath = storage_.getTableDataPath(dbName, node.tableName);
        std::string idxPath = storage_.getIndexFilePath(dbName, node.tableName);

        // 删除表文件和索引文件
        Table table(dbName, node.tableName, columns, dataPath, idxPath);
        table.drop();

        // 删除元数据
        storage_.removeTableSchema(dbName, node.tableName);

        return ResultSet(SUCCESS, "Table dropped");
    }

    ResultSet executeInsert(const InsertNode& node) {
        std::string dbName = storage_.getCurrentDb();
        if (dbName.empty()) {
            return ResultSet(ERR_GENERAL, "No database selected");
        }

        if (!storage_.tableExists(dbName, node.tableName)) {
            return ResultSet(ERR_NOT_FOUND, "Table '" + node.tableName + "' not found");
        }

        // 加载表结构
        ArrayList<Column> columns = storage_.loadTableSchema(dbName, node.tableName);
        std::string dataPath = storage_.getTableDataPath(dbName, node.tableName);
        std::string idxPath = storage_.getIndexFilePath(dbName, node.tableName);

        Table table(dbName, node.tableName, columns, dataPath, idxPath);

        // 构造行数据
        Row row;
        for (size_t i = 0; i < node.values.size(); ++i) {
            DataType expectedType = (i < columns.size()) ? columns[i].type : DataType::INT;
            row.addValue(parseValue(node.values[i], expectedType));
        }

        return table.insert(row);
    }

    ResultSet executeSelect(const SelectNode& node) {
        std::string dbName = storage_.getCurrentDb();
        if (dbName.empty()) {
            return ResultSet(ERR_GENERAL, "No database selected");
        }

        // 检查是否是视图查询
        if (storage_.viewExists(dbName, node.tableName)) {
            return executeSelectFromView(dbName, node);
        }

        if (!storage_.tableExists(dbName, node.tableName)) {
            return ResultSet(ERR_NOT_FOUND, "Table '" + node.tableName + "' not found");
        }

        ArrayList<Column> columns = storage_.loadTableSchema(dbName, node.tableName);
        std::string dataPath = storage_.getTableDataPath(dbName, node.tableName);
        std::string idxPath = storage_.getIndexFilePath(dbName, node.tableName);

        Table table(dbName, node.tableName, columns, dataPath, idxPath);

        Operator op = Operator::NONE;
        Value whereVal;
        if (node.condition.hasCondition) {
            op = getOperator(node.condition.op);

            // 根据列类型解析值
            int colIdx = -1;
            for (size_t i = 0; i < columns.size(); ++i) {
                if (columns[i].name == node.condition.column) {
                    colIdx = static_cast<int>(i);
                    break;
                }
            }
            if (colIdx >= 0) {
                whereVal = parseValue(node.condition.value, columns[colIdx].type);
            } else {
                whereVal = inferValue(node.condition.value);
            }
        }

        return table.select(node.columns, node.condition.column, op, whereVal);
    }

    // 创建视图
    ResultSet executeCreateView(const CreateViewNode& node) {
        std::string dbName = storage_.getCurrentDb();
        if (dbName.empty()) {
            return ResultSet(ERR_GENERAL, "No database selected");
        }

        // 检查视图名是否已存在
        if (storage_.viewExists(dbName, node.viewName)) {
            return ResultSet(ERR_EXISTS, "View '" + node.viewName + "' already exists");
        }

        // 检查源表是否存在
        if (!storage_.tableExists(dbName, node.sourceTable)) {
            return ResultSet(ERR_NOT_FOUND, "Table '" + node.sourceTable + "' not found");
        }

        // 保存视图定义
        return storage_.saveViewDefinition(dbName, node.viewName,
                                           node.selectColumns,
                                           node.sourceTable,
                                           node.condition);
    }

    // 删除视图
    ResultSet executeDropView(const DropViewNode& node) {
        std::string dbName = storage_.getCurrentDb();
        if (dbName.empty()) {
            return ResultSet(ERR_GENERAL, "No database selected");
        }

        if (!storage_.viewExists(dbName, node.viewName)) {
            return ResultSet(ERR_NOT_FOUND, "View '" + node.viewName + "' not found");
        }

        return storage_.removeViewDefinition(dbName, node.viewName);
    }

    // 从视图查询（将视图展开为底层表查询）
    ResultSet executeSelectFromView(const std::string& dbName, const SelectNode& node) {
        // 加载视图定义
        StorageEngine::ViewDefinition vd = storage_.loadViewDefinition(dbName, node.tableName);
        if (vd.viewName.empty()) {
            return ResultSet(ERR_NOT_FOUND, "View '" + node.tableName + "' not found");
        }

        // 检查源表是否存在
        if (!storage_.tableExists(dbName, vd.sourceTable)) {
            return ResultSet(ERR_NOT_FOUND, "Source table '" + vd.sourceTable + "' not found");
        }

        // 加载源表结构
        ArrayList<Column> columns = storage_.loadTableSchema(dbName, vd.sourceTable);
        std::string dataPath = storage_.getTableDataPath(dbName, vd.sourceTable);
        std::string idxPath = storage_.getIndexFilePath(dbName, vd.sourceTable);

        Table table(dbName, vd.sourceTable, columns, dataPath, idxPath);

        // 确定要查询的列
        // 如果用户查询 *，使用视图定义的列
        // 否则使用用户指定的列（必须属于视图定义的列集）
        ArrayList<std::string> queryColumns;
        bool isStar = (node.columns.size() == 1 && node.columns[0] == "*");
        if (isStar) {
            queryColumns = vd.selectColumns;
        } else {
            queryColumns = node.columns;
        }

        // 组合 WHERE 条件：视图定义的条件 + 用户查询的条件
        Operator op = Operator::NONE;
        Value whereVal;
        std::string whereColumn;

        // 先使用视图定义的条件
        if (vd.condition.hasCondition) {
            whereColumn = vd.condition.column;
            op = getOperator(vd.condition.op);
            int colIdx = -1;
            for (size_t i = 0; i < columns.size(); ++i) {
                if (columns[i].name == vd.condition.column) {
                    colIdx = static_cast<int>(i);
                    break;
                }
            }
            if (colIdx >= 0) {
                whereVal = parseValue(vd.condition.value, columns[colIdx].type);
            } else {
                whereVal = inferValue(vd.condition.value);
            }
        }

        // 如果用户查询也有条件，覆盖或组合（此处简单覆盖，实际应 AND 合并）
        if (node.condition.hasCondition) {
            whereColumn = node.condition.column;
            op = getOperator(node.condition.op);
            int colIdx = -1;
            for (size_t i = 0; i < columns.size(); ++i) {
                if (columns[i].name == node.condition.column) {
                    colIdx = static_cast<int>(i);
                    break;
                }
            }
            if (colIdx >= 0) {
                whereVal = parseValue(node.condition.value, columns[colIdx].type);
            } else {
                whereVal = inferValue(node.condition.value);
            }
        }

        return table.select(queryColumns, whereColumn, op, whereVal);
    }

    ResultSet executeUpdate(const UpdateNode& node) {
        std::string dbName = storage_.getCurrentDb();
        if (dbName.empty()) {
            return ResultSet(ERR_GENERAL, "No database selected");
        }

        if (!storage_.tableExists(dbName, node.tableName)) {
            return ResultSet(ERR_NOT_FOUND, "Table '" + node.tableName + "' not found");
        }

        ArrayList<Column> columns = storage_.loadTableSchema(dbName, node.tableName);
        std::string dataPath = storage_.getTableDataPath(dbName, node.tableName);
        std::string idxPath = storage_.getIndexFilePath(dbName, node.tableName);

        Table table(dbName, node.tableName, columns, dataPath, idxPath);

        Operator op = Operator::NONE;
        Value whereVal;
        if (node.condition.hasCondition) {
            op = getOperator(node.condition.op);
            int colIdx = -1;
            for (size_t i = 0; i < columns.size(); ++i) {
                if (columns[i].name == node.condition.column) {
                    colIdx = static_cast<int>(i);
                    break;
                }
            }
            if (colIdx >= 0) {
                whereVal = parseValue(node.condition.value, columns[colIdx].type);
            } else {
                whereVal = inferValue(node.condition.value);
            }
        }

        // 确定设置列的类型
        Value setVal = inferValue(node.setValue);
        // 尝试根据列类型调整
        int setColIdx = -1;
        for (size_t i = 0; i < columns.size(); ++i) {
            if (columns[i].name == node.setColumn) {
                setColIdx = static_cast<int>(i);
                break;
            }
        }
        if (setColIdx >= 0) {
            setVal = parseValue(node.setValue, columns[setColIdx].type);
        }

        return table.update(node.setColumn, setVal, node.condition.column, op, whereVal);
    }

    ResultSet executeDelete(const DeleteNode& node) {
        std::string dbName = storage_.getCurrentDb();
        if (dbName.empty()) {
            return ResultSet(ERR_GENERAL, "No database selected");
        }

        if (!storage_.tableExists(dbName, node.tableName)) {
            return ResultSet(ERR_NOT_FOUND, "Table '" + node.tableName + "' not found");
        }

        ArrayList<Column> columns = storage_.loadTableSchema(dbName, node.tableName);
        std::string dataPath = storage_.getTableDataPath(dbName, node.tableName);
        std::string idxPath = storage_.getIndexFilePath(dbName, node.tableName);

        Table table(dbName, node.tableName, columns, dataPath, idxPath);

        Operator op = Operator::NONE;
        Value whereVal;
        if (node.condition.hasCondition) {
            op = getOperator(node.condition.op);
            int colIdx = -1;
            for (size_t i = 0; i < columns.size(); ++i) {
                if (columns[i].name == node.condition.column) {
                    colIdx = static_cast<int>(i);
                    break;
                }
            }
            if (colIdx >= 0) {
                whereVal = parseValue(node.condition.value, columns[colIdx].type);
            } else {
                whereVal = inferValue(node.condition.value);
            }
        }

        return table.remove(node.condition.column, op, whereVal);
    }
};

} // namespace minidb

#endif // EXECUTOR_H
