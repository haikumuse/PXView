/*
 * This file is part of the PulseView project.
 * PXView is based on PulseView.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2016 DreamSourceLab <support@dreamsourcelab.com>
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

/* __STDC_FORMAT_MACROS is required for PRIu64 and friends (in C++). */
#define __STDC_FORMAT_MACROS

#include "csvexporter.h"
#include "storesession.h"
#include "sigsession.h"

#include "data/logicsnapshot.h"
#include "data/dsosnapshot.h"
#include "data/analogsnapshot.h"
#include "data/decoderstack.h"
#include "data/decode/decoder.h"
#include "data/decode/row.h"
#include "data/signalmodel.h"
#include "view/trace.h"
#include "view/signal.h"
#include "view/logicsignal.h"
#include "view/dsosignal.h"
#include "view/decodetrace.h"
#include "dock/protocoldock.h"

#include <QFileDialog>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <math.h>
#include <QTextStream>
#include <list>

#include <libsigrokdecode.h>
#include "config/appconfig.h"
#include "dsvdef.h"
#include "utility/encoding.h"
#include "utility/path.h"
#include "log.h"

#include "ui/langresource.h"

namespace pv {

CsvExporter::CsvExporter(StoreSession *store)
    : _store(store)
{
}

CsvExporter::~CsvExporter()
{
}

QList<QString> CsvExporter::getSuportedExportFormats(){
    SigSession *_session = _store->_session;
    const struct sr_output_module** supportedModules = sr_output_list();
    QList<QString> list;
    while(*supportedModules){
        if(*supportedModules == NULL)
            break;
        if (_session->get_device()->get_work_mode() != LOGIC &&
            strcmp((*supportedModules)->id, "csv"))
            break;
        QString format((*supportedModules)->desc);
        format.append(" (*.");
        format.append((*supportedModules)->id);
        format.append(")");
        list.append(format);
        supportedModules++;
    }
    return list;
}

//export as csv file
bool CsvExporter::export_start()
{
    SigSession *_session = _store->_session;
    std::set<int> type_set;
    for(auto m : _session->get_signal_models()) {
        if (!_store->_export_channels.empty()) {
            if (std::find(_store->_export_channels.begin(), _store->_export_channels.end(), m->index()) == _store->_export_channels.end()) {
                continue;
            }
        } else if (_store->_export_channel_type >= 0 && m->type() != _store->_export_channel_type) {
            continue;
        }
        int _tp = m->type();
        type_set.insert(_tp);
    }

    if (type_set.size() > 1) {
        _store->_error = L_S(STR_PAGE_DLG, S_ID(IDS_MSG_STORESESS_EXPORTSTART_ERROR1),
                "PXView does not currently support\nfile export for multiple data types.");
        return false;
    } else if (type_set.size() == 0) {
        _store->_error = L_S(STR_PAGE_DLG, S_ID(IDS_MSG_STORESESS_EXPORTSTART_ERROR2), "No data to save.");
        return false;
    }

    const auto snapshot = _session->get_snapshot(*type_set.begin());
    if (!snapshot) {
        // Don't dereference a NULL snapshot (the original `assert(snapshot)`
        // is a no-op in Release builds and would crash on the next line).
        _store->_error = L_S(STR_PAGE_DLG, S_ID(IDS_MSG_STORESESS_EXPORTSTART_ERROR2), "No data to save.");
        return false;
    }
    // Check we have data
    if (snapshot->empty()) {
        _store->_error = L_S(STR_PAGE_DLG, S_ID(IDS_MSG_STORESESS_EXPORTSTART_ERROR2), "No data to save.");
        return false;
    }

    if (_store->_file_name == ""){
        _store->_error = L_S(STR_PAGE_DLG, S_ID(IDS_MSG_STORESESS_EXPORTSTART_ERROR3), "No set file name.");
        return false;
    }

    const struct sr_output_module **supportedModules = sr_output_list();
    while (*supportedModules)
    {
        if (*supportedModules == NULL)
            break;
        if (!strcmp((*supportedModules)->id, _store->_suffix.toUtf8().data()))
        {
            _store->_outModule = *supportedModules;
            break;
        }
        supportedModules++;
    }

    if (_store->_outModule == NULL)
    {
        // Preserve the error message — the previous code fell through to
        // `_error.clear(); return false;` here, which wiped the "Invalid
        // export format" message set just above and left callers with an
        // empty error string. Return immediately so the message survives.
        _store->_error = L_S(STR_PAGE_DLG, S_ID(IDS_MSG_STORESESS_EXPORTSTART_ERROR4), "Invalid export format.");
        return false;
    }

    // Spec SubTask 8.3: route through StoreSession::start_worker so the
    // worker thread signals completion via _thread_done_cv, enabling
    // wait_for() with a timeout in MCP / ~StoreProgress.
    _store->start_worker([this, snapshot]() { export_proc(snapshot); });
    return !_store->_has_error;
}

void CsvExporter::export_proc(data::Snapshot *snapshot)
{
    _store->_is_busy = true;

    pxv_info("export task start.");

    export_exec(snapshot);

    pxv_info("export task end.");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    _store->_is_busy = false;
}

void CsvExporter::export_exec(data::Snapshot *snapshot)
{
    SigSession *_session = _store->_session;
    assert(snapshot);
    if (!snapshot) {
        pxv_warn("StoreSession::export_exec called with NULL snapshot.");
        _store->_has_error = true;
        _store->_error = L_S(STR_PAGE_DLG, S_ID(IDS_MSG_STORESESS_EXPORTSTART_ERROR2), "No data to save.");
        return;
    }

        //set export all data flag
    AppConfig &app = AppConfig::Instance();
    int origin_flag = app.appOptions.originalData ? 1 : 0;

    data::LogicSnapshot *logic_snapshot = NULL;
    data::AnalogSnapshot *analog_snapshot = NULL;
    data::DsoSnapshot *dso_snapshot = NULL;
    int channel_type;

    if ((logic_snapshot = dynamic_cast<data::LogicSnapshot*>(snapshot))) {
        channel_type = SR_CHANNEL_LOGIC;
    } else if ((dso_snapshot = dynamic_cast<data::DsoSnapshot*>(snapshot))) {
        channel_type = SR_CHANNEL_DSO;
    } else if ((analog_snapshot = dynamic_cast<data::AnalogSnapshot*>(snapshot))) {
        channel_type = SR_CHANNEL_ANALOG;
    } else {
        _store->_has_error = true;
        _store->_error = L_S(STR_PAGE_DLG, S_ID(IDS_MSG_STORESESS_EXPORTPROC_ERROR1), "data type don't support.");
        return;
    }

    GHashTable *params = g_hash_table_new(g_str_hash, g_str_equal);
    GVariant* filenameGVariant = g_variant_new_bytestring(_store->_file_name.toUtf8().data());
    g_hash_table_insert(params, (char*)"filename", filenameGVariant);
    GVariant* typeGVariant = g_variant_new_int16(channel_type);
    g_hash_table_insert(params, (char*)"type", typeGVariant);

    struct sr_output output;
    output.module = (sr_output_module*) _store->_outModule;
    output.sdi = _session->get_device()->inst();
    output.param = NULL;
    output.start_sample_index = 0;

    struct ChannelStateRestorer {
        GSList *channels;
        std::vector<bool> original_states;
        DeviceAgent *device_agent;
        struct sr_dev_inst *saved_sdi;
        ChannelStateRestorer(GSList *channels, const std::vector<int32_t> &export_channels,
                             DeviceAgent *agent, struct sr_dev_inst *sdi)
            : channels(channels), device_agent(agent), saved_sdi(sdi) {
            if (channels) {
                for (GSList *l = channels; l; l = l->next) {
                    struct sr_channel *ch = (struct sr_channel *)l->data;
                    original_states.push_back(ch->enabled);
                    if (!export_channels.empty() && std::find(export_channels.begin(), export_channels.end(), ch->index) == export_channels.end()) {
                        ch->enabled = FALSE;
                    }
                }
            }
        }
        ~ChannelStateRestorer() {
            // Task 10.5: device lifetime guard. If the device was released or
            // swapped between construction and destruction (e.g. USB unplug,
            // session switch), the channels GSList and sr_channel structs may
            // be freed → UAF. Verify the device is still the same sdi before
            // restoring. Also wrap in try/catch as belt-and-suspenders.
            if (!channels || !device_agent || !saved_sdi) {
                return;
            }
            if (!device_agent->have_instance() || device_agent->inst() != saved_sdi) {
                pxv_warn("%s", "ChannelStateRestorer: device released/swapped before dtor, skipping state restore");
                return;
            }
            try {
                size_t i = 0;
                for (GSList *l = channels; l; l = l->next) {
                    struct sr_channel *ch = (struct sr_channel *)l->data;
                    if (i < original_states.size()) {
                        ch->enabled = original_states[i++];
                    }
                }
            } catch (...) {
                pxv_err("%s", "ChannelStateRestorer: exception during state restore, skipping");
            }
        }
    } restorer(_session->get_device()->get_channels(), _store->_export_channels,
               _session->get_device(), _session->get_device()->inst());

    if (channel_type == SR_CHANNEL_LOGIC){
        output.start_sample_index = _store->_start_index;
    }

    if(_store->_outModule->init){
       if(_store->_outModule->init(&output, params) != SR_OK){
        pxv_err("Failed to init export module.");
        return;
       }
    }

    QFile file(_store->_file_name);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }
    QTextStream out(&file);
    encoding::set_utf8(out);
    //out.setGenerateByteOrderMark(true);  // UTF-8 without BOM

    // Meta
    GString *data_out;
    struct sr_datafeed_packet p;
    struct sr_datafeed_meta meta;
    struct sr_config *src;

    src = _session->get_device()->new_config(SR_CONF_SAMPLERATE,
                g_variant_new_uint64(_session->cur_snap_samplerate()));

    meta.config = g_slist_append(NULL, src);

    src = _session->get_device()->new_config(SR_CONF_LIMIT_SAMPLES,
                g_variant_new_uint64(snapshot->get_sample_count()));

    meta.config = g_slist_append(meta.config, src);

    GVariant *gvar;
    int bits=0;

    _session->get_device()->get_config_byte(SR_CONF_UNIT_BITS, bits);

    gvar = _session->get_device()->get_config(SR_CONF_REF_MIN);
    if (gvar != NULL) {
        src = _session->get_device()->new_config(SR_CONF_REF_MIN, gvar);
        g_variant_unref(gvar);
    }
    else {
        src = _session->get_device()->new_config(SR_CONF_REF_MIN, g_variant_new_uint32(1));
    }

    meta.config = g_slist_append(meta.config, src);

    gvar = _session->get_device()->get_config(SR_CONF_REF_MAX);
    if (gvar != NULL) {
        src = _session->get_device()->new_config(SR_CONF_REF_MAX, gvar);
        g_variant_unref(gvar);
    }
    else {
        src = _session->get_device()->new_config(SR_CONF_REF_MAX, g_variant_new_uint32((1 << bits) - 1));
    }
    meta.config = g_slist_append(meta.config, src);

    p.type = SR_DF_META;
    p.status = SR_PKT_OK;
    p.payload = &meta;
    p.bExportOriginalData = 0;
    _store->_outModule->receive(&output, &p, &data_out);

    if(data_out){
        out << QString::fromUtf8((char*) data_out->str);
        g_string_free(data_out,TRUE);
    }
    for (GSList *l = meta.config; l; l = l->next) {
        src = (struct sr_config *)l->data;
        _session->get_device()->free_config(src);
    }
    g_slist_free(meta.config);

    if (channel_type == SR_CHANNEL_LOGIC) {
        _store->_unit_count = logic_snapshot->get_ring_sample_count();
        int blk_num = logic_snapshot->get_block_num();
        bool sample;
        std::vector<uint8_t *> buf_vec;
        std::vector<bool> buf_sample;

        uint64_t start_index = _store->_start_index;
        uint64_t end_index = _store->_end_index;
        uint64_t start_offset = 0;
        uint64_t end_offset = 0;
        int start_block = 0;
        int end_block = 0;

        if (start_index > logic_snapshot->get_ring_sample_count()){
            pxv_err("ERROR:the start curosr is invalid!");
            _store->_units_stored = -1;
            _store->progress_updated();
            return;
        }
        if (end_index > logic_snapshot->get_ring_sample_count()){
            end_index = 0;
        }

        if (start_index > 0){
            start_block = LogicSnapshot::get_block_with_sample(start_index, &start_offset);
        }
        if (end_index > 0){
            end_block = LogicSnapshot::get_block_with_sample(end_index, &end_offset);
        }

        if (start_index > 0 && end_index > 0){
            _store->_unit_count = (end_index - start_index);
        }
        else if (start_index > 0){
            _store->_unit_count = (logic_snapshot->get_ring_sample_count() - start_index);
        }
        else if (end_index > 0){
            _store->_unit_count = end_index;
        }

        for (int blk = 0; !_store->_canceled  &&  blk < blk_num; blk++) {
            uint64_t buf_sample_num = logic_snapshot->get_block_size(blk) * 8;
            buf_vec.clear();
            buf_sample.clear();

            if (blk < start_block)
                continue;
            if (blk > end_block && end_block > 0)
                break;

            for(auto m : _session->get_signal_models()) {
                if (!_store->_export_channels.empty() && std::find(_store->_export_channels.begin(), _store->_export_channels.end(), m->index()) == _store->_export_channels.end()) {
                    continue;
                }
                auto ch_type = m->type();
                if (ch_type == SR_CHANNEL_LOGIC) {
                    int ch_index = m->index();
                    if (!logic_snapshot->has_data(ch_index))
                        continue;
                    uint8_t *buf = logic_snapshot->get_block_buf(blk, ch_index, sample);
                    buf_vec.push_back(buf);
                    buf_sample.push_back(sample);
                }
            }

            uint16_t unitsize = ceil(buf_vec.size() / 8.0);
            unsigned int usize = 8192;
            unsigned int size = usize;
            struct sr_datafeed_logic lp;

            for(uint64_t i = 0; !_store->_canceled && i < buf_sample_num; i+=usize){
                if(buf_sample_num - i < usize)
                    size = buf_sample_num - i;
                uint8_t *xbuf = (uint8_t *)malloc(size * unitsize);
                if (xbuf == NULL) {
                    _store->_has_error = true;
                    _store->_error = L_S(STR_PAGE_DLG, S_ID(IDS_MSG_STORESESS_EXPORTPROC_ERROR2), "xbuffer malloc failed.");
                    return;
                }
                memset(xbuf, 0, size * unitsize);

                for (uint64_t j = 0; j < size; j++) {
                    for (unsigned int k = 0; k < buf_vec.size(); k++) {
                        if (buf_vec[k] == NULL && buf_sample[k])
                            xbuf[j*unitsize+k/8] +=  1 << k%8;
                        else if (buf_vec[k] && (buf_vec[k][(i+j)/8] & (1 << j%8)))
                            xbuf[j*unitsize+k/8] +=  1 << k%8;
                    }
                }

                lp.data = xbuf;
                lp.length = size * unitsize;
                lp.unitsize = unitsize;
                p.type = SR_DF_LOGIC;
                p.status = SR_PKT_OK;
                p.payload = &lp;
                p.bExportOriginalData = origin_flag;
                _store->_outModule->receive(&output, &p, &data_out);

                if(data_out){
                    out << QString::fromUtf8((char*) data_out->str);
                    g_string_free(data_out,TRUE);
                }

                _store->_units_stored += size;
                if (xbuf)
                    free(xbuf);
                _store->progress_updated();
            }
        }
    }
    else if (channel_type == SR_CHANNEL_DSO) {
        _store->_unit_count = snapshot->get_sample_count();
        unsigned int usize = 8192;
        unsigned int size = usize;
        struct sr_datafeed_dso dp;

        uint8_t *ch_data_buffer = (uint8_t*)malloc(usize * dso_snapshot->get_channel_num() + 1);
        if (ch_data_buffer == NULL){
            pxv_err("StoreSession::export_proc, malloc failed.");
            return;
        }

        int ch_num = dso_snapshot->get_channel_num();

        for(uint64_t i = 0; !_store->_canceled && i < _store->_unit_count; i+=usize){
            if(_store->_unit_count - i < usize)
                size = _store->_unit_count - i;

            int ch = 0;
            // Make the cross data buffer.
           for(auto m : _session->get_signal_models())
            {
                if (m->type() != SR_CHANNEL_DSO)
                    continue;

                if (!dso_snapshot->has_data(m->index()))
                    continue;

                uint8_t *wr = ch_data_buffer + ch;
                ch++;
                const uint8_t *rd = dso_snapshot->get_samples(0,0, m->index()) + i;
                const uint8_t *rd_end = rd + size;

                while (rd < rd_end)
                {
                    *wr = *rd;
                    wr += ch_num;
                    rd++;
                }
            }

            dp.data = ch_data_buffer;
            dp.num_samples = size;
            p.type = SR_DF_DSO;
            p.status = SR_PKT_OK;
            p.payload = &dp;
            p.bExportOriginalData = 0;
            _store->_outModule->receive(&output, &p, &data_out);

            if(data_out){
                out << (char*) data_out->str;
                g_string_free(data_out,TRUE);
            }

            _store->_units_stored += size;
            _store->progress_updated();
        }

        if (ch_data_buffer){
            free(ch_data_buffer);
            ch_data_buffer = NULL;
        }

    } else if (channel_type == SR_CHANNEL_ANALOG) {
        _store->_unit_count = snapshot->get_sample_count();
        uint64_t unit_count = _store->_unit_count;
        void* data_buffer = analog_snapshot->get_data();
        unsigned int usize = 8192;
        struct sr_datafeed_analog ap;

        const uint64_t ring_start = analog_snapshot->get_ring_start();

        int ch_count = snapshot->get_channel_num();

        void *block_buffer[2];
        uint64_t block_samples[2];
        block_buffer[0] =  (unsigned char*)data_buffer + ring_start * ch_count;
        block_samples[0] = unit_count - ring_start;
        block_buffer[1] = data_buffer;
        block_samples[1] = ring_start;

        for (int j=0; j<2; j++)
        {
            uint64_t sample_count = block_samples[j];

            if (sample_count == 0)
                break;

            //pxv_info("sample_count:%llu,total:%llu", sample_count, unit_count);

            for(uint64_t i = 0; i < sample_count; i += usize){

                if (_store->_canceled)
                    break;

                unsigned int size = usize;

                if(sample_count - i < usize){
                    size = sample_count - i;
                }

                ap.data = (unsigned char*)block_buffer[j] + i * ch_count;
                ap.num_samples = size;
                p.type = SR_DF_ANALOG;
                p.status = SR_PKT_OK;
                p.payload = &ap;
                p.bExportOriginalData = 0;
                _store->_outModule->receive(&output, &p, &data_out);

                if(data_out){
                    out << (char*) data_out->str;
                    g_string_free(data_out,TRUE);
                }

                _store->_units_stored += size;
                _store->progress_updated();

               // pxv_info("size:%llu;_units_stored:%llu", size, _units_stored);
            }
        }
    }

    // optional, as QFile destructor will already do it:
    file.close();
    _store->_outModule->cleanup(&output);
    g_hash_table_destroy(params);
    if (filenameGVariant != NULL)
        g_variant_unref(filenameGVariant);

    _store->progress_updated();
}

QString CsvExporter::MakeExportFile(bool bDlg)
{
    SigSession *_session = _store->_session;
    QString default_name;
    AppConfig &app = AppConfig::Instance();

    if (app.userHistory.exportDir != "")
    {
        default_name = app.userHistory.exportDir  + "/"  + _session->get_device()->name() + "-";
    }
    else{
        QDir _dir;
        QString _root = _dir.home().path();
        default_name =  _root + "/" + _session->get_device()->name() + "-";
    }

    for (const GSList *l = _session->get_device()->get_device_mode_list(); l; l = l->next) {
        const sr_dev_mode *mode = (const sr_dev_mode *)l->data;
        if (_session->get_device()->get_work_mode() == mode->mode) {
            default_name += mode->acronym;
            break;
        }
    }
    default_name += _session->get_session_time().toString("-yyMMdd-hhmmss");

    //ext name
    QList<QString> supportedFormats = getSuportedExportFormats();
    QString filter;
    for(int i = 0; i < supportedFormats.count();i++){
        filter.append(supportedFormats[i]);
        if(i < supportedFormats.count() - 1)
            filter.append(";;");
    }

    QString selfilter;
    if (app.userHistory.exportFormat != ""
            && _session->get_device()->get_work_mode() == LOGIC){
        selfilter.append(app.userHistory.exportFormat);
    }
    else{
        selfilter.append(".csv");
    }

    if (bDlg)
    {
        default_name = QFileDialog::getSaveFileName(
            NULL,
            L_S(STR_PAGE_MSG, S_ID(IDS_MSG_EXPORT_DATA),"Export Data"),
            default_name,
            filter,
            &selfilter);

        if (default_name == "")
        {
            return "";
        }

        bool bChange = false;
        QString _dir_path = path::GetDirectoryName(default_name);
        if (_dir_path != app.userHistory.exportDir)
        {
            app.userHistory.exportDir = _dir_path;
            bChange = true;
        }

        if (selfilter != app.userHistory.exportFormat
                && _session->get_device()->get_work_mode() == LOGIC){
            app.userHistory.exportFormat = selfilter;
             bChange = true;
        }

        if (bChange){
            app.SaveHistory();
        }
    }

    QString extName = selfilter;
    if (extName == ""){
        extName = filter;
    }

    QStringList list = extName.split('.').last().split(')');
    _store->_suffix = list.first();

    QFileInfo f(default_name);
    if(f.suffix().compare(_store->_suffix)){
        //tr
         default_name += "." + _store->_suffix;
    }

    _store->_file_name = default_name;
    return default_name;
}

} // pv
