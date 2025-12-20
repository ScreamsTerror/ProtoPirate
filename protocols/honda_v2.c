#include "honda_v2.h"

#define TAG "HondaV2"

// Protocol timing constants based on Honda KR5V2X keyfob (CVE-2022-27254)
// Operating at 433.657MHz and 434.176MHz with custom presets
static const SubGhzBlockConst honda_protocol_v2_const = {
    .te_short = 200,    // Short pulse duration (based on Honda preset analysis)
    .te_long = 400,     // Long pulse duration
    .te_delta = 60,     // Timing tolerance
    .min_count_bit_for_found = 64,  // Honda typically uses 64-bit packets
};

// Decoder structure
struct SubGhzProtocolDecoderHondaV2
{
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;
    uint16_t header_count;

    uint8_t raw_bits[20];
    uint16_t raw_bit_count;
};

// Encoder structure
struct SubGhzProtocolEncoderHondaV2
{
    SubGhzProtocolEncoderBase base;
    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;

    uint32_t serial;
    uint8_t button;
    uint16_t counter;

    bool is_running;
    size_t preamble_count;
    size_t data_bit_index;
    uint8_t last_bit;
    bool send_high;
};

// Decoder states
typedef enum
{
    HondaV2DecoderStepReset = 0,
    HondaV2DecoderStepCheckPreamble,
    HondaV2DecoderStepCollectRawBits,
} HondaV2DecoderStep;

// Protocol registry declarations
const SubGhzProtocolDecoder honda_protocol_v2_decoder = {
    .alloc = honda_protocol_decoder_v2_alloc,
    .free = honda_protocol_decoder_v2_free,
    .feed = honda_protocol_decoder_v2_feed,
    .reset = honda_protocol_decoder_v2_reset,
    .get_hash_data = honda_protocol_decoder_v2_get_hash_data,
    .serialize = honda_protocol_decoder_v2_serialize,
    .deserialize = honda_protocol_decoder_v2_deserialize,
    .get_string = honda_protocol_decoder_v2_get_string,
};

const SubGhzProtocolEncoder honda_protocol_v2_encoder = {
    .alloc = honda_protocol_encoder_v2_alloc,
    .free = honda_protocol_encoder_v2_free,
    .deserialize = honda_protocol_encoder_v2_deserialize,
    .stop = honda_protocol_encoder_v2_stop,
    .yield = honda_protocol_encoder_v2_yield,
};

const SubGhzProtocol honda_protocol_v2 = {
    .name = HONDA_PROTOCOL_V2_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_FM | 
            SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_Save | SubGhzProtocolFlag_Send,
    .decoder = &honda_protocol_v2_decoder,
    .encoder = &honda_protocol_v2_encoder,
};

// Helper function to add raw bits
static void honda_v2_add_raw_bit(SubGhzProtocolDecoderHondaV2* instance, bool bit)
{
    if (instance->raw_bit_count < 160)
    {
        uint16_t byte_idx = instance->raw_bit_count / 8;
        uint8_t bit_idx = 7 - (instance->raw_bit_count % 8);
        if (bit)
        {
            instance->raw_bits[byte_idx] |= (1 << bit_idx);
        }
        else
        {
            instance->raw_bits[byte_idx] &= ~(1 << bit_idx);
        }
        instance->raw_bit_count++;
    }
}

// Helper function to get raw bit
static inline bool honda_v2_get_raw_bit(SubGhzProtocolDecoderHondaV2* instance, uint16_t idx)
{
    uint16_t byte_idx = idx / 8;
    uint8_t bit_idx = 7 - (idx % 8);
    return (instance->raw_bits[byte_idx] >> bit_idx) & 1;
}

// Manchester decode function (Honda KR5V2X uses Manchester encoding)
static bool honda_v2_manchester_decode(SubGhzProtocolDecoderHondaV2* instance)
{
    if (instance->raw_bit_count < 120)
    {
        return false;
    }

    uint16_t best_bits = 0;
    uint64_t best_data = 0;

    for (uint16_t offset = 0; offset < 8; offset++)
    {
        uint64_t data = 0;
        uint16_t decoded_bits = 0;

        for (uint16_t i = offset; i + 1 < instance->raw_bit_count && decoded_bits < 64; i += 2)
        {
            bool bit1 = honda_v2_get_raw_bit(instance, i);
            bool bit2 = honda_v2_get_raw_bit(instance, i + 1);

            uint8_t two_bits = (bit1 << 1) | bit2;

            if (two_bits == 0x02)  // 01 = 1
            {
                data = (data << 1) | 1;
                decoded_bits++;
            }
            else if (two_bits == 0x01)  // 10 = 0
            {
                data = (data << 1);
                decoded_bits++;
            }
            else
            {
                break;
            }
        }

        if (decoded_bits > best_bits)
        {
            best_bits = decoded_bits;
            best_data = data;
        }
    }

    instance->decoder.decode_data = best_data;
    instance->decoder.decode_count_bit = best_bits;

    return best_bits >= honda_protocol_v2_const.min_count_bit_for_found;
}

// Decoder allocation
void* honda_protocol_decoder_v2_alloc(SubGhzEnvironment* environment)
{
    UNUSED(environment);
    SubGhzProtocolDecoderHondaV2* instance = malloc(sizeof(SubGhzProtocolDecoderHondaV2));
    instance->base.protocol = &honda_protocol_v2;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

// Decoder cleanup
void honda_protocol_decoder_v2_free(void* context)
{
    furi_assert(context);
    SubGhzProtocolDecoderHondaV2* instance = context;
    free(instance);
}

// Decoder reset
void honda_protocol_decoder_v2_reset(void* context)
{
    furi_assert(context);
    SubGhzProtocolDecoderHondaV2* instance = context;
    instance->decoder.parser_step = HondaV2DecoderStepReset;
    instance->header_count = 0;
    instance->raw_bit_count = 0;
    memset(instance->raw_bits, 0, sizeof(instance->raw_bits));
}

// Main decoder feed function
void honda_protocol_decoder_v2_feed(void* context, bool level, uint32_t duration)
{
    furi_assert(context);
    SubGhzProtocolDecoderHondaV2* instance = context;

    switch (instance->decoder.parser_step)
    {
    case HondaV2DecoderStepReset:
        if ((level) && (DURATION_DIFF(duration, honda_protocol_v2_const.te_long) <
                        honda_protocol_v2_const.te_delta))
        {
            instance->decoder.parser_step = HondaV2DecoderStepCheckPreamble;
            instance->decoder.te_last = duration;
            instance->header_count = 1;
        }
        break;

    case HondaV2DecoderStepCheckPreamble:
        if (level)
        {
            if (DURATION_DIFF(duration, honda_protocol_v2_const.te_long) <
                honda_protocol_v2_const.te_delta)
            {
                instance->decoder.te_last = duration;
                instance->header_count++;
            }
            else if (
                DURATION_DIFF(duration, honda_protocol_v2_const.te_short) <
                honda_protocol_v2_const.te_delta)
            {
                instance->decoder.te_last = duration;
            }
            else
            {
                instance->decoder.parser_step = HondaV2DecoderStepReset;
            }
        }
        else
        {
            if (DURATION_DIFF(duration, honda_protocol_v2_const.te_long) <
                honda_protocol_v2_const.te_delta)
            {
                instance->header_count++;
            }
            else if (
                DURATION_DIFF(duration, honda_protocol_v2_const.te_short) <
                honda_protocol_v2_const.te_delta)
            {
                if (instance->header_count > 8 &&
                    DURATION_DIFF(instance->decoder.te_last, honda_protocol_v2_const.te_short) <
                        honda_protocol_v2_const.te_delta)
                {
                    instance->decoder.parser_step = HondaV2DecoderStepCollectRawBits;
                    instance->raw_bit_count = 0;
                    memset(instance->raw_bits, 0, sizeof(instance->raw_bits));
                }
            }
            else
            {
                instance->decoder.parser_step = HondaV2DecoderStepReset;
            }
        }
        break;

    case HondaV2DecoderStepCollectRawBits:
        if (duration > 2000)
        {
            if (honda_v2_manchester_decode(instance))
            {
                instance->generic.data = instance->decoder.decode_data;
                instance->generic.data_count_bit = instance->decoder.decode_count_bit;

                // Extract Honda-specific fields
                instance->generic.serial = (uint32_t)((instance->generic.data >> 32) & 0xFFFFFFFF);
                instance->generic.btn = (uint8_t)((instance->generic.data >> 24) & 0xFF);
                instance->generic.cnt = (uint16_t)(instance->generic.data & 0xFFFF);

                if (instance->base.callback)
                    instance->base.callback(&instance->base, instance->base.context);
            }

            instance->decoder.parser_step = HondaV2DecoderStepReset;
            break;
        }

        int num_bits = 0;
        if (DURATION_DIFF(duration, honda_protocol_v2_const.te_short) <
            honda_protocol_v2_const.te_delta)
        {
            num_bits = 1;
        }
        else if (
            DURATION_DIFF(duration, honda_protocol_v2_const.te_long) <
            honda_protocol_v2_const.te_delta)
        {
            num_bits = 2;
        }
        else
        {
            instance->decoder.parser_step = HondaV2DecoderStepReset;
            break;
        }

        for (int i = 0; i < num_bits; i++)
        {
            honda_v2_add_raw_bit(instance, level);
        }

        break;
    }
}

// Get hash data for identification
uint8_t honda_protocol_decoder_v2_get_hash_data(void* context)
{
    furi_assert(context);
    SubGhzProtocolDecoderHondaV2* instance = context;
    return subghz_protocol_blocks_get_hash_data(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

// Serialize data for storage
SubGhzProtocolStatus honda_protocol_decoder_v2_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset)
{
    furi_assert(context);
    SubGhzProtocolDecoderHondaV2* instance = context;

    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;

    ret = subghz_block_generic_serialize(&instance->generic, flipper_format, preset);

    if (ret == SubGhzProtocolStatusOk)
    {
        // Save Honda-specific fields
        flipper_format_write_uint32(flipper_format, "Serial", &instance->generic.serial, 1);

        uint32_t temp = instance->generic.btn;
        flipper_format_write_uint32(flipper_format, "Btn", &temp, 1);

        flipper_format_write_uint32(flipper_format, "Cnt", &instance->generic.cnt, 1);

        // Save full 64-bit data for exact reproduction
        flipper_format_write_hex(
            flipper_format, "Data", (uint8_t*)&instance->generic.data, sizeof(instance->generic.data));
    }

    return ret;
}

// Deserialize data from storage
SubGhzProtocolStatus
honda_protocol_decoder_v2_deserialize(void* context, FlipperFormat* flipper_format)
{
    furi_assert(context);
    SubGhzProtocolDecoderHondaV2* instance = context;
    return subghz_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, honda_protocol_v2_const.min_count_bit_for_found);
}

// Get string representation for display
void honda_protocol_decoder_v2_get_string(void* context, FuriString* output)
{
    furi_assert(context);
    SubGhzProtocolDecoderHondaV2* instance = context;

    uint32_t code_found_hi = (uint32_t)(instance->generic.data >> 32);
    uint32_t code_found_lo = (uint32_t)(instance->generic.data & 0x00000000ffffffff);

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%08lX%08lX\r\n"
        "Sn:%08lX Btn:%02X\r\n"
        "Cnt:%04lX\r\n"
        "Honda KR5V2X",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        code_found_hi,
        code_found_lo,
        instance->generic.serial,
        instance->generic.btn,
        instance->generic.cnt);
}

// Encoder allocation
void* honda_protocol_encoder_v2_alloc(SubGhzEnvironment* environment)
{
    UNUSED(environment);
    SubGhzProtocolEncoderHondaV2* instance = malloc(sizeof(SubGhzProtocolEncoderHondaV2));
    instance->base.protocol = &honda_protocol_v2;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

// Encoder cleanup
void honda_protocol_encoder_v2_free(void* context)
{
    furi_assert(context);
    SubGhzProtocolEncoderHondaV2* instance = context;
    free(instance);
}

// Stop encoding
void honda_protocol_encoder_v2_stop(void* context)
{
    furi_assert(context);
    SubGhzProtocolEncoderHondaV2* instance = context;
    instance->is_running = false;
}

// Generate upload data for transmission
static bool honda_v2_encoder_get_upload(SubGhzProtocolEncoderHondaV2* instance)
{
    furi_assert(instance);

    // Generate Manchester encoded signal for Honda KR5V2X
    size_t index = 0;
    
    // Add preamble (multiple long pulses)
    for (size_t i = 0; i < 12; i++)
    {
        instance->encoder.upload[index++] = 
            level_duration_make(true, (uint32_t)honda_protocol_v2_const.te_long);
        instance->encoder.upload[index++] = 
            level_duration_make(false, (uint32_t)honda_protocol_v2_const.te_long);
    }

    // Add sync pulse
    instance->encoder.upload[index++] = 
        level_duration_make(true, (uint32_t)honda_protocol_v2_const.te_long);
    instance->encoder.upload[index++] = 
        level_duration_make(false, (uint32_t)honda_protocol_v2_const.te_short);

    // Manchester encode the data
    uint64_t data = instance->generic.data;
    
    for (int16_t i = instance->generic.data_count_bit - 1; i >= 0; i--)
    {
        bool bit = (data >> i) & 1;
        
        if (bit)
        {
            // Manchester 1: short high, long low
            instance->encoder.upload[index++] = 
                level_duration_make(true, (uint32_t)honda_protocol_v2_const.te_short);
            instance->encoder.upload[index++] = 
                level_duration_make(false, (uint32_t)honda_protocol_v2_const.te_long);
        }
        else
        {
            // Manchester 0: long high, short low
            instance->encoder.upload[index++] = 
                level_duration_make(true, (uint32_t)honda_protocol_v2_const.te_long);
            instance->encoder.upload[index++] = 
                level_duration_make(false, (uint32_t)honda_protocol_v2_const.te_short);
        }
    }

    // Add final gap
    instance->encoder.upload[index++] = 
        level_duration_make(false, (uint32_t)honda_protocol_v2_const.te_long * 10);

    instance->encoder.size_upload = index;
    return true;
}

// Deserialize for encoder
SubGhzProtocolStatus
honda_protocol_encoder_v2_deserialize(void* context, FlipperFormat* flipper_format)
{
    furi_assert(context);
    SubGhzProtocolEncoderHondaV2* instance = context;

    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;
    
    do
    {
        ret = subghz_block_generic_deserialize(&instance->generic, flipper_format);
        if (ret != SubGhzProtocolStatusOk) break;

        if (instance->generic.data_count_bit != honda_protocol_v2_const.min_count_bit_for_found)
        {
            FURI_LOG_E(TAG, "Wrong number of bits in key");
            ret = SubGhzProtocolStatusError;
            break;
        }

        // Extract Honda-specific fields
        instance->serial = instance->generic.serial;
        instance->button = instance->generic.btn;
        instance->counter = instance->generic.cnt;

        honda_v2_encoder_get_upload(instance);
        instance->is_running = true;

        ret = SubGhzProtocolStatusOk;
    } while (false);

    return ret;
}

// Yield next transmission data
LevelDuration honda_protocol_encoder_v2_yield(void* context)
{
    SubGhzProtocolEncoderHondaV2* instance = context;

    if (!instance || !instance->is_running) {
        return level_duration_reset();
    }
    
    // 64 preamble + 2 sync + 128 data + 1 trailing = 195
    if (instance->preamble_count >= 195) {
        instance->is_running = false;
        instance->preamble_count = 0;
        return level_duration_reset();
    }
    
    LevelDuration result;
    
    // Preamble: 64 transitions (32 long-long pairs)
    if (instance->preamble_count < 64) {
        bool is_high = (instance->preamble_count % 2) == 0;
        result = level_duration_make(is_high, honda_protocol_v2_const.te_long);
    }
    // Sync high
    else if (instance->preamble_count == 64) {
        result = level_duration_make(true, honda_protocol_v2_const.te_long);
    }
    // Sync low
    else if (instance->preamble_count == 65) {
        result = level_duration_make(false, honda_protocol_v2_const.te_short);
    }
    // Data bits: 64 bits (128 transitions, counts 66-193)
    else if (instance->preamble_count < 194) {
        uint32_t data_transition = instance->preamble_count - 66;
        uint32_t bit_num = data_transition / 2;
        bool is_high = (data_transition % 2) == 0;
        
        uint64_t bit_mask = 1ULL << (63 - bit_num);
        uint8_t current_bit = (instance->generic.data & bit_mask) ? 1 : 0;
        
        if (current_bit) {
            // Manchester 1: short high, long low
            uint32_t duration = is_high ? honda_protocol_v2_const.te_short : honda_protocol_v2_const.te_long;
            result = level_duration_make(is_high, duration);
        } else {
            // Manchester 0: long high, short low
            uint32_t duration = is_high ? honda_protocol_v2_const.te_long : honda_protocol_v2_const.te_short;
            result = level_duration_make(is_high, duration);
        }
    }
    // Trailing HIGH pulse to signal end-of-transmission (count 194)
    else {
        // Send a long HIGH that exceeds the decoder's end threshold (>2000µs)
        result = level_duration_make(true, honda_protocol_v2_const.te_long * 5);
    }
    
    instance->preamble_count++;
    return result;
}
