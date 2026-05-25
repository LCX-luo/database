#ifndef TABLE_H
#define TABLE_H

#include <string>
#include <fstream>
#include <cstring>
#include <cstdio>
#include <sys/stat.h>
#include "Common.h"
#include "ArrayList.h"
#include "Value.h"
#include "Column.h"
#include "Row.h"
#include "ResultSet.h"
#include "../index/BPlusTree.h"

namespace minidb {

/**
 * @brief 表管理
 * 
 * 负责表级操作：create/drop table, insert/select/update/delete 数据。
 * 支持 B+树索引的主键查询优化。
 */
class Table {
private:
    std::string dbName_;
    std::string tableName_;
    std::string dataFilePath_;
    std::string indexFilePath_;
    ArrayList<Column> columns_;
    BPlusTree index_;
    bool hasIndex_;     // 是否有主键索引

    // 计算一行数据的序列化大小
    size_t rowSize() const {
        size_t sz = 0;
        for (size_t i = 0; i < columns_.size(); ++i) {
            sz += sizeof(DataType);  // type 标记
            if (columns_[i].type == DataType::INT) {
                sz += sizeof(int);
            } else {
                sz += MAX_STRING_LEN + 1;
            }
        }
        return sz;
    }

    // 序列化一行到文件
    void writeRow(std::fstream& file, int64_t offset, const Row& row) {
        file.seekp(offset);
        for (size_t i = 0; i < row.size(); ++i) {
            row[i].serialize(file);
        }
    }

    // 从文件读取一行
    Row readRow(std::fstream& file, int64_t offset) {
        Row row;
        file.seekg(offset);
        for (size_t i = 0; i < columns_.size(); ++i) {
            Value val;
            val.deserialize(file);
            row.addValue(val);
        }
        return row;
    }

    // 获取所有满足条件的行和偏移
    void scanRows(ArrayList<int64_t>& offsets, ArrayList<Row>& rows,
                  const std::string& whereCol, Operator op, const Value& whereVal) {
        fprintf(stderr, "[DBG-SCAN] opening path=%s\n", dataFilePath_.c_str());
        std::fstream file(dataFilePath_, std::ios::in | std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            fprintf(stderr, "[DBG-SCAN] FAILED to open!\n");
            return;
        }

        file.seekg(0, std::ios::end);
        int64_t fileSize = file.tellg();
        fprintf(stderr, "[DBG-SCAN] fileSize from tellg=%ld\n", (long)fileSize);

        // 额外用 stat 检查
        struct stat st;
        if (stat(dataFilePath_.c_str(), &st) == 0) {
            fprintf(stderr, "[DBG-SCAN] fileSize from stat=%ld\n", (long)st.st_size);
        }

        if (fileSize <= 0) {
            file.close();
            fprintf(stderr, "[DBG-SCAN] file empty, returning\n");
            return;
        }

        size_t rSize = rowSize();
        fprintf(stderr, "[DBG-SCAN] rSize=%zu\n", rSize);
        if (rSize == 0) { file.close(); return; }

        int64_t numRows = fileSize / rSize;
        fprintf(stderr, "[DBG-SCAN] numRows=%ld\n", (long)numRows);

        // 如果是主键等值查询且有索引，使用索引优化
        int pkColIdx = -1;
        for (size_t i = 0; i < columns_.size(); ++i) {
            if (columns_[i].isPrimaryKey) {
                pkColIdx = static_cast<int>(i);
                break;
            }
        }

        if (hasIndex_ && op == Operator::EQ && pkColIdx >= 0 && 
            columns_[pkColIdx].name == whereCol && whereVal.type == DataType::INT) {
            int64_t offset = index_.search(whereVal.intValue);
            if (offset >= 0) {
                offsets.push_back(offset);
                rows.push_back(readRow(file, offset));
            }
            file.close();
            return;
        }

        // 否则全表扫描
        for (int64_t i = 0; i < numRows; ++i) {
            int64_t offset = i * rSize;
            fprintf(stderr, "[DBG-ROW] reading row %ld at offset %ld\n", (long)i, (long)offset);
            
            // debug: 读之前检查文件状态
            long tellg_before = file.tellg();
            
            Row row = readRow(file, offset);
            
            long tellg_after = file.tellg();
            fprintf(stderr, "[DBG-ROW] after readRow: tellg_before=%ld tellg_after=%ld failbit=%d badbit=%d\n",
                    tellg_before, tellg_after, file.fail() ? 1 : 0, file.bad() ? 1 : 0);
            
            bool match = true;

            if (!whereCol.empty()) {
                // 查找列索引
                int colIdx = -1;
                for (size_t j = 0; j < columns_.size(); ++j) {
                    if (columns_[j].name == whereCol) {
                        colIdx = static_cast<int>(j);
                        break;
                    }
                }
                if (colIdx >= 0) {
                    const Value& val = row[colIdx];
                    switch (op) {
                        case Operator::EQ: match = (val == whereVal); break;
                        case Operator::LT: match = (val < whereVal); break;
                        case Operator::GT: match = (val > whereVal); break;
                        default: match = true; break;
                    }
                }
            }

            if (match) {
                fprintf(stderr, "[DBG-ROW] row %ld MATCHED, offset=%ld\n", (long)i, (long)offset);
                offsets.push_back(offset);
                rows.push_back(row);
            } else {
                fprintf(stderr, "[DBG-ROW] row %ld FILTERED OUT\n", (long)i);
            }
        }

        file.close();
    }

public:
    Table() : hasIndex_(false) {}

    Table(const std::string& dbName, const std::string& tableName,
          const ArrayList<Column>& columns, const std::string& dataFilePath,
          const std::string& indexFilePath)
        : dbName_(dbName), tableName_(tableName),
          dataFilePath_(dataFilePath), indexFilePath_(indexFilePath),
          columns_(columns), hasIndex_(false) {
        
        // 检查是否有主键
        for (size_t i = 0; i < columns_.size(); ++i) {
            if (columns_[i].isPrimaryKey) {
                hasIndex_ = true;
                index_.open(indexFilePath_);
                break;
            }
        }
    }

    ArrayList<Column>& getColumns() { return columns_; }
    const ArrayList<Column>& getColumns() const { return columns_; }
    std::string getTableName() const { return tableName_; }
    bool hasIndex() const { return hasIndex_; }

    // 创建表（初始化数据文件和索引文件）
    ResultSet create() {
        // 创建数据文件
        std::ofstream dataFile(dataFilePath_, std::ios::binary | std::ios::trunc);
        if (!dataFile.is_open()) {
            return ResultSet(ERR_GENERAL, "Failed to create table file");
        }
        dataFile.close();

        // 如果有主键，创建索引文件
        if (hasIndex_) {
            if (!index_.create(indexFilePath_)) {
                return ResultSet(ERR_GENERAL, "Failed to create index file");
            }
        }

        return ResultSet(SUCCESS, "Table created");
    }

    // 插入数据
    ResultSet insert(const Row& row) {
        if (row.size() != columns_.size()) {
            return ResultSet(ERR_GENERAL, "Column count mismatch");
        }

        // 类型检查
        for (size_t i = 0; i < row.size(); ++i) {
            if (row[i].type != columns_[i].type) {
                return ResultSet(ERR_TYPE, "Type mismatch for column '" + columns_[i].name + "'");
            }
        }

        // 调试：检查文件路径和当前文件大小
        fprintf(stderr, "[DBG-INSERT] path=%s\n", dataFilePath_.c_str());
        int64_t offset = 0;
        struct stat st_before;
        if (stat(dataFilePath_.c_str(), &st_before) == 0) {
            offset = st_before.st_size;
        }
        fprintf(stderr, "[DBG-INSERT] stat_before=%ld\n", (long)offset);

        // 使用 C 风格 FILE I/O 追加模式
        FILE* file = fopen(dataFilePath_.c_str(), "ab");
        if (!file) {
            fprintf(stderr, "[DBG-INSERT] fopen FAILED!\n");
            return ResultSet(ERR_GENERAL, "Failed to open table file");
        }
        fprintf(stderr, "[DBG-INSERT] fopen OK, fileno=%d\n", fileno(file));

        // 获取文件描述符的当前位置
        long pos_before = ftell(file);
        fprintf(stderr, "[DBG-INSERT] ftell_before=%ld\n", pos_before);

        // 逐字段序列化写入
        size_t totalWritten = 0;
        for (size_t i = 0; i < row.size(); ++i) {
            DataType t = row[i].type;
            size_t w1 = fwrite(&t, sizeof(DataType), 1, file);
            totalWritten += w1 * sizeof(DataType);
            if (t == DataType::INT) {
                size_t w2 = fwrite(&row[i].intValue, sizeof(int), 1, file);
                totalWritten += w2 * sizeof(int);
            } else {
                size_t w3 = fwrite(row[i].strValue, 1, MAX_STRING_LEN + 1, file);
                totalWritten += w3;
            }
        }
        fprintf(stderr, "[DBG-INSERT] totalWritten=%zu\n", totalWritten);

        long pos_after = ftell(file);
        fprintf(stderr, "[DBG-INSERT] ftell_after=%ld\n", pos_after);

        int flush_ret = fflush(file);
        fprintf(stderr, "[DBG-INSERT] fflush_ret=%d, ferror=%d\n", flush_ret, ferror(file));

        // 关闭前获取文件大小
        long file_size_before_close = 0;
        {
            struct stat st_after;
            if (stat(dataFilePath_.c_str(), &st_after) == 0) {
                file_size_before_close = st_after.st_size;
            }
        }
        fprintf(stderr, "[DBG-INSERT] stat_after( before close)=%ld\n", file_size_before_close);

        int close_ret = fclose(file);
        fprintf(stderr, "[DBG-INSERT] fclose_ret=%d\n", close_ret);

        // 关闭后再次检查文件大小
        long file_size_after_close = 0;
        {
            struct stat st_final;
            if (stat(dataFilePath_.c_str(), &st_final) == 0) {
                file_size_after_close = st_final.st_size;
            }
        }
        fprintf(stderr, "[DBG-INSERT] stat_after(after close)=%ld\n\n", file_size_after_close);

        // 如果有主键索引，更新索引
        if (hasIndex_) {
            for (size_t i = 0; i < columns_.size(); ++i) {
                if (columns_[i].isPrimaryKey && row[i].type == DataType::INT) {
                    index_.insert(row[i].intValue, offset);
                    break;
                }
            }
        }

        ResultSet rs;
        rs.affectedRows = 1;
        rs.setSuccess("1 row inserted");
        return rs;
    }

    // 条件查询
    ResultSet select(const ArrayList<std::string>& selectCols, 
                     const std::string& whereCol, Operator op, const Value& whereVal) {
        ArrayList<int64_t> offsets;
        ArrayList<Row> rows;
        scanRows(offsets, rows, whereCol, op, whereVal);

        ResultSet rs;

        // 构造结果列的列信息
        if (selectCols.size() == 1 && selectCols[0] == "*") {
            rs.columns = columns_;
        } else {
            for (size_t i = 0; i < selectCols.size(); ++i) {
                int idx = -1;
                for (size_t j = 0; j < columns_.size(); ++j) {
                    if (columns_[j].name == selectCols[i]) {
                        idx = static_cast<int>(j);
                        break;
                    }
                }
                if (idx >= 0) {
                    rs.columns.push_back(columns_[idx]);
                }
            }
        }

        // 填充数据行
        for (size_t i = 0; i < rows.size(); ++i) {
            Row resultRow;
            if (selectCols.size() == 1 && selectCols[0] == "*") {
                resultRow = rows[i];
            } else {
                for (size_t j = 0; j < selectCols.size(); ++j) {
                    int idx = -1;
                    for (size_t k = 0; k < columns_.size(); ++k) {
                        if (columns_[k].name == selectCols[j]) {
                            idx = static_cast<int>(k);
                            break;
                        }
                    }
                    if (idx >= 0) {
                        resultRow.addValue(rows[i][idx]);
                    }
                }
            }
            rs.rows.push_back(resultRow);
        }

        rs.affectedRows = static_cast<int>(rs.rows.size());
        return rs;
    }

    // 按条件删除
    ResultSet remove(const std::string& whereCol, Operator op, const Value& whereVal) {
        ArrayList<int64_t> offsets;
        ArrayList<Row> rows;
        scanRows(offsets, rows, whereCol, op, whereVal);

        if (rows.empty()) {
            ResultSet rs;
            rs.affectedRows = 0;
            rs.setSuccess("0 rows deleted");
            return rs;
        }

        // 读取所有剩余行
        std::fstream file(dataFilePath_, std::ios::in | std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            return ResultSet(ERR_GENERAL, "Failed to open table file");
        }

        file.seekg(0, std::ios::end);
        int64_t fileSize = file.tellg();
        size_t rSize = rowSize();
        int64_t totalRows = fileSize / rSize;

        // 标记要删除的行
        bool* deleted = new bool[totalRows]();
        for (size_t i = 0; i < offsets.size(); ++i) {
            int64_t idx = offsets[i] / rSize;
            if (idx >= 0 && idx < totalRows) {
                deleted[idx] = true;
            }
        }

        // 重建文件：写入未被删除的行
        std::string tmpFile = dataFilePath_ + ".tmp";
        std::fstream tmp(tmpFile, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!tmp.is_open()) {
            delete[] deleted;
            file.close();
            return ResultSet(ERR_GENERAL, "Failed to create temp file");
        }

        for (int64_t i = 0; i < totalRows; ++i) {
            if (!deleted[i]) {
                Row row = readRow(file, i * rSize);
                writeRow(tmp, tmp.tellp(), row);
            }
        }

        delete[] deleted;
        file.close();
        tmp.close();

        // 替换原文件
        ::remove(dataFilePath_.c_str());
        ::rename(tmpFile.c_str(), dataFilePath_.c_str());

        // 重建索引
        if (hasIndex_) {
            index_.create(indexFilePath_);
            rebuildIndex();
        }

        ResultSet rs;
        rs.affectedRows = static_cast<int>(offsets.size());
        if (rs.affectedRows == 1) {
            rs.setSuccess("1 row deleted");
        } else {
            rs.setSuccess(std::to_string(rs.affectedRows) + " rows deleted");
        }
        return rs;
    }

    // 按条件更新
    ResultSet update(const std::string& setCol, const Value& setVal,
                     const std::string& whereCol, Operator op, const Value& whereVal) {
        ArrayList<int64_t> offsets;
        ArrayList<Row> rows;
        scanRows(offsets, rows, whereCol, op, whereVal);

        if (rows.empty()) {
            ResultSet rs;
            rs.affectedRows = 0;
            rs.setSuccess("0 rows updated");
            return rs;
        }

        // 找到要更新的列索引
        int setColIdx = -1;
        for (size_t i = 0; i < columns_.size(); ++i) {
            if (columns_[i].name == setCol) {
                setColIdx = static_cast<int>(i);
                break;
            }
        }
        if (setColIdx < 0) {
            return ResultSet(ERR_NOT_FOUND, "Column '" + setCol + "' not found");
        }

        // 类型检查
        if (setVal.type != columns_[setColIdx].type) {
            return ResultSet(ERR_TYPE, "Type mismatch for column '" + setCol + "'");
        }

        // 更新行
        std::fstream file(dataFilePath_, std::ios::in | std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            return ResultSet(ERR_GENERAL, "Failed to open table file");
        }

        // 如果是更新主键，需要重建索引
        bool updatePK = columns_[setColIdx].isPrimaryKey;

        for (size_t i = 0; i < offsets.size(); ++i) {
            Row row = readRow(file, offsets[i]);
            row[setColIdx] = setVal;
            writeRow(file, offsets[i], row);
        }
        file.close();

        // 重建索引（如果更新了主键）
        if (updatePK && hasIndex_) {
            index_.create(indexFilePath_);
            rebuildIndex();
        }

        ResultSet rs;
        rs.affectedRows = static_cast<int>(offsets.size());
        if (rs.affectedRows == 1) {
            rs.setSuccess("1 row updated");
        } else {
            rs.setSuccess(std::to_string(rs.affectedRows) + " rows updated");
        }
        return rs;
    }

    // 重建索引（从数据文件重新构建）
    void rebuildIndex() {
        if (!hasIndex_) return;

        std::fstream file(dataFilePath_, std::ios::in | std::ios::binary);
        if (!file.is_open()) return;

        file.seekg(0, std::ios::end);
        int64_t fileSize = file.tellg();
        size_t rSize = rowSize();
        if (rSize == 0) { file.close(); return; }
        int64_t numRows = fileSize / rSize;

        // 找到主键列索引
        int pkColIdx = -1;
        for (size_t i = 0; i < columns_.size(); ++i) {
            if (columns_[i].isPrimaryKey) {
                pkColIdx = static_cast<int>(i);
                break;
            }
        }
        if (pkColIdx < 0) { file.close(); return; }

        for (int64_t i = 0; i < numRows; ++i) {
            int64_t offset = i * rSize;
            Row row = readRow(file, offset);
            if (row[pkColIdx].type == DataType::INT) {
                index_.insert(row[pkColIdx].intValue, offset);
            }
        }
        file.close();
    }

    // 检查指定列中是否存在某值（用于外键约束检查）
    bool existsByKey(const std::string& columnName, const Value& val) {
        std::fstream file(dataFilePath_, std::ios::in | std::ios::binary);
        if (!file.is_open()) return false;

        file.seekg(0, std::ios::end);
        int64_t fileSize = file.tellg();
        size_t rSize = rowSize();
        if (rSize == 0) { file.close(); return false; }
        int64_t numRows = fileSize / rSize;

        // 找到列索引
        int colIdx = -1;
        for (size_t i = 0; i < columns_.size(); ++i) {
            if (columns_[i].name == columnName) {
                colIdx = static_cast<int>(i);
                break;
            }
        }
        if (colIdx < 0) { file.close(); return false; }

        // 如果是有索引的主键等值查询，使用索引优化
        if (hasIndex_ && val.type == DataType::INT) {
            for (size_t i = 0; i < columns_.size(); ++i) {
                if (columns_[i].isPrimaryKey && columns_[i].name == columnName) {
                    int64_t offset = index_.search(val.intValue);
                    file.close();
                    return offset >= 0;
                }
            }
        }

        // 全表扫描
        for (int64_t i = 0; i < numRows; ++i) {
            Row row = readRow(file, i * rSize);
            if (row[colIdx] == val) {
                file.close();
                return true;
            }
        }

        file.close();
        return false;
    }

    // 查找指定列匹配某值的所有行（用于级联删除）
    void findMatchingRows(const std::string& columnName, const Value& val,
                          ArrayList<int64_t>& outOffsets, ArrayList<Row>& outRows) {
        ArrayList<int64_t> offsets;
        ArrayList<Row> rows;
        scanRows(offsets, rows, columnName, Operator::EQ, val);
        outOffsets = std::move(offsets);
        outRows = std::move(rows);
    }

    // 删除表文件
    ResultSet drop() {
        ::remove(dataFilePath_.c_str());
        if (hasIndex_) {
            ::remove(indexFilePath_.c_str());
        }
        return ResultSet(SUCCESS, "Table dropped");
    }
};

} // namespace minidb

#endif // TABLE_H
