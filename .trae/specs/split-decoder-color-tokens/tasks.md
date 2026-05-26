# Tasks

- [x] Task 1: 在 decodetrace.h 中新增 getAnnColor() 和 getAnnOutlineColor() 静态方法声明
- [x] Task 2: 在 decodetrace.cpp 中实现 getAnnColor() 和 getAnnOutlineColor()
  - [x] SubTask 2.1: getAnnColor(int) — 优先读 `@decoder-ann-N`，回退到 `@decoder-channel-N`，再回退到 defaultColours[N]
  - [x] SubTask 2.2: getAnnOutlineColor(int) — 优先读 `@decoder-ann-outline-N`，回退到 OutlineColours[N]
- [x] Task 3: 修改 decodetrace.cpp 的 draw_annotation() 使用新方法
  - [x] SubTask 3.1: 将 `getChannelColor(colour)` 替换为 `getAnnColor(colour)`
  - [x] SubTask 3.2: 将 `OutlineColours[colour]` 替换为 `getAnnOutlineColor(colour)`
- [x] Task 4: 更新 theme-schema.json，新增 decoder.ann.colors 分类（32个 token）
- [x] Task 5: 更新 theme.qss 顶部注释区，新增32个 token 声明
- [x] Task 6: 更新 dark.json — 新增32个 token（注释色=通道色，边框色=OutlineColours）
- [x] Task 7: 更新 light.json — 新增32个 token（注释色=通道色，边框色=OutlineColours）
- [x] Task 8: 更新 monokai.json — 新增32个自定义 token
- [x] Task 9: 更新 atom.json — 新增32个自定义 token

# Task Dependencies
- Task 3 depends on Task 1, Task 2
- Task 4 ~ Task 9 are independent of each other, but all depend on Task 1 for token naming convention
