/*
 * This file is part of the PXView project.
 * PXView is based on DSView.
 * PXView is based on PulseView.
 *
 * Copyright (C) 2013 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "samplingbar.h"
#include <assert.h>
#include <QAction>
#include <QLabel>
#include <QAbstractItemView>
#include <math.h>
#include <libusb-1.0/libusb.h>
#include "../dialogs/deviceoptions.h"
#include "../dialogs/waitingdialog.h"
#include "../dialogs/dsmessagebox.h"
#include "../view/dsosignal.h"
#include "../dialogs/interval.h"
#include "../config/appconfig.h"
#include "../dsvdef.h"
#include "../log.h"
#include "../deviceagent.h"
#include "../ui/msgbox.h"
#include "../ui/langresource.h"
#include "../view/view.h"
#include "../ui/fn.h"
#include "../tabcontext.h"
#include "../data/sessiondocument.h"

#include <QWidgetAction>
#include <QSpacerItem>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QButtonGroup>
#include <QRadioButton>

#define SINGLE_ACTION_ICON  "/once.svg"
#define REPEAT_ACTION_ICON  "/repeat.svg"
#define LOOP_ACTION_ICON  "/loop.svg"

using std::map;
using std::max;
using std::min;
using std::string;

namespace pv
{
    namespace toolbars
    {

        const QString SamplingBar::RLEString = "(RLE)";
        const QString SamplingBar::DIVString = " / div";

        SamplingBar::SamplingBar(SigSession *session, QWidget *parent) : QToolBar("Sampling Bar", parent),
                                                                         _device_type(this),
                                                                         _device_selector(this),
                                                                         _configure_button(this),
                                                                         _sample_count(this),
                                                                         _sample_rate(this),
                                                                         _run_stop_button(this),
                                                                         _instant_button(this),
                                                                         _mode_button(this)
        {
            _updating_device_list = false;
            _updating_sample_rate = false;
            _updating_sample_count = false;
            _is_run_as_instant = false;
            _is_readonly = false;
            _context = nullptr;

            _last_device_handle = NULL_HANDLE;
            _last_device_index = -1;
            _next_switch_device = NULL_HANDLE;
            _view = NULL;
            _mode_group = nullptr;
            _radio_single = nullptr;
            _radio_repeat = nullptr;
            _radio_loop = nullptr;

            _session = session;
            _device_agent = _session->get_device();

            setMovable(false);
            setContentsMargins(0, 0, 0, 0);
            layout()->setSpacing(0);

            _mode_button.setPopupMode(QToolButton::InstantPopup);

            _device_selector.setSizeAdjustPolicy(DsComboBox::AdjustToContents);
            _sample_rate.setSizeAdjustPolicy(DsComboBox::AdjustToContents);
            _sample_count.setSizeAdjustPolicy(DsComboBox::AdjustToContents);
            _device_selector.setMaximumWidth(ComboBoxMaxWidth);

            //tr
            _run_stop_button.setObjectName("run_stop_button");

            QWidget *leftMargin = new QWidget(this);
            leftMargin->setFixedWidth(4);
            addWidget(leftMargin);

            // _device_type.setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            _device_type.setToolButtonStyle(Qt::ToolButtonIconOnly);
            addWidget(&_device_type);
            addWidget(new QLabel("  "));
            _device_type_label = new QLabel(this);
            addWidget(_device_type_label);
            addWidget(new QLabel("  "));
            addWidget(&_device_selector);
            
            _configure_button.setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            _configure_button.setCheckable(true);
            _configure_button.setChecked(false);

            addWidget(new QLabel("  "));

            addWidget(&_sample_count);
            //tr
            // addWidget(new QLabel(" @ "));
            addWidget(new QLabel("  "));
            addWidget(&_sample_rate);

            _action_single = new QAction(this);
            _action_repeat = new QAction(this);
            _action_loop = new QAction(this);

            _mode_menu = new QMenu(this);
            _mode_menu->addAction(_action_single);
            _mode_menu->addAction(_action_repeat);
            _mode_menu->addAction(_action_loop);
            _mode_button.setMenu(_mode_menu);

            auto widgetToAction = [](QWidget* widget, QWidget* parent = nullptr) -> QAction* {
                QWidgetAction *action = new QWidgetAction(parent);
                action->setDefaultWidget(widget);
                return action;
            };

            _configure_action = widgetToAction(&_configure_button);

            _mode_button.setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            _mode_action = widgetToAction(&_mode_button);

            _run_stop_button.setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            _run_stop_action = widgetToAction(&_run_stop_button);
            _instant_button.setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            _instant_action = widgetToAction(&_instant_button);
            _instant_action->setVisible(true);

            update_view_status();

            connect(&_device_selector, SIGNAL(currentIndexChanged(int)), this, SLOT(on_device_selected()));
            connect(&_configure_button, SIGNAL(clicked()), this, SLOT(on_configure()));
            connect(&_run_stop_button, SIGNAL(clicked()), this, SLOT(on_run_stop()));
            connect(&_instant_button, SIGNAL(clicked()), this, SLOT(on_instant_stop()));
            connect(&_sample_count, SIGNAL(currentIndexChanged(int)), this, SLOT(on_samplecount_sel(int)));
            connect(_action_single, SIGNAL(triggered()), this, SLOT(on_collect_mode()));
            connect(_action_repeat, SIGNAL(triggered()), this, SLOT(on_collect_mode()));
            connect(_action_loop, SIGNAL(triggered()), this, SLOT(on_collect_mode()));
            connect(&_sample_rate, SIGNAL(currentIndexChanged(int)), this, SLOT(on_samplerate_sel(int)));

            ADD_UI(this);
        }

        SamplingBar::~SamplingBar()
        {
            REMOVE_UI(this);
        }

        QGroupBox* SamplingBar::createSamplingSettingsWidget(QWidget *parent)
        {
            QGroupBox *group = new QGroupBox(
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_SAMPLING_SETTINGS), "采样设置"), parent);
            QGridLayout *grid = new QGridLayout(group);
            grid->setHorizontalSpacing(8);
            grid->setVerticalSpacing(4);
            grid->setContentsMargins(8, 16, 8, 8);
            grid->setColumnStretch(1, 1);

            QFont font = group->font();
            font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);
            group->setFont(font);

            // Row 0: 设备
            QLabel *devLabel = new QLabel(
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_DEVICE), "设备"), group);
            devLabel->setFont(font);
            grid->addWidget(devLabel, 0, 0, Qt::AlignRight | Qt::AlignVCenter);

            QHBoxLayout *devRow = new QHBoxLayout();
            devRow->setSpacing(4);
            devRow->setContentsMargins(0, 0, 0, 0);
            devRow->addStretch();
            devRow->addWidget(&_device_type);
            devRow->addWidget(&_device_selector);
            grid->addLayout(devRow, 0, 1, Qt::AlignLeft);

            // Row 1: 采样深度
            QLabel *depthLabel = new QLabel(
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_SAMPLE_DEPTH), "采样深度"), group);
            depthLabel->setFont(font);
            grid->addWidget(depthLabel, 1, 0, Qt::AlignRight | Qt::AlignVCenter);

            QHBoxLayout *depthRow = new QHBoxLayout();
            depthRow->setSpacing(0);
            depthRow->setContentsMargins(0, 0, 0, 0);
            depthRow->addStretch();
            depthRow->addWidget(&_sample_count);
            grid->addLayout(depthRow, 1, 1, Qt::AlignLeft);

            // Row 2: 采样率
            QLabel *rateLabel = new QLabel(
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_SAMPLE_RATE), "采样率"), group);
            rateLabel->setFont(font);
            grid->addWidget(rateLabel, 2, 0, Qt::AlignRight | Qt::AlignVCenter);

            QHBoxLayout *rateRow = new QHBoxLayout();
            rateRow->setSpacing(0);
            rateRow->setContentsMargins(0, 0, 0, 0);
            rateRow->addStretch();
            rateRow->addWidget(&_sample_rate);
            grid->addLayout(rateRow, 2, 1, Qt::AlignLeft);

            // Row 3: 捕获模式
            QLabel *modeLabel = new QLabel(
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_CAPTURE_MODE_ROW), "捕获模式"), group);
            modeLabel->setFont(font);
            modeLabel->setObjectName("mode_label");
            grid->addWidget(modeLabel, 3, 0, Qt::AlignRight | Qt::AlignVCenter);

            _mode_group = new QButtonGroup(group);
            _radio_single = new QRadioButton(
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_CAPTURE_MODE_SINGLE), "单次"), group);
            _radio_repeat = new QRadioButton(
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_CAPTURE_MODE_REPEAT), "重复"), group);
            _radio_loop = new QRadioButton(
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_CAPTURE_MODE_LOOP), "循环"), group);
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
            grid->addLayout(modeRow, 3, 1, Qt::AlignLeft);

            connect(_mode_group, SIGNAL(buttonClicked(int)), this, SLOT(on_mode_radio_clicked(int)));

            // 控件从 QToolBar 移出时 QWidgetAction::releaseWidget() 会自动 hide()，
            // 需要显式 show() 恢复可见性
            _device_type.show();
            _device_selector.show();
            _sample_count.show();
            _sample_rate.show();

            return group;
        }

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

        void SamplingBar::retranslateUi()
        {
            bool bDev = _device_agent->have_instance();

            if (bDev)
            {
                if (_device_agent->is_demo())
                {
                    _device_type_label->setText(L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_DEVICE_TYPE_DEMO), "Demo"));
                }
                else if (_device_agent->is_file())
                {
                    _device_type_label->setText(L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_DEVICE_TYPE_FILE), "File"));
                }
                else
                {
                    int usb_speed = LIBUSB_SPEED_HIGH;
                    _device_agent->get_config_int32(SR_CONF_USB_SPEED, usb_speed);

                    if (usb_speed == LIBUSB_SPEED_HIGH)
                        _device_type_label->setText("USB 2.0");
                    else if (usb_speed == LIBUSB_SPEED_SUPER)
                        _device_type_label->setText("USB 3.0");
                    else
                        _device_type_label->setText("USB UNKNOWN");
                }
            }
            _configure_button.setText(L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_DEVICE_OPTION), "Options"));
           _mode_button.setText(L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_CAPTURE_MODE), "Mode"));

            int mode = _device_agent->get_work_mode();
            bool is_working = _session->is_working();

            auto str_start = L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_RUN_START), "Start");
            auto str_stop  = L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_RUN_STOP), "Stop");
            auto str_single  = L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_ONE_SINGLE), "Single");
            auto str_instant  = L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_ONE_INSTANT), "Instant");
            auto str_one_stop  = L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_ONE_STOP), "Stop");

            if (_is_run_as_instant)
            {
                if (bDev && mode == DSO)
                    _instant_button.setText(is_working ? str_one_stop : str_single);
                else
                    _instant_button.setText(is_working ? str_one_stop : str_instant);

                _run_stop_button.setText(str_start);
            }
            else
            {
                _run_stop_button.setText(is_working ? str_stop: str_start);

                if (bDev && mode == DSO)
                    _instant_button.setText(str_single);
                else
                    _instant_button.setText(str_instant);
            }

            _action_single->setText(L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_CAPTURE_MODE_SINGLE), "&Single"));
            _action_repeat->setText(L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_CAPTURE_MODE_REPEAT), "&Repetitive"));
            _action_loop->setText(L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_CAPTURE_MODE_LOOP), "&Loop"));
        }

        void SamplingBar::reStyle()
        { 
            bool bDev = _device_agent->have_instance();

            if (bDev)
            {
                if (_device_agent->is_demo())
                    _device_type.setIcon(QIcon(":/icons/demo.svg"));
                else if (_device_agent->is_file())
                    _device_type.setIcon(QIcon(":/icons/data.svg"));
                else
                {
                    int usb_speed = LIBUSB_SPEED_HIGH;
                    _device_agent->get_config_int32(SR_CONF_USB_SPEED, usb_speed);

                    if (usb_speed == LIBUSB_SPEED_SUPER)
                        _device_type.setIcon(QIcon(":/icons/usb3.svg"));
                    else
                        _device_type.setIcon(QIcon(":/icons/usb2.svg"));
                }
            }

            if (true)
            {
                QString iconPath = GetIconPath();
                _configure_button.setIcon(QIcon(iconPath + "/params.svg"));
            
                QString icon2 = _session->is_working() ? "stop.svg" : "start.svg";
                _run_stop_button.setIcon(QIcon(iconPath + "/" + icon2));
                _instant_button.setIcon(QIcon(iconPath + "/single.svg"));

                _action_single->setIcon(QIcon(iconPath + SINGLE_ACTION_ICON));
                _action_repeat->setIcon(QIcon(iconPath + REPEAT_ACTION_ICON));
                _action_loop->setIcon(QIcon(iconPath + LOOP_ACTION_ICON));

                update_mode_icon();
            }
        }

        void SamplingBar::on_configure()
        {
            if (_device_agent->have_instance() == false)
            {
                dsv_info("Have no device, can't to set device config.");
                _configure_button.setChecked(false);
                return;
            }

            emit sig_device_options(_configure_button.isChecked());
        }

        void SamplingBar::zero_adj()
        { 
            for (auto s : _session->get_signals())
            {
                if (s->signal_type() == SR_CHANNEL_DSO){
                    view::DsoSignal *dsoSig = (view::DsoSignal*)s;
                    dsoSig->set_enable(true);
                }
            }

            const int index_back = _sample_count.currentIndex();
            int i = 0;

            for (i = 0; i < _sample_count.count(); i++){
                if (_sample_count.itemData(i).value<uint64_t>() == ZeroTimeBase)
                    break;
            }

            set_sample_count_index(i);
            commit_hori_res();

            if (_session->is_working() == false)
                _session->start_capture(false);

            pv::dialogs::WaitingDialog wait(this, _session, SR_CONF_ZERO);
            if (wait.start() == QDialog::Rejected)
            {
                for (auto s : _session->get_signals())
                {
                    if (s->signal_type() == SR_CHANNEL_DSO){
                        view::DsoSignal *dsoSig = (view::DsoSignal*)s;
                        dsoSig->commit_settings();
                    }
                }
            }

            if (_session->is_working())
                _session->stop_capture();

            set_sample_count_index(index_back);
            commit_hori_res();
        }

        void SamplingBar::set_sample_rate(uint64_t sample_rate)
        {
            for (int i = _sample_rate.count() - 1; i >= 0; i--)
            {
                uint64_t cur_index_sample_rate = _sample_rate.itemData(
                                                                 i)
                                                     .value<uint64_t>();
                if (sample_rate >= cur_index_sample_rate)
                {
                    _sample_rate.setCurrentIndex(i);
                    break;
                }
            }
            commit_settings();
        }

        void SamplingBar::update_sample_rate_selector()
        { 
            GVariant *gvar_dict, *gvar_list;
            const uint64_t *elements = NULL;
            gsize num_elements;

            dsv_info("Update rate list.");

            if (_updating_sample_rate)
            {
                dsv_err("Error! The rate list is updating.");
                return;
            }

            disconnect(&_sample_rate, SIGNAL(currentIndexChanged(int)),
                       this, SLOT(on_samplerate_sel(int)));

            if (_device_agent->have_instance() == false)
            {
                dsv_info("SamplingBar::update_sample_rate_selector, have no device.");
                return;
            }

            _updating_sample_rate = true;

            gvar_dict = _device_agent->get_config_list(NULL, SR_CONF_SAMPLERATE);
            if (gvar_dict == NULL)
            {
                _sample_rate.clear();
                _sample_rate.show();
                _updating_sample_rate = false;
                return;
            }

            if ((gvar_list = g_variant_lookup_value(gvar_dict,
                                                    "samplerates", G_VARIANT_TYPE("at"))))
            {
                elements = (const uint64_t *)g_variant_get_fixed_array(
                    gvar_list, &num_elements, sizeof(uint64_t));
                _sample_rate.clear();

                for (unsigned int i = 0; i < num_elements; i++)
                {
                    char *const s = sr_samplerate_string(elements[i]);
                    _sample_rate.addItem(QString(s),
                                         QVariant::fromValue(elements[i]));
                    g_free(s);
                }

                _sample_rate.show();
                g_variant_unref(gvar_list);
            }

            _sample_rate.setMinimumWidth(_sample_rate.sizeHint().width() + 15);
            _sample_rate.view()->setMinimumWidth(_sample_rate.sizeHint().width() + 30);

            _updating_sample_rate = false;
            g_variant_unref(gvar_dict);

            update_sample_rate_selector_value();

            connect(&_sample_rate, SIGNAL(currentIndexChanged(int)),
                    this, SLOT(on_samplerate_sel(int)));

            update_sample_count_selector();
        }

        void SamplingBar::update_sample_rate_selector_value()
        {
            if (_updating_sample_rate)
                return;
            _updating_sample_rate = true;

            const uint64_t samplerate = _device_agent->get_sample_rate();
            uint64_t cur_value = _sample_rate.itemData(_sample_rate.currentIndex()).value<uint64_t>();

            if (samplerate != cur_value)
            {
                for (int i = _sample_rate.count() - 1; i >= 0; i--)
                {
                    if (samplerate >= _sample_rate.itemData(i).value<uint64_t>())
                    {
                        _sample_rate.setCurrentIndex(i);
                        break;
                    }
                }
            }

            _updating_sample_rate = false;
        }

        void SamplingBar::on_samplerate_sel(int index)
        {
            (void)index;
            if (_device_agent->get_work_mode() != DSO)
                update_sample_count_selector();
        }

        void SamplingBar::update_sample_count_selector()
        {
            bool stream_mode = false;
            uint64_t hw_depth = 0;
            uint64_t sw_depth;
            uint64_t rle_depth = 0;
            uint64_t max_timebase = 0;
            uint64_t min_timebase = SR_NS(10);
            double pre_duration = SR_SEC(1);
            double duration;
            bool rle_support = false;

            dsv_info("Update sample count list.");

            if (_updating_sample_count)
            {
                dsv_err("Error! The sample count is updating.");
                return;
            }

            disconnect(&_sample_count, SIGNAL(currentIndexChanged(int)),
                       this, SLOT(on_samplecount_sel(int)));

            assert(!_updating_sample_count);
            _updating_sample_count = true;

            _device_agent->get_config_bool(SR_CONF_STREAM, stream_mode);
            _device_agent->get_config_uint64(SR_CONF_HW_DEPTH, hw_depth);
            int mode = _device_agent->get_work_mode();

            if (mode == LOGIC)
            {
#if defined(__x86_64__) || defined(_M_X64)
                sw_depth = LogicMaxSWDepth64;
#elif defined(__i386) || defined(_M_IX86)
                int ch_num = _session->get_ch_num(SR_CHANNEL_LOGIC);
                if (ch_num <= 0)
                    sw_depth = LogicMaxSWDepth32;
                else
                    sw_depth = LogicMaxSWDepth32 / ch_num;
#endif
            }
            else
            {
                sw_depth = AnalogMaxSWDepth;
            }

            if (mode == LOGIC)
            {
                _device_agent->get_config_bool(SR_CONF_RLE_SUPPORT, rle_support);
                if (rle_support)
                    rle_depth = min(hw_depth * SR_KB(1), sw_depth);
            }
            else if (mode == DSO)
            {
                _device_agent->get_config_uint64(SR_CONF_MAX_TIMEBASE, max_timebase);
                _device_agent->get_config_uint64(SR_CONF_MIN_TIMEBASE, min_timebase);
            }

            if (0 != _sample_count.count())
                pre_duration = _sample_count.itemData(
                                                _sample_count.currentIndex())
                                   .value<double>();
            _sample_count.clear();
            const uint64_t samplerate = _sample_rate.itemData(
                                                        _sample_rate.currentIndex())
                                            .value<uint64_t>();
            const double hw_duration = hw_depth / (samplerate * (1.0 / SR_SEC(1)));

            if (mode == DSO)
                duration = max_timebase;
            // else if (stream_mode) //取消流模式软件buff大小限制
            //     duration = sw_depth / (samplerate * (1.0 / SR_SEC(1)));
            else if (rle_support)
                duration = rle_depth / (samplerate * (1.0 / SR_SEC(1)));
            else
                duration = hw_duration;

            assert(duration > 0);
            bool not_last = true;

            do
            {
                QString suffix = (mode == DSO) ? DIVString : (!stream_mode && duration > hw_duration) ? RLEString
                                                                                                      : "";
                char *const s = sr_time_string(duration);
                _sample_count.addItem(QString(s) + suffix, QVariant::fromValue(duration));
                g_free(s);

                double unit;
                if (duration >= SR_DAY(1))
                    unit = SR_DAY(1);
                else if (duration >= SR_HOUR(1))
                    unit = SR_HOUR(1);
                else if (duration >= SR_MIN(1))
                    unit = SR_MIN(1);
                else
                    unit = 1;

                const double log10_duration = pow(10, floor(log10(duration / unit)));

                if (duration > 5 * log10_duration * unit)
                    duration = 5 * log10_duration * unit;
                else if (duration > 2 * log10_duration * unit)
                    duration = 2 * log10_duration * unit;
                else if (duration > log10_duration * unit)
                    duration = log10_duration * unit;
                else
                    duration = log10_duration > 1 ? duration * 0.5 : (unit == SR_DAY(1) ? SR_HOUR(20) : unit == SR_HOUR(1) ? SR_MIN(50)
                                                                                                    : unit == SR_MIN(1)    ? SR_SEC(50)
                                                                                                                           : duration * 0.5);

                if (mode == DSO)
                    not_last = duration >= min_timebase;
                else if (mode == ANALOG)
                    not_last = (duration >= SR_MS(200)) &&
                               (duration / SR_SEC(1) * samplerate >= SR_KB(1));
                else
                    not_last = (duration / SR_SEC(1) * samplerate >= SR_KB(1));

            } while (not_last);

            _updating_sample_count = true;

            if (pre_duration > _sample_count.itemData(0).value<double>())
            {
                set_sample_count_index(0);
            }
            else if (pre_duration < _sample_count.itemData(_sample_count.count() - 1).value<double>())
            {
                set_sample_count_index(_sample_count.count() - 1);
            }
            else
            {
                for (int i = 0; i < _sample_count.count(); i++)
                {
                    double sel_val = _sample_count.itemData(i).value<double>();
                    if (pre_duration >= sel_val)
                    {
                        set_sample_count_index(i);
                        break;
                    }
                }
            }
            _updating_sample_count = false;

            update_sample_count_selector_value();
            on_samplecount_sel(_sample_count.currentIndex());

            connect(&_sample_count, SIGNAL(currentIndexChanged(int)), this, SLOT(on_samplecount_sel(int)));
        }

        void SamplingBar::update_sample_count_selector_value()
        {
            if (_updating_sample_count)
                return;

            double duration;
            uint64_t v;

            if (_device_agent->get_work_mode() == DSO)
            { 
                if (_device_agent->get_config_uint64(SR_CONF_TIMEBASE, v))
                {
                    duration = (double)v; 
                }
                else
                {
                    dsv_err("ERROR: config_get SR_CONF_TIMEBASE failed.");
                    return;
                }
            }
            else
            { 
                if (_device_agent->get_config_uint64(SR_CONF_LIMIT_SAMPLES, v))
                {
                    duration = (double)v; 
                }
                else
                {
                    dsv_err("ERROR: config_get SR_CONF_TIMEBASE failed.");
                    return;
                }
                const uint64_t samplerate = _device_agent->get_sample_rate();
                duration = duration / samplerate * SR_SEC(1);
            }
            assert(!_updating_sample_count);
            _updating_sample_count = true;

            double cur_duration = _sample_count.itemData(_sample_count.currentIndex()).value<double>();
            if (duration != cur_duration)
            {
                for (int i = 0; i < _sample_count.count(); i++)
                {
                    double sel_val = _sample_count.itemData(i).value<double>();
                    if (duration >= sel_val)
                    {
                        set_sample_count_index(i);
                        break;
                    }
                }
            }

            _updating_sample_count = false;
        }

        void SamplingBar::apply_sample_count(double &hori_res)
        {   
            hori_res = -1;

            if (_device_agent->get_work_mode() == DSO){
                hori_res = commit_hori_res();

                if (_session->have_view_data() == false){
                    _session->apply_samplerate();
                }
            }

            _session->broadcast_msg(DSV_MSG_DEVICE_DURATION_UPDATED);
        }

        void SamplingBar::on_samplecount_sel(int index)
        {
            (void)index;

            double hori_res = -1;
            apply_sample_count(hori_res);
        }

        double SamplingBar::get_hori_res()
        {
            return _sample_count.itemData(_sample_count.currentIndex()).value<double>();
        }

        double SamplingBar::hori_knob(int dir)
        {
            double hori_res = -1;

            if (_session->get_device()->get_work_mode() != DSO){
                assert(false);
            }

            disconnect(&_sample_count, SIGNAL(currentIndexChanged(int)), this, SLOT(on_samplecount_sel(int)));

            if (0 == dir)
            {
                hori_res = commit_hori_res();
            }
            else if ((dir > 0) && (_sample_count.currentIndex() > 0))
            {
                set_sample_count_index(_sample_count.currentIndex() - 1);
                hori_res = commit_hori_res();

                if (_session->have_view_data() == false){                   
                    _session->apply_samplerate(); 
                    _session->broadcast_msg(DSV_MSG_DEVICE_DURATION_UPDATED);                 
                }
            }
            else if ((dir < 0) && (_sample_count.currentIndex() < _sample_count.count() - 1))
            {
                set_sample_count_index(_sample_count.currentIndex() + 1); 
                hori_res = commit_hori_res();             

                if (_session->have_view_data() == false){
                    _session->apply_samplerate();
                    _session->broadcast_msg(DSV_MSG_DEVICE_DURATION_UPDATED);
                }
            }

            connect(&_sample_count, SIGNAL(currentIndexChanged(int)),
                    this, SLOT(on_samplecount_sel(int)));

            return hori_res;
        }

        double SamplingBar::commit_hori_res()
        {
            const double hori_res = _sample_count.itemData(
                                                     _sample_count.currentIndex())
                                        .value<double>();

            const uint64_t sample_limit = _device_agent->get_sample_limit();
            uint64_t max_sample_rate;

            if (_device_agent->get_config_uint64(SR_CONF_MAX_DSO_SAMPLERATE, max_sample_rate) == false)
            {
                dsv_err("ERROR: config_get SR_CONF_MAX_DSO_SAMPLERATE failed.");
                return -1;
            }

            const uint64_t sample_rate = min((uint64_t)(sample_limit * SR_SEC(1) /
                                                        (hori_res * DS_CONF_DSO_HDIVS)),
                                             (uint64_t)(max_sample_rate /
                                                        (_session->get_ch_num(SR_CHANNEL_DSO) ? _session->get_ch_num(SR_CHANNEL_DSO) : 1)));
            set_sample_rate(sample_rate);

            _device_agent->set_config_uint64( SR_CONF_TIMEBASE, hori_res);

            return hori_res;
        }

        void SamplingBar::commit_settings()
        {
            bool test = false;
            if (_device_agent->have_instance())
            {
                _device_agent->get_config_bool(SR_CONF_TEST, test);
            }

            if (test)
            {
                update_sample_rate_selector_value();
                update_sample_count_selector_value();
            }
            else
            {
                const double sample_duration = _sample_count.itemData(
                                                                _sample_count.currentIndex())
                                                   .value<double>();
                const uint64_t sample_rate = _sample_rate.itemData(
                                                             _sample_rate.currentIndex())
                                                 .value<uint64_t>();

                if (_device_agent->have_instance())
                {
                    if (sample_rate != _device_agent->get_sample_rate())
                        _device_agent->set_config_uint64(
                                                  SR_CONF_SAMPLERATE,
                                                  sample_rate);

                    if (_device_agent->get_work_mode() != DSO)
                    {
                        const uint64_t sample_count = ((uint64_t)ceil(sample_duration / SR_SEC(1) *
                                                                      sample_rate) +
                                                       SAMPLES_ALIGN) &
                                                      ~SAMPLES_ALIGN;
                        if (sample_count != _device_agent->get_sample_limit())
                            _device_agent->set_config_uint64(
                                                      SR_CONF_LIMIT_SAMPLES,
                                                      sample_count);

                        bool rle_mode = _sample_count.currentText().contains(RLEString);
                        _device_agent->set_config_bool(
                                                  SR_CONF_RLE,
                                                  rle_mode);
                    }
                }
            }
        }

        void SamplingBar::on_run_stop()
        {
            _run_stop_button.setEnabled(false);
            QTimer::singleShot(10, this, &SamplingBar::on_run_stop_action);
        }

        void SamplingBar::on_run_stop_action()
        {
            action_run_stop();
            _run_stop_button.setEnabled(true);
        }
      
        // start or stop capture
        bool SamplingBar::action_run_stop()
        {    
            if (_is_readonly)
                return false;

            if (_session->is_doing_action()){
                dsv_info("Task is busy.");              
                return false;
            }

            if (_session->is_working()){
                return _session->stop_capture();
            }

            if (_device_agent->have_instance() == false)
            {
                dsv_info("Have no device, can't to collect data.");
                return false;
            }

            commit_settings();

            if (_device_agent->get_work_mode() == DSO)
            {
                bool zero;

                bool ret = _device_agent->get_config_bool(SR_CONF_ZERO, zero);
                if (ret && zero)
                {   
                    QString str1(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_AUTO_CALIB), "Auto Calibration"));
                    QString str2(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_ADJUST_SAVE), "Please adjust zero skew and save the result"));
                    bool bRet = MsgBox::Confirm(str1, str2);

                    if (bRet)
                    {
                        zero_adj();
                    }
                    else
                    {
                        _device_agent->set_config_bool(SR_CONF_ZERO, false);
                        update_view_status();
                    }
                    return false;                    
                }
            }

            if (_device_agent->get_work_mode() == LOGIC && _view != NULL){
                if (_session->is_realtime_refresh())
                    _view->auto_set_max_scale();
            }

            _is_run_as_instant = false;
            bool ret = _session->start_capture(false);

            return ret;
        }

        void SamplingBar::on_instant_stop()
        {
            if (_instant_action->isVisible() == false){
                return;
            }
            _instant_button.setEnabled(false);
            QTimer::singleShot(10, this, &SamplingBar::on_instant_stop_action);
        }

        void SamplingBar::on_instant_stop_action()
        {
            action_instant_stop();
            _instant_button.setEnabled(true);
        }

        bool SamplingBar::action_instant_stop()
        { 
            if (_is_readonly)
                return false;

            if (_session->is_doing_action()){
                dsv_info("Task is busy.");
                return false;
            }

            if (_session->is_working()){ 
                return _session->stop_capture();
            }            
            
            if (_device_agent->have_instance() == false)
            {
                dsv_info("Error! Have no device, can't to collect data.");
                return false;
            }

            commit_settings();

            if (_device_agent->get_work_mode() == DSO)
            {
                bool zero;

                bool ret = _device_agent->get_config_bool(SR_CONF_ZERO, zero);
                if (ret && zero)
                {  
                    QString strMsg(L_S(STR_PAGE_MSG,S_ID(IDS_MSG_AUTO_CALIB_START), "Auto Calibration program will be started. Don't connect any probes. \nIt can take a while!"));

                    if (MsgBox::Confirm(strMsg))
                    {
                        zero_adj();
                    }
                    else
                    {
                        _device_agent->set_config_bool(SR_CONF_ZERO, false);
                        update_view_status();
                    }
                    return false;            
                }
            }

            if (_device_agent->get_work_mode() == LOGIC && _session->is_realtime_refresh()){
                if (_view != NULL)
                    _view->auto_set_max_scale();
            }
            
            _is_run_as_instant = true;
            bool ret = _session->start_capture(true);

            return ret;  
        }

        void SamplingBar::on_device_selected()
        {
            if (_updating_device_list)
            {
                return;
            }
            if (_device_selector.currentIndex() == -1)
            {
                dsv_err("Have no selected device.");
                return;
            }
            _session->stop_capture();
            _session->session_save();

            ds_device_handle devHandle = (ds_device_handle)_device_selector.currentData().toULongLong();
            if (_session->have_hardware_data() && _session->is_first_store_confirm()){
                if (MsgBox::Confirm(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_SAVE_CAPDATE), "Save captured data?")))
                {
                    _updating_device_list = true;
                    _device_selector.setCurrentIndex(_last_device_index);
                    _updating_device_list = false;
                    _next_switch_device = devHandle; // Save end, auto switch to this device.
                    sig_store_session_data();
                    return;
                }
            }

            if (_session->set_device(devHandle)){
                _last_device_index = _device_selector.currentIndex();   
            }
            else{
                update_device_list(); // Reload the list.
            }
        }

        void SamplingBar::enable_toggle(bool enable)
        {
            bool test = false;

            if (_device_agent->have_instance())
            {
                _device_agent->get_config_bool(SR_CONF_TEST, test);
            }
            if (!test)
            {
                _sample_count.setDisabled(!enable);

                if (_device_agent->get_work_mode() == DSO)
                    _sample_rate.setDisabled(true);
                else
                    _sample_rate.setDisabled(!enable);
            }
            else
            {
                _sample_count.setDisabled(true);
                _sample_rate.setDisabled(true);
            }
        }

        void SamplingBar::reload()
        {
            QString iconPath = GetIconPath();

            bool show_mode_row = false;
            bool show_loop = false;
            int mode = _device_agent->get_work_mode();

            if (mode == LOGIC)
            {
                if (!_device_agent->is_file()) {
                    show_mode_row = true;
                    if (_device_agent->is_stream_mode() || _device_agent->is_demo())
                        show_loop = true;

                    if (_session->is_loop_mode() && !_device_agent->is_stream_mode()
                        && _device_agent->is_hardware()) {
                        _session->set_collect_mode(COLLECT_SINGLE);
                    }
                }
                _run_stop_action->setVisible(true);
                _instant_action->setVisible(true);
            }
            else if (mode == ANALOG)
            {
                _run_stop_action->setVisible(true);
                _instant_action->setVisible(false);
            }
            else if (mode == DSO)
            {
                _run_stop_action->setVisible(true);
                _instant_action->setVisible(true);
            }

            if (_radio_single) {
                _radio_single->setVisible(show_mode_row);
                _radio_repeat->setVisible(show_mode_row);
                _radio_loop->setVisible(show_mode_row && show_loop);
                QLabel *ml = _radio_single->parentWidget()->findChild<QLabel*>("mode_label");
                if (ml) ml->setVisible(show_mode_row);

                if (show_mode_row) {
                    int cur_mode = _session->get_collect_mode();
                    if (cur_mode == COLLECT_SINGLE) _radio_single->setChecked(true);
                    else if (cur_mode == COLLECT_REPEAT) _radio_repeat->setChecked(true);
                    else if (cur_mode == COLLECT_LOOP) _radio_loop->setChecked(true);
                }
            }

            retranslateUi();
            reStyle();
            update();
        }

        void SamplingBar::on_mode_radio_clicked(int id)
        {
            if (_is_readonly)
                return;

            switch (id) {
            case COLLECT_SINGLE:
                _session->set_collect_mode(COLLECT_SINGLE);
                if (_device_agent->is_demo()) {
                    _device_agent->set_config_string(SR_CONF_PATTERN_MODE, "protocol");
                    _session->broadcast_msg(DSV_MSG_DEMO_OPERATION_MODE_CHNAGED);
                }
                break;
            case COLLECT_REPEAT:
                if (_device_agent->is_stream_mode() || _device_agent->is_demo()) {
                    _session->set_repeat_intvl(0.1);
                    _session->set_collect_mode(COLLECT_REPEAT);
                } else {
                    pv::dialogs::Interval interval_dlg(this);
                    interval_dlg.set_interval(_session->get_repeat_intvl());
                    interval_dlg.exec();
                    if (interval_dlg.is_done()) {
                        _session->set_repeat_intvl(interval_dlg.get_interval());
                        _session->set_collect_mode(COLLECT_REPEAT);
                    } else {
                        return;
                    }
                }
                if (_device_agent->is_demo()) {
                    _device_agent->set_config_string(SR_CONF_PATTERN_MODE, "random");
                    _session->broadcast_msg(DSV_MSG_DEMO_OPERATION_MODE_CHNAGED);
                }
                break;
            case COLLECT_LOOP:
                _session->set_collect_mode(COLLECT_LOOP);
                if (_device_agent->is_demo()) {
                    _device_agent->set_config_string(SR_CONF_PATTERN_MODE, "random");
                    _session->broadcast_msg(DSV_MSG_DEMO_OPERATION_MODE_CHNAGED);
                }
                break;
            }
        }

        void SamplingBar::on_collect_mode()
        {
            QString iconPath = GetIconPath();
            QAction *act = qobject_cast<QAction *>(sender());

            if (act == _action_single)
            {  
                _session->set_collect_mode(COLLECT_SINGLE);

                if (_device_agent->is_demo()){
                    _device_agent->set_config_string(SR_CONF_PATTERN_MODE, "protocol");
                    _session->broadcast_msg(DSV_MSG_DEMO_OPERATION_MODE_CHNAGED);
                }
            }
            else if (act == _action_repeat)
            { 
                if (_device_agent->is_stream_mode() || _device_agent->is_demo())
                {
                    _session->set_repeat_intvl(0.1);
                    _session->set_collect_mode(COLLECT_REPEAT);
                }
                else{
                    pv::dialogs::Interval interval_dlg(this);

                    interval_dlg.set_interval(_session->get_repeat_intvl());
                    interval_dlg.exec();

                    if (interval_dlg.is_done())
                    {
                        _session->set_repeat_intvl(interval_dlg.get_interval());
                        _session->set_collect_mode(COLLECT_REPEAT);
                       
                    }
                }

                if (_device_agent->is_demo()){
                    _device_agent->set_config_string(SR_CONF_PATTERN_MODE, "random");
                    _session->broadcast_msg(DSV_MSG_DEMO_OPERATION_MODE_CHNAGED);
                }          
            }
            else if (act == _action_loop)
            {  
                _session->set_collect_mode(COLLECT_LOOP);

                if (_device_agent->is_demo()){
                    _device_agent->set_config_string(SR_CONF_PATTERN_MODE, "random");
                    _session->broadcast_msg(DSV_MSG_DEMO_OPERATION_MODE_CHNAGED);
                }
            }

            update_mode_icon();
        }

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
            _device_selector.setFixedWidth(min(width + 15, _device_selector.maximumWidth()));
            _device_selector.view()->setMinimumWidth(width + 30);

            _updating_device_list = false;
        }

        void SamplingBar::config_device()
        {
            if (_configure_button.isVisible() && _configure_button.isEnabled()){
                _configure_button.setChecked(true);
                emit sig_device_options(true);
            }
        }

        void SamplingBar::update_view_status()
        {
            int bEnable = _session->is_working() == false;
            int mode = _session->get_device()->get_work_mode();

            _device_type.setEnabled(bEnable);
            _configure_button.setEnabled(bEnable);
            _device_selector.setEnabled(bEnable);

            if (_radio_single) {
                _radio_single->setEnabled(bEnable);
                _radio_repeat->setEnabled(bEnable);
                _radio_loop->setEnabled(bEnable);
                _radio_loop->setVisible(false);
            }

            if (_session->get_device()->is_file()){
                _sample_rate.setEnabled(false);
                _sample_count.setEnabled(false);
            }
            else if (mode == DSO){
                _sample_rate.setEnabled(false);
                _sample_count.setEnabled(bEnable);

                if (_session->is_working() && _session->is_instant() == false)
                {
                    _sample_count.setEnabled(true);
                }
            }
            else{
                _sample_rate.setEnabled(bEnable);
                _sample_count.setEnabled(bEnable);

                if (mode == LOGIC && _session->get_device()->is_hardware())
                {
                    int mode_val = 0;
                    if (_session->get_device()->get_config_int16(SR_CONF_OPERATION_MODE, mode_val)){
                        if (mode_val == LO_OP_INTEST){
                            _sample_rate.setEnabled(false);
                            _sample_count.setEnabled(false);
                        }
                    }
                }

                if (mode == LOGIC && _device_agent->is_file() == false){
                    if (_device_agent->is_stream_mode() || _device_agent->is_demo())
                        if (_radio_loop) _radio_loop->setVisible(true);
                }
            }

            if (_session->is_working()){
                if (_is_run_as_instant)
                    _run_stop_button.setEnabled(false);
                else
                    _instant_button.setEnabled(false);
            } else {
                _run_stop_button.setEnabled(true);
                _instant_button.setEnabled(true);                
            }
 
            QString iconPath = GetIconPath();

            if (_is_run_as_instant)
                _instant_button.setIcon(!bEnable ? QIcon(iconPath + "/stop.svg") : QIcon(iconPath + "/single.svg"));
            else
                _run_stop_button.setIcon(!bEnable ? QIcon(iconPath + "/stop.svg") : QIcon(iconPath + "/start.svg"));
 
            retranslateUi();

            if (bEnable){
                _is_run_as_instant = false;
            }

            update_mode_icon(); 

            if (_session->get_device()->is_demo() && bEnable)
            {
                QString opt_mode = _device_agent->get_demo_operation_mode();
                
                if (opt_mode != "random" && mode == LOGIC){
                    _sample_rate.setEnabled(false);
                    _sample_count.setEnabled(false);
                }
            }
        }

        ds_device_handle SamplingBar::get_next_device_handle()
        {
            ds_device_handle h = _next_switch_device;
            _next_switch_device = NULL_HANDLE;
            return h;
        }

        void SamplingBar::update_mode_icon()
        {  
            QString iconPath = GetIconPath();

            if (_session->is_repeat_mode())
                _mode_button.setIcon(QIcon(iconPath + REPEAT_ACTION_ICON));
            else if (_session->is_loop_mode())
                _mode_button.setIcon(QIcon(iconPath + LOOP_ACTION_ICON));
            else
                _mode_button.setIcon(QIcon(iconPath + SINGLE_ACTION_ICON));
        }

        void SamplingBar::run_or_stop()
        {
            on_run_stop();
        }

        void SamplingBar::run_or_stop_instant()
        {
            on_instant_stop();
        }

        void SamplingBar::UpdateLanguage()
        {
            retranslateUi();
        }

        void SamplingBar::UpdateTheme()
        {
            reStyle();
        }

        void SamplingBar::UpdateFont()
        {  
            QFont font = this->font();
            font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);
            ui::set_toolbar_font(this, font);

            update_view_status();
        }

        void SamplingBar::device_selected()
        {
            _mode_button.click();
        }


        void SamplingBar::set_context(SigSession *session, pv::view::View *view)
        {
            _session = session;
            _device_agent = _session->get_device();
            _view = view;
            update_device_list();
            update_sample_rate_list();
        }

        void SamplingBar::set_readonly(bool readonly)
        {
            _is_readonly = readonly;

            _run_stop_button.setEnabled(!readonly);
            _instant_button.setEnabled(!readonly);
            _device_selector.setEnabled(!readonly);
            _sample_rate.setEnabled(!readonly);
            _sample_count.setEnabled(!readonly);
            _configure_button.setEnabled(!readonly);
            _mode_button.setEnabled(!readonly);
        }

        void SamplingBar::set_sample_count_index(int index)
        {
            _sample_count.setCurrentIndex(index);
        }

    } // namespace toolbars
} // namespace pv
