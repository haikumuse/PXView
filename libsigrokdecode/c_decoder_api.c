#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

#ifdef SRD_C_DECODER_DLL
  #define _srd_err(fmt, ...) fprintf(stderr, "libsigrokdecode: " fmt "\n", ##__VA_ARGS__)
#else
  #include <Python.h>
  #include "libsigrokdecode-internal.h"
  #include "log.h"
  #define _srd_err srd_err
#endif

#ifndef SRD_C_DECODER_DLL
extern GSList *pd_list;
#endif

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

    if (!di)
        return SRD_ERR_ARG;

    GSList *out_list = g_slist_nth(di->pd_output, output_id);
    if (!out_list) {
        _srd_err("C decoder %s submitted invalid output ID %d.",
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
            if (ann->ann_class >= 0 && ann->ann_class < (int)g_slist_length(di->decoder->ann_types)) {
                pda.ann_type = GPOINTER_TO_INT(g_slist_nth_data(di->decoder->ann_types, ann->ann_class));
            } else {
                pda.ann_type = ann->ann_class;
            }
            pda.ann_text = ann->ann_text;
            strncpy(pda.str_number_hex, ann->str_number_hex, DECODE_NUM_HEX_MAX_LEN - 1);
            pda.str_number_hex[DECODE_NUM_HEX_MAX_LEN - 1] = '\0';
            pda.numberic_value = ann->numberic_value;
            cb->cb(&pdata, cb->cb_data);
        }
        break;

    case SRD_OUTPUT_PYTHON:
        _srd_err("C decoder %s: SRD_OUTPUT_PYTHON output is not fully "
                 "compatible with Python decoder stack. Consider using "
                 "SRD_OUTPUT_ANN instead.", di->c_dec_inst->name);
        if ((cb = srd_pd_output_callback_find_c(di->sess, SRD_OUTPUT_ANN))) {
            pdata.data = &pda;
            memset(&pda, 0, sizeof(pda));
            pda.ann_class = ann ? ann->ann_class : 0;
            pda.ann_text = ann ? ann->ann_text : NULL;
            cb->cb(&pdata, cb->cb_data);
        }
        if ((cb = srd_pd_output_callback_find_c(di->sess, pdo->output_type))) {
            pdata.data = ann;
            cb->cb(&pdata, cb->cb_data);
        }
        break;

    case SRD_OUTPUT_BINARY:
        _srd_err("C decoder %s: Use c_decoder_put_binary() for BINARY output "
                 "instead of c_decoder_put().", di->c_dec_inst->name);
        return SRD_ERR_ARG;
    case SRD_OUTPUT_META:
        if ((cb = srd_pd_output_callback_find_c(di->sess, pdo->output_type))) {
            pdata.data = ann;
            cb->cb(&pdata, cb->cb_data);
        }
        break;

    default:
        _srd_err("C decoder %s submitted invalid output type %d.",
            di->c_dec_inst->name, pdo->output_type);
        return SRD_ERR_ARG;
    }

    return SRD_OK;
}

SRD_API int c_decoder_put_binary(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    int output_id, int bin_class, uint64_t size, const unsigned char *data)
{
    struct srd_pd_output *pdo;
    struct srd_pd_callback *cb;
    struct srd_proto_data pdata;
    struct srd_proto_data_binary pdb;

    if (!di)
        return SRD_ERR_ARG;

    GSList *out_list = g_slist_nth(di->pd_output, output_id);
    if (!out_list) {
        _srd_err("C decoder %s submitted invalid output ID %d.",
            di->c_dec_inst->name, output_id);
        return SRD_ERR_ARG;
    }
    pdo = out_list->data;

    if (pdo->output_type != SRD_OUTPUT_BINARY) {
        _srd_err("C decoder %s: c_decoder_put_binary() called for non-BINARY output type %d.",
            di->c_dec_inst->name, pdo->output_type);
        return SRD_ERR_ARG;
    }

    pdata.start_sample = start_sample;
    pdata.end_sample = end_sample;
    pdata.pdo = pdo;

    if ((cb = srd_pd_output_callback_find_c(di->sess, SRD_OUTPUT_BINARY))) {
        pdb.bin_class = bin_class;
        pdb.size = size;
        pdb.data = data;
        pdata.data = &pdb;
        cb->cb(&pdata, cb->cb_data);
    }

    return SRD_OK;
}

SRD_API int c_decoder_wait(struct srd_decoder_inst *di,
    GSList *condition_list, uint64_t *samplenum, uint64_t *matched)
{
    if (!di)
        return SRD_ERR_ARG;

    if (di->runtime && di->runtime->wait)
        return di->runtime->wait(di, condition_list, samplenum, matched);

    return SRD_ERR_ARG;
}

SRD_API uint8_t c_decoder_get_pin(struct srd_decoder_inst *di, int ch, uint64_t samplenum)
{
    if (!di)
        return 0;

    if (di->runtime && di->runtime->get_pin)
        return di->runtime->get_pin(di, ch, samplenum);

    return 0;
}

SRD_API void *c_decoder_get_private(struct srd_decoder_inst *di)
{
    if (!di)
        return NULL;

    if (di->runtime && di->runtime->get_private)
        return di->runtime->get_private(di);

    return di->user_data;
}

SRD_API void c_decoder_set_private(struct srd_decoder_inst *di, void *data)
{
    if (!di)
        return;

    if (di->runtime && di->runtime->set_private) {
        di->runtime->set_private(di, data);
        return;
    }

    di->user_data = data;
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

    pdo = g_malloc0(sizeof(struct srd_pd_output));
    pdo->pdo_id = g_slist_length(di->pd_output);
    pdo->output_type = output_type;
    pdo->di = di;
    pdo->proto_id = g_strdup(proto_id ? proto_id : "");

    if (output_type == SRD_OUTPUT_PYTHON) {
        _srd_err("C decoder %s: Registering SRD_OUTPUT_PYTHON output. "
                 "This output type cannot be properly consumed by "
                 "upper-layer Python decoders.", di->c_dec_inst->name);
    }

    di->pd_output = g_slist_append(di->pd_output, pdo);

    return pdo->pdo_id;
}

SRD_API uint64_t c_decoder_get_samplerate(struct srd_decoder_inst *di)
{
    if (!di)
        return 0;
    return di->samplerate;
}

SRD_API int64_t c_decoder_get_option_int(struct srd_decoder_inst *di,
    const char *key, int64_t defval)
{
    if (!di || !di->c_options || !key)
        return defval;
    GVariant *val = g_hash_table_lookup(di->c_options, key);
    if (!val)
        return defval;
    if (g_variant_is_of_type(val, G_VARIANT_TYPE_INT64))
        return g_variant_get_int64(val);
    if (g_variant_is_of_type(val, G_VARIANT_TYPE_UINT64))
        return (int64_t)g_variant_get_uint64(val);
    if (g_variant_is_of_type(val, G_VARIANT_TYPE_INT32))
        return (int64_t)g_variant_get_int32(val);
    if (g_variant_is_of_type(val, G_VARIANT_TYPE_UINT32))
        return (int64_t)g_variant_get_uint32(val);
    if (g_variant_is_of_type(val, G_VARIANT_TYPE_DOUBLE))
        return (int64_t)g_variant_get_double(val);
    return defval;
}

SRD_API double c_decoder_get_option_double(struct srd_decoder_inst *di,
    const char *key, double defval)
{
    if (!di || !di->c_options || !key)
        return defval;
    GVariant *val = g_hash_table_lookup(di->c_options, key);
    if (!val)
        return defval;
    if (g_variant_is_of_type(val, G_VARIANT_TYPE_DOUBLE))
        return g_variant_get_double(val);
    if (g_variant_is_of_type(val, G_VARIANT_TYPE_INT64))
        return (double)g_variant_get_int64(val);
    if (g_variant_is_of_type(val, G_VARIANT_TYPE_UINT64))
        return (double)g_variant_get_uint64(val);
    return defval;
}

SRD_API const char *c_decoder_get_option_string(struct srd_decoder_inst *di,
    const char *key, const char *defval)
{
    if (!di || !di->c_options || !key)
        return defval;
    GVariant *val = g_hash_table_lookup(di->c_options, key);
    if (!val)
        return defval;
    if (g_variant_is_of_type(val, G_VARIANT_TYPE_STRING))
        return g_variant_get_string(val, NULL);
    return defval;
}

SRD_API int c_decoder_register_output_meta(struct srd_decoder_inst *di,
    int output_type, const char *proto_id,
    const char *meta_type_str, const char *meta_name, const char *meta_descr)
{
    struct srd_pd_output *pdo;
    int pdo_id;

    if (!di)
        return SRD_ERR_ARG;

    pdo_id = c_decoder_register_output(di, output_type, proto_id);
    if (pdo_id < 0)
        return pdo_id;

    GSList *out_list = g_slist_nth(di->pd_output, pdo_id);
    if (!out_list)
        return SRD_ERR_ARG;

    pdo = out_list->data;
    if (meta_type_str) {
        if (strcmp(meta_type_str, "int") == 0)
            pdo->meta_type = G_VARIANT_TYPE_INT64;
        else if (strcmp(meta_type_str, "float") == 0 || strcmp(meta_type_str, "double") == 0)
            pdo->meta_type = G_VARIANT_TYPE_DOUBLE;
    }
    pdo->meta_name = g_strdup(meta_name ? meta_name : "");
    pdo->meta_descr = g_strdup(meta_descr ? meta_descr : "");

    return pdo_id;
}

SRD_API int c_decoder_put_meta_int(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    int output_id, int64_t value)
{
    struct srd_pd_output *pdo;
    struct srd_pd_callback *cb;
    struct srd_proto_data pdata;
    struct srd_proto_data_meta pdm;

    if (!di)
        return SRD_ERR_ARG;

    GSList *out_list = g_slist_nth(di->pd_output, output_id);
    if (!out_list)
        return SRD_ERR_ARG;
    pdo = out_list->data;

    pdata.start_sample = start_sample;
    pdata.end_sample = end_sample;
    pdata.pdo = pdo;
    pdata.data = &pdm;
    pdm.key = pdo->pdo_id;
    pdm.value = g_variant_new_int64(value);

    if ((cb = srd_pd_output_callback_find_c(di->sess, SRD_OUTPUT_META))) {
        cb->cb(&pdata, cb->cb_data);
    }

    g_variant_unref(pdm.value);
    return SRD_OK;
}

SRD_API int c_decoder_put_meta_double(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    int output_id, double value)
{
    struct srd_pd_output *pdo;
    struct srd_pd_callback *cb;
    struct srd_proto_data pdata;
    struct srd_proto_data_meta pdm;

    if (!di)
        return SRD_ERR_ARG;

    GSList *out_list = g_slist_nth(di->pd_output, output_id);
    if (!out_list)
        return SRD_ERR_ARG;
    pdo = out_list->data;

    pdata.start_sample = start_sample;
    pdata.end_sample = end_sample;
    pdata.pdo = pdo;
    pdata.data = &pdm;
    pdm.key = pdo->pdo_id;
    pdm.value = g_variant_new_double(value);

    if ((cb = srd_pd_output_callback_find_c(di->sess, SRD_OUTPUT_META))) {
        cb->cb(&pdata, cb->cb_data);
    }

    g_variant_unref(pdm.value);
    return SRD_OK;
}

SRD_API uint64_t c_decoder_get_last_samplenum(struct srd_decoder_inst *di)
{
    if (!di)
        return 0;
    return di->last_samplenum;
}

SRD_API int c_decoder_put_python(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    int output_id, const char *cmd, const unsigned char *data, uint64_t data_len)
{
    struct srd_pd_output *pdo;
    struct srd_pd_callback *cb;
    struct srd_proto_data pdata;
    struct srd_proto_data_annotation pda;

    if (!di)
        return SRD_ERR_ARG;

    GSList *out_list = g_slist_nth(di->pd_output, output_id);
    if (!out_list)
        return SRD_ERR_ARG;
    pdo = out_list->data;

    pdata.start_sample = start_sample;
    pdata.end_sample = end_sample;
    pdata.pdo = pdo;
    pdata.data = NULL;

    if (pdo->output_type == SRD_OUTPUT_PYTHON) {
        if (di->next_di) {
            GSList *l;
            for (l = di->next_di; l; l = l->next) {
                struct srd_decoder_inst *next_di = l->data;
                if (next_di->is_c_inst && next_di->c_dec_inst) {
                    if (next_di->c_dec_inst->recv_proto) {
                        next_di->c_dec_inst->recv_proto(next_di, start_sample, end_sample, cmd, data, data_len);
                    }
                }
            }
        }

        if ((cb = srd_pd_output_callback_find_c(di->sess, SRD_OUTPUT_ANN))) {
            pdata.data = &pda;
            memset(&pda, 0, sizeof(pda));
            char ann_text[256];
            if (data && data_len > 0)
                snprintf(ann_text, sizeof(ann_text), "%s: 0x%02X", cmd ? cmd : "", data[0]);
            else
                snprintf(ann_text, sizeof(ann_text), "%s", cmd ? cmd : "");
            char *ann_texts[] = {ann_text, NULL};
            pda.ann_class = 0;
            pda.ann_type = 0;
            pda.ann_text = ann_texts;
            cb->cb(&pdata, cb->cb_data);
        }
    }

    return SRD_OK;
}

struct srd_cond_builder {
    GSList *or_groups;
    GSList *current_and;
    gboolean waited;
};

SRD_API srd_cond_builder *c_cond_new(void)
{
    srd_cond_builder *b = g_malloc0(sizeof(srd_cond_builder));
    return b;
}

static srd_cond_builder *c_cond_add_term(srd_cond_builder *b, int type, int ch)
{
    struct srd_term *t;
    if (!b)
        return NULL;
    t = g_malloc0(sizeof(struct srd_term));
    t->type = type;
    t->channel = ch;
    b->current_and = g_slist_append(b->current_and, t);
    return b;
}

SRD_API srd_cond_builder *c_cond_rise(srd_cond_builder *b, int ch)
{
    return c_cond_add_term(b, SRD_TERM_RISING_EDGE, ch);
}

SRD_API srd_cond_builder *c_cond_fall(srd_cond_builder *b, int ch)
{
    return c_cond_add_term(b, SRD_TERM_FALLING_EDGE, ch);
}

SRD_API srd_cond_builder *c_cond_high(srd_cond_builder *b, int ch)
{
    return c_cond_add_term(b, SRD_TERM_HIGH, ch);
}

SRD_API srd_cond_builder *c_cond_low(srd_cond_builder *b, int ch)
{
    return c_cond_add_term(b, SRD_TERM_LOW, ch);
}

SRD_API srd_cond_builder *c_cond_edge(srd_cond_builder *b, int ch)
{
    return c_cond_add_term(b, SRD_TERM_EITHER_EDGE, ch);
}

SRD_API srd_cond_builder *c_cond_noedge(srd_cond_builder *b, int ch)
{
    return c_cond_add_term(b, SRD_TERM_NO_EDGE, ch);
}

SRD_API srd_cond_builder *c_cond_skip(srd_cond_builder *b, uint64_t count)
{
    struct srd_term *t;
    if (!b)
        return NULL;
    t = g_malloc0(sizeof(struct srd_term));
    t->type = SRD_TERM_SKIP;
    t->channel = -1;
    t->num_samples_to_skip = count;
    t->num_samples_already_skipped = 0;
    b->current_and = g_slist_append(b->current_and, t);
    return b;
}

SRD_API srd_cond_builder *c_cond_or(srd_cond_builder *b)
{
    if (!b)
        return NULL;
    if (b->current_and) {
        b->or_groups = g_slist_append(b->or_groups, b->current_and);
        b->current_and = NULL;
    }
    return b;
}

SRD_API int c_cond_wait(srd_cond_builder *b, struct srd_decoder_inst *di,
    uint64_t *samplenum, uint64_t *matched)
{
    int ret;

    if (!b || !di)
        return SRD_ERR_ARG;

    if (b->waited) {
        _srd_err("c_cond_wait() called on a builder that was already used. "
                 "Create a new builder for each wait call.");
        return SRD_ERR_ARG;
    }

    if (b->current_and) {
        b->or_groups = g_slist_append(b->or_groups, b->current_and);
        b->current_and = NULL;
    }

    ret = c_decoder_wait(di, b->or_groups, samplenum, matched);

    b->or_groups = NULL;
    b->waited = TRUE;

    return ret;
}

static void c_cond_free_term_list(gpointer data)
{
    g_slist_free_full((GSList *)data, g_free);
}

SRD_API void c_cond_free(srd_cond_builder *b)
{
    if (!b)
        return;
    if (b->current_and)
        g_slist_free_full(b->current_and, g_free);
    if (b->or_groups)
        g_slist_free_full(b->or_groups, c_cond_free_term_list);
    g_free(b);
}
