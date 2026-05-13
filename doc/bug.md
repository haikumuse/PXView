我已经定位到导致你提到的两个问题的原因，并给出了根本性的修复方案。

### 1. 为什么首次进入时下拉框“非常窄”？
这其实是由 Qt 的 `QGridLayout` 布局引擎的特性引起的。之前我们在将控件放入布局时使用了 `Qt::AlignRight` 对齐标志，这会强制告诉布局引擎：“**不要拉伸这个控件，保持它的最小建议大小，然后把它推到右边**”。由于刚启动时文本内容可能为空或较短，建议宽度非常窄，因此控件就被压缩了。
此外，`update_device_list` 函数中原本存在强制设定 `setFixedWidth(200)` 的历史遗留代码，这会打断我们的自适应布局逻辑。

**修复方法**：
- 移除了 `grid->addWidget` 中的水平对齐限制（只保留 `Qt::AlignVCenter`），同时将控件的策略设置为 `QSizePolicy::Expanding`，这让**它们自动横向拉伸并填满所在的整列**。因为它们都在同一列，所以宽度自然会变成像素级等宽，且跟随侧边栏自动调整大小。
- 移除了 `update_device_list` 中干扰布局的 `setFixedWidth(200)`。

### 2. 为什么切换标签页时参数没有改变？
因为之前 `unbind_context` 时，代码是去向底层的 `_device_agent` 索要当前采样率（`get_sample_rate()`）。但是，如果用户在 UI 上切换了采样率（比如选了 200Mhz）**但还没有点击“运行”**，底层的设备参数是不会更新的！这导致系统把旧的参数（500Mhz）保存到了 document 里，等你切换回来时，恢复的也是旧的参数。

**修复方法**：
- 修改了 `unbind_context` 的逻辑：现在它会**直接读取你 UI 界面上下拉框选中的真实值**并保存到 document 中。
- 修改了 `bind_context` 的逻辑：在恢复参数时，强制让界面中的下拉框跳到 document 保存的选项卡位置，彻底做到“所见即所存，所存即所得”。

以下是需要进行的修改：

```cpp
<<<<
        QWidget* SamplingBar::createSamplingSettingsWidget(QWidget *parent)
        {
            QWidget *group = new QWidget(parent);
            QVBoxLayout *vbox = new QVBoxLayout(group);
            vbox->setContentsMargins(0, 0, 0, 0);
            vbox->setSpacing(0);

            QLabel *titleLabel = new QLabel(
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_SAMPLING_SETTINGS), "采样设置"), group);
            titleLabel->setObjectName("dock_section_title");
            vbox->addWidget(titleLabel);

            QWidget *inner = new QWidget(group);
            QGridLayout *grid = new QGridLayout(inner);
            grid->setHorizontalSpacing(8);
            grid->setVerticalSpacing(4);
            grid->setContentsMargins(5, 2, 5, 5);
            grid->setColumnStretch(1, 1);

            QFont font = group->font();
            font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);
            inner->setFont(font);
            titleLabel->setFont(font);

            // Row 0: 设备
            QLabel *devLabel = new QLabel(
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_DEVICE), "设备"), inner);
            devLabel->setFont(font);
            grid->addWidget(devLabel, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);

            int target_w = 220;

            QHBoxLayout *devLayout = new QHBoxLayout();
            devLayout->setContentsMargins(0, 0, 0, 0);
            devLayout->setSpacing(4);
            devLayout->addStretch();
            devLayout->addWidget(&_device_type);
            _device_selector.setMinimumWidth(target_w);
            _device_selector.setMaximumWidth(target_w);
            devLayout->addWidget(&_device_selector);
            grid->addLayout(devLayout, 0, 1, Qt::AlignRight | Qt::AlignVCenter);

            // Row 1: 采样深度
            QLabel *depthLabel = new QLabel(
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_SAMPLE_DEPTH), "采样深度"), inner);
            depthLabel->setFont(font);
            grid->addWidget(depthLabel, 1, 0, Qt::AlignLeft | Qt::AlignVCenter);

            _sample_count.setMinimumWidth(target_w);
            _sample_count.setMaximumWidth(target_w);
            grid->addWidget(&_sample_count, 1, 1, Qt::AlignRight | Qt::AlignVCenter);

            // Row 2: 采样率
            QLabel *rateLabel = new QLabel(
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_SAMPLE_RATE), "采样率"), inner);
            rateLabel->setFont(font);
            grid->addWidget(rateLabel, 2, 0, Qt::AlignLeft | Qt::AlignVCenter);

            _sample_rate.setMinimumWidth(target_w);
            _sample_rate.setMaximumWidth(target_w);
            grid->addWidget(&_sample_rate, 2, 1, Qt::AlignRight | Qt::AlignVCenter);

            // Row 3: 捕获模式
            QLabel *modeLabel = new QLabel(
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_CAPTURE_MODE_ROW), "捕获模式"), inner);
            modeLabel->setFont(font);
            modeLabel->setObjectName("mode_label");
            grid->addWidget(modeLabel, 3, 0, Qt::AlignLeft | Qt::AlignVCenter);

            _mode_group = new QButtonGroup(inner);
            _radio_single = new QRadioButton(
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_CAPTURE_MODE_SINGLE), "单次"), inner);
            _radio_repeat = new QRadioButton(
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_CAPTURE_MODE_REPEAT), "重复"), inner);
            _radio_loop = new QRadioButton(
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_CAPTURE_MODE_LOOP), "循环"), inner);
            _radio_single->setFont(font);
            _radio_repeat->setFont(font);
            _radio_loop->setFont(font);
            _mode_group->addButton(_radio_single, COLLECT_SINGLE);
            _mode_group->addButton(_radio_repeat, COLLECT_REPEAT);
            _mode_group->addButton(_radio_loop, COLLECT_LOOP);

            QHBoxLayout *modeRow = new QHBoxLayout();
            modeRow->setSpacing(4);
            modeRow->setContentsMargins(0, 0, 0, 0);
            modeRow->addStretch();
            modeRow->addWidget(_radio_single);
            modeRow->addWidget(_radio_repeat);
            modeRow->addWidget(_radio_loop);
            grid->addLayout(modeRow, 3, 1, Qt::AlignRight | Qt::AlignVCenter);

            connect(_mode_group, SIGNAL(buttonClicked(int)), this, SLOT(on_mode_radio_clicked(int)));
====
        QWidget* SamplingBar::createSamplingSettingsWidget(QWidget *parent)
        {
            QWidget *group = new QWidget(parent);
            QVBoxLayout *vbox = new QVBoxLayout(group);
            vbox->setContentsMargins(0, 0, 0, 0);
            vbox->setSpacing(0);

            QLabel *titleLabel = new QLabel(
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_SAMPLING_SETTINGS), "采样设置"), group);
            titleLabel->setObjectName("dock_section_title");
            vbox->addWidget(titleLabel);

            QWidget *inner = new QWidget(group);
            QGridLayout *grid = new QGridLayout(inner);
            grid->setHorizontalSpacing(8);
            grid->setVerticalSpacing(4);
            grid->setContentsMargins(5, 2, 5, 5);
            grid->setColumnStretch(1, 1);

            QFont font = group->font();
            font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);
            inner->setFont(font);
            titleLabel->setFont(font);

            // Row 0: 设备
            QLabel *devLabel = new QLabel(
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_DEVICE), "设备"), inner);
            devLabel->setFont(font);
            grid->addWidget(devLabel, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);

            QHBoxLayout *devLayout = new QHBoxLayout();
            devLayout->setContentsMargins(0, 0, 0, 0);
            devLayout->setSpacing(4);
            devLayout->addWidget(&_device_type);
            
            _device_selector.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            _device_selector.setMinimumWidth(100);
            _device_selector.setMaximumWidth(QWIDGETSIZE_MAX);
            devLayout->addWidget(&_device_selector, 1);
            grid->addLayout(devLayout, 0, 1, Qt::AlignVCenter);

            // Row 1: 采样深度
            QLabel *depthLabel = new QLabel(
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_SAMPLE_DEPTH), "采样深度"), inner);
            depthLabel->setFont(font);
            grid->addWidget(depthLabel, 1, 0, Qt::AlignLeft | Qt::AlignVCenter);

            _sample_count.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            _sample_count.setMinimumWidth(100);
            _sample_count.setMaximumWidth(QWIDGETSIZE_MAX);
            grid->addWidget(&_sample_count, 1, 1, Qt::AlignVCenter);

            // Row 2: 采样率
            QLabel *rateLabel = new QLabel(
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_SAMPLE_RATE), "采样率"), inner);
            rateLabel->setFont(font);
            grid->addWidget(rateLabel, 2, 0, Qt::AlignLeft | Qt::AlignVCenter);

            _sample_rate.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            _sample_rate.setMinimumWidth(100);
            _sample_rate.setMaximumWidth(QWIDGETSIZE_MAX);
            grid->addWidget(&_sample_rate, 2, 1, Qt::AlignVCenter);

            // Row 3: 捕获模式
            QLabel *modeLabel = new QLabel(
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_CAPTURE_MODE_ROW), "捕获模式"), inner);
            modeLabel->setFont(font);
            modeLabel->setObjectName("mode_label");
            grid->addWidget(modeLabel, 3, 0, Qt::AlignLeft | Qt::AlignVCenter);

            _mode_group = new QButtonGroup(inner);
            _radio_single = new QRadioButton(
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_CAPTURE_MODE_SINGLE), "单次"), inner);
            _radio_repeat = new QRadioButton(
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_CAPTURE_MODE_REPEAT), "重复"), inner);
            _radio_loop = new QRadioButton(
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_CAPTURE_MODE_LOOP), "循环"), inner);
            _radio_single->setFont(font);
            _radio_repeat->setFont(font);
            _radio_loop->setFont(font);
            _mode_group->addButton(_radio_single, COLLECT_SINGLE);
            _mode_group->addButton(_radio_repeat, COLLECT_REPEAT);
            _mode_group->addButton(_radio_loop, COLLECT_LOOP);

            QHBoxLayout *modeRow = new QHBoxLayout();
            modeRow->setSpacing(4);
            modeRow->setContentsMargins(0, 0, 0, 0);
            modeRow->addStretch();
            modeRow->addWidget(_radio_single);
            modeRow->addWidget(_radio_repeat);
            modeRow->addWidget(_radio_loop);
            grid->addLayout(modeRow, 3, 1, Qt::AlignRight | Qt::AlignVCenter);

            connect(_mode_group, SIGNAL(buttonClicked(int)), this, SLOT(on_mode_radio_clicked(int)));
>>>>
```

```cpp
<<<<
        void SamplingBar::bind_context(TabContext *ctx)
        {
            assert(ctx);
            _context = ctx;
            _session = ctx->session();
            _view = ctx->view();
            _device_agent = _session->get_device();
            set_readonly(!ctx->is_live());
            if (_device_agent && _device_agent->have_instance()) {
                update_device_list();
                auto doc = ctx->document();
                if (doc && doc->_dock_sample_rate > 0) {
                    _device_agent->set_config_uint64(SR_CONF_SAMPLERATE, doc->_dock_sample_rate);
                    _device_agent->set_config_uint64(SR_CONF_LIMIT_SAMPLES, doc->_dock_sample_limit);
                    _session->set_collect_mode((DEVICE_COLLECT_MODE)doc->_dock_collect_mode);
                }
                update_sample_rate_selector();
                update_sample_rate_selector_value();
                update_sample_count_selector_value();
                reload();
            }
        }

        void SamplingBar::unbind_context()
        {
            if (_context && _context->document() && _device_agent && _session && _device_agent->have_instance()) {
                auto doc = _context->document();
                doc->_dock_sample_rate = _device_agent->get_sample_rate();
                doc->_dock_sample_limit = _device_agent->get_sample_limit();
                doc->_dock_collect_mode = (int)_session->get_collect_mode();
            }
            _context = nullptr;
            set_readonly(false);
        }
====
        void SamplingBar::bind_context(TabContext *ctx)
        {
            assert(ctx);
            _context = ctx;
            _session = ctx->session();
            _view = ctx->view();
            _device_agent = _session->get_device();
            set_readonly(!ctx->is_live());
            if (_device_agent && _device_agent->have_instance()) {
                update_device_list();
                auto doc = ctx->document();
                if (doc && doc->_dock_sample_rate > 0) {
                    _device_agent->set_config_uint64(SR_CONF_SAMPLERATE, doc->_dock_sample_rate);
                    _device_agent->set_config_uint64(SR_CONF_LIMIT_SAMPLES, doc->_dock_sample_limit);
                    _session->set_collect_mode((DEVICE_COLLECT_MODE)doc->_dock_collect_mode);
                }
                
                update_sample_rate_selector();

                if (doc && doc->_dock_sample_rate > 0) {
                    for (int i = _sample_rate.count() - 1; i >= 0; i--) {
                        if (doc->_dock_sample_rate >= _sample_rate.itemData(i).value<uint64_t>()) {
                            _sample_rate.setCurrentIndex(i);
                            break;
                        }
                    }
                }

                if (doc && doc->_dock_sample_limit > 0 && doc->_dock_sample_rate > 0) {
                    double duration = (double)doc->_dock_sample_limit / doc->_dock_sample_rate * SR_SEC(1);
                    for (int i = 0; i < _sample_count.count(); i++) {
                        if (duration >= _sample_count.itemData(i).value<double>()) {
                            _sample_count.setCurrentIndex(i);
                            break;
                        }
                    }
                }
                
                reload();
            }
        }

        void SamplingBar::unbind_context()
        {
            if (_context && _context->document() && _device_agent && _session && _device_agent->have_instance()) {
                auto doc = _context->document();
                
                if (_sample_rate.count() > 0 && _sample_rate.currentIndex() >= 0) {
                    doc->_dock_sample_rate = _sample_rate.itemData(_sample_rate.currentIndex()).value<uint64_t>();
                } else {
                    doc->_dock_sample_rate = _device_agent->get_sample_rate();
                }

                if (_sample_count.count() > 0 && _sample_count.currentIndex() >= 0) {
                    double duration = _sample_count.itemData(_sample_count.currentIndex()).value<double>();
                    uint64_t s_rate = doc->_dock_sample_rate > 0 ? doc->_dock_sample_rate : _device_agent->get_sample_rate();
                    if (s_rate > 0) {
                        doc->_dock_sample_limit = ((uint64_t)ceil(duration / SR_SEC(1) * s_rate) + SAMPLES_ALIGN) & ~SAMPLES_ALIGN;
                    } else {
                        doc->_dock_sample_limit = _device_agent->get_sample_limit();
                    }
                } else {
                    doc->_dock_sample_limit = _device_agent->get_sample_limit();
                }

                doc->_dock_collect_mode = (int)_session->get_collect_mode();
            }
            _context = nullptr;
            set_readonly(false);
        }
>>>>
```

```cpp
<<<<
        void SamplingBar::update_device_list()
        {
            struct ds_device_base_info *array = NULL;
            int dev_count = 0;
            int select_index = 0;

            dsv_info("Update device list.");

            array = _session->get_device_list(dev_count, select_index);

            if (array == NULL)
            {
                dsv_err("Get deivce list error!");
                return;
            }

            _updating_device_list = true;
            struct ds_device_base_info *p = NULL;
            ds_device_handle    cur_dev_handle = NULL_HANDLE;

            _device_selector.clear();

            for (int i = 0; i < dev_count; i++)
            {
                p = (array + i);
                _device_selector.addItem(QString(p->name), QVariant::fromValue((unsigned long long)p->handle));
                
                if (i == select_index)
                    cur_dev_handle = p->handle;
            }
            free(array);

            _device_selector.setCurrentIndex(select_index);

            if (cur_dev_handle != _last_device_handle){                
                update_sample_rate_list();
                _last_device_handle = cur_dev_handle;                
            }

            _last_device_index = select_index;
            int width = _device_selector.sizeHint().width();
            _device_selector.setFixedWidth(200);
            _sample_count.setFixedWidth(200);
            _sample_rate.setFixedWidth(200);
            _device_selector.view()->setMinimumWidth(width + 30);

            _updating_device_list = false;
        }
====
        void SamplingBar::update_device_list()
        {
            struct ds_device_base_info *array = NULL;
            int dev_count = 0;
            int select_index = 0;

            dsv_info("Update device list.");

            array = _session->get_device_list(dev_count, select_index);

            if (array == NULL)
            {
                dsv_err("Get deivce list error!");
                return;
            }

            _updating_device_list = true;
            struct ds_device_base_info *p = NULL;
            ds_device_handle    cur_dev_handle = NULL_HANDLE;

            _device_selector.clear();

            for (int i = 0; i < dev_count; i++)
            {
                p = (array + i);
                _device_selector.addItem(QString(p->name), QVariant::fromValue((unsigned long long)p->handle));
                
                if (i == select_index)
                    cur_dev_handle = p->handle;
            }
            free(array);

            _device_selector.setCurrentIndex(select_index);

            if (cur_dev_handle != _last_device_handle){                
                update_sample_rate_list();
                _last_device_handle = cur_dev_handle;                
            }

            _last_device_index = select_index;
            int width = _device_selector.sizeHint().width();
            _device_selector.view()->setMinimumWidth(width + 30);

            _updating_device_list = false;
        }
>>>>
```