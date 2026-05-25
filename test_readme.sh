#!/bin/bash
# README 生命周期自动化测试脚本
# 先清理旧数据
rm -rf /home/yahboom/桌面/database/data/school

# 连接到客户端并执行 README 中的示例
./build/client << 'EOF'
create database school
use school
create table student (id int primary, name string, age int)
insert student values(2024001, "张三", 20)
insert student values(2024002, "李四", 21)
insert student values(2024003, "王五", 19)
select * from student
select name from student where id = 2024002
update student set age = 22 where name = "李四"
delete student where age < 20
select * from student
exit
EOF
