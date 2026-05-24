# 母Spec验证清单

## 规划完整性
- [x] 母spec完整列出所有178个待移植Python解码器
- [x] 每个解码器正确分类为TIER 1/2/3A/3B/3C/3D/3E
- [x] 分批计划合理，每批5个解码器（最后几批可少于5个）
- [x] 子spec依赖关系正确标注
- [x] C解码器实现标准完整且可操作
- [x] 全部37个子Spec已完成编写
- [x] API变更已记录（SRD_OUTPUT_PROTO重命名等）
- [x] 阻塞状态已标注（Batch 34部分/Batch 36-37全部被阻塞）

## API完备性（终审已通过）
- [x] 所有输出类型已实现（ANN/PROTO/BINARY/META/LOGIC）
- [x] 条件等待系统完备（c_cond_* + c_cond_wait_current）
- [x] C→C堆叠通道已实现（c_decoder_put_proto → recv_proto）
- [x] Python→C桥接崩溃已修复
- [x] 混合堆叠保护已添加
- [x] BITS v2格式已实现
- [x] SPI DATA 17字节格式已实现

## 子Spec质量标准 (每个子spec需满足)
- [x] 包含每个解码器的完整Python元数据
- [x] 包含每个解码器的状态机分析
- [x] 包含每个解码器的C实现计划
- [x] 包含samplerate时序防护方案
- [x] 包含条件构建器使用方案
- [x] 包含上层解码器的recv_proto实现方案
- [x] tasks.md列出每个解码器的实现步骤
- [x] checklist.md列出每个解码器的验证项
- [x] 37批spec过时内容已审查更新

## C解码器实现验证 (每个解码器需满足)
- [ ] 编译通过无警告
- [ ] 通道定义与Python一致
- [ ] 选项定义与Python一致
- [ ] 注解定义与Python一致
- [ ] annotation_rows覆盖所有注解类
- [ ] ann_labels第一列为空字符串
- [ ] 解码逻辑与Python一致
- [ ] samplerate时序防护已实现
- [ ] 已在CMakeLists.txt中注册
