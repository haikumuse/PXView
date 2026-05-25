# 第三轮审查修复任务

## 逻辑Bug修复

- [x] Task 1: 修复 graycode_c.c — forced_exp条件错误
  - [x] SubTask 1.1: 将 `format_si_value` 中 `forced_exp > 0` 改为 `forced_exp >= 0`
  - [x] SubTask 1.2: 区分"无强制"和"强制到0"语义，si_prefix_exp为空时返回-100而非0

- [x] Task 2: 修复 c2_c.c — 缺失采样率检查和ATK颜色
  - [x] SubTask 2.1: 在c2_decode入口添加采样率检查，为0时返回
  - [x] SubTask 2.2: 将c2_ann_labels前两个条目的ATK颜色从空字符串改为"106"

- [x] Task 3: 修复 pwm_c.c — 微秒格式字符串多余空格
  - [x] SubTask 3.1: 将"%.1f \xce\xbc s"改为"%.1f\xce\xbcs"

- [x] Task 4: 修复 ds1307_c.c — 缺失ADDRESS READ处理
  - [x] SubTask 4.1: 在STATE_GET_SLAVE_ADDR中添加对ADDRESS READ的处理

- [x] Task 5: 修复 ds3231_c.c — 缺失地址警告和注解ID格式
  - [x] SubTask 5.1: 地址不匹配时输出警告注解
  - [x] SubTask 5.2: 将注解ID中的下划线改为连字符

## idn字段补充

- [x] Task 6: 补充 lpc_c.c 通道idn字段
  - [x] SubTask 6.1: 为13个通道添加idn字符串

- [x] Task 7: 补充 counter_c.c 通道idn字段
  - [x] SubTask 7.1: 为通道添加idn字符串

- [x] Task 8: 补充 seven_segment_c.c 通道和选项idn字段
  - [x] SubTask 8.1: 为通道和选项添加idn字符串

- [x] Task 9: 补充 pwm_c.c 通道idn字段
  - [x] SubTask 9.1: 为通道添加idn字符串

- [x] Task 10: 补充 can_fd_c.c 通道和选项idn字段
  - [x] SubTask 10.1: 为通道和选项添加idn字符串

## 编译验证

- [x] Task 11: 编译验证所有修复
  - [x] SubTask 11.1: 增量构建所有修改的C解码器
  - [x] SubTask 11.2: 确保无编译错误和警告

# Task Dependencies
- [Task 1-5] 可并行
- [Task 6-10] 可并行
- [Task 11] 依赖 [Task 1-10]
