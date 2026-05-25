#include <iostream>
#include <string>
#include <cassert>
#include "Common.h"
#include "ArrayList.h"
#include "Value.h"
#include "Column.h"
#include "Row.h"
#include "ResultSet.h"
#include "BPlusTree.h"
#include "StorageEngine.h"
#include "Table.h"
#include "SQLParser.h"
#include "Executor.h"

using namespace minidb;

int testCount = 0;
int passCount = 0;

#define TEST(name) \
    do { \
        testCount++; \
        std::cout << "Test " << testCount << ": " << name << " ... "; \
        try {

#define END_TEST \
            std::cout << "PASS" << std::endl; \
            passCount++; \
        } catch (const std::exception& e) { \
            std::cout << "FAIL (" << e.what() << ")" << std::endl; \
        } catch (...) { \
            std::cout << "FAIL (unknown error)" << std::endl; \
        } \
    } while(0)

// ==================== 测试 ArrayList ====================
void testArrayList() {
    TEST("ArrayList basic operations") {
        ArrayList<int> list;
        assert(list.empty());
        assert(list.size() == 0);

        list.push_back(10);
        list.push_back(20);
        list.push_back(30);
        assert(list.size() == 3);
        assert(!list.empty());
        assert(list[0] == 10);
        assert(list[1] == 20);
        assert(list[2] == 30);

        list.pop_back();
        assert(list.size() == 2);

        list.clear();
        assert(list.empty());
    }
    END_TEST;

    TEST("ArrayList find and erase") {
        ArrayList<int> list;
        list.push_back(1);
        list.push_back(2);
        list.push_back(3);
        list.push_back(4);

        assert(list.find(3) == 2);
        assert(list.find(99) == -1);

        list.erase_at(1);
        assert(list.size() == 3);
        assert(list[1] == 3);

        list.remove(3);
        assert(list.size() == 2);
    }
    END_TEST;

    TEST("ArrayList copy and assignment") {
        ArrayList<int> list1;
        list1.push_back(1);
        list1.push_back(2);

        ArrayList<int> list2(list1);
        assert(list2.size() == 2);
        assert(list2[0] == 1);

        ArrayList<int> list3;
        list3 = list1;
        assert(list3.size() == 2);
    }
    END_TEST;

    TEST("ArrayList iteration") {
        ArrayList<int> list;
        list.push_back(10);
        list.push_back(20);
        list.push_back(30);

        int sum = 0;
        for (auto it = list.begin(); it != list.end(); ++it) {
            sum += *it;
        }
        assert(sum == 60);
    }
    END_TEST;
}

// ==================== 测试 Value ====================
void testValue() {
    TEST("Value int") {
        Value v1(42);
        assert(v1.type == DataType::INT);
        assert(v1.intValue == 42);
        assert(v1.toString() == "42");
    }
    END_TEST;

    TEST("Value string") {
        Value v1(std::string("hello"));
        assert(v1.type == DataType::STRING);
        assert(std::string(v1.strValue) == "hello");
    }
    END_TEST;

    TEST("Value comparison") {
        Value v1(10);
        Value v2(10);
        Value v3(20);
        assert(v1 == v2);
        assert(v1 < v3);
        assert(v3 > v1);
    }
    END_TEST;

    TEST("Value serialization") {
        Value v1(42);
        Value v2(std::string("test"));

        std::stringstream ss;
        v1.serialize(ss);
        v2.serialize(ss);

        Value r1, r2;
        r1.deserialize(ss);
        r2.deserialize(ss);

        assert(r1.type == DataType::INT);
        assert(r1.intValue == 42);
        assert(r2.type == DataType::STRING);
        assert(std::string(r2.strValue) == "test");
    }
    END_TEST;
}

// ==================== 测试 SQL 解析器 ====================
void testParser() {
    SQLParser parser;

    TEST("Parse create database") {
        auto node = parser.parse("create database testdb");
        assert(node != nullptr);
        assert(node->type == StatementType::CREATE_DATABASE);
        auto* dbNode = static_cast<CreateDatabaseNode*>(node.get());
        assert(dbNode->dbName == "testdb");
    }
    END_TEST;

    TEST("Parse drop database") {
        auto node = parser.parse("drop database testdb");
        assert(node != nullptr);
        assert(node->type == StatementType::DROP_DATABASE);
    }
    END_TEST;

    TEST("Parse use database") {
        auto node = parser.parse("use testdb");
        assert(node != nullptr);
        assert(node->type == StatementType::USE_DATABASE);
    }
    END_TEST;

    TEST("Parse create table") {
        auto node = parser.parse("create table person (id int primary, name string)");
        assert(node != nullptr);
        assert(node->type == StatementType::CREATE_TABLE);
        auto* ctNode = static_cast<CreateTableNode*>(node.get());
        assert(ctNode->tableName == "person");
        assert(ctNode->columns.size() == 2);
        assert(ctNode->columns[0].name == "id");
        assert(ctNode->columns[0].type == "int");
        assert(ctNode->columns[0].isPrimaryKey == true);
        assert(ctNode->columns[1].name == "name");
        assert(ctNode->columns[1].type == "string");
    }
    END_TEST;

    TEST("Parse drop table") {
        auto node = parser.parse("drop table person");
        assert(node != nullptr);
        assert(node->type == StatementType::DROP_TABLE);
    }
    END_TEST;

    TEST("Parse insert") {
        auto node = parser.parse("insert person values(1001, \"peter\")");
        assert(node != nullptr);
        assert(node->type == StatementType::INSERT);
        auto* inNode = static_cast<InsertNode*>(node.get());
        assert(inNode->tableName == "person");
        assert(inNode->values.size() == 2);
    }
    END_TEST;

    TEST("Parse select all") {
        auto node = parser.parse("select * from person");
        assert(node != nullptr);
        assert(node->type == StatementType::SELECT);
        auto* selNode = static_cast<SelectNode*>(node.get());
        assert(selNode->tableName == "person");
        assert(selNode->columns.size() == 1);
        assert(selNode->columns[0] == "*");
    }
    END_TEST;

    TEST("Parse select with where") {
        auto node = parser.parse("select name from person where id = 1001");
        assert(node != nullptr);
        assert(node->type == StatementType::SELECT);
        auto* selNode = static_cast<SelectNode*>(node.get());
        assert(selNode->condition.hasCondition);
        assert(selNode->condition.column == "id");
        assert(selNode->condition.op == "=");
        assert(selNode->condition.value == "1001");
    }
    END_TEST;

    TEST("Parse update") {
        auto node = parser.parse("update person set name = \"john\" where id = 1001");
        assert(node != nullptr);
        assert(node->type == StatementType::UPDATE);
    }
    END_TEST;

    TEST("Parse delete") {
        auto node = parser.parse("delete person where id = 1001");
        assert(node != nullptr);
        assert(node->type == StatementType::DELETE);
    }
    END_TEST;

    TEST("Parse exit") {
        auto node = parser.parse("exit");
        assert(node != nullptr);
        assert(node->type == StatementType::EXIT);
    }
    END_TEST;

    TEST("Parse create view") {
        auto node = parser.parse("create view v1 as select * from person");
        assert(node != nullptr);
        assert(node->type == StatementType::CREATE_VIEW);
        auto* cvNode = static_cast<CreateViewNode*>(node.get());
        assert(cvNode->viewName == "v1");
        assert(cvNode->selectColumns.size() == 1);
        assert(cvNode->selectColumns[0] == "*");
        assert(cvNode->sourceTable == "person");
        assert(!cvNode->condition.hasCondition);
    }
    END_TEST;

    TEST("Parse create view with columns and where") {
        auto node = parser.parse("create view v2 as select id, name from person where age > 18");
        assert(node != nullptr);
        assert(node->type == StatementType::CREATE_VIEW);
        auto* cvNode = static_cast<CreateViewNode*>(node.get());
        assert(cvNode->viewName == "v2");
        assert(cvNode->selectColumns.size() == 2);
        assert(cvNode->selectColumns[0] == "id");
        assert(cvNode->selectColumns[1] == "name");
        assert(cvNode->sourceTable == "person");
        assert(cvNode->condition.hasCondition);
        assert(cvNode->condition.column == "age");
        assert(cvNode->condition.op == ">");
        assert(cvNode->condition.value == "18");
    }
    END_TEST;

    TEST("Parse drop view") {
        auto node = parser.parse("drop view v1");
        assert(node != nullptr);
        assert(node->type == StatementType::DROP_VIEW);
        auto* dvNode = static_cast<DropViewNode*>(node.get());
        assert(dvNode->viewName == "v1");
    }
    END_TEST;
}

// ==================== 测试 B+树 ====================
void testBPlusTree() {
    TEST("B+Tree insert and search") {
        BPlusTree tree;
        tree.create("/tmp/test_btree.idx");

        tree.insert(10, 100);
        tree.insert(20, 200);
        tree.insert(5, 50);
        tree.insert(15, 150);
        tree.insert(25, 250);

        assert(tree.search(10) == 100);
        assert(tree.search(20) == 200);
        assert(tree.search(5) == 50);
        assert(tree.search(99) == -1);

        remove("/tmp/test_btree.idx");
    }
    END_TEST;

    TEST("B+Tree getAll") {
        BPlusTree tree;
        tree.create("/tmp/test_btree2.idx");

        tree.insert(3, 30);
        tree.insert(1, 10);
        tree.insert(2, 20);

        auto all = tree.getAll();
        assert(all.size() == 3);

        // 检查排序（B+树中序遍历得到有序结果）
        assert(all[0].key == 1);
        assert(all[1].key == 2);
        assert(all[2].key == 3);

        remove("/tmp/test_btree2.idx");
    }
    END_TEST;
}

// ==================== 测试存储引擎和执行器 ====================
void testStorageAndExecutor() {
    // 清理
    system("rm -rf data/test_db");

    StorageEngine storage;
    Executor executor(storage);

    TEST("Create database") {
        ResultSet rs = executor.execute(*SQLParser().parse("create database test_db"));
        assert(rs.code == SUCCESS);
    }
    END_TEST;

    TEST("Use database") {
        ResultSet rs = executor.execute(*SQLParser().parse("use test_db"));
        assert(rs.code == SUCCESS);
        assert(storage.getCurrentDb() == "test_db");
    }
    END_TEST;

    TEST("Create table") {
        ResultSet rs = executor.execute(*SQLParser().parse("create table person (id int primary, name string)"));
        assert(rs.code == SUCCESS);
    }
    END_TEST;

    TEST("Insert data") {
        ResultSet rs = executor.execute(*SQLParser().parse("insert person values(1001, \"peter\")"));
        assert(rs.code == SUCCESS);
        assert(rs.affectedRows == 1);

        rs = executor.execute(*SQLParser().parse("insert person values(1002, \"john\")"));
        assert(rs.code == SUCCESS);
    }
    END_TEST;

    TEST("Select all") {
        ResultSet rs = executor.execute(*SQLParser().parse("select * from person"));
        assert(rs.code == SUCCESS);
        assert(rs.rows.size() == 2);
    }
    END_TEST;

    TEST("Select with where condition (EQ)") {
        ResultSet rs = executor.execute(*SQLParser().parse("select name from person where id = 1001"));
        assert(rs.code == SUCCESS);
        assert(rs.rows.size() == 1);
        assert(rs.rows[0][0].toString() == "peter");
    }
    END_TEST;

    TEST("Update data") {
        ResultSet rs = executor.execute(*SQLParser().parse("update person set name = \"peter_updated\" where id = 1001"));
        assert(rs.code == SUCCESS);
        assert(rs.affectedRows == 1);
    }
    END_TEST;

    TEST("Select after update") {
        ResultSet rs = executor.execute(*SQLParser().parse("select * from person where id = 1001"));
        assert(rs.code == SUCCESS);
        assert(rs.rows.size() == 1);
        assert(rs.rows[0][1].toString() == "peter_updated");
    }
    END_TEST;

    TEST("Delete data") {
        ResultSet rs = executor.execute(*SQLParser().parse("delete person where id = 1002"));
        assert(rs.code == SUCCESS);
        assert(rs.affectedRows == 1);
    }
    END_TEST;

    TEST("Select after delete") {
        ResultSet rs = executor.execute(*SQLParser().parse("select * from person"));
        assert(rs.code == SUCCESS);
        assert(rs.rows.size() == 1);
    }
    END_TEST;

    TEST("Create view") {
        ResultSet rs = executor.execute(*SQLParser().parse("create view v_person as select id, name from person"));
        assert(rs.code == SUCCESS);
        // 验证视图定义已保存
        assert(storage.viewExists("test_db", "v_person"));
    }
    END_TEST;

    TEST("Select from view") {
        ResultSet rs = executor.execute(*SQLParser().parse("select * from v_person"));
        assert(rs.code == SUCCESS);
        assert(rs.rows.size() == 1); // 经过 delete 后剩 1 行
        assert(rs.columns.size() == 2);
        assert(rs.columns[0] == "id");
        assert(rs.columns[1] == "name");
    }
    END_TEST;

    TEST("Select from view with user where") {
        ResultSet rs = executor.execute(*SQLParser().parse("select * from v_person where id = 1001"));
        assert(rs.code == SUCCESS);
        assert(rs.rows.size() == 1);
        assert(rs.rows[0][0].toString() == "1001");
    }
    END_TEST;

    TEST("Create view with where clause") {
        // 先插入一条数据供视图筛选
        executor.execute(*SQLParser().parse("insert person values(1003, \"alice\")"));
        ResultSet rs = executor.execute(*SQLParser().parse("create view v_adult as select * from person where id = 1001"));
        assert(rs.code == SUCCESS);
        assert(storage.viewExists("test_db", "v_adult"));
    }
    END_TEST;

    TEST("Select from view with view where") {
        ResultSet rs = executor.execute(*SQLParser().parse("select * from v_adult"));
        assert(rs.code == SUCCESS);
        assert(rs.rows.size() == 1); // 视图条件 id=1001
        assert(rs.rows[0][0].toString() == "1001");
    }
    END_TEST;

    TEST("Drop view") {
        ResultSet rs = executor.execute(*SQLParser().parse("drop view v_person"));
        assert(rs.code == SUCCESS);
        assert(!storage.viewExists("test_db", "v_person"));
    }
    END_TEST;

    TEST("Drop table") {
        ResultSet rs = executor.execute(*SQLParser().parse("drop table person"));
        assert(rs.code == SUCCESS);
    }
    END_TEST;

    TEST("Drop database") {
        ResultSet rs = executor.execute(*SQLParser().parse("drop database test_db"));
        assert(rs.code == SUCCESS);
    }
    END_TEST;
}

// ==================== 测试外键约束 ====================
void testForeignKey() {
    system("rm -rf data/test_fk_db");

    StorageEngine storage;
    Executor executor(storage);

    TEST("Create FK database") {
        ResultSet rs = executor.execute(*SQLParser().parse("create database test_fk_db"));
        assert(rs.code == SUCCESS);
    }
    END_TEST;

    TEST("Use FK database") {
        ResultSet rs = executor.execute(*SQLParser().parse("use test_fk_db"));
        assert(rs.code == SUCCESS);
    }
    END_TEST;

    TEST("Create parent table (departments)") {
        ResultSet rs = executor.execute(*SQLParser().parse("create table departments (id int primary, name string)"));
        assert(rs.code == SUCCESS);
    }
    END_TEST;

    TEST("Insert parent data") {
        ResultSet rs = executor.execute(*SQLParser().parse("insert departments values(1, \"Engineering\")"));
        assert(rs.code == SUCCESS);
        rs = executor.execute(*SQLParser().parse("insert departments values(2, \"HR\")"));
        assert(rs.code == SUCCESS);
    }
    END_TEST;

    TEST("Parse create table with foreign key") {
        auto node = SQLParser().parse("create table employees (id int primary, dept_id int, foreign key (dept_id) references departments(id))");
        assert(node != nullptr);
        assert(node->type == StatementType::CREATE_TABLE);
        auto* ctNode = static_cast<CreateTableNode*>(node.get());
        assert(ctNode->foreignKeys.size() == 1);
        assert(ctNode->foreignKeys[0].column == "dept_id");
        assert(ctNode->foreignKeys[0].refTable == "departments");
        assert(ctNode->foreignKeys[0].refColumn == "id");
        assert(ctNode->foreignKeys[0].onDeleteCascade == false);
    }
    END_TEST;

    TEST("Create child table with foreign key") {
        ResultSet rs = executor.execute(*SQLParser().parse("create table employees (id int primary, dept_id int, foreign key (dept_id) references departments(id))"));
        assert(rs.code == SUCCESS);
    }
    END_TEST;

    TEST("Insert valid FK value") {
        ResultSet rs = executor.execute(*SQLParser().parse("insert employees values(101, 1)"));
        assert(rs.code == SUCCESS);
        assert(rs.affectedRows == 1);
    }
    END_TEST;

    TEST("Insert another valid FK value") {
        ResultSet rs = executor.execute(*SQLParser().parse("insert employees values(102, 2)"));
        assert(rs.code == SUCCESS);
    }
    END_TEST;

    TEST("Reject insert with invalid FK value") {
        // dept_id=99 does not exist in departments
        ResultSet rs = executor.execute(*SQLParser().parse("insert employees values(103, 99)"));
        assert(rs.code != SUCCESS);  // 应该被拒绝
    }
    END_TEST;

    TEST("Reject delete parent row referenced by child (no cascade)") {
        ResultSet rs = executor.execute(*SQLParser().parse("delete departments where id = 1"));
        assert(rs.code != SUCCESS);  // employees 引用了 id=1
    }
    END_TEST;

    TEST("Parse FK with ON DELETE CASCADE") {
        auto node = SQLParser().parse("create table tasks (id int primary, dept_id int, foreign key (dept_id) references departments(id) on delete cascade)");
        assert(node != nullptr);
        assert(node->type == StatementType::CREATE_TABLE);
        auto* ctNode = static_cast<CreateTableNode*>(node.get());
        assert(ctNode->foreignKeys.size() == 1);
        assert(ctNode->foreignKeys[0].column == "dept_id");
        assert(ctNode->foreignKeys[0].refTable == "departments");
        assert(ctNode->foreignKeys[0].refColumn == "id");
        assert(ctNode->foreignKeys[0].onDeleteCascade == true);
    }
    END_TEST;

    TEST("Create task table with CASCADE") {
        ResultSet rs = executor.execute(*SQLParser().parse("create table tasks (id int primary, dept_id int, foreign key (dept_id) references departments(id) on delete cascade)"));
        assert(rs.code == SUCCESS);
    }
    END_TEST;

    TEST("Insert task data referencing department") {
        ResultSet rs = executor.execute(*SQLParser().parse("insert tasks values(1, 1)"));
        assert(rs.code == SUCCESS);
        rs = executor.execute(*SQLParser().parse("insert tasks values(2, 1)"));
        assert(rs.code == SUCCESS);
        rs = executor.execute(*SQLParser().parse("insert tasks values(3, 2)"));
        assert(rs.code == SUCCESS);
    }
    END_TEST;

    TEST("CASCADE delete - delete parent cascades to children") {
        // Delete Engineering department (id=1), should cascade delete tasks 1 and 2
        ResultSet rs = executor.execute(*SQLParser().parse("delete departments where id = 1"));
        assert(rs.code == SUCCESS);
    }
    END_TEST;

    TEST("Verify cascade - task rows deleted") {
        ResultSet rs = executor.execute(*SQLParser().parse("select * from tasks"));
        assert(rs.code == SUCCESS);
        assert(rs.rows.size() == 1);  // 只有 task 3 (dept_id=2) 剩下
        assert(rs.rows[0][0].toString() == "3");
    }
    END_TEST;

    TEST("Reject drop table referenced by FK") {
        ResultSet rs = executor.execute(*SQLParser().parse("drop table departments"));
        assert(rs.code != SUCCESS);  // 被 employees 和 tasks 引用
    }
    END_TEST;

    TEST("Drop child table first, then parent") {
        ResultSet rs = executor.execute(*SQLParser().parse("drop table employees"));
        assert(rs.code == SUCCESS);
        rs = executor.execute(*SQLParser().parse("drop table tasks"));
        assert(rs.code == SUCCESS);
        rs = executor.execute(*SQLParser().parse("drop table departments"));
        assert(rs.code == SUCCESS);
    }
    END_TEST;

    TEST("Drop FK database") {
        ResultSet rs = executor.execute(*SQLParser().parse("drop database test_fk_db"));
        assert(rs.code == SUCCESS);
    }
    END_TEST;
}

int main() {
    std::cout << "=======================================" << std::endl;
    std::cout << "  MiniDB Unit Tests" << std::endl;
    std::cout << "=======================================" << std::endl;

    testArrayList();
    testValue();
    testParser();
    testBPlusTree();
    testStorageAndExecutor();
    testForeignKey();

    std::cout << "=======================================" << std::endl;
    std::cout << "  Results: " << passCount << "/" << testCount << " tests passed" << std::endl;
    std::cout << "=======================================" << std::endl;

    return (passCount == testCount) ? 0 : 1;
}
