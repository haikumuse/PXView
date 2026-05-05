# Tasks

- [x] Task 1: 创建 SearchPatternInput 自定义控件
  - [x] SubTask 1.1: 创建 `pv/widgets/searchpatterninput.h`，继承 QLineEdit
  - [x] SubTask 1.2: 声明属性：`channel_count`、自定义 keyPressEvent、focusInEvent
  - [x] SubTask 1.3: 创建 `pv/widgets/searchpatterninput.cpp`
  - [x] SubTask 1.4: 实现 keyPressEvent：仅接受 X/0/1/R/F/C/Backspace/Delete/Left/Right，替换当前字符并前进光标
  - [x] SubTask 1.5: 实现 focusInEvent：自动选中光标位置处一个字符
  - [x] SubTask 1.6: 设置等宽字体、字符间距（letterSpacing）、最大长度=通道数
  - [x] SubTask 1.7: 实现 set_channel_count(int n) 方法，初始化为 N 个 "X"
  - [x] SubTask 1.8: 实现 get_pattern() 方法，返回 std::map<uint16_t, QString>
  - [x] SubTask 1.9: 实现 set_pattern() 方法，从 map 填充文本

- [x] Task 2: 修改 SearchDock 头文件
  - [x] SubTask 2.1: 移除 `_search_grid`、`_search_lineEdit_vec` 成员
  - [x] SubTask 2.2: 新增 `SearchPatternInput *_pattern_input` 成员
  - [x] SubTask 2.3: 新增位范围标签容器 `QHBoxLayout *_bit_range_layout`
  - [x] SubTask 2.4: 新增图例相关布局成员
  - [x] SubTask 2.5: 添加 SearchPatternInput 头文件 include

- [x] Task 3: 重写 SearchDock 构造函数布局
  - [x] SubTask 3.1: 创建位范围标签行（QHBoxLayout，每8位一组显示 "高位---低位"）
  - [x] SubTask 3.2: 创建 SearchPatternInput 输入框
  - [x] SubTask 3.3: 创建分隔线（QFrame HLine）
  - [x] SubTask 3.4: 创建图例三列布局
  - [x] SubTask 3.5: 创建底部导航按钮行
  - [x] SubTask 3.6: 用 QVBoxLayout 组合所有元素
  - [x] SubTask 3.7: 移除 QScrollArea

- [x] Task 4: 重写 build_editors() 为 rebuild_pattern()
  - [x] SubTask 4.1: 计算逻辑通道数量
  - [x] SubTask 4.2: 更新位范围标签
  - [x] SubTask 4.3: 更新 _pattern_input 的 channel_count
  - [x] SubTask 4.4: 从已有 _pattern 映射恢复输入框内容
  - [x] SubTask 4.5: 清理 _pattern 中已不存在通道的条目

- [x] Task 5: 重写 on_pattern_changed()
  - [x] SubTask 5.1: 从 _pattern_input->get_pattern() 获取新模式
  - [x] SubTask 5.2: 更新 _pattern 映射
  - [x] SubTask 5.3: 调用 _view->set_search_pos() 重置搜索光标

- [x] Task 6: 更新 retranslateUi()、reStyle()、UpdateFont()
  - [x] SubTask 6.1: retranslateUi() 更新图例文本（国际化）
  - [x] SubTask 6.2: UpdateFont() 更新 _pattern_input 字体
  - [x] SubTask 6.3: reStyle() 保持不变

- [x] Task 7: 在 CMakeLists.txt 中添加新源文件
  - [x] SubTask 7.1: 添加 searchpatterninput.h 和 searchpatterninput.cpp

- [x] Task 8: 编译验证
  - [x] SubTask 8.1: 确保 searchdock.cpp 和 searchpatterninput.cpp 编译通过
  - [x] SubTask 8.2: 确保无未使用变量警告

# Task Dependencies
- [Task 2] depends on [Task 1]
- [Task 3] depends on [Task 1] and [Task 2]
- [Task 4] depends on [Task 3]
- [Task 5] depends on [Task 4]
- [Task 6] depends on [Task 3]
- [Task 7] depends on [Task 1]
- [Task 8] depends on [Task 1] through [Task 7]
