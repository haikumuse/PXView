#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <Python.h>
#include "libsigrokdecode.h"
#include "libsigrokdecode-internal.h"
#include "log.h"

extern GSList *pd_list;

static struct srd_pd_callback *srd_pd_output_callback_find_c(struct srd_session *sess, int output_type)
{
    GSList *l;
    struct srd_pd_callback *cb;

    if (!sess)
        return NULL;

    for (l = sess->callbacks; l; l = l->next) {
        cb = l->data;
        if (cb->output_type == output_type)
            return cb;
    }

    return NULL;
}

SRD_API int c_decoder_put(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    int output_id, struct srd_c_annotation *ann)
{
    struct srd_pd_output *pdo;
    struct srd_pd_callback *cb;
    struct srd_proto_data pdata;
    struct srd_proto_data_annotation pda;
    GSList *l;

    if (!di)
        return SRD_ERR_ARG;

    GSList *out_list = g_slist_nth(di->pd_output, output_id);
    if (!out_list) {
        srd_err("C decoder %s submitted invalid output ID %d.",
            di->c_dec_inst->name, output_id);
        return SRD_ERR_ARG;
    }
    pdo = out_list->data;

    pdata.start_sample = start_sample;
    pdata.end_sample = end_sample;
    pdata.pdo = pdo;
    pdata.data = NULL;

    switch (pdo->output_type) {
    case SRD_OUTPUT_ANN:
        if ((cb = srd_pd_output_callback_find_c(di->sess, pdo->output_type))) {
            pdata.data = &pda;
            memset(&pda, 0, sizeof(pda));
            pda.ann_class = ann->ann_class;
            pda.ann_text = ann->ann_text;
            cb->cb(&pdata, cb->cb_data);
        }
        break;

    case SRD_OUTPUT_PYTHON:
        for (l = di->next_di; l; l = l->next) {
            struct srd_decoder_inst *next_di = l->data;
            if (next_di->py_inst) {
                PyGILState_STATE gstate = PyGILState_Ensure();
                PyObject *py_data = Py_BuildValue("(KK)", start_sample, end_sample);
                PyObject *py_res = PyObject_CallMethod(
                    next_di->py_inst, "decode", "KKO",
                    start_sample, end_sample, py_data);
                Py_XDECREF(py_res);
                Py_XDECREF(py_data);
                PyGILState_Release(gstate);
            }
        }
        if ((cb = srd_pd_output_callback_find_c(di->sess, pdo->output_type))) {
            pdata.data = ann;
            cb->cb(&pdata, cb->cb_data);
        }
        break;

    case SRD_OUTPUT_BINARY:
    case SRD_OUTPUT_META:
        if ((cb = srd_pd_output_callback_find_c(di->sess, pdo->output_type))) {
            pdata.data = ann;
            cb->cb(&pdata, cb->cb_data);
        }
        break;

    default:
        srd_err("C decoder %s submitted invalid output type %d.",
            di->c_dec_inst->name, pdo->output_type);
        return SRD_ERR_ARG;
    }

    return SRD_OK;
}

SRD_API int c_decoder_wait(struct srd_decoder_inst *di,
    GSList *condition_list, uint64_t *samplenum, uint64_t *matched)
{
    if (!di)
        return SRD_ERR_ARG;

    if (condition_list) {
        if (di->condition_list) {
            g_slist_free_full(di->condition_list, g_free);
        }
        di->condition_list = condition_list;
    }

    while (1) {
        g_mutex_lock(&di->data_mutex);

        while (!di->got_new_samples && !di->want_wait_terminate)
            g_cond_wait(&di->got_new_samples_cond, &di->data_mutex);

        if (di->want_wait_terminate) {
            g_mutex_unlock(&di->data_mutex);
            return SRD_ERR_TERM_REQ;
        }

        di->got_new_samples = FALSE;

        gboolean found = FALSE;
        uint64_t i = di->abs_start_samplenum;

        if (!di->condition_list) {
            if (!di->first_pos && di->abs_cur_samplenum)
                i = di->abs_cur_samplenum + 1;
            found = TRUE;
        } else {
            for (; i < di->abs_end_samplenum; i++) {
                uint64_t cur_match = 0;
                GSList *cond;
                int cond_idx = 0;
                for (cond = di->condition_list; cond; cond = cond->next, cond_idx++) {
                    GSList *terms = cond->data;
                    gboolean all_match = TRUE;
                    GSList *term;
                    for (term = terms; term; term = term->next) {
                        struct srd_term *t = term->data;
                        int ch = t->channel;
                        if (ch >= 0 && ch < di->dec_num_channels) {
                            int sig_idx = di->dec_channelmap[ch];
                            if (sig_idx >= 0 && di->inbuf && di->inbuf[sig_idx]) {
                                uint64_t byte_offset = i / 8;
                                uint8_t bit_offset = i % 8;
                                uint8_t val = (di->inbuf[sig_idx][byte_offset] >> bit_offset) & 1;
                                if (t->type == SRD_TERM_SKIP) {
                                    continue;
                                } else if (t->type == SRD_TERM_HIGH && val != 1) {
                                    all_match = FALSE;
                                    break;
                                } else if (t->type == SRD_TERM_LOW && val != 0) {
                                    all_match = FALSE;
                                    break;
                                } else if (t->type == SRD_TERM_RISING_EDGE) {
                                    uint8_t old_val = 0;
                                    if (i > 0) {
                                        uint64_t prev_byte = (i - 1) / 8;
                                        uint8_t prev_bit = (i - 1) % 8;
                                        old_val = (di->inbuf[sig_idx][prev_byte] >> prev_bit) & 1;
                                    }
                                    if (!(old_val == 0 && val == 1)) {
                                        all_match = FALSE;
                                        break;
                                    }
                                } else if (t->type == SRD_TERM_FALLING_EDGE) {
                                    uint8_t old_val = 0;
                                    if (i > 0) {
                                        uint64_t prev_byte = (i - 1) / 8;
                                        uint8_t prev_bit = (i - 1) % 8;
                                        old_val = (di->inbuf[sig_idx][prev_byte] >> prev_bit) & 1;
                                    }
                                    if (!(old_val == 1 && val == 0)) {
                                        all_match = FALSE;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    if (all_match) {
                        cur_match |= (1ULL << cond_idx);
                        found = TRUE;
                    }
                }
                if (found) {
                    di->abs_cur_samplenum = i;
                    di->match_array = cur_match;
                    break;
                }
            }
        }

        if (found) {
            di->first_pos = FALSE;
            if (samplenum)
                *samplenum = di->abs_cur_samplenum;
            if (matched)
                *matched = di->match_array;
            g_mutex_unlock(&di->data_mutex);
            return SRD_OK;
        }

        di->handled_all_samples = TRUE;
        g_cond_signal(&di->handled_all_samples_cond);
        g_mutex_unlock(&di->data_mutex);
    }

    return SRD_OK;
}

SRD_API int c_decoder_has_channel(struct srd_decoder_inst *di, int ch)
{
    if (!di || ch < 0 || ch >= di->dec_num_channels)
        return 0;
    return (di->dec_channelmap[ch] >= 0) ? 1 : 0;
}

SRD_API int c_decoder_register_output(struct srd_decoder_inst *di,
    int output_type, const char *proto_id)
{
    struct srd_pd_output *pdo;

    if (!di)
        return SRD_ERR_ARG;

    pdo = g_malloc(sizeof(struct srd_pd_output));
    pdo->pdo_id = g_slist_length(di->pd_output);
    pdo->output_type = output_type;
    pdo->di = di;
    pdo->proto_id = g_strdup(proto_id ? proto_id : "");

    di->pd_output = g_slist_append(di->pd_output, pdo);

    return pdo->pdo_id;
}
