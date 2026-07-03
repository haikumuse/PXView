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

#include "pxcserializer.h"
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

#define DEOCDER_CONFIG_VERSION  2

namespace pv {

PxcSerializer::PxcSerializer(StoreSession *store)
    : _store(store)
{
}

PxcSerializer::~PxcSerializer()
{
}

bool PxcSerializer::save_start()
{
    assert(_store->_sessionDataGetter);
    if (!_store->_sessionDataGetter) {
        pxv_warn("StoreSession::save_start called with no _sessionDataGetter.");
        _store->_error = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_STORESESS_SAVESTART_ERROR2), "No data to save.");
        return false;
    }

    SigSession *_session = _store->_session;

    std::set<int> type_set;
    for(auto m : _session->get_signal_models()) {
        type_set.insert(m->type());
    }

    if (type_set.size() > 1) {
        _store->_error = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_STORESESS_SAVESTART_ERROR1),
                "PXView does not currently support\nfile saving for multiple data types.");
        return false;

    } else if (type_set.size() == 0) {
        _store->_error = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_STORESESS_SAVESTART_ERROR2), "No data to save.");
        return false;
    }

    if (_store->_file_name == ""){
        _store->_error = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_STORESESS_SAVESTART_ERROR3), "No file name.");
        return false;
    }

    const auto snapshot = _session->get_snapshot(*type_set.begin());
	assert(snapshot);
    if (!snapshot) {
        pxv_warn("StoreSession::save_start: get_snapshot returned NULL.");
        _store->_error = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_STORESESS_SAVESTART_ERROR2), "No data to save.");
        return false;
    }
    // Check we have data
    if (snapshot->empty()) {
        _store->_error = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_STORESESS_SAVESTART_ERROR2), "No data to save.");
        return false;
    }

    std::string meta_data;
    std::string decoder_data;
    std::string session_data;

    meta_gen(snapshot, meta_data);
    decoders_gen(decoder_data);
    _store->_sessionDataGetter->genSessionData(session_data);

    if (meta_data.empty()) {
        _store->_error = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_STORESESS_SAVESTART_ERROR4), "Generate temp file data failed.");
        QFile::remove(_store->_file_name);
        return false;
    }
    if (decoder_data.empty()){
        _store->_error = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_STORESESS_SAVESTART_ERROR5), "Generate decoder file data failed.");
        QFile::remove(_store->_file_name);
        return false;
    }
    if (session_data.empty()){
        _store->_error = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_STORESESS_SAVESTART_ERROR6), "Generate session file data failed.");
        QFile::remove(_store->_file_name);
        return false;
    }

    auto _filename = path::ConvertPath(_store->_file_name);

    if (_store->m_zipDoc.CreateNew(_filename.c_str(), false))
    {
        if ( !_store->m_zipDoc.AddFromBuffer("header", meta_data.c_str(), meta_data.size())
            || !_store->m_zipDoc.AddFromBuffer("decoders", decoder_data.c_str(), decoder_data.size())
            || !_store->m_zipDoc.AddFromBuffer("session", session_data.c_str(), session_data.size())
        )
        {
            _store->_has_error = true;
            _store->_error = _store->m_zipDoc.GetError();
        }
        else
        {
            // Spec SubTask 8.3: route through StoreSession::start_worker so
            // the worker thread signals completion via _thread_done_cv,
            // enabling wait_for() with a timeout in MCP / ~StoreProgress.
            _store->start_worker([this, snapshot]() { save_proc(snapshot); });
            return !_store->_has_error;
        }
    }
    else{
         _store->_error = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_STORESESS_SAVESTART_ERROR7), "Generate zip file failed.");
    }

    QFile::remove(_store->_file_name);
    return false;
}

void PxcSerializer::save_logic(pv::data::LogicSnapshot *logic_snapshot)
{
    SigSession *_session = _store->_session;
    char chunk_name[20] = {0};
    uint16_t to_save_probes = 0;
    bool sample;
    int ret = SR_ERR;
    int num;

    for(auto m : _session->get_signal_models()) {
        if (m->enabled() && logic_snapshot->has_data(m->index()))
            to_save_probes++;
    }

    _store->_unit_count = logic_snapshot->get_ring_sample_count() / 8 * to_save_probes;
    num = logic_snapshot->get_block_num();

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
        start_index -= start_index % 64;
        start_block = LogicSnapshot::get_block_with_sample(start_index, &start_offset);
    }
    if (end_index > 0){
        if (end_index % 64 != 0){
            end_index += (64 - end_index % 64);
        }

        if (end_index > logic_snapshot->get_ring_sample_count()){
            end_index = 0;
        }
        else{
            end_block = LogicSnapshot::get_block_with_sample(end_index, &end_offset);
        }
    }

    if (start_index > 0 && end_index > 0){
        _store->_unit_count = (end_index - start_index) / 8 * to_save_probes;
    }
    else if (start_index > 0){
        _store->_unit_count = (logic_snapshot->get_ring_sample_count() - start_index) / 8 * to_save_probes;
    }
    else if (end_index > 0){
        _store->_unit_count = end_index / 8 * to_save_probes;
    }

    for(auto m : _session->get_signal_models())
    {
        auto ch_type = m->type();
        if (ch_type == SR_CHANNEL_LOGIC) {
            int ch_index = m->index();
            if (!m->enabled() || !logic_snapshot->has_data(ch_index))
                continue;

            for (int i = 0; !_store->_canceled && i < num; i++)
            {
                if (i < start_block){
                    continue;
                }
                if (i > end_block && end_block > 0){
                    break;
                }

                uint8_t *buf = logic_snapshot->get_block_buf(i, ch_index, sample);
                uint64_t size = logic_snapshot->get_block_size(i);
                bool need_malloc = (buf == NULL);

                if (i == end_block && end_offset / 8 < size && end_offset > 0){
                    size = end_offset / 8;
                }

                if (i == start_block && start_offset > 0){
                    if (buf != NULL){
                        buf += start_offset / 8;
                    }
                    size -= start_offset / 8;
                }

                if (need_malloc) {
                    buf = (uint8_t *)malloc(size);
                    if (buf == NULL) {
                        _store->_has_error = true;
                        _store->_error = L_S(STR_PAGE_DLG, S_ID(IDS_MSG_STORESESS_SAVEPROC_ERROR1),
                                    "Failed to create zip file. Malloc error.");
                    } else {
                        memset(buf, sample ? 0xff : 0x0, size);
                    }
                }

                // MakeChunkName compares `type` against SR_CHANNEL_LOGIC/DSO/ANALOG
                // (10000+) to get correct "L-/O-/A-" chunk names. Since type() now
                // returns SR_CHANNEL_* directly, we just pass type().
                MakeChunkName(chunk_name, i - start_block, ch_index, m->type(), HEADER_FORMAT_VERSION);
                ret = _store->m_zipDoc.AddFromBuffer(chunk_name, (const char*)buf, size) ? SR_OK : -1;

                if (ret != SR_OK) {
                    if (!_store->_has_error) {
                        _store->_has_error = true;
                        _store->_error = L_S(STR_PAGE_DLG, S_ID(IDS_MSG_STORESESS_SAVEPROC_ERROR2),
                                    "Failed to create zip file. Please check write permission of this path.");
                    }
                    _store->progress_updated();
                    if (_store->_has_error)
                        QFile::remove(_store->_file_name);
                    return;
                }
                _store->_units_stored += size;

                if (_store->_units_stored > _store->_unit_count
                        && start_index == 0
                        && end_index == 0){
                    pxv_err("Read block data error!");
                    assert(false);
                    return;
                }

                if (need_malloc)
                    free(buf);
                _store->progress_updated();
            }
        }
    }

    _store->progress_updated();

    if (_store->_canceled || num == 0){
        QFile::remove(_store->_file_name);
    }
    else {
        bool bret = _store->m_zipDoc.Close();
        _store->m_zipDoc.Release();

        if (!bret){
            _store->_has_error = true;
            _store->_error = _store->m_zipDoc.GetError();
        }
    }
}

void PxcSerializer::save_analog(pv::data::AnalogSnapshot *analog_snapshot)
{
    SigSession *_session = _store->_session;
    char chunk_name[20] = {0};
    int num = 0;
    int ret = SR_ERR;

    int ch_type = -1;
    for(auto m : _session->get_signal_models()) {
        // type() returns SR_CHANNEL_* (10000+) directly — correct for
        // MakeChunkName's "L-/O-/A-" chunk name comparison.
        ch_type = m->type();
        break;
    }

    if (ch_type != -1) {
        num = analog_snapshot->get_block_num();
        _store->_unit_count = analog_snapshot->get_sample_count() *
                        analog_snapshot->get_unit_bytes() *
                        analog_snapshot->get_channel_num();
        uint8_t *buf = NULL;
        uint8_t *buf_start = NULL;

        buf = (uint8_t *)analog_snapshot->get_data() +
                        (analog_snapshot->get_ring_start() * analog_snapshot->get_unit_bytes()
                                         * analog_snapshot->get_channel_num());

        buf_start = (uint8_t *)analog_snapshot->get_data();

        const uint8_t *buf_end = buf_start + _store->_unit_count;

        for (int i = 0; !_store->_canceled && i < num; i++) {
            const uint64_t size = analog_snapshot->get_block_size(i);
            if ((buf + size) > buf_end) {
                uint8_t *tmp = (uint8_t *)malloc(size);
                if (tmp == NULL) {
                    _store->_has_error = true;
                    _store->_error = L_S(STR_PAGE_DLG, S_ID(IDS_MSG_STORESESS_SAVEPROC_ERROR1),
                                "Failed to create zip file. Malloc error.");
                } else {
                    memcpy(tmp, buf, buf_end-buf);
                    memcpy(tmp+(buf_end-buf), buf_start, buf+size-buf_end);
                }

                MakeChunkName(chunk_name, i, 0, ch_type, HEADER_FORMAT_VERSION);
                ret = _store->m_zipDoc.AddFromBuffer(chunk_name, (const char*)tmp, size) ? SR_OK : -1;

                buf += (size - _store->_unit_count);
                if (tmp)
                    free(tmp);
            }
            else {
                MakeChunkName(chunk_name, i, 0, ch_type, HEADER_FORMAT_VERSION);
                ret = _store->m_zipDoc.AddFromBuffer(chunk_name, (const char*)buf, size) ? SR_OK : -1;

                buf += size;
            }

            if (ret != SR_OK) {
                if (!_store->_has_error) {
                    _store->_has_error = true;
                    _store->_error = L_S(STR_PAGE_DLG, S_ID(IDS_MSG_STORESESS_SAVEPROC_ERROR2),
                            "Failed to create zip file. Please check write permission of this path.");
                }
                _store->progress_updated();
                if (_store->_has_error)
                    QFile::remove(_store->_file_name);
                return;
            }
            _store->_units_stored += size;
            _store->progress_updated();
        }
    }

    _store->progress_updated();

    if (_store->_canceled || num == 0){
        QFile::remove(_store->_file_name);
    }
    else {
        bool bret = _store->m_zipDoc.Close();
        _store->m_zipDoc.Release();

        if (!bret){
            _store->_has_error = true;
            _store->_error = _store->m_zipDoc.GetError();
        }
    }
}

void PxcSerializer::save_dso(pv::data::DsoSnapshot *dso_snapshot)
{
    SigSession *_session = _store->_session;
    char chunk_name[20] = {0};
    int ret = SR_ERR;

    uint64_t size = dso_snapshot->get_sample_count();
    int ch_num = dso_snapshot->get_channel_num();
    _store->_unit_count = size * ch_num;

    for(auto m : _session->get_signal_models())
    {
        if (m->type() == SR_CHANNEL_DSO) {
            int ch_index = m->index();

            if (!dso_snapshot->has_data(ch_index))
                continue;

            if (_store->_canceled)
                break;

            const uint8_t *data_buffer = dso_snapshot->get_samples(0, 0, ch_index);

            snprintf(chunk_name, 19, "O-%d/0", ch_index);
            ret = _store->m_zipDoc.AddFromBuffer(chunk_name, (const char*)data_buffer, size) ? SR_OK : -1;

            if (ret != SR_OK) {
                if (!_store->_has_error) {
                    _store->_has_error = true;
                    _store->_error = L_S(STR_PAGE_DLG, S_ID(IDS_MSG_STORESESS_SAVEPROC_ERROR2),
                            "Failed to create zip file. Please check write permission of this path.");
                }
                _store->progress_updated();
                if (_store->_has_error)
                    QFile::remove(_store->_file_name);
                return;
            }

            _store->_units_stored += size;
            _store->progress_updated();
        }
    }

    _store->progress_updated();

    if (_store->_canceled || size == 0 || ch_num == 0){
        QFile::remove(_store->_file_name);
    }
    else {
        bool bret = _store->m_zipDoc.Close();
        _store->m_zipDoc.Release();

        if (!bret){
            _store->_has_error = true;
            _store->_error = _store->m_zipDoc.GetError();
        }
    }
}

void PxcSerializer::save_proc(data::Snapshot *snapshot)
{
	assert(snapshot);
    if (!snapshot) {
        pxv_warn("StoreSession::save_proc called with NULL snapshot.");
        return;
    }

    data::LogicSnapshot *logic_snapshot = NULL;
    data::AnalogSnapshot *analog_snapshot = NULL;
    data::DsoSnapshot *dso_snapshot = NULL;

    _store->_is_busy = true;

    pxv_info("save task start.");

    if ((logic_snapshot = dynamic_cast<data::LogicSnapshot*>(snapshot))) {
        save_logic(logic_snapshot);
    }
    else if ((analog_snapshot = dynamic_cast<data::AnalogSnapshot*>(snapshot))) {
        save_analog(analog_snapshot);
    }
    else if ((dso_snapshot = dynamic_cast<data::DsoSnapshot*>(snapshot))) {
        save_dso(dso_snapshot);
    }

    pxv_info("save task end.");

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    _store->_is_busy = false;
}

bool PxcSerializer::meta_gen(data::Snapshot *snapshot, std::string &str)
{
    SigSession *_session = _store->_session;
    GSList *l;
    struct sr_channel *probe;
    int probecnt;
    char *s;
    char meta[300] = {0};

    sprintf(meta, "%s", "[version]\n"); str += meta;
    sprintf(meta, "version = %d\n", HEADER_FORMAT_VERSION); str += meta;
    sprintf(meta, "%s", "[header]\n"); str += meta;

    int mode = _session->get_device()->get_work_mode();

    if (true) {
        sprintf(meta, "driver = %s\n", _session->get_device()->driver_name().toLocal8Bit().data()); str += meta;
        sprintf(meta, "device mode = %d\n", mode); str += meta;
    }

    sprintf(meta, "capturefile = data\n"); str += meta;
    sprintf(meta, "total samples = %" PRIu64 "\n", snapshot->get_sample_count()); str += meta;

    if (mode != LOGIC) {
        sprintf(meta, "total probes = %d\n", snapshot->get_channel_num()); str += meta;
        sprintf(meta, "total blocks = %d\n", snapshot->get_block_num()); str += meta;
    }

    data::LogicSnapshot *logic_snapshot = NULL;
    if ((logic_snapshot = dynamic_cast<data::LogicSnapshot*>(snapshot))) {
        uint16_t to_save_probes = 0;
        for (l = _session->get_device()->get_channels(); l; l = l->next) {
            probe = (struct sr_channel *)l->data;
            if (probe->enabled && logic_snapshot->has_data(probe->index))
                to_save_probes++;
        }

        int block_count = logic_snapshot->get_block_num();

        uint64_t start_index = _store->_start_index;
        uint64_t end_index = _store->_end_index;
        uint64_t start_offset = 0;
        uint64_t end_offset = 0;
        int start_block = 0;
        int end_block = 0;

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
            block_count = end_block - start_block + 1;
        }
        else if (start_index > 0){
            block_count = block_count - start_block;
        }
        else if (end_index > 0){
            block_count = end_block + 1;
        }

        sprintf(meta, "total probes = %d\n", to_save_probes); str += meta;
        sprintf(meta, "total blocks = %d\n", block_count); str += meta;
    }

    s = sr_samplerate_string(_session->cur_snap_samplerate());

    sprintf(meta, "samplerate = %s\n", s); str += meta;

    uint64_t tmp_u64;
    int tmp_u8;
    uint32_t tmp_u32;

    if (mode == DSO) {
        if (_session->get_device()->get_config_uint64(SR_CONF_TIMEBASE, tmp_u64)) {
            sprintf(meta, "hDiv = %" PRIu64 "\n", tmp_u64); str += meta;
        }

        if (_session->get_device()->get_config_uint64(SR_CONF_MAX_TIMEBASE, tmp_u64)) {
            sprintf(meta, "hDiv max = %" PRIu64 "\n", tmp_u64); str += meta;
        }

        if (_session->get_device()->get_config_uint64(SR_CONF_MIN_TIMEBASE, tmp_u64)) {
            sprintf(meta, "hDiv min = %" PRIu64 "\n", tmp_u64); str += meta;
        }

        if (_session->get_device()->get_config_byte(SR_CONF_UNIT_BITS, tmp_u8)) {
            sprintf(meta, "bits = %d\n", tmp_u8); str += meta;
        }

        if (_session->get_device()->get_config_uint32(SR_CONF_REF_MIN, tmp_u32)) {
            sprintf(meta, "ref min = %d\n", tmp_u32); str += meta;
        }

        if (_session->get_device()->get_config_uint32(SR_CONF_REF_MAX, tmp_u32)) {
            sprintf(meta, "ref max = %d\n", tmp_u32); str += meta;
        }
    }
    else if (mode == LOGIC) {
        sprintf(meta, "trigger time = %lld\n", _session->get_session_time().toMSecsSinceEpoch()); str += meta;
    }
    else if (mode == ANALOG) {
        data::AnalogSnapshot *analog_snapshot = NULL;
        if ((analog_snapshot = dynamic_cast<data::AnalogSnapshot*>(snapshot))) {
            uint8_t tmp_u8 = analog_snapshot->get_unit_bytes();
            sprintf(meta, "bits = %d\n", tmp_u8*8); str += meta;
        }

        if (_session->get_device()->get_config_uint32(SR_CONF_REF_MIN, tmp_u32)) {
            sprintf(meta, "ref min = %d\n", tmp_u32); str += meta;
        }

        if (_session->get_device()->get_config_uint32(SR_CONF_REF_MAX, tmp_u32)) {
            sprintf(meta, "ref max = %d\n", tmp_u32); str += meta;
        }
    }
    sprintf(meta, "trigger pos = %" PRIu64 "\n", _session->get_trigger_pos()); str += meta;

    probecnt = 0;

    for (l = _session->get_device()->get_channels(); l; l = l->next) {

        probe = (struct sr_channel *)l->data;

        if (!snapshot->has_data(probe->index))
            continue;

        if (mode == LOGIC && !probe->enabled)
            continue;

        if (probe->name)
        {
            int sigdex = (mode == LOGIC) ? probe->index : probecnt;
            sprintf(meta, "probe%d = %s\n", sigdex, probe->name);
            str += meta;
        }

        if (probe->trigger){
            sprintf(meta, " trigger%d = %s\n", probecnt, probe->trigger);
            str += meta;
        }

        if (mode == DSO)
        {
            sprintf(meta, " enable%d = %d\n", probecnt, probe->enabled);
            str += meta;
            sprintf(meta, " coupling%d = %d\n", probecnt, probe->coupling);
            str += meta;
            sprintf(meta, " vDiv%d = %" PRIu64 "\n", probecnt, probe->vdiv);
            str += meta;
            sprintf(meta, " vFactor%d = %" PRIu64 "\n", probecnt, probe->vfactor);
            str += meta;
            sprintf(meta, " vOffset%d = %d\n", probecnt, probe->hw_offset);
            str += meta;
            sprintf(meta, " vTrig%d = %d\n", probecnt, probe->trig_value);
            str += meta;

            if (_session->dso_status_is_valid())
            {
                sr_status status = _session->get_dso_status();

                if (probe->index == 0)
                {
                    sprintf(meta, " period%d = %" PRIu32 "\n", probecnt, status.ch0_cyc_tlen);
                    str += meta;
                    sprintf(meta, " pcnt%d = %" PRIu32 "\n", probecnt, status.ch0_cyc_cnt);
                    str += meta;
                    sprintf(meta, " max%d = %d\n", probecnt, status.ch0_max);
                    str += meta;
                    sprintf(meta, " min%d = %d\n", probecnt, status.ch0_min);
                    str += meta;
                    sprintf(meta, " plen%d = %" PRIu32 "\n", probecnt, status.ch0_cyc_plen);
                    str += meta;
                    sprintf(meta, " llen%d = %" PRIu32 "\n", probecnt, status.ch0_cyc_llen);
                    str += meta;
                    sprintf(meta, " level%d = %d\n", probecnt, status.ch0_level_valid);
                    str += meta;
                    sprintf(meta, " plevel%d = %d\n", probecnt, status.ch0_plevel);
                    str += meta;
                    sprintf(meta, " low%d = %" PRIu32 "\n", probecnt, status.ch0_low_level);
                    str += meta;
                    sprintf(meta, " high%d = %" PRIu32 "\n", probecnt, status.ch0_high_level);
                    str += meta;
                    sprintf(meta, " rlen%d = %" PRIu32 "\n", probecnt, status.ch0_cyc_rlen);
                    str += meta;
                    sprintf(meta, " flen%d = %" PRIu32 "\n", probecnt, status.ch0_cyc_flen);
                    str += meta;
                    sprintf(meta, " rms%d = %" PRIu64 "\n", probecnt, status.ch0_acc_square);
                    str += meta;
                    sprintf(meta, " mean%d = %" PRIu32 "\n", probecnt, status.ch0_acc_mean);
                    str += meta;
                }
                else
                {
                    sprintf(meta, " period%d = %" PRIu32 "\n", probecnt, status.ch1_cyc_tlen);
                    str += meta;
                    sprintf(meta, " pcnt%d = %" PRIu32 "\n", probecnt, status.ch1_cyc_cnt);
                    str += meta;
                    sprintf(meta, " max%d = %d\n", probecnt, status.ch1_max);
                    str += meta;
                    sprintf(meta, " min%d = %d\n", probecnt, status.ch1_min);
                    str += meta;
                    sprintf(meta, " plen%d = %" PRIu32 "\n", probecnt, status.ch1_cyc_plen);
                    str += meta;
                    sprintf(meta, " llen%d = %" PRIu32 "\n", probecnt, status.ch1_cyc_llen);
                    str += meta;
                    sprintf(meta, " level%d = %d\n", probecnt, status.ch1_level_valid);
                    str += meta;
                    sprintf(meta, " plevel%d = %d\n", probecnt, status.ch1_plevel);
                    str += meta;
                    sprintf(meta, " low%d = %" PRIu32 "\n", probecnt, status.ch1_low_level);
                    str += meta;
                    sprintf(meta, " high%d = %" PRIu32 "\n", probecnt, status.ch1_high_level);
                    str += meta;
                    sprintf(meta, " rlen%d = %" PRIu32 "\n", probecnt, status.ch1_cyc_rlen);
                    str += meta;
                    sprintf(meta, " flen%d = %" PRIu32 "\n", probecnt, status.ch1_cyc_flen);
                    str += meta;
                    sprintf(meta, " rms%d = %" PRIu64 "\n", probecnt, status.ch1_acc_square);
                    str += meta;
                    sprintf(meta, " mean%d = %" PRIu32 "\n", probecnt, status.ch1_acc_mean);
                    str += meta;
                }
            }
        }
        else if (mode == ANALOG)
        {
            sprintf(meta, " enable%d = %d\n", probecnt, probe->enabled);
            str += meta;
            sprintf(meta, " coupling%d = %d\n", probecnt, probe->coupling);
            str += meta;
            sprintf(meta, " vDiv%d = %" PRIu64 "\n", probecnt, probe->vdiv);
            str += meta;
            sprintf(meta, " vOffset%d = %d\n", probecnt, probe->hw_offset);
            str += meta;
            sprintf(meta, " mapUnit%d = %s\n", probecnt, probe->map_unit);
            str += meta;
            sprintf(meta, " mapMax%d = %lf\n", probecnt, probe->map_max);
            str += meta;
            sprintf(meta, " mapMin%d = %lf\n", probecnt, probe->map_min);
            str += meta;
        }
        probecnt++;
    }

    return true;
}

bool PxcSerializer::decoders_gen(std::string &str)
{
    QJsonArray dec_array;
    if (!gen_decoders_json(dec_array))
        return false;
    QJsonDocument sessionDoc(dec_array);
    QString data = QString::fromUtf8(sessionDoc.toJson());
    str = std::string(data.toLocal8Bit().data());
    return true;
}

bool PxcSerializer::gen_decoders_json(QJsonArray &array)
{
    SigSession *_session = _store->_session;
    for(auto stack : _session->get_decoder_stacks()) {
        QJsonObject dec_obj;
        QJsonArray stack_array;
        QJsonObject show_obj;
        const auto &decoderList = stack->stack();

        for(auto dec : decoderList)
        {
            QJsonArray ch_array;
            const srd_decoder *const d = dec->decoder();;
            const bool have_probes = (d->channels || d->opt_channels) != 0;

            if (have_probes) {
                auto binded_probes = dec->binded_probe_list();
                for(auto probe : binded_probes) {
                    QJsonObject ch_obj;
                    int binded_index = dec->binded_probe_index(probe);
                    ch_obj[probe->id] = QJsonValue::fromVariant(binded_index);
                    ch_array.push_back(ch_obj);
                }
            }

            QJsonObject options_obj;
            auto dec_binding = new prop::binding::DecoderOptions(stack, dec);

            for (GSList *l = d->options; l; l = l->next)
            {
                const srd_decoder_option *const opt =
                    (srd_decoder_option*)l->data;

                if (g_variant_is_of_type(opt->def, G_VARIANT_TYPE("d"))) {
                    GVariant *const var = dec_binding->getter(opt->id);
                    if (var != NULL) {
                        options_obj[opt->id] = QJsonValue::fromVariant(g_variant_get_double(var));
                        g_variant_unref(var);
                    }
                } else if (g_variant_is_of_type(opt->def, G_VARIANT_TYPE("x"))) {
                    GVariant *const var = dec_binding->getter(opt->id);
                    if (var != NULL) {
                        options_obj[opt->id] = QJsonValue::fromVariant(get_integer(var));
                        g_variant_unref(var);
                    }
                } else if (g_variant_is_of_type(opt->def, G_VARIANT_TYPE("s"))) {
                    GVariant *const var = dec_binding->getter(opt->id);
                    if (var != NULL) {
                        const char *sz = g_variant_get_string(var, NULL);
                        options_obj[opt->id] = QJsonValue::fromVariant(QString(sz));
                        g_variant_unref(var);
                    }
                }else {
                    continue;
                }
            }

            if (have_probes) {
                dec_obj["id"] = QJsonValue::fromVariant(QString(d->id));
                dec_obj["channel"] = ch_array;
                dec_obj["options"] = options_obj;
            } else {
                QJsonObject stack_obj;
                stack_obj["id"] = QJsonValue::fromVariant(QString(d->id));
                stack_obj["options"] = options_obj;
                stack_array.push_back(stack_obj);
            }
            show_obj[d->id] = QJsonValue::fromVariant(dec->shown());
        }

        dec_obj["version"] = DEOCDER_CONFIG_VERSION;
        // TODO: adapt — DecoderStack no longer carries a UI label; the
        // label was previously read from the view::DecodeTrace. Use the
        // first decoder's id as a fallback label.
        if (!decoderList.empty()) {
            dec_obj["label"] = QString(decoderList.front()->decoder()->id);
        } else {
            dec_obj["label"] = QString();
        }
        dec_obj["stacked decoders"] = stack_array;
        // TODO: adapt — view_index is UI state owned by view::DecodeTrace;
        // DecoderStack does not expose it. Persist 0 for now and let the
        // View layer restore the index after it creates DecodeTrace.
        dec_obj["view_index"] = 0;

        auto rows = stack->get_rows_gshow();
        for (auto i = rows.begin(); i != rows.end(); i++) {
            pv::data::decode::Row _row = (*i).first;
            QString kn = _row.title_id();
            show_obj[kn] = QJsonValue::fromVariant((*i).second);
        }
        dec_obj["show"] = show_obj;

        array.push_back(dec_obj);
    }

    return true;
}

bool PxcSerializer::load_decoders(dock::ProtocolDock *widget, QJsonArray &dec_array)
{
    SigSession *_session = _store->_session;
    if (_session->get_device()->get_work_mode() != LOGIC)
    {
        pxv_info("StoreSession::load_decoders(), is not LOGIC mode.");
        return false;
    }

    if (dec_array.isEmpty()){
        pxv_info("StoreSession::load_decoders(), json object array is empty.");
        return false;
    }

    int dec_index = -1;

    pxv_info("StoreSession::load_decoders: starting to process %d decoders", dec_array.size());
    for (const QJsonValue &dec_value : dec_array)
    {
        QJsonObject dec_obj = dec_value.toObject();
        pxv_info("StoreSession::load_decoders: processing decoder %s", dec_obj["id"].toString().toStdString().c_str());
        auto &pre_dsigs = _session->get_decoder_stacks();
        std::list<pv::data::decode::Decoder*> sub_decoders;

        //get sub decoders
        if (dec_obj.contains("stacked decoders")) {
                for(const QJsonValue &value : dec_obj["stacked decoders"].toArray()) {
                    QJsonObject stacked_obj = value.toObject();

                    GSList *dl = g_slist_copy((GSList*)srd_decoder_list());
                    for(; dl; dl = dl->next) {
                        const srd_decoder *const d = (srd_decoder*)dl->data;
                        assert(d);
                        if (!d) {
                            pxv_warn("StoreSession::load_decoders: srd_decoder list node has NULL data, skipping.");
                            continue;
                        }

                        if (QString::fromUtf8(d->id) == stacked_obj["id"].toString()) {
                            sub_decoders.push_back(new data::decode::Decoder(d));
                            break;
                        }
                    }
                    g_slist_free(dl);
                }
        }

        //create protocol
        bool ret = widget->add_protocol_by_id(dec_obj["id"].toString(), true, sub_decoders);
        if (!ret)
        {
            for(auto sub : sub_decoders){
                delete sub;
            }
            sub_decoders.clear();

            continue; //protocol is not exists;
        }

        dec_index++;

        if (dec_obj.contains("label")){
            _session->set_decoder_row_label(dec_index, dec_obj["label"].toString());
        }

        if (dec_obj.contains("view_index")){
            int chan_view_index = dec_obj["view_index"].toInt();
            // TODO: adapt — DecoderStack no longer exposes set_view_index; UI state
            // should be restored by the View layer after it creates DecodeTrace.
            (void)chan_view_index;
        }

        std::list<int> bind_indexs;

        auto &aft_dsigs = _session->get_decoder_stacks();
        pxv_info("StoreSession::load_decoders: pre_dsigs.size()=%d, aft_dsigs.size()=%d", (int)pre_dsigs.size(), (int)aft_dsigs.size());

        if (aft_dsigs.size() >= pre_dsigs.size()) {
            const GSList *l;

            auto new_dsig = aft_dsigs.back();
            auto stack = new_dsig;
            pxv_info("StoreSession::load_decoders: new_dsig=%p", new_dsig);

            auto &decoder_list = stack->stack();

            for(auto dec : decoder_list)
            {
                const srd_decoder *const d = dec->decoder();
                QJsonObject options_obj;

                if (dec == decoder_list.front()) {
                    std::map<const srd_channel*, int> probe_map;
                    // Load the mandatory channels
                    for(l = d->channels; l; l = l->next) {
                        const struct srd_channel *const pdch = (struct srd_channel *)l->data;
                        pxv_info("StoreSession::load_decoders: checking mandatory channel '%s'", pdch->id);

                        for (const QJsonValue &value : dec_obj["channel"].toArray()) {
                            QJsonObject ch_obj = value.toObject();
                            if (ch_obj.contains(pdch->id)) {
                                int bind_chan = ch_obj[pdch->id].toInt();
                                probe_map[pdch] = bind_chan;
                                pxv_info("StoreSession::load_decoders: mapped mandatory channel '%s' to bind_chan %d", pdch->id, bind_chan);

                                auto fd_it = find(bind_indexs.begin(), bind_indexs.end(), bind_chan);
                                if (fd_it == bind_indexs.end())
                                    bind_indexs.push_back(bind_chan);
                                break;
                            }
                        }
                    }

                    // Load the optional channels
                    for(l = d->opt_channels; l; l = l->next) {
                        const struct srd_channel *const pdch = (struct srd_channel *)l->data;
                        pxv_info("StoreSession::load_decoders: checking optional channel '%s'", pdch->id);

                        for (const QJsonValue &value : dec_obj["channel"].toArray()) {
                            QJsonObject ch_obj = value.toObject();
                            if (ch_obj.contains(pdch->id)) {
                                int bind_chan = ch_obj[pdch->id].toInt();
                                probe_map[pdch] = bind_chan;
                                pxv_info("StoreSession::load_decoders: mapped optional channel '%s' to bind_chan %d", pdch->id, bind_chan);

                                auto fd_it = find(bind_indexs.begin(), bind_indexs.end(), bind_chan);
                                if (fd_it == bind_indexs.end())
                                    bind_indexs.push_back(bind_chan);
                                break;
                            }
                        }
                    }
                    pxv_info("StoreSession::load_decoders: setting %d probes on decoder", (int)probe_map.size());
                    dec->set_probes(probe_map);
                    options_obj = dec_obj["options"].toObject();
                }
                else {
                    for(const QJsonValue &value : dec_obj["stacked decoders"].toArray()) {
                        QJsonObject stacked_obj = value.toObject();
                        if (QString::fromUtf8(d->id) == stacked_obj["id"].toString()) {
                            options_obj = stacked_obj["options"].toObject();
                            break;
                        }
                    }
                }

                for (l = d->options; l; l = l->next) {
                    const srd_decoder_option *const opt = (srd_decoder_option*)l->data;

                    if (options_obj.contains(opt->id))
                    {
                        GVariant *new_value = NULL;
                        // When the numberic value is a string, it got zero always,
                        // so must convert from string.
                        QString vs = options_obj[opt->id].toString();

                        if (g_variant_is_of_type(opt->def, G_VARIANT_TYPE("d")))
                        {
                            double vi = options_obj[opt->id].toDouble();
                            if (vs != "") vi = vs.toDouble();
                            new_value = g_variant_new_double(vi);
                        }
                        else if (g_variant_is_of_type(opt->def, G_VARIANT_TYPE("x"))) {
                            const GVariantType *const type = g_variant_get_type(opt->def);

                            if (g_variant_type_equal(type, G_VARIANT_TYPE_BYTE)){
                                int vi = options_obj[opt->id].toInt();
                                if (vs != "") vi = vs.toInt();
                                new_value = g_variant_new_byte(vi);
                            }
                            else if (g_variant_type_equal(type, G_VARIANT_TYPE_INT16)){
                                int vi = options_obj[opt->id].toInt();
                                if (vs != "") vi = vs.toInt();
                                new_value = g_variant_new_int16(vi);
                            }
                            else if (g_variant_type_equal(type, G_VARIANT_TYPE_UINT16)){
                                int vi = options_obj[opt->id].toInt();
                                if (vs != "") vi = vs.toInt();
                                new_value = g_variant_new_uint16(vi);
                            }
                            else if (g_variant_type_equal(type, G_VARIANT_TYPE_INT32)){
                                int vi = options_obj[opt->id].toInt();
                                if (vs != "") vi = vs.toInt();
                                new_value = g_variant_new_int32(vi);
                            }
                            else if (g_variant_type_equal(type, G_VARIANT_TYPE_UINT32)){
                                int vi = options_obj[opt->id].toInt();
                                if (vs != "") vi = vs.toInt();
                                new_value = g_variant_new_uint32(vi);
                            }
                            else if (g_variant_type_equal(type, G_VARIANT_TYPE_INT64)){
                                int vi = options_obj[opt->id].toInt();
                                if (vs != "") vi = vs.toInt();
                                new_value = g_variant_new_int64(vi);
                            }
                            else if (g_variant_type_equal(type, G_VARIANT_TYPE_UINT64)){
                                int vi = options_obj[opt->id].toInt();
                                if (vs != "") vi = vs.toInt();
                                new_value = g_variant_new_uint64(vi);
                            }
                        }
                        else if (g_variant_is_of_type(opt->def, G_VARIANT_TYPE("s"))) {
                            new_value = g_variant_new_string(vs.toUtf8().data());
                        }

                        if (new_value != NULL){
                            dec->set_option(opt->id, new_value);
                        }
                    }
                }
                dec->commit();

                if (dec_obj.contains("show")) {
                    QJsonObject show_obj = dec_obj["show"].toObject();
                    if (show_obj.contains(d->id)) {
                        dec->show(show_obj[d->id].toBool());
                    }
                }
            }

            // Restore the binded channel index
            if (bind_indexs.size() > 0){
                // TODO: adapt — DecoderStack no longer exposes set_index_list;
                // channel binding should be set via the decoder's probe map API.
                // auto dec_trace = _session->get_decoder_trace(dec_index);
                // if (dec_trace != NULL) dec_trace->set_index_list(bind_indexs);
            }

            int decoder_cfg_version = -1;

            if (dec_obj.contains("version")){
                decoder_cfg_version = dec_obj["version"].toInt();
            }

            if (dec_obj.contains("show")) {
                QJsonObject show_obj = dec_obj["show"].toObject();
                std::map<const pv::data::decode::Row, bool> rows = stack->get_rows_gshow();

                for (auto i = rows.begin();i != rows.end(); i++) {
                    QString key;

                    if (decoder_cfg_version == -1)
                        key = (*i).first.title();
                    else
                        key = (*i).first.title_id();

                    if (show_obj.contains(key)) {
                        bool bShow = show_obj[key].toBool();
                        const pv::data::decode::Row r = (*i).first;
                        stack->set_rows_gshow(r, bShow);
                    }
                }
            }

            // Call frame_ended() to set _options_changed flag, allowing decode to work properly
            new_dsig->frame_ended();
        }
    }

    return true;
}


double PxcSerializer::get_integer(GVariant *var)
{
    double val = 0;
    const GVariantType *const type = g_variant_get_type(var);
    assert(type);
    if (!type) {
        pxv_warn("StoreSession::get_integer: g_variant_get_type returned NULL.");
        return 0.0;
    }

    if (g_variant_type_equal(type, G_VARIANT_TYPE_BYTE))
        val = g_variant_get_byte(var);
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_INT16))
        val = g_variant_get_int16(var);
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_UINT16))
        val = g_variant_get_uint16(var);
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_INT32))
        val = g_variant_get_int32(var);
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_UINT32))
        val = g_variant_get_uint32(var);
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_INT64))
        val = g_variant_get_int64(var);
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_UINT64))
        val = g_variant_get_uint64(var);
    else
        assert(false);

    return val;
}

QString PxcSerializer::MakeSaveFile(bool bDlg)
{
    SigSession *_session = _store->_session;
    QString default_name;

    AppConfig &app = AppConfig::Instance();
    if (app.userHistory.saveDir != "")
    {
        default_name = app.userHistory.saveDir + "/"  + _session->get_device()->name() + "-";
    }
    else{
        QDir _dir;
        QString _root = _dir.home().path();
        default_name =  _root + "/" + _session->get_device()->name() + "-";
    }

    for (const GSList *l = _session->get_device()->get_device_mode_list(); l; l = l->next)
    {
        const sr_dev_mode *mode = (const sr_dev_mode *)l->data;
        if (_session->get_device()->get_work_mode() == mode->mode) {
            default_name += mode->acronym;
            break;
        }
    }

    default_name += _session->get_session_time().toString("-yyMMdd-hhmmss");

    // Show the dialog
    if (bDlg)
    {
        default_name = QFileDialog::getSaveFileName(
            NULL,
            L_S(STR_PAGE_MSG, S_ID(IDS_MSG_SAVE_FILE),"Save File"),
            default_name,
            //tr
            "PXView Data (*.pxl)");

        if (default_name.isEmpty())
        {
            return ""; //no select file
        }

        QString _dir_path = path::GetDirectoryName(default_name);

        if (_dir_path != app.userHistory.saveDir)
        {
            app.userHistory.saveDir = _dir_path;
            app.SaveHistory();
        }
    }

    QFileInfo f(default_name);
    if (f.suffix().compare("pxl"))
    {
        //Tr
        default_name.append(".pxl");
    }
    _store->_file_name = default_name;
    return default_name;
}

void PxcSerializer::MakeChunkName(char *chunk_name, int chunk_num, int index, int type, int version)
{
    chunk_name[0] = 0;

    if (version >= 2)
    {
        const char *type_name = NULL;
        type_name = (type == SR_CHANNEL_LOGIC) ? "L" : (type == SR_CHANNEL_DSO)  ? "O"
                                                   : (type == SR_CHANNEL_ANALOG) ? "A"
                                                                                 : "U";
        snprintf(chunk_name, 15, "%s-%d/%d", type_name, index, chunk_num);
    }
    else
    {
        snprintf(chunk_name, 15, "data");
    }
}

} // pv
