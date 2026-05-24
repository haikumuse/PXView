#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum morse_ann {
    ANN_TIME = 0,
    ANN_UNITS,
    ANN_SYMBOL,
    ANN_LETTER,
    ANN_WORD,
    NUM_ANN,
};

typedef struct {
    const char *code;   /* e.g. ".-" */
    const char *letter; /* e.g. "a" */
} morse_entry;

static const morse_entry morse_alphabet[] = {
    {".-", "a"}, {"-...", "b"}, {"-.-.", "c"}, {"-..", "d"},
    {".", "e"}, {"..-.", "f"}, {"--.", "g"}, {"....", "h"},
    {"..", "i"}, {".---", "j"}, {"-.-", "k"}, {".-..", "l"},
    {"--", "m"}, {"-.", "n"}, {"---", "o"}, {".--.", "p"},
    {"--.-", "q"}, {".-.", "r"}, {"...", "s"}, {"-", "t"},
    {"..-", "u"}, {"...-", "v"}, {".--", "w"}, {"-..-", "x"},
    {"-.--", "y"}, {"--..", "z"},
    {".----", "1"}, {"..---", "2"}, {"...--", "3"}, {"....-", "4"},
    {".....", "5"}, {"-....", "6"}, {"--...", "7"}, {"---..", "8"},
    {"----.", "9"}, {"-----", "0"},
    {".-.-.-", "."}, {"--..--", ","}, {"..--..", "?"},
    {".----.", "'"}, {"-.-.--", "!"}, {"-..-.", "/"},
    {"-.--.", "("}, {"-.--.-", ")"}, {".-...", "&"},
    {"---...", ":"}, {"-.-.-.", ";"}, {"-...-", "="},
    {".-.-.", "+"}, {"-....-", "-"}, {"..--.-", "_"},
    {".-..-.", "\""}, {"...-..-", "$"}, {".--.-.", "@"},
    {NULL, NULL}  /* sentinel */
};

struct morse_priv {
    uint64_t samplerate;
    double timeunit;
    int out_ann;
    /* symbol decoding state */
    int prev_val;
    uint64_t prev_time;
    /* letter decoding state */
    uint8_t sequence[16];   /* dit=1, dah=3 */
    int seq_len;
    uint64_t letter_ss, letter_es;
    /* word decoding state */
    char word[256];
    int word_len;
    uint64_t word_ss, word_es;
};

static struct srd_channel morse_channels[] = {
    {"data", "Data", "Data line", 0, SRD_CHANNEL_SDATA, "dec_morse_chan_data"},
};

static struct srd_decoder_option morse_options[] = {
    {"timeunit", "dec_morse_opt_timeunit", "Time unit (guess)", NULL, NULL},
};

static const char *morse_ann_labels[][3] = {
    {"", "time", "Time"},
    {"", "units", "Units"},
    {"", "symbol", "Symbol"},
    {"", "letter", "Letter"},
    {"", "word", "Word"},
};

static const int morse_row_time_classes[] = {ANN_TIME, -1};
static const int morse_row_units_classes[] = {ANN_UNITS, -1};
static const int morse_row_symbol_classes[] = {ANN_SYMBOL, -1};
static const int morse_row_letter_classes[] = {ANN_LETTER, -1};
static const int morse_row_word_classes[] = {ANN_WORD, -1};

static const struct srd_c_ann_row morse_ann_rows[] = {
    {"time", "Time", morse_row_time_classes, 1},
    {"units", "Units", morse_row_units_classes, 1},
    {"symbol", "Symbol", morse_row_symbol_classes, 1},
    {"letter", "Letter", morse_row_letter_classes, 1},
    {"word", "Word", morse_row_word_classes, 1},
};

static const char *morse_inputs[] = {"logic"};
static const char *morse_tags[] = {"Encoding"};

static const char *lookup_morse(uint8_t *seq, int seq_len)
{
    /* Convert sequence to code string */
    char code[17] = {0};
    for (int i = 0; i < seq_len && i < 16; i++)
        code[i] = (seq[i] == 1) ? '.' : '-';
    /* Linear search in alphabet */
    for (int i = 0; morse_alphabet[i].code != NULL; i++) {
        if (strcmp(code, morse_alphabet[i].code) == 0)
            return morse_alphabet[i].letter;
    }
    return NULL; /* unknown */
}

static void flush_letter(struct srd_decoder_inst *di, struct morse_priv *s)
{
    if (s->seq_len == 0)
        return;

    const char *letter = lookup_morse(s->sequence, s->seq_len);
    if (letter) {
        C_ANN_PUT(di, s->letter_ss, s->letter_es, s->out_ann, ANN_LETTER, letter);
        /* Add to word */
        if (s->word_len < 250) {
            int len = strlen(letter);
            if (s->word_len + len < 250) {
                memcpy(s->word + s->word_len, letter, len);
                s->word_len += len;
            }
        }
    } else {
        /* Unknown letter - output ditdah representation */
        char ditdah[17] = {0};
        for (int i = 0; i < s->seq_len && i < 16; i++)
            ditdah[i] = (s->sequence[i] == 1) ? '.' : '-';
        C_ANN_PUT(di, s->letter_ss, s->letter_es, s->out_ann, ANN_LETTER, ditdah);
        if (s->word_len + s->seq_len < 250) {
            memcpy(s->word + s->word_len, ditdah, s->seq_len);
            s->word_len += s->seq_len;
        }
    }

    s->seq_len = 0;
}

static void flush_word(struct srd_decoder_inst *di, struct morse_priv *s)
{
    flush_letter(di, s);
    if (s->word_len == 0)
        return;

    s->word[s->word_len] = '\0';
    C_ANN_PUT(di, s->word_ss, s->word_es, s->out_ann, ANN_WORD, s->word);
    s->word_len = 0;
}

static void process_symbol(struct srd_decoder_inst *di, struct morse_priv *s,
                           int sval, int sunits, uint64_t ss, uint64_t es)
{
    if (sval == 1) {
        /* Mark: dit (1 unit) or dah (3 units) */
        int sym = (sunits <= 2) ? 1 : 3;
        const char *sym_name = (sym == 1) ? "dit" : "dah";
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_SYMBOL, sym_name);

        if (s->seq_len == 0)
            s->letter_ss = ss;
        s->letter_es = es;

        if (s->seq_len < 16)
            s->sequence[s->seq_len++] = (uint8_t)sym;
    } else {
        /* Space: intra-char gap (1 unit), letter gap (3 units), word gap (7 units) */
        if (sunits >= 7) {
            /* Word gap */
            C_ANN_PUT(di, ss, es, s->out_ann, ANN_SYMBOL, "word gap");
            flush_word(di, s);
        } else if (sunits >= 3) {
            /* Letter gap */
            C_ANN_PUT(di, ss, es, s->out_ann, ANN_SYMBOL, "letter gap");
            flush_letter(di, s);
        } else {
            /* Intra-character gap */
            C_ANN_PUT(di, ss, es, s->out_ann, ANN_SYMBOL, "intra-char gap");
        }
    }
}

static void morse_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di))
        c_decoder_set_private(di, g_malloc0(sizeof(struct morse_priv)));
    struct morse_priv *s = (struct morse_priv *)c_decoder_get_private(di);
    memset(s, 0, sizeof(struct morse_priv));
    s->out_ann = -1;
    s->timeunit = 0.1;  /* default 0.1 seconds */
    s->prev_val = -1;
}

static void morse_start(struct srd_decoder_inst *di)
{
    struct morse_priv *s = (struct morse_priv *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "morse");

    double tu = c_decoder_get_option_double(di, "timeunit", 0.1);
    if (tu > 0)
        s->timeunit = tu;
}

static void morse_metadata(struct srd_decoder_inst *di, int key, uint64_t value)
{
    struct morse_priv *s = (struct morse_priv *)c_decoder_get_private(di);
    if (key == SRD_CONF_SAMPLERATE)
        s->samplerate = value;
}

static void morse_decode(struct srd_decoder_inst *di)
{
    struct morse_priv *s = (struct morse_priv *)c_decoder_get_private(di);
    uint64_t samplenum = 0;
    uint64_t matched = 0;

    if (!s->samplerate)
        s->samplerate = c_decoder_get_samplerate(di);
    if (!s->samplerate)
        s->samplerate = 1;  /* fallback */

    /* Wait for first rising edge */
    srd_cond_builder *cb = c_cond_new();
    c_cond_rise(cb, 0);
    int ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK)
        return;

    s->prev_time = samplenum;
    s->prev_val = 1;
    s->word_ss = samplenum;
    s->word_es = samplenum;

    while (1) {
        /* Wait for edge or timeout */
        cb = c_cond_new();
        c_cond_edge(cb, 0);
        c_cond_or(cb);
        uint64_t timeout = (uint64_t)(5.0 * (double)s->samplerate * s->timeunit);
        if (timeout == 0) timeout = 1;
        c_cond_skip(cb, timeout);
        ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK) {
            flush_word(di, s);
            return;
        }

        int is_timeout = (matched & (1ULL << 1)) && !(matched & (1ULL << 0));

        int val = c_decoder_get_pin(di, 0, samplenum);
        int pval = s->prev_val;
        uint64_t curtime = samplenum;

        if (is_timeout) {
            /* Timeout - flush word */
            flush_word(di, s);
            s->prev_time = curtime;
            s->prev_val = val;
            s->word_ss = curtime;
            continue;
        }

        double dt = (double)(curtime - s->prev_time) / (double)s->samplerate;
        double units = dt / s->timeunit;
        int iunits = (int)(round(units));
        if (iunits < 1)
            iunits = 1;
        double error = fabs(units - iunits);

        /* Output time annotation */
        char time_str[32];
        snprintf(time_str, sizeof(time_str), "%.3g s", dt);
        C_ANN_PUT(di, s->prev_time, curtime, s->out_ann, ANN_TIME, time_str);

        /* Check symbol validity */
        int sval = pval;
        int sunits = iunits;

        if ((sval == 1 && (sunits == 1 || sunits == 3)) ||
            (sval == 0 && (sunits == 1 || sunits == 3 || sunits == 7))) {
            /* Valid symbol */
            char units_str[64];
            snprintf(units_str, sizeof(units_str), "%.1f * %.3g s", units, s->timeunit);
            C_ANN_PUT(di, s->prev_time, curtime, s->out_ann, ANN_UNITS, units_str);

            process_symbol(di, s, sval, sunits, s->prev_time, curtime);
        } else {
            /* Unknown symbol */
            char err_str[64];
            snprintf(err_str, sizeof(err_str), "!! %.1f * %.3g s !!", units, s->timeunit);
            C_ANN_PUT(di, s->prev_time, curtime, s->out_ann, ANN_UNITS, err_str);
        }

        /* Adaptive timeunit */
        double thisunit = dt / (double)iunits;
        double weight = 0.2 * fmax(0.0, 1.0 - 2.0 * error);
        s->timeunit += (thisunit - s->timeunit) * weight;

        s->word_es = curtime;
        s->prev_time = curtime;
        s->prev_val = val;
    }
}

static void morse_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

static struct srd_c_decoder morse_c_decoder = {
    .id = "morse_c",
    .name = "Morse(C)",
    .longname = "Morse code (C)",
    .desc = "Demodulated morse code protocol (C implementation)",
    .license = "gplv2+",
    .channels = morse_channels,
    .num_channels = 1,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = morse_options,
    .num_options = 1,
    .num_annotations = NUM_ANN,
    .ann_labels = morse_ann_labels,
    .num_annotation_rows = 5,
    .annotation_rows = morse_ann_rows,
    .inputs = morse_inputs,
    .num_inputs = 1,
    .outputs = NULL,
    .num_outputs = 0,
    .binary = NULL,
    .num_binary = 0,
    .tags = morse_tags,
    .num_tags = 1,
    .reset = morse_reset,
    .start = morse_start,
    .decode = morse_decode,
    .destroy = morse_destroy,
    .metadata = morse_metadata,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    morse_options[0].def = g_variant_new_double(0.1);
    return &morse_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
