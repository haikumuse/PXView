#ifndef C_DECODER_UTILS_H
#define C_DECODER_UTILS_H

static inline uint8_t c_decoder_get_pin(struct srd_decoder_inst *di, int ch, uint64_t samplenum)
{
    if (ch < 0 || ch >= di->dec_num_channels)
        return 0;
    int sig_idx = di->dec_channelmap[ch];
    if (sig_idx < 0 || !di->inbuf || !di->inbuf[sig_idx])
        return 0;
    uint64_t byte_offset = samplenum / 8;
    uint8_t bit_offset = samplenum % 8;
    return (di->inbuf[sig_idx][byte_offset] >> bit_offset) & 1;
}

#endif
