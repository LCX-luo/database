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

    // 保存表元数据
    ResultSet saveTableSchema(const std::string& dbName, const std::string& tableName,
                               const ArrayList<Column>& columns) {
        std::string schemaFile = schemaFilePath(dbName);
        std::ofstream file(schemaFile, std::ios::app);
        if (!file.is_open()) {
            // 可能文件不存在，尝试创建
            file.open(schemaFile);
            if (!file.is_open()) {
                return ResultSet(ERR_GENERAL, "Failed to save schema");
            }
        }

        // 格式：tablename,colname,coltype,primary;...
        // 追加一行
        file << tableName;
        for (size_t i = 0; i < columns.size(); ++i) {
            file << "," << columns[i].name;
            file << "," << (columns[i].type == DataType::INT ? "int" : "string");
            file << "," << (columns[i].isPrimaryKey ? "1" : "0");
        }
        file << "\n";
        file.close();
        return ResultSet(SUCCESS);
    }

    // 加载表元数据
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

            // 后续是列信息
            while (std::getline(ss, token, ',')) {
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
