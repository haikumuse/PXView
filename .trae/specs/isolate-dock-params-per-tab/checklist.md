* [ ] SessionDocument 添加了所有 Dock 参数缓存字段和序列化

* [ ] DeviceOptionsDock 实现了 IContextAware 接口

* [ ] DeviceOptionsDock 添加了 get\_session/set\_session 方法

* [ ] DeviceOptionsDock bind\_context 恢复设备选项参数到硬件和 UI

* [ ] DeviceOptionsDock unbind\_context 保存设备选项参数到 SessionDocument

* [ ] SamplingBar bind\_context 恢复采样率、采样深度、采集模式

* [ ] SamplingBar unbind\_context 保存采样参数到 SessionDocument

* [ ] SearchDock bind\_context 恢复搜索模式

* [ ] SearchDock unbind\_context 保存搜索模式到 SessionDocument

* [ ] MeasureDock bind\_context 恢复测量配置

* [ ] MeasureDock unbind\_context 保存测量配置到 SessionDocument

* [ ] TriggerDock bind\_context 恢复触发配置

* [ ] TriggerDock unbind\_context 保存触发配置到 SessionDocument

* [ ] DsoTriggerDock 添加了 get\_session/set\_session 方法

* [ ] DsoTriggerDock bind\_context 恢复 DSO 触发配置

* [ ] DsoTriggerDock unbind\_context 保存 DSO 触发配置到 SessionDocument

* [ ] on\_tab\_changed 中添加了所有 Dock 的 unbind/bind 调用

* [ ] 编译通过，无编译错误

* [ ] 切换标签时采样参数正确保存/恢复

* [ ] 切换标签时搜索模式正确保存/恢复

* [ ] 切换标签时测量配置正确保存/恢复

* [ ] 切换标签时触发配置正确保存/恢复

* [ ] 切换标签时 Vdiv/Coupling 正确保存/恢复

* [ ] 切换标签时 DeviceOptionsDock 正确刷新

