/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
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

#include "deviceoptionsdock.h"

#include <QListWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QGuiApplication>
#include <QScreen>
#include <QScrollArea>
#include <QLayoutItem>
#include <QGroupBox>
#include <QTabWidget>
#include <QRadioButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QPushButton>
#include <QHideEvent>
#include <assert.h>

#include "../prop/property.h"
#include "../dsvdef.h"
#include "../config/appconfig.h"
#include "../appcontrol.h"
#include "../sigsession.h"
#include "../ui/langresource.h"
#include "../log.h"
#include "../ui/msgbox.h"
#include "../ui/fn.h"
#include "../interface/icallbacks.h"
#include "../tabcontext.h"
#include "../data/sessiondocument.h"

using namespace boost;
using namespace std;

namespace pv {
namespace dock {

DeviceOptionsDock::DeviceOptionsDock(QWidget *parent, SigSession *session) :
    QScrollArea(parent),
    _session(session),
    _context(nullptr)
{
    _scroll_panel = NULL;
    _scroll = this;
    _container_panel = NULL;
    _container_lay = NULL;
    _dynamic_panel = NULL;
    _width = 0;
    _groupHeight1 = 0;
    _groupHeight2 = 0;
    _isBuilding = false;
    _cur_analog_tag_index = 0;
    _opt_mode = 0;

    _device_agent = session->get_device();
    _device_options_binding = NULL;

    this->setWidgetResizable(true);
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setStyleSheet("QScrollArea{border:none;}");

    _container_panel = new QWidget();
    _container_lay = new QVBoxLayout();
    _container_lay->setDirection(QBoxLayout::TopToBottom);
    _container_lay->setAlignment(Qt::AlignTop);
    _container_lay->setContentsMargins(0, 0, 0, 0);
    _container_lay->setSpacing(5);
    _container_panel->setLayout(_container_lay);

    this->setWidget(_container_panel);

    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);

    if (_device_agent->have_instance()) {
        _device_options_binding = new pv::prop::binding::DeviceOptions();

        QGroupBox *props_box = new QGroupBox(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_MODE), "Mode"), _container_panel);
        props_box->setFont(font);
        props_box->setMinimumHeight(70);
        props_box->setAlignment(Qt::AlignTop);
        QLayout *props_lay = get_property_form(props_box);
        props_lay->setContentsMargins(5, 20, 5, 5);
        props_box->setLayout(props_lay);
        _container_lay->addWidget(props_box);

        QWidget *minWid = new QWidget();
        minWid->setFixedHeight(1);
        minWid->setMinimumWidth(230);
        _container_lay->addWidget(minWid);

        this->build_dynamic_panel();

        _device_agent->get_config_int16(SR_CONF_OPERATION_MODE, _opt_mode);

        if (_device_agent->is_demo())
            _demo_operation_mode = _device_agent->get_demo_operation_mode();

        try_resize_scroll();
    }

    connect(&_mode_check_timer, SIGNAL(timeout()), this, SLOT(mode_check_timeout()));

    _mode_check_timer.setInterval(100);
}

DeviceOptionsDock::~DeviceOptionsDock()
{
    for (auto ptr : _probe_options_binding_list) {
        const auto &props = ptr->properties();
        for (auto p : props) {
            delete p;
        }
        delete ptr;
    }
    _probe_options_binding_list.clear();

    if (_device_options_binding) {
        const auto &dev_props = _device_options_binding->properties();
        for (auto p : dev_props) {
            delete p;
        }
        delete _device_options_binding;
        _device_options_binding = NULL;
    }
}

void DeviceOptionsDock::ChannelChecked(int index, QObject *object)
{
    (void)index;

    QCheckBox* sc = dynamic_cast<QCheckBox*>(object);
    channel_checkbox_clicked(sc);
    commit_channels();
}

void DeviceOptionsDock::on_property_committed()
{
    emit settings_applied();
}

void DeviceOptionsDock::commit_channels()
{
    using namespace Qt;
    bool hasEnabled = false;

    int mode = _device_agent->get_work_mode();
    if (mode == LOGIC || mode == ANALOG) {
        int index = 0;
        for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
            sr_channel *const probe = (sr_channel*)l->data;
            assert(probe);
            probe->enabled = _probes_checkBox_list.at(index)->isChecked();
            index++;
            if (probe->enabled)
                hasEnabled = true;
        }
    }
    else {
        hasEnabled = true;
    }

    if (hasEnabled) {
        auto it = _probe_options_binding_list.begin();
        while(it != _probe_options_binding_list.end()) {
            const auto &probe_props = (*it)->properties();

            for(auto p : probe_props) {
                p->commit();
            }
            it++;
        }

        QTimer::singleShot(0, this, [this](){
            _session->broadcast_msg(DSV_MSG_DEVICE_OPTIONS_UPDATED);
            _session->broadcast_msg(DSV_MSG_END_DEVICE_OPTIONS);
            emit settings_applied();
        });
    }
    else {
        QString strMsg(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_ALL_CHANNEL_DISABLE), "All channel disabled! Please enable at least one channel."));
        MsgBox::Show(strMsg);
    }
}

QLayout * DeviceOptionsDock::get_property_form(QWidget * parent)
{
    QGridLayout *const layout = new QGridLayout(parent);
    layout->setVerticalSpacing(2);
    const auto &properties = _device_options_binding->properties();

    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);

    int i = 0;
    for(auto p : properties)
    {
        const QString label = p->labeled_widget() ? QString() : p->label();
        QString lable_text = "";

        if (label != ""){
            QByteArray bytes = label.toLocal8Bit();
            const char *lang_str = LangResource::Instance()->get_lang_text(STR_PAGE_DSL, bytes.data(), bytes.data());
            lable_text = QString(lang_str);
        }

        QLabel *lb = new QLabel(lable_text, parent);
        lb->setFont(font);
        layout->addWidget(lb, i, 0);

        if (label ==  QString("Operation Mode")){
            QWidget *wid = p->get_widget(parent, true);
            wid->setFont(font);
            layout->addWidget(wid, i, 1);
        }
        else{
            QWidget *wid = p->get_widget(parent, true);
            wid->setFont(font);
            layout->addWidget(wid, i, 1);
        }
        layout->setRowMinimumHeight(i, 22);

        connect(p, &pv::prop::Property::committed, this, &DeviceOptionsDock::on_property_committed);

        i++;
    }

    return layout;
}

void DeviceOptionsDock::logic_probes(QVBoxLayout &layout)
{
    using namespace Qt;

    layout.setSpacing(2);

    int row1 = 0;
    int row2 = 0;
    int vld_ch_num = 0;
    int cur_ch_num = 0;
    int contentHeight = 0;

    _probes_checkBox_list.clear();

    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);

    if (_device_agent->get_work_mode()== LOGIC) {
        GVariant * gvar_opts = _device_agent->get_config_list(NULL, SR_CONF_CHANNEL_MODE);

        if (gvar_opts != NULL)
        {
            struct sr_list_item *plist = (struct sr_list_item*)g_variant_get_uint64(gvar_opts);
            g_variant_unref(gvar_opts);

            int ch_mode = 0;
            _device_agent->get_config_int16(SR_CONF_CHANNEL_MODE, ch_mode);
            _channel_mode_indexs.clear();

            while (plist != NULL && plist->id >= 0)
            {
                row1++;
                QString mode_bt_text = LangResource::Instance()->get_lang_text(STR_PAGE_DSL, plist->name, plist->name);
                QRadioButton *mode_button = new QRadioButton(mode_bt_text);
                mode_button->setFont(font);
                ChannelModePair mode_index;
                mode_index.key = mode_button;
                mode_index.value = plist->id;
                _channel_mode_indexs.push_back(mode_index);

                layout.addWidget(mode_button);
                contentHeight += mode_button->sizeHint().height();

                connect(mode_button, SIGNAL(pressed()), this, SLOT(channel_check()));

                if (plist->id == ch_mode)
                    mode_button->setChecked(true);

                plist++;
            }
        }
    }

    _device_agent->get_config_int16(SR_CONF_VLD_CH_NUM, vld_ch_num);

    QWidget *channel_pannel = new QWidget();
    QGridLayout *channel_grid = new QGridLayout();
    channel_grid->setContentsMargins(0,0,0,0);
    channel_grid->setSpacing(0);
    channel_pannel->setLayout(channel_grid);

    int channel_row = 0;
    int channel_column = 0;
    int channel_line_height = 0;
    row2++;

    for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
        sr_channel *const probe = (sr_channel*)l->data;

        if (probe->enabled)
            cur_ch_num++;

        if (cur_ch_num > vld_ch_num)
            probe->enabled = false;

        ChannelLabel *ch_item = new ChannelLabel(this, NULL, probe->index);
        channel_grid->addWidget(ch_item, channel_row, channel_column++, Qt::AlignCenter);
        _probes_checkBox_list.push_back(ch_item->getCheckBox());
        ch_item->getCheckBox()->setCheckState(probe->enabled ? Qt::Checked : Qt::Unchecked);
        channel_line_height = ch_item->height();

         if (channel_column == 8){
            channel_column = 0;
            channel_row++;

            if (l->next != NULL){
                row2++;
            }
         }
    }

    layout.addWidget(channel_pannel);

    QWidget *space = new QWidget();
    space->setFixedHeight(10);
    layout.addWidget(space);
    contentHeight += 10;

    QHBoxLayout *line_lay = new QHBoxLayout();
    layout.addLayout(line_lay);
    line_lay->setSpacing(10);

    QPushButton *enable_all_probes = new QPushButton(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_ENABLE_ALL), "Enable All"));
    QPushButton *disable_all_probes = new QPushButton(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_DISABLE_ALL), "Disable All"));
    enable_all_probes->setMaximumHeight(33);
    disable_all_probes->setMaximumHeight(33);
    enable_all_probes->setFont(font);
    disable_all_probes->setFont(font);

    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        int bt_width = enable_all_probes->fontMetrics().horizontalAdvance(enable_all_probes->text()) + 20;
    #else
        int bt_width = enable_all_probes->fontMetrics().width(enable_all_probes->text()) + 20;
    #endif

    enable_all_probes->setMaximumWidth(bt_width);
    disable_all_probes->setMaximumWidth(bt_width);

    ui::set_form_font(this, font);

    contentHeight += enable_all_probes->sizeHint().height();
    contentHeight += channel_line_height * row2 + 50;

    connect(enable_all_probes, SIGNAL(clicked()),
            this, SLOT(enable_all_probes()));
    connect(disable_all_probes, SIGNAL(clicked()),
            this, SLOT(disable_all_probes()));

    line_lay->addWidget(enable_all_probes);
    line_lay->addWidget(disable_all_probes);

    _groupHeight2 = contentHeight + (row1 + row2) * 2 + 38;

#ifdef Q_OS_DARWIN
    _groupHeight2 += 5;
#endif
}

void DeviceOptionsDock::set_all_probes(bool set)
{
    for (auto box : _probes_checkBox_list) {
        box->setCheckState(set ? Qt::Checked : Qt::Unchecked);
    }
}

void DeviceOptionsDock::enable_max_probes() {
    int cur_ch_num = 0;
    for (auto box : _probes_checkBox_list) {
        if (box->isChecked())
            cur_ch_num++;
    }

    int vld_ch_num;

    if (_device_agent->get_config_int16(SR_CONF_VLD_CH_NUM, vld_ch_num) == false)
        return;

    while(cur_ch_num < vld_ch_num  && cur_ch_num < (int)_probes_checkBox_list.size()) {
        auto box = _probes_checkBox_list[cur_ch_num];
        if (box->isChecked() == false) {
            box->setChecked(true);
            cur_ch_num++;
        }
    }
}

void DeviceOptionsDock::enable_all_probes()
{
    bool stream_mode;

    if (_device_agent->get_config_bool(SR_CONF_STREAM, stream_mode)) {
        if (stream_mode) {
            enable_max_probes();
            commit_channels();
            return;
        }
    }

    set_all_probes(true);
    commit_channels();
}

void DeviceOptionsDock::disable_all_probes()
{
    set_all_probes(false);
    commit_channels();
}

void DeviceOptionsDock::zero_adj()
{
    commit_channels();

    QString strMsg(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_AUTO_CALIB_START),
                                   "Auto Calibration program will be started. Don't connect any probes. \nIt can take a while!"));
    bool bRet = MsgBox::Confirm(strMsg);

    if (bRet) {
        _device_agent->set_config_bool(SR_CONF_ZERO, true);
    }
    else {
        _device_agent->set_config_bool(SR_CONF_ZERO, false);
    }
}

void DeviceOptionsDock::on_calibration()
{
    commit_channels();
    _device_agent->set_config_bool(SR_CONF_CALI, true);
}

void DeviceOptionsDock::mode_check_timeout()
{
    if (_isBuilding)
        return;

    if (_device_agent->is_hardware())
    {
        bool test;
        int mode;

        if (_device_agent->get_config_int16(SR_CONF_OPERATION_MODE, mode)) {
            if (mode != _opt_mode) {
                _opt_mode = mode;
                build_dynamic_panel();
                try_resize_scroll();
            }
        }

        if (_device_agent->get_config_bool(SR_CONF_TEST, test)) {
            if (test) {
                for (auto box : _probes_checkBox_list) {
                    box->setCheckState(Qt::Checked);
                    box->setDisabled(true);
                }
            }
        }
    }
    else if (_device_agent->is_demo())
    {
        QString opt_mode = _device_agent->get_demo_operation_mode();
        if (opt_mode != _demo_operation_mode){
            _demo_operation_mode = opt_mode;
            build_dynamic_panel();
            try_resize_scroll();
        }
    }
}

void DeviceOptionsDock::channel_check()
{
    QRadioButton* bt = dynamic_cast<QRadioButton*>(sender());
    assert(bt);

    int mode_index = -1;

    for( auto p : _channel_mode_indexs){
        if (p.key == bt){
            mode_index = p.value;
            break;
        }
    }
    assert(mode_index >= 0);
    _device_agent->set_config_int16(SR_CONF_CHANNEL_MODE, mode_index);

    build_dynamic_panel();
    try_resize_scroll();

    QTimer::singleShot(0, this, [this](){
        _session->broadcast_msg(DSV_MSG_DEVICE_OPTIONS_UPDATED);
        _session->broadcast_msg(DSV_MSG_END_DEVICE_OPTIONS);
        emit settings_applied();
    });
}

void DeviceOptionsDock::analog_channel_check()
{
    QCheckBox* sc=dynamic_cast<QCheckBox*>(sender());
    if(sc != NULL)
    {
        for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
            sr_channel *const probe = (sr_channel*)l->data;

            if (sc->property("index").toInt() == probe->index){
               _device_agent->set_config_bool(SR_CONF_PROBE_MAP_DEFAULT, sc->isChecked(), probe);
            }
        }
    }

    _lst_probe_enabled_status.clear();
    for (auto ck : _probes_checkBox_list){
        _lst_probe_enabled_status.push_back(ck->isChecked());
    }

    build_dynamic_panel();
    try_resize_scroll();
}

void DeviceOptionsDock::on_analog_channel_enable()
{
    QCheckBox* sc = dynamic_cast<QCheckBox*>(sender());
    channel_checkbox_clicked(sc);
    commit_channels();
}

void DeviceOptionsDock::channel_checkbox_clicked(QCheckBox *sc)
{
    if (_device_agent->get_work_mode() == LOGIC) {
        if (sc == NULL || !sc->isChecked())
            return;

        bool stream_mode;
        if (_device_agent->get_config_bool(SR_CONF_STREAM, stream_mode) == false)
            return;

        if (!stream_mode)
            return;

        int cur_ch_num = 0;
        for (auto box : _probes_checkBox_list) {
            if (box->isChecked())
                cur_ch_num++;
        }

        int vld_ch_num;
        if (_device_agent->get_config_int16(SR_CONF_VLD_CH_NUM, vld_ch_num) == false)
            return;

        if (cur_ch_num > vld_ch_num) {
            QString msg_str(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_MAX_CHANNEL_COUNT_WARNING), "max count of channels!"));
            msg_str = msg_str.replace("{0}", QString::number(vld_ch_num) );
            MsgBox::Show(msg_str);

            sc->setChecked(false);
        }
    }
    else if (_device_agent->get_work_mode() == ANALOG) {
        if (sc != NULL) {
            QGridLayout *const layout = (QGridLayout *)sc->property("Layout").value<void *>();
            int i = layout->count();

            int ck_index = -1;
            int i_dex = 0;
            bool map_default = false;

            for(auto ck : _probes_checkBox_list){
                if (ck == sc){
                    ck_index = i_dex;
                    break;
                }
                i_dex++;
            }

            if (ck_index != -1)
            {
                _device_agent->get_config_bool(SR_CONF_PROBE_MAP_DEFAULT,
                                map_default, _dso_channel_list[ck_index], NULL);
            }

            while(i--)
            {
                QWidget* w = layout->itemAt(i)->widget();

                if (w->objectName() == "map-enable"){
                    QCheckBox *map_ckbox = dynamic_cast<QCheckBox*>(w);
                    map_ckbox->isChecked();
                }

                if (w->property("Enable").isNull()) {

                    if (map_default && w->objectName() == "map-row"){
                        w->setEnabled(false);
                    }
                    else{
                        w->setEnabled(sc->isChecked());
                    }
                }
            }
        }
    }
}

void DeviceOptionsDock::analog_probes(QGridLayout &layout)
{
    using namespace Qt;

    _probes_checkBox_list.clear();
    _probe_options_binding_list.clear();
    _dso_channel_list.clear();

    QTabWidget *tabWidget = new QTabWidget();
    tabWidget->setTabPosition(QTabWidget::North);
    tabWidget->setUsesScrollButtons(false);

    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);

    int ch_dex = 0;

    for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
        sr_channel *const probe = (sr_channel*)l->data;
        assert(probe);

        _dso_channel_list.push_back(probe);

        QWidget *probe_widget = new QWidget(tabWidget);
        QGridLayout *probe_layout = new QGridLayout(probe_widget);
        probe_widget->setLayout(probe_layout);

        bool ch_enabled = probe->enabled;
        if (ch_dex < _lst_probe_enabled_status.size()){
            ch_enabled = _lst_probe_enabled_status[ch_dex];
        }

        ch_dex++;

        QCheckBox *probe_checkBox = new QCheckBox(_container_panel);
        QVariant vlayout = QVariant::fromValue((void *)probe_layout);
        probe_checkBox->setProperty("Layout", vlayout);
        probe_checkBox->setProperty("Enable", true);
        probe_checkBox->setChecked(ch_enabled);
        _probes_checkBox_list.push_back(probe_checkBox);

        QLabel *en_label = new QLabel(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_ENABLE), "Enable: "), _container_panel);
        en_label->setFont(font);
        en_label->setProperty("Enable", true);
        probe_layout->addWidget(en_label, 0, 0, 1, 1);
        probe_layout->addWidget(probe_checkBox, 0, 1, 1, 3);

        auto *probe_options_binding = new pv::prop::binding::ProbeOptions(probe);
        const auto &properties = probe_options_binding->properties();
        int i = 1;

        for(auto p : properties)
        {
            const QString label = p->labeled_widget() ? QString() : p->label();
            QLabel *lb = new QLabel(label, probe_widget);
            lb->setFont(font);
            probe_layout->addWidget(lb, i, 0, 1, 1);

            QWidget * pow = p->get_widget(probe_widget);
            pow->setEnabled(probe_checkBox->isChecked());
            pow->setFont(font);

            if (p->name().contains("Map Default")) {
                pow->setProperty("index", probe->index);
                connect(pow, SIGNAL(clicked()), this, SLOT(analog_channel_check()));
            }
            else {
                if (probe_checkBox->isChecked() && p->name().contains("Map")) {
                    bool map_default = true;

                    _device_agent->get_config_bool(SR_CONF_PROBE_MAP_DEFAULT, map_default, probe, NULL);

                    if (map_default)
                        pow->setEnabled(false);

                    pow->setObjectName("map-row");
                }
            }
            probe_layout->addWidget(pow, i, 1, 1, 3);
            i++;
        }
        _probe_options_binding_list.push_back(probe_options_binding);

        connect(probe_checkBox, SIGNAL(released()), this, SLOT(on_analog_channel_enable()));

        QString tabName = QString::fromUtf8(probe->name);
        tabName += " ";

        tabWidget->addTab(probe_widget, tabName);
    }

    layout.addWidget(tabWidget, 0, 0, 1, 1);

    ui::set_form_font(this, font);
    _groupHeight2 = tabWidget->sizeHint().height() + 50;

    connect(tabWidget, SIGNAL(currentChanged(int)), this, SLOT(on_anlog_tab_changed(int)));
    tabWidget->setCurrentIndex(_cur_analog_tag_index);
}

void DeviceOptionsDock::on_anlog_tab_changed(int index)
{
    _cur_analog_tag_index = index;
}

QString DeviceOptionsDock::dynamic_widget(QLayout *lay)
{
    int mode = _device_agent->get_work_mode();

    if (mode == LOGIC) {
        QVBoxLayout *grid = dynamic_cast<QVBoxLayout*>(lay);
        assert(grid);
        logic_probes(*grid);
        return L_S(STR_PAGE_DLG, S_ID(IDS_DLG_CHANNEL), "Channel");
    }
    else if (mode == DSO) {
        bool have_zero;

        if (_device_agent->get_config_bool(SR_CONF_HAVE_ZERO, have_zero)) {
            QGridLayout *grid = dynamic_cast<QGridLayout*>(lay);
            assert(grid);

            QFont font = this->font();
            font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);

            if (have_zero) {
                auto config_button = new QPushButton(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_AUTO_CALIBRATION), "Auto Calibration"), _container_panel);
                config_button->setFont(font);
                grid->addWidget(config_button, 0, 0, 1, 1);
                connect(config_button, SIGNAL(clicked()), this, SLOT(zero_adj()));

                auto cali_button = new QPushButton(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_MANUAL_CALIBRATION), "Manual Calibration"), _container_panel);
                cali_button->setFont(font);
                grid->addWidget(cali_button, 1, 0, 1, 1);
                connect(cali_button, SIGNAL(clicked()), this, SLOT(on_calibration()));

                config_button->setFixedHeight(35);
                cali_button->setFixedHeight(35);

                _groupHeight2 = 135;

                return L_S(STR_PAGE_DLG, S_ID(IDS_DLG_CALIBRATION), "Calibration");
            }
        }
    }
    else if (mode == ANALOG) {
        QGridLayout *grid = dynamic_cast<QGridLayout*>(lay);
        assert(grid);
        analog_probes(*grid);
        return L_S(STR_PAGE_DLG, S_ID(IDS_DLG_CHANNEL), "Channel");
    }
    return NULL;
}

void DeviceOptionsDock::build_dynamic_panel()
{
    _isBuilding = true;

    if (_dynamic_panel != NULL){
        _dynamic_panel->deleteLater();
        _dynamic_panel = NULL;
    }

    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);

    if (_dynamic_panel == NULL)
    {
        _dynamic_panel = new QGroupBox("group", _dynamic_panel);
        _dynamic_panel->setFont(font);
        _container_lay->addWidget(_dynamic_panel);

        if (_device_agent->get_work_mode() == LOGIC)
            _dynamic_panel->setLayout(new QVBoxLayout());
        else
            _dynamic_panel->setLayout(new QGridLayout());
    }

    QString title = dynamic_widget(_dynamic_panel->layout());
    QGroupBox *box = dynamic_cast<QGroupBox*>(_dynamic_panel);
    box->setFont(font);
    box->setTitle(title);

    if (title == ""){
        box->setVisible(false);
    }

    _dynamic_panel->layout()->setContentsMargins(5, 20, 5, 5);

    _isBuilding = false;
}

void DeviceOptionsDock::try_resize_scroll()
{
#ifdef _WIN32
    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);
    QFontMetrics fm(font);

    auto labels = this->findChildren<QLabel*>();
    int max_label_width = 0;
    for(auto o : labels)
    {
        QRect rc = fm.boundingRect(o->text());
        QSize size(rc.width() + 15, rc.height());
        o->setFixedSize(size);

        if (size.width() > max_label_width){
            max_label_width = size.width();
        }
    }

    if (_device_agent->get_work_mode() == LOGIC
            && _device_agent->is_demo()){
        _dynamic_panel->setFixedWidth(max_label_width + 250);
    }
#endif
}

void DeviceOptionsDock::update_view()
{
    for (auto ptr : _probe_options_binding_list) {
        const auto &props = ptr->properties();
        for (auto p : props) {
            delete p;
        }
        delete ptr;
    }
    _probe_options_binding_list.clear();

    if (_device_options_binding) {
        const auto &old_dev_props = _device_options_binding->properties();
        for (auto p : old_dev_props) {
            delete p;
        }
        delete _device_options_binding;
        _device_options_binding = NULL;
    }

    if (_device_agent->have_instance()) {
        _device_options_binding = new pv::prop::binding::DeviceOptions();
    }

    QLayoutItem *item;
    while ((item = _container_lay->takeAt(0)) != NULL) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }

    _dynamic_panel = NULL;
    _probes_checkBox_list.clear();
    _channel_mode_indexs.clear();
    _dso_channel_list.clear();

    if (_device_options_binding == NULL)
        return;

    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);

    QGroupBox *props_box = new QGroupBox(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_MODE), "Mode"), _container_panel);
    props_box->setFont(font);
    props_box->setMinimumHeight(70);
    props_box->setAlignment(Qt::AlignTop);
    QLayout *props_lay = get_property_form(props_box);
    props_lay->setContentsMargins(5, 20, 5, 5);
    props_box->setLayout(props_lay);
    _container_lay->addWidget(props_box);

    QWidget *minWid = new QWidget();
    minWid->setFixedHeight(1);
    minWid->setMinimumWidth(230);
    _container_lay->addWidget(minWid);

    build_dynamic_panel();

    _device_agent->get_config_int16(SR_CONF_OPERATION_MODE, _opt_mode);

    if (_device_agent->is_demo())
        _demo_operation_mode = _device_agent->get_demo_operation_mode();

    try_resize_scroll();
}

void DeviceOptionsDock::device_updated()
{
    if (_device_agent->have_instance() == false) {
        QLayoutItem *item;
        while ((item = _container_lay->takeAt(0)) != NULL) {
            if (item->widget()) {
                delete item->widget();
            }
            delete item;
        }
        _dynamic_panel = NULL;
        _probes_checkBox_list.clear();
        _channel_mode_indexs.clear();
        _dso_channel_list.clear();

        if (_device_options_binding) {
            const auto &old_dev_props = _device_options_binding->properties();
            for (auto p : old_dev_props) {
                delete p;
            }
            delete _device_options_binding;
            _device_options_binding = NULL;
        }
        for (auto ptr : _probe_options_binding_list) {
            const auto &props = ptr->properties();
            for (auto p : props) {
                delete p;
            }
            delete ptr;
        }
        _probe_options_binding_list.clear();
        return;
    }

    if (_device_options_binding == NULL) {
        update_view();
    }
}

void DeviceOptionsDock::UpdateLanguage()
{
    update_view();
}

void DeviceOptionsDock::UpdateTheme()
{
    update_view();
}

void DeviceOptionsDock::UpdateFont()
{
    update_view();
}

void DeviceOptionsDock::showEvent(QShowEvent *event)
{
    QScrollArea::showEvent(event);
    if (_device_agent->have_instance())
        _mode_check_timer.start();
}

void DeviceOptionsDock::hideEvent(QHideEvent *event)
{
    QScrollArea::hideEvent(event);
    _mode_check_timer.stop();
}

void DeviceOptionsDock::bind_context(TabContext *ctx)
{
    _context = ctx;
    if (_device_agent && _device_agent->have_instance()) {
        auto &saved = ctx->document()->_dock_device_options_session;
        if (!saved.isEmpty()) {
            set_session(saved);
        } else {
            update_view();
        }
    } else {
        update_view();
    }
}

void DeviceOptionsDock::unbind_context()
{
    if (_context && _context->document() && _device_agent && _device_agent->have_instance()) {
        _context->document()->_dock_device_options_session = get_session();
    }
    _context = nullptr;
}

QJsonObject DeviceOptionsDock::get_session()
{
    QJsonObject obj;

    int mode = _device_agent->get_work_mode();
    obj["work_mode"] = mode;

    int opt_mode;
    if (_device_agent->get_config_int16(SR_CONF_OPERATION_MODE, opt_mode))
        obj["operation_mode"] = opt_mode;

    if (mode == LOGIC) {
        int ch_mode = 0;
        _device_agent->get_config_int16(SR_CONF_CHANNEL_MODE, ch_mode);
        obj["channel_mode"] = ch_mode;
    }

    QJsonArray ch_array;
    int idx = 0;
    for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
        sr_channel *const probe = (sr_channel*)l->data;
        QJsonObject ch_obj;
        ch_obj["index"] = (int)probe->index;
        ch_obj["enabled"] = probe->enabled;

        if (idx < (int)_probes_checkBox_list.size()) {
            ch_obj["ui_enabled"] = _probes_checkBox_list[idx]->isChecked();
        }

        if (mode == ANALOG || mode == DSO) {
            uint64_t vdiv;
            if (_device_agent->get_config_uint64(SR_CONF_PROBE_VDIV, vdiv, probe, NULL))
                ch_obj["vdiv"] = (qint64)vdiv;

            int coupling;
            if (_device_agent->get_config_int16(SR_CONF_PROBE_COUPLING, coupling, probe, NULL))
                ch_obj["coupling"] = coupling;

            bool map_default = true;
            _device_agent->get_config_bool(SR_CONF_PROBE_MAP_DEFAULT, map_default, probe, NULL);
            ch_obj["map_default"] = map_default;
        }

        ch_array.append(ch_obj);
        idx++;
    }
    obj["channels"] = ch_array;

    if (_device_agent->is_demo()) {
        obj["demo_operation_mode"] = _device_agent->get_demo_operation_mode();
    }

    return obj;
}

void DeviceOptionsDock::set_session(QJsonObject &obj)
{
    if (obj.contains("operation_mode")) {
        _device_agent->set_config_int16(SR_CONF_OPERATION_MODE, obj["operation_mode"].toInt());
    }

    if (obj.contains("channel_mode")) {
        _device_agent->set_config_int16(SR_CONF_CHANNEL_MODE, obj["channel_mode"].toInt());
    }

    if (obj.contains("demo_operation_mode")) {
        QString demo_mode = obj["demo_operation_mode"].toString();
        _device_agent->set_config_string(SR_CONF_PATTERN_MODE, demo_mode.toLocal8Bit().data());
    }

    update_view();

    int mode = _device_agent->get_work_mode();

    if (obj.contains("channels")) {
        QJsonArray ch_array = obj["channels"].toArray();
        int idx = 0;
        for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
            sr_channel *const probe = (sr_channel*)l->data;
            if (idx < ch_array.size()) {
                QJsonObject ch_obj = ch_array[idx].toObject();
                probe->enabled = ch_obj["enabled"].toBool();
                if (idx < (int)_probes_checkBox_list.size()) {
                    _probes_checkBox_list[idx]->setChecked(probe->enabled);
                }

                if (mode == ANALOG || mode == DSO) {
                    if (ch_obj.contains("vdiv")) {
                        _device_agent->set_config_uint64(SR_CONF_PROBE_VDIV, (uint64_t)ch_obj["vdiv"].toVariant().toULongLong(), probe, NULL);
                    }
                    if (ch_obj.contains("coupling")) {
                        _device_agent->set_config_int16(SR_CONF_PROBE_COUPLING, ch_obj["coupling"].toInt(), probe, NULL);
                    }
                    if (ch_obj.contains("map_default")) {
                        _device_agent->set_config_bool(SR_CONF_PROBE_MAP_DEFAULT, ch_obj["map_default"].toBool(), probe, NULL);
                    }
                }
            }
            idx++;
        }
    }

    _device_agent->get_config_int16(SR_CONF_OPERATION_MODE, _opt_mode);
    if (_device_agent->is_demo())
        _demo_operation_mode = _device_agent->get_demo_operation_mode();
}

} // namespace dock
} // namespace pv
