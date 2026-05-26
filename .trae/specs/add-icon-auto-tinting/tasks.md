# Tasks

- [x] Task 1: 在 theme-schema.json 中添加 @icon-accent 和 @icon-foreground token 定义
  - [x] 在 accent.colors 分类中添加 `@icon-accent`（label: IDS_STYLE_ICON_ACCENT, desc: IDS_STYLE_ICON_ACCENT_DESC）
  - [x] 在 accent.colors 分类中添加 `@icon-foreground`（label: IDS_STYLE_ICON_FOREGROUND, desc: IDS_STYLE_ICON_FOREGROUND_DESC）

- [x] Task 2: 在所有主题 JSON 文件中添加 token 默认值
  - [x] dark.json: `@icon-accent: "#1E90FF"`, `@icon-foreground: "#E0E0E0"`
  - [x] light.json: `@icon-accent: "#1E90FF"`, `@icon-foreground: "#424242"`
  - [x] atom.json: `@icon-accent: "#5b8def"`, `@icon-foreground: "#7f848e"`
  - [x] monokai.json: `@icon-accent: "#fd971f"`, `@icon-foreground: "#a6a594"`

- [x] Task 3: 在语言文件中添加 IDS 字符串
  - [x] cn/dlg.json: IDS_STYLE_ICON_ACCENT="图标强调色", IDS_STYLE_ICON_ACCENT_DESC="蓝色功能图标的着色颜色", IDS_STYLE_ICON_FOREGROUND="图标前景色", IDS_STYLE_ICON_FOREGROUND_DESC="窗口控件及搜索等图标的着色颜色"
  - [x] en/dlg.json: IDS_STYLE_ICON_ACCENT="Icon Accent", IDS_STYLE_ICON_ACCENT_DESC="Color for blue functional icons", IDS_STYLE_ICON_FOREGROUND="Icon Foreground", IDS_STYLE_ICON_FOREGROUND_DESC="Color for window control and search icons"
  - [x] traditional/dlg.json: 同理添加繁体中文

- [x] Task 4: 修改 IconCache::icon() 实现自动着色映射
  - [x] 在 iconcache.cpp 中添加静态映射表 `kIconTokenMap`，将 Category A 文件名映射到 "@icon-accent"，Category B 文件名映射到 "@icon-foreground"
  - [x] 修改 `icon()` 方法：从路径提取文件名 → 查映射表 → 若命中则调用 `tintedIcon()` → 否则原逻辑
  - [x] 缓存 key 需包含 token 值以区分不同主题下的同路径图标（复用 tintedIcon 的 key 策略：`path + "_" + color.name()`）

# Task Dependencies
- Task 4 依赖 Task 1（需要 token 在 schema 中定义才能在 UI 中编辑）
- Task 1, 2, 3 可并行
