#ifndef STORAGE_ENGINE_H
#define STORAGE_ENGINE_H

#include <string>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include "Common.h"
#include "ArrayList.h"
#include "Value.h"
#include "Column.h"
#include "Row.h"
#include "ResultSet.h"
#include "../parser/ASTNode.h"

namespace minidb {

/**
 * @brief 存储引擎
 * 
 * 负责文件级 I/O 管理，包括：
 * - 数据库目录的创建/删除
 * - 表数据文件的读写（二进制定长记录）
 * - 元数据（schema）的读写
 */
class StorageEngine {
private:
    std::string dataDir_;       // 数据根目录
    std::string currentDb_;     // 当前使用的数据库
    ArrayList<std::string> databases_; // 已知的数据库列表

    static constexpr const char* SCHEMA_FILE = "schema.json";

    // 获取数据库路径
    std::string dbPath(const std::string& dbName) const {
        return dataDir_ + "/" + dbName;
    }

    // 获取表数据文件路径
    std::string tableDataPath(const std::string& dbName, const std::string& tableName) const {
        return dbPath(dbName) + "/" + tableName + ".dat";
    }

    // 获取索引文件路径
    std::string indexFilePath(const std::string& dbName, const std::string& tableName) const {
        return dbPath(dbName) + "/" + tableName + ".idx";
    }

    // 获取schema文件路径
    std::string schemaFilePath(const std::string& dbName) const {
        return dbPath(dbName) + "/" + SCHEMA_FILE;
    }

    bool directoryExists(const std::string& path) const {
        struct stat st;
        return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    }

    bool fileExists(const std::string& path) const {
        struct stat st;
        return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
    }

public:
    StorageEngine() : dataDir_("data") {
        // 确保数据目录存在
        if (!directoryExists(dataDir_)) {
            mkdir(dataDir_.c_str(), 0755);
        }
        refreshDatabases();
    }

    explicit StorageEngine(const std::string& dataDir) : dataDir_(dataDir) {
        if (!directoryExists(dataDir_)) {
            mkdir(dataDir_.c_str(), 0755);
        }
        refreshDatabases();
    }

    // 刷新数据库列表
    void refreshDatabases() {
        databases_.clear();
        DIR* dir = opendir(dataDir_.c_str());
        if (!dir) return;

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type == DT_DIR) {
                std::string name = entry->d_name;
                if (name != "." && name != "..") {
                    databases_.push_back(name);
                }
            }
        }
        closedir(dir);
    }

    // ---- 数据库操作 ----

    ResultSet createDatabase(const std::string& dbName) {
        std::string path = dbPath(dbName);
        if (directoryExists(path)) {
            return ResultSet(ERR_EXISTS, "Database '" + dbName + "' already exists");
        }
        if (mkdir(path.c_str(), 0755) != 0) {
            return ResultSet(ERR_GENERAL, "Failed to create database '" + dbName + "'");
        }
        refreshDatabases();
        return ResultSet(SUCCESS, "Database created");
    }

    ResultSet dropDatabase(const std::string& dbName) {
        std::string path = dbPath(dbName);
        if (!directoryExists(path)) {
            return ResultSet(ERR_NOT_FOUND, "Database '" + dbName + "' not found");
        }

        // 删除数据库目录下所有文件
        DIR* dir = opendir(path.c_str());
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                std::string name = entry->d_name;
                if (name != "." && name != "..") {
                    std::string filePath = path + "/" + name;
                    remove(filePath.c_str());
                }
            }
            closedir(dir);
        }
        rmdir(path.c_str());

        if (currentDb_ == dbName) {
            currentDb_.clear();
        }
        refreshDatabases();
        return ResultSet(SUCCESS, "Database dropped");
    }

    ResultSet useDatabase(const std::string& dbName) {
        if (!directoryExists(dbPath(dbName))) {
            return ResultSet(ERR_NOT_FOUND, "Database '" + dbName + "' not found");
        }
        currentDb_ = dbName;
        return ResultSet(SUCCESS, "Database changed");
    }

    std::string getCurrentDb() const { return currentDb_; }

    // ---- 表操作 ----

    // 保存表元数据（支持外键）
    ResultSet saveTableSchema(const std::string& dbName, const std::string& tableName,
                               const ArrayList<Column>& columns,
                               const ArrayList<ForeignKeyDef>& foreignKeys = ArrayList<ForeignKeyDef>()) {
        std::string schemaFile = schemaFilePath(dbName);
        std::ofstream file(schemaFile, std::ios::app);
        if (!file.is_open()) {
            // 可能文件不存在，尝试创建
            file.open(schemaFile);
            if (!file.is_open()) {
                return ResultSet(ERR_GENERAL, "Failed to save schema");
            }
        }

        // 格式：tablename,colname,coltype,primary,...[,FK:fkcol,reftable,refcol,cascade]...
        // 追加一行
        file << tableName;
        for (size_t i = 0; i < columns.size(); ++i) {
            file << "," << columns[i].name;
            file << "," << (columns[i].type == DataType::INT ? "int" : "string");
            file << "," << (columns[i].isPrimaryKey ? "1" : "0");
        }
        // 追加外键定义
        for (size_t i = 0; i < foreignKeys.size(); ++i) {
            file << ",FK:" << foreignKeys[i].column;
            file << "," << foreignKeys[i].refTable;
            file << "," << foreignKeys[i].refColumn;
            file << "," << (foreignKeys[i].onDeleteCascade ? "1" : "0");
        }
        file << "\n";
        file.close();
        return ResultSet(SUCCESS);
    }

    // 加载表元数据（遇到 FK: 标记自动停止）
    ArrayList<Column> loadTableSchema(const std::string& dbName, const std::string& tableName) {
        ArrayList<Column> columns;
        std::string schemaFile = schemaFilePath(dbName);
        std::ifstream file(schemaFile);
        if (!file.is_open()) return columns;

        std::string line;
        bool found = false;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            std::stringstream ss(line);
            std::string token;
            // 第一个token是表名
            std::getline(ss, token, ',');
            if (token != tableName) continue;
            found = true;

            // 后续是列信息，遇到 FK: 标记停止
            while (std::getline(ss, token, ',')) {
                // 遇到外键标记，停止读取列
                if (token.size() >= 3 && token.substr(0, 3) == "FK:") break;
                Column col;
                col.name = token;
                if (!std::getline(ss, token, ',')) break;
                col.type = (token == "int") ? DataType::INT : DataType::STRING;
                if (!std::getline(ss, token, ',')) break;
                col.isPrimaryKey = (token == "1");
                columns.push_back(col);
            }
            break;
        }
        file.close();
        return columns;
    }

    // 加载表的外键定义
    ArrayList<ForeignKeyDef> loadTableForeignKeys(const std::string& dbName, const std::string& tableName) {
        ArrayList<ForeignKeyDef> fks;
        std::string schemaFile = schemaFilePath(dbName);
        std::ifstream file(schemaFile);
        if (!file.is_open()) return fks;

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            std::stringstream ss(line);
            std::string token;
            std::getline(ss, token, ',');
            if (token != tableName) continue;

            // 跳过列定义直到 FK: 标记
            while (std::getline(ss, token, ',')) {
                if (token.size() >= 3 && token.substr(0, 3) == "FK:") {
                    ForeignKeyDef fk;
                    fk.column = token.substr(3);  // 去掉 "FK:" 前缀
                    if (!std::getline(ss, token, ',')) break;
                    fk.refTable = token;
                    if (!std::getline(ss, token, ',')) break;
                    fk.refColumn = token;
                    if (!std::getline(ss, token, ',')) break;
                    fk.onDeleteCascade = (token == "1");
                    fks.push_back(fk);
                }
            }
            break;
        }
        file.close();
        return fks;
    }

    // 获取所有引用了指定表的表名（用于外键约束检查）
    ArrayList<std::string> getReferencingTables(const std::string& dbName, const std::string& tableName) {
        ArrayList<std::string> refTables;
        std::string schemaFile = schemaFilePath(dbName);
        std::ifstream file(schemaFile);
        if (!file.is_open()) return refTables;

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line.substr(0, 5) == "VIEW:") continue;
            std::stringstream ss(line);
            std::string token;
            std::getline(ss, token, ',');
            std::string thisTable = token;

            // 查找 FK: 标记
            while (std::getline(ss, token, ',')) {
                if (token.size() >= 3 && token.substr(0, 3) == "FK:") {
                    // 读取 refTable
                    if (!std::getline(ss, token, ',')) break;
                    if (token == tableName) {
                        refTables.push_back(thisTable);
                        break;
                    }
                    // 跳过 refColumn 和 cascade
                    if (!std::getline(ss, token, ',')) break;
                    if (!std::getline(ss, token, ',')) break;
                }
            }
        }
        file.close();
        return refTables;
    }

    // 删除表元数据
    ResultSet removeTableSchema(const std::string& dbName, const std::string& tableName) {
        std::string schemaFile = schemaFilePath(dbName);
        std::ifstream file(schemaFile);
        if (!file.is_open()) return ResultSet(SUCCESS);

        std::string content;
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            std::stringstream ss(line);
            std::string token;
            std::getline(ss, token, ',');
            if (token != tableName) {
                content += line + "\n";
            }
        }
        file.close();

        std::ofstream outFile(schemaFile, std::ios::trunc);
        outFile << content;
        outFile.close();
        return ResultSet(SUCCESS);
    }

    // 检查表是否存在
    bool tableExists(const std::string& dbName, const std::string& tableName) {
        return fileExists(tableDataPath(dbName, tableName));
    }

    // 获取表数据文件路径
    std::string getTableDataPath(const std::string& dbName, const std::string& tableName) {
        return tableDataPath(dbName, tableName);
    }

    // 获取索引文件路径
    std::string getIndexFilePath(const std::string& dbName, const std::string& tableName) {
        return indexFilePath(dbName, tableName);
    }

    // 获取所有数据库
    ArrayList<std::string> getDatabases() const {
        return databases_;
    }

    // ---- 视图操作 ----

    // 视图定义在 schema.json 中以 VIEW: 前缀存储
    // 格式: VIEW:viewname,col1,col2,...,sourcetable,condcol,condop,condval
    // 例如: VIEW:good_student,id,name,age,student,age,=,20

    // 保存视图定义
    ResultSet saveViewDefinition(const std::string& dbName, const std::string& viewName,
                                  const ArrayList<std::string>& selectColumns,
                                  const std::string& sourceTable,
                                  const Condition& condition) {
        std::string schemaFile = schemaFilePath(dbName);
        std::ofstream file(schemaFile, std::ios::app);
        if (!file.is_open()) {
            file.open(schemaFile);
            if (!file.is_open()) {
                return ResultSet(ERR_GENERAL, "Failed to save view definition");
            }
        }

        file << "VIEW:" << viewName;
        for (size_t i = 0; i < selectColumns.size(); ++i) {
            file << "," << selectColumns[i];
        }
        file << "," << sourceTable;
        // 存储条件信息
        if (condition.hasCondition) {
            file << "," << condition.column;
            file << "," << condition.op;
            file << "," << condition.value;
        }
        file << "\n";
        file.close();
        return ResultSet(SUCCESS, "View created");
    }

    // 加载视图定义（返回完整行，由调用方解析）
    // 返回格式：{viewName, [columns...], sourceTable, condColumn, condOp, condValue}
    struct ViewDefinition {
        std::string viewName;
        ArrayList<std::string> selectColumns;
        std::string sourceTable;
        Condition condition;
    };

    ViewDefinition loadViewDefinition(const std::string& dbName, const std::string& viewName) {
        ViewDefinition vd;
        std::string schemaFile = schemaFilePath(dbName);
        std::ifstream file(schemaFile);
        if (!file.is_open()) return vd;

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            // 检查是否是 VIEW: 开头
            if (line.substr(0, 5) != "VIEW:") continue;
            std::string content = line.substr(5); // 去掉 "VIEW:"
            std::stringstream ss(content);
            std::string token;

            // 第一个token是视图名
            std::getline(ss, token, ',');
            if (token != viewName) continue;

            vd.viewName = viewName;

            // 读取所有列直到最后一个分隔区为 sourcetable
            // 格式: viewname,col1,col2,...,sourcetable[,condcol,condop,condval]
            // 策略：先读所有token，sourceTable 是倒数第4个（无条件时就是最后一个）
            ArrayList<std::string> allTokens;
            while (std::getline(ss, token, ',')) {
                allTokens.push_back(token);
            }

            if (allTokens.size() == 0) break;

            // 检查是否有条件（至少 sourceTable + 3 个条件字段）
            // 无条件: [col1, col2, ..., sourceTable]
            // 有条件: [col1, col2, ..., sourceTable, condCol, condOp, condVal]
            bool hasCondition = false;
            size_t sourceTableIdx = allTokens.size() - 1;

            // 尝试判断：如果最后三个token看起来像条件
            if (allTokens.size() >= 4) {
                std::string lastToken = allTokens[allTokens.size() - 1];
                // 如果最后一个token是数字或带引号字符串，可能是条件值
                if (!lastToken.empty() && (isdigit(lastToken[0]) || lastToken[0] == '"')) {
                    hasCondition = true;
                    sourceTableIdx = allTokens.size() - 4;
                }
            }

            vd.sourceTable = allTokens[sourceTableIdx];

            // 收集列
            for (size_t i = 0; i < sourceTableIdx; ++i) {
                vd.selectColumns.push_back(allTokens[i]);
            }

            // 解析条件
            if (hasCondition && sourceTableIdx + 3 <= allTokens.size()) {
                vd.condition.column = allTokens[sourceTableIdx + 1];
                vd.condition.op = allTokens[sourceTableIdx + 2];
                vd.condition.value = allTokens[sourceTableIdx + 3];
                vd.condition.hasCondition = true;
            }

            break;
        }
        file.close();
        return vd;
    }

    // 检查是否是视图
    bool viewExists(const std::string& dbName, const std::string& viewName) {
        std::string schemaFile = schemaFilePath(dbName);
        std::ifstream file(schemaFile);
        if (!file.is_open()) return false;

        std::string line;
        while (std::getline(file, line)) {
            if (line.substr(0, 5) != "VIEW:") continue;
            std::string content = line.substr(5);
            std::stringstream ss(content);
            std::string token;
            std::getline(ss, token, ',');
            if (token == viewName) {
                file.close();
                return true;
            }
        }
        file.close();
        return false;
    }

    // 删除视图定义
    ResultSet removeViewDefinition(const std::string& dbName, const std::string& viewName) {
        std::string schemaFile = schemaFilePath(dbName);
        std::ifstream file(schemaFile);
        if (!file.is_open()) return ResultSet(SUCCESS);

        std::string content;
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            // 只过滤掉匹配的 VIEW 行
            if (line.substr(0, 5) == "VIEW:") {
                std::string rest = line.substr(5);
                std::stringstream ss(rest);
                std::string token;
                std::getline(ss, token, ',');
                if (token == viewName) continue; // 跳过此行
            }
            content += line + "\n";
        }
        file.close();

        std::ofstream outFile(schemaFile, std::ios::trunc);
        outFile << content;
        outFile.close();
        return ResultSet(SUCCESS, "View dropped");
    }

    // 获取数据库中所有表名
    ArrayList<std::string> getTables(const std::string& dbName) {
        ArrayList<std::string> tables;
        std::string schemaFile = schemaFilePath(dbName);
        std::ifstream file(schemaFile);
        if (!file.is_open()) return tables;

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            std::stringstream ss(line);
            std::string token;
            std::getline(ss, token, ',');
            if (!token.empty()) {
                tables.push_back(token);
            }
        }
        file.close();
        return tables;
    }
};

} // namespace minidb

#endif // STORAGE_ENGINE_H
