#ifndef BPLUS_TREE_H
#define BPLUS_TREE_H

#include <string>
#include <fstream>
#include <sstream>
#include <cstring>
#include "Common.h"
#include "ArrayList.h"

namespace minidb {

/**
 * @brief B+树索引
 * 
 * 用于主键索引。阶数(order)=4，即每个节点最多4个键。
 * - 内部节点：存储键和子节点指针（文件偏移）
 * - 叶子节点：存储键和记录位置（文件偏移）
 * - 叶子节点之间有兄弟指针，支持范围查询
 */

// 键值对
struct KeyValuePair {
    int key;            // 假设主键为 int 类型
    int64_t fileOffset; // 记录在数据文件中的偏移

    KeyValuePair() : key(0), fileOffset(0) {}
    KeyValuePair(int k, int64_t off) : key(k), fileOffset(off) {}
};

// B+树节点
struct BPlusNode {
    bool isLeaf;
    int keyCount;                         // 当前键的数量
    int keys[BPLUS_ORDER];                // 键数组
    int64_t children[BPLUS_ORDER + 1];    // 子节点文件偏移（内部节点用）
    int64_t fileOffset;                   // 本节点文件偏移位置
    int64_t nextLeaf;                     // 下一个叶子节点偏移（叶子节点用）
    int64_t dataOffsets[BPLUS_ORDER];     // 数据文件偏移（叶子节点用）

    BPlusNode() : isLeaf(true), keyCount(0), fileOffset(-1), nextLeaf(-1) {
        for (int i = 0; i < BPLUS_ORDER; ++i) {
            keys[i] = 0;
            children[i] = -1;
            dataOffsets[i] = -1;
        }
        children[BPLUS_ORDER] = -1;
    }

    // 序列化到文件
    void write(std::fstream& file) const {
        file.write(reinterpret_cast<const char*>(&isLeaf), sizeof(isLeaf));
        file.write(reinterpret_cast<const char*>(&keyCount), sizeof(keyCount));
        file.write(reinterpret_cast<const char*>(keys), sizeof(int) * BPLUS_ORDER);
        file.write(reinterpret_cast<const char*>(children), sizeof(int64_t) * (BPLUS_ORDER + 1));
        file.write(reinterpret_cast<const char*>(&nextLeaf), sizeof(nextLeaf));
        file.write(reinterpret_cast<const char*>(dataOffsets), sizeof(int64_t) * BPLUS_ORDER);
        file.flush();
    }

    // 从文件反序列化
    void read(std::fstream& file) {
        file.read(reinterpret_cast<char*>(&isLeaf), sizeof(isLeaf));
        file.read(reinterpret_cast<char*>(&keyCount), sizeof(keyCount));
        file.read(reinterpret_cast<char*>(keys), sizeof(int) * BPLUS_ORDER);
        file.read(reinterpret_cast<char*>(children), sizeof(int64_t) * (BPLUS_ORDER + 1));
        file.read(reinterpret_cast<char*>(&nextLeaf), sizeof(nextLeaf));
        file.read(reinterpret_cast<char*>(dataOffsets), sizeof(int64_t) * BPLUS_ORDER);
    }

    // 获取节点大小（字节）
    static int64_t nodeSize() {
        return sizeof(bool) + sizeof(int) + sizeof(int) * BPLUS_ORDER +
               sizeof(int64_t) * (BPLUS_ORDER + 1) + sizeof(int64_t) +
               sizeof(int64_t) * BPLUS_ORDER;
    }
};

class BPlusTree {
private:
    std::string filename_;
    int64_t rootOffset_;   // 根节点文件偏移

    int64_t allocateNode(std::fstream& file) {
        file.seekp(0, std::ios::end);
        int64_t offset = file.tellp();
        // 写入一个空节点占位
        BPlusNode emptyNode;
        emptyNode.write(file);
        return offset;
    }

    void readNode(std::fstream& file, int64_t offset, BPlusNode& node) const {
        file.seekg(offset);
        node.read(file);
        node.fileOffset = offset;
    }

    void writeNode(std::fstream& file, const BPlusNode& node) {
        if (node.fileOffset < 0) {
            // 新节点，分配空间
            BPlusNode& n = const_cast<BPlusNode&>(node);
            n.fileOffset = allocateNode(file);
            writeNode(file, node);
        } else {
            file.seekp(node.fileOffset);
            node.write(file);
        }
    }

    // 在节点中查找键的位置
    int findKeyPos(const BPlusNode& node, int key) const {
        int pos = 0;
        while (pos < node.keyCount && node.keys[pos] < key) {
            ++pos;
        }
        return pos;
    }

    // 分裂子节点
    void splitChild(std::fstream& file, BPlusNode& parent, int childIdx) {
        int64_t childOffset = parent.children[childIdx];
        BPlusNode child;
        readNode(file, childOffset, child);

        int midIdx = BPLUS_ORDER / 2;
        int midKey = child.keys[midIdx];

        // 创建新节点
        BPlusNode newNode;
        newNode.isLeaf = child.isLeaf;
        newNode.keyCount = child.keyCount - midIdx - 1;

        // 复制后半部分键到新节点
        for (int i = 0; i < newNode.keyCount; ++i) {
            newNode.keys[i] = child.keys[midIdx + 1 + i];
        }

        if (child.isLeaf) {
            for (int i = 0; i < newNode.keyCount; ++i) {
                newNode.dataOffsets[i] = child.dataOffsets[midIdx + 1 + i];
            }
            newNode.nextLeaf = child.nextLeaf;
            child.nextLeaf = newNode.fileOffset;
        } else {
            for (int i = 0; i <= newNode.keyCount; ++i) {
                newNode.children[i] = child.children[midIdx + 1 + i];
            }
        }

        // 缩减原节点
        child.keyCount = midIdx + (child.isLeaf ? 0 : 1);
        // 对于叶子节点，保留midIdx个键；对于内部节点，保留midIdx个键（midKey上提）

        // 写入分裂后的节点
        int64_t newOffset = allocateNode(file);
        newNode.fileOffset = newOffset;
        writeNode(file, child);
        writeNode(file, newNode);

        // 将 midKey 插入父节点
        for (int i = parent.keyCount; i > childIdx; --i) {
            parent.keys[i] = parent.keys[i - 1];
            parent.children[i + 1] = parent.children[i];
        }
        parent.keys[childIdx] = midKey;
        parent.children[childIdx + 1] = newOffset;
        parent.keyCount++;

        writeNode(file, parent);
    }

    // 插入非满节点
    void insertNonFull(std::fstream& file, BPlusNode& node, int key, int64_t dataOffset) {
        if (node.isLeaf) {
            // 找到插入位置
            int pos = node.keyCount - 1;
            while (pos >= 0 && node.keys[pos] > key) {
                node.keys[pos + 1] = node.keys[pos];
                node.dataOffsets[pos + 1] = node.dataOffsets[pos];
                --pos;
            }
            node.keys[pos + 1] = key;
            node.dataOffsets[pos + 1] = dataOffset;
            node.keyCount++;
            writeNode(file, node);
        } else {
            // 内部节点，找到子节点
            int i = node.keyCount - 1;
            while (i >= 0 && node.keys[i] > key) {
                --i;
            }
            ++i;

            BPlusNode child;
            readNode(file, node.children[i], child);

            if (child.keyCount == BPLUS_ORDER) {
                splitChild(file, node, i);
                // 确定分裂后进入哪个子节点
                if (node.keys[i] < key) {
                    ++i;
                }
            }

            readNode(file, node.children[i], child);
            insertNonFull(file, child, key, dataOffset);
        }
    }

    // 查找键，返回记录偏移
    int64_t searchInternal(std::fstream& file, int64_t nodeOffset, int key) const {
        if (nodeOffset < 0) return -1;

        BPlusNode node;
        readNode(file, nodeOffset, node);

        int pos = findKeyPos(node, key);

        if (node.isLeaf) {
            if (pos < node.keyCount && node.keys[pos] == key) {
                return node.dataOffsets[pos];
            }
            return -1;
        }

        return searchInternal(file, node.children[pos], key);
    }

    // 范围查询 [startKey, endKey]
    void searchRangeInternal(std::fstream& file, int64_t nodeOffset, 
                              int startKey, int endKey,
                              ArrayList<KeyValuePair>& results) const {
        if (nodeOffset < 0) return;

        BPlusNode node;
        readNode(file, nodeOffset, node);

        if (!node.isLeaf) {
            // 找到第一个可能包含结果的子节点
            int pos = findKeyPos(node, startKey);
            if (pos > 0) --pos;
            searchRangeInternal(file, node.children[pos], startKey, endKey, results);
        } else {
            // 叶子节点，遍历所有键
            for (int i = 0; i < node.keyCount; ++i) {
                if (node.keys[i] >= startKey && node.keys[i] <= endKey) {
                    results.push_back(KeyValuePair(node.keys[i], node.dataOffsets[i]));
                }
            }
            // 继续下一个叶子节点
            if (node.nextLeaf >= 0) {
                searchRangeInternal(file, node.nextLeaf, startKey, endKey, results);
            }
        }
    }

    // 删除键
    bool removeInternal(std::fstream& file, int64_t nodeOffset, int key) {
        BPlusNode node;
        readNode(file, nodeOffset, node);

        int pos = findKeyPos(node, key);

        if (node.isLeaf) {
            if (pos < node.keyCount && node.keys[pos] == key) {
                // 移除键
                for (int i = pos; i < node.keyCount - 1; ++i) {
                    node.keys[i] = node.keys[i + 1];
                    node.dataOffsets[i] = node.dataOffsets[i + 1];
                }
                node.keyCount--;
                writeNode(file, node);
                return true;
            }
            return false;
        }

        // 内部节点
        bool found = false;
        if (pos < node.keyCount && node.keys[pos] == key) {
            // 键在当前节点中，找前驱或后继替代
            BPlusNode leaf;
            readNode(file, node.children[pos], leaf);
            while (!leaf.isLeaf) {
                readNode(file, leaf.children[leaf.keyCount], leaf);
            }
            if (leaf.keyCount > 0) {
                node.keys[pos] = leaf.keys[leaf.keyCount - 1];
                writeNode(file, node);
                found = removeInternal(file, node.children[pos], node.keys[pos]);
            }
            return found;
        }

        found = removeInternal(file, node.children[pos], key);
        return found;
    }

public:
    BPlusTree() : rootOffset_(-1) {}

    bool open(const std::string& filename) {
        filename_ = filename;
        std::fstream file(filename, std::ios::in | std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            // 文件不存在，创建新文件
            file.open(filename, std::ios::out | std::ios::binary);
            if (!file.is_open()) return false;
            file.close();
            file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
            rootOffset_ = -1;
            return true;
        }

        // 读取根节点偏移
        file.seekg(0);
        file.read(reinterpret_cast<char*>(&rootOffset_), sizeof(rootOffset_));
        file.close();
        return true;
    }

    bool create(const std::string& filename) {
        filename_ = filename;
        std::fstream file(filename, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!file.is_open()) return false;
        file.close();

        // 重新以读写方式打开
        file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
        if (!file.is_open()) return false;

        // 写入根节点偏移（初始为-1）
        rootOffset_ = -1;
        file.write(reinterpret_cast<const char*>(&rootOffset_), sizeof(rootOffset_));
        file.close();
        return true;
    }

    void insert(int key, int64_t dataOffset) {
        std::fstream file(filename_, std::ios::in | std::ios::out | std::ios::binary);
        if (!file.is_open()) return;

        if (rootOffset_ < 0) {
            // 创建根节点（叶子节点）
            BPlusNode root;
            root.isLeaf = true;
            root.keys[0] = key;
            root.dataOffsets[0] = dataOffset;
            root.keyCount = 1;
            rootOffset_ = allocateNode(file);
            root.fileOffset = rootOffset_;
            writeNode(file, root);

            // 更新文件头的根节点偏移
            file.seekp(0);
            file.write(reinterpret_cast<const char*>(&rootOffset_), sizeof(rootOffset_));
        } else {
            BPlusNode root;
            readNode(file, rootOffset_, root);

            if (root.keyCount == BPLUS_ORDER) {
                // 根节点满，分裂
                BPlusNode newRoot;
                newRoot.isLeaf = false;
                newRoot.keyCount = 0;
                newRoot.children[0] = rootOffset_;

                int64_t oldRootOffset = rootOffset_;
                splitChild(file, newRoot, 0);

                rootOffset_ = allocateNode(file);
                newRoot.fileOffset = rootOffset_;
                writeNode(file, newRoot);

                // 更新文件头的根节点偏移
                file.seekp(0);
                file.write(reinterpret_cast<const char*>(&rootOffset_), sizeof(rootOffset_));

                // 确定插入到哪个子树
                BPlusNode checkRoot;
                readNode(file, rootOffset_, checkRoot);
                int i = 0;
                while (i < checkRoot.keyCount && checkRoot.keys[i] < key) {
                    ++i;
                }
                BPlusNode child;
                readNode(file, checkRoot.children[i], child);
                insertNonFull(file, child, key, dataOffset);
            } else {
                insertNonFull(file, root, key, dataOffset);
            }
        }
        file.close();
    }

    int64_t search(int key) {
        if (rootOffset_ < 0) return -1;
        std::fstream file(filename_, std::ios::in | std::ios::out | std::ios::binary);
        if (!file.is_open()) return -1;
        int64_t result = searchInternal(file, rootOffset_, key);
        file.close();
        return result;
    }

    ArrayList<KeyValuePair> searchRange(int startKey, int endKey) {
        ArrayList<KeyValuePair> results;
        if (rootOffset_ < 0) return results;
        std::fstream file(filename_, std::ios::in | std::ios::out | std::ios::binary);
        if (!file.is_open()) return results;
        searchRangeInternal(file, rootOffset_, startKey, endKey, results);
        file.close();
        return results;
    }

    void remove(int key) {
        if (rootOffset_ < 0) return;
        std::fstream file(filename_, std::ios::in | std::ios::out | std::ios::binary);
        if (!file.is_open()) return;
        removeInternal(file, rootOffset_, key);
        file.close();
    }

    // 获取所有键值对（用于遍历）
    ArrayList<KeyValuePair> getAll() {
        ArrayList<KeyValuePair> results;
        if (rootOffset_ < 0) return results;

        std::fstream file(filename_, std::ios::in | std::ios::out | std::ios::binary);
        if (!file.is_open()) return results;

        // 找到最左边的叶子节点
        BPlusNode node;
        readNode(file, rootOffset_, node);
        while (!node.isLeaf) {
            readNode(file, node.children[0], node);
        }

        // 遍历所有叶子节点
        while (true) {
            for (int i = 0; i < node.keyCount; ++i) {
                results.push_back(KeyValuePair(node.keys[i], node.dataOffsets[i]));
            }
            if (node.nextLeaf < 0) break;
            readNode(file, node.nextLeaf, node);
        }

        file.close();
        return results;
    }
};

} // namespace minidb

#endif // BPLUS_TREE_H
