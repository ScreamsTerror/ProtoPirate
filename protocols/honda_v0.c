#include "honda_v0.h"

#define TAG "HondaProtocolV0"

static const SubGhzBlockConst subghz_protocol_honda_const = {
    .te_short = 300,
    .te_long = 600,
    .te_delta = 120,
    .min_count_bit_for_found = 56,
};

struct SubGhzProtocolDecoderHonda
{
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;
    uint16_t header_count;
};

struct SubGhzProtocolEncoderHonda
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

typedef enum
{
    HondaDecoderStepReset = 0,
    HondaDecoderStepCheckPreambula,
    HondaDecoderStepSaveDuration,
    HondaDecoderStepCheckDuration,
} HondaDecoderStep;

// Forward declarations for encoder
void *subghz_protocol_encoder_honda_alloc(SubGhzEnvironment *environment);
void subghz_protocol_encoder_honda_free(void *context);
SubGhzProtocolStatus subghz_protocol_encoder_honda_deserialize(void *context, FlipperFormat *flipper_format);
void subghz_protocol_encoder_honda_stop(void *context);
LevelDuration subghz_protocol_encoder_honda_yield(void *context);

const SubGhzProtocolDecoder subghz_protocol_honda_decoder = {
    .alloc = subghz_protocol_decoder_honda_alloc,
    .free = subghz_protocol_decoder_honda_free,
    .feed = subghz_protocol_decoder_honda_feed,
    .reset = subghz_protocol_decoder_honda_reset,
    .get_hash_data = subghz_protocol_decoder_honda_get_hash_data,
    .serialize = subghz_protocol_decoder_honda_serialize,
    .deserialize = subghz_protocol_decoder_honda_deserialize,
    .get_string = subghz_protocol_decoder_honda_get_string,
};

const SubGhzProtocolEncoder subghz_protocol_honda_encoder = {
    .alloc = subghz_protocol_encoder_honda_alloc,
    .free = subghz_protocol_encoder_honda_free,
    .deserialize = subghz_protocol_encoder_honda_deserialize,
    .stop = subghz_protocol_encoder_honda_stop,
    .yield = subghz_protocol_encoder_honda_yield,
};

const SubGhzProtocol honda_protocol_v0 = {
    .name = HONDA_PROTOCOL_V0_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_FM | SubGhzProtocolFlag_Decodable |
            SubGhzProtocolFlag_Save | SubGhzProtocolFlag_Send,
    .decoder = &subghz_protocol_honda_decoder,
    .encoder = &subghz_protocol_honda_encoder,
};

// Encoder implementation
void *subghz_protocol_encoder_honda_alloc(SubGhzEnvironment *environment)
{
    UNUSED(environment);
    SubGhzProtocolEncoderHonda *instance = malloc(sizeof(SubGhzProtocolEncoderHonda));
    instance->base.protocol = &honda_protocol_v0;
    instance->is_running = false;
    return instance;
}

void subghz_protocol_encoder_honda_free(void *context)
{
    furi_assert(context);
    SubGhzProtocolEncoderHonda *instance = context;
    free(instance);
}

SubGhzProtocolStatus subghz_protocol_encoder_honda_deserialize(void *context, FlipperFormat *flipper_format)
{
    furi_assert(context);
    SubGhzProtocolEncoderHonda *instance = context;

    SubGhzProtocolStatus res = SubGhzProtocolStatusError;
    do
    {
        // Read protocol name and validate
        FuriString *temp_str = furi_string_alloc();
        if (!flipper_format_read_string(flipper_format, "Protocol", temp_str))
        {
            FURI_LOG_E(TAG, "Missing Protocol");
            furi_string_free(temp_str);
            break;
        }

        if (!furi_string_equal(temp_str, instance->base.protocol->name))
        {
            FURI_LOG_E(TAG, "Wrong protocol %s != %s",
                       furi_string_get_cstr(temp_str), instance->base.protocol->name);
            furi_string_free(temp_str);
            break;
        }
        furi_string_free(temp_str);

        // Read bit count as uint32_t first
        uint32_t bit_count_temp;
        if (!flipper_format_read_uint32(flipper_format, "Bit", &bit_count_temp, 1))
        {
            FURI_LOG_E(TAG, "Missing Bit");
            break;
        }
        instance->generic.data_count_bit = (uint16_t)bit_count_temp;

        if (instance->generic.data_count_bit != 56)
        {
            FURI_LOG_E(TAG, "Wrong bit count %u", instance->generic.data_count_bit);
            break;
        }

        // Read key data
        temp_str = furi_string_alloc();
        if (!flipper_format_read_string(flipper_format, "Key", temp_str))
        {
            FURI_LOG_E(TAG, "Missing Key");
            furi_string_free(temp_str);
            break;
        }

        // Parse hex key
        uint64_t key = 0;
        if (sscanf(furi_string_get_cstr(temp_str), "%016llX", &key) != 1)
        {
            FURI_LOG_E(TAG, "Failed to parse Key");
            furi_string_free(temp_str);
            break;
        }
        furi_string_free(temp_str);

        instance->generic.data = key;

        // Read or extract serial
        if (!flipper_format_read_uint32(flipper_format, "Serial", &instance->serial, 1))
        {
            // Extract from key data (bits 12-39)
            instance->serial = (uint32_t)((key >> 12) & 0x0FFFFFFF);
        }

        // Read or extract button
        uint32_t btn_temp;
        if (flipper_format_read_uint32(flipper_format, "Btn", &btn_temp, 1))
        {
            instance->button = (uint8_t)btn_temp;
        }
        else
        {
            // Extract from key data (bits 8-11)
            instance->button = (key >> 8) & 0x0F;
        }

        // Read or extract counter
        uint32_t cnt_temp;
        if (flipper_format_read_uint32(flipper_format, "Cnt", &cnt_temp, 1))
        {
            instance->counter = (uint16_t)cnt_temp;
        }
        else
        {
            // Extract from key data (bits 40-55)
            instance->counter = (key >> 40) & 0xFFFF;
        }

        // Initialize encoder state
        instance->is_running = true;
        instance->preamble_count = 0;
        instance->data_bit_index = 0;
        instance->last_bit = 0;
        instance->send_high = true;

        res = SubGhzProtocolStatusOk;
    } while (false);

    return res;
}

void subghz_protocol_encoder_honda_stop(void *context)
{
    SubGhzProtocolEncoderHonda *instance = context;
    instance->is_running = false;
}

// Function to update the data with new button/counter values
static void subghz_protocol_encoder_honda_update_data(SubGhzProtocolEncoderHonda *instance)
{
    // Reconstruct the 56-bit data with updated button and counter
    uint64_t data = 0;

    // Bits 0-7: Fixed header/sync (seems to be 0x55 based on decoder)
    data |= 0x55;

    // Bits 8-11: Button (4 bits)
    data |= ((uint64_t)(instance->button & 0x0F) << 8);

    // Bits 12-39: Serial (28 bits)
    data |= ((uint64_t)(instance->serial & 0x0FFFFFFF) << 12);

    // Bits 40-55: Counter (16 bits)
    data |= ((uint64_t)(instance->counter & 0xFFFF) << 40);

    instance->generic.data = data;
}

LevelDuration subghz_protocol_encoder_honda_yield(void *context)
{
    SubGhzProtocolEncoderHonda *instance = context;

    if (!instance->is_running)
    {
        return level_duration_reset();
    }

    // Update data with current button/counter
    subghz_protocol_encoder_honda_update_data(instance);

    // Send preamble: 14+ short HIGH/LOW pairs
    if (instance->preamble_count < 28)
    {
        if (instance->preamble_count % 2 == 0)
        {
            // HIGH pulse
            instance->preamble_count++;
            return level_duration_make(true, subghz_protocol_honda_const.te_short);
        }
        else
        {
            // LOW pulse
            instance->preamble_count++;
            return level_duration_make(false, subghz_protocol_honda_const.te_short);
        }
    }

    // Send sync: long HIGH, long LOW
    if (instance->preamble_count == 28)
    {
        instance->preamble_count++;
        return level_duration_make(true, subghz_protocol_honda_const.te_long);
    }

    if (instance->preamble_count == 29)
    {
        instance->preamble_count++;
        instance->data_bit_index = 0;
        return level_duration_make(false, subghz_protocol_honda_const.te_long);
    }

    // Send data bits (56 bits total)
    if (instance->data_bit_index < instance->generic.data_count_bit)
    {
        // Honda V0 encoding: Similar to Kia but different timing
        // Bit 0: short HIGH + short LOW
        // Bit 1: long HIGH + long LOW

        if (instance->send_high)
        {
            // Send HIGH part of bit
            instance->send_high = false;

            // Get current bit from data (MSB first)
            uint64_t bit_mask = 1ULL << (55 - instance->data_bit_index);
            uint8_t current_bit = (instance->generic.data & bit_mask) ? 1 : 0;

            instance->last_bit = current_bit;

            // HIGH duration based on bit value
            uint32_t duration = current_bit ? subghz_protocol_honda_const.te_long : subghz_protocol_honda_const.te_short;

            return level_duration_make(true, duration);
        }
        else
        {
            // Send LOW part of bit
            instance->send_high = true;
            instance->data_bit_index++;

            // LOW duration matches HIGH duration
            uint32_t duration = instance->last_bit ? subghz_protocol_honda_const.te_long : subghz_protocol_honda_const.te_short;

            // Check if we're done
            if (instance->data_bit_index >= instance->generic.data_count_bit)
            {
                // Add final gap before repeat
                instance->is_running = false;
                return level_duration_make(false, duration + 12000); // 12ms gap
            }

            return level_duration_make(false, duration);
        }
    }

    return level_duration_reset();
}

// Allow button/counter updates
void subghz_protocol_encoder_honda_set_button(void *context, uint8_t button)
{
    SubGhzProtocolEncoderHonda *instance = context;
    instance->button = button & 0x0F;
}

void subghz_protocol_encoder_honda_set_counter(void *context, uint16_t counter)
{
    SubGhzProtocolEncoderHonda *instance = context;
    instance->counter = counter;
}

// Decoder implementation
void *subghz_protocol_decoder_honda_alloc(SubGhzEnvironment *environment)
{
    UNUSED(environment);
    SubGhzProtocolDecoderHonda *instance = malloc(sizeof(SubGhzProtocolDecoderHonda));
    instance->base.protocol = &honda_protocol_v0;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void subghz_protocol_decoder_honda_free(void *context)
{
    furi_assert(context);
    SubGhzProtocolDecoderHonda *instance = context;
    free(instance);
}

void subghz_protocol_decoder_honda_reset(void *context)
{
    furi_assert(context);
    SubGhzProtocolDecoderHonda *instance = context;
    instance->decoder.parser_step = HondaDecoderStepReset;
}

void subghz_protocol_decoder_honda_feed(void *context, bool level, uint32_t duration)
{
    furi_assert(context);
    SubGhzProtocolDecoderHonda *instance = context;

    switch (instance->decoder.parser_step)
    {
    case HondaDecoderStepReset:
        if ((level) && (DURATION_DIFF(duration, subghz_protocol_honda_const.te_short) < subghz_protocol_honda_const.te_delta))
        {
            instance->decoder.parser_step = HondaDecoderStepCheckPreambula;
            instance->decoder.te_last = duration;
            instance->header_count = 0;
        }
        break;
    case HondaDecoderStepCheckPreambula:
        if (level)
        {
            if ((DURATION_DIFF(duration, subghz_protocol_honda_const.te_short) < subghz_protocol_honda_const.te_delta) ||
                (DURATION_DIFF(duration, subghz_protocol_honda_const.te_long) < subghz_protocol_honda_const.te_delta))
            {
                instance->decoder.te_last = duration;
            }
            else
            {
                instance->decoder.parser_step = HondaDecoderStepReset;
            }
        }
        else if (
            (DURATION_DIFF(duration, subghz_protocol_honda_const.te_short) < subghz_protocol_honda_const.te_delta) &&
            (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_honda_const.te_short) < subghz_protocol_honda_const.te_delta))
        {
            instance->header_count++;
            break;
        }
        else if (
            (DURATION_DIFF(duration, subghz_protocol_honda_const.te_long) < subghz_protocol_honda_const.te_delta) &&
            (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_honda_const.te_long) < subghz_protocol_honda_const.te_delta))
        {
            if (instance->header_count > 12)
            {
                instance->decoder.parser_step = HondaDecoderStepSaveDuration;
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
            }
            else
            {
                instance->decoder.parser_step = HondaDecoderStepReset;
            }
        }
        else
        {
            instance->decoder.parser_step = HondaDecoderStepReset;
        }
        break;
    case HondaDecoderStepSaveDuration:
        if (level)
        {
            if (duration >=
                (subghz_protocol_honda_const.te_long + subghz_protocol_honda_const.te_delta * 2UL))
            {
                instance->decoder.parser_step = HondaDecoderStepReset;
                if (instance->decoder.decode_count_bit ==
                    subghz_protocol_honda_const.min_count_bit_for_found)
                {
                    instance->generic.data = instance->decoder.decode_data;
                    instance->generic.data_count_bit = instance->decoder.decode_count_bit;
                    if (instance->base.callback)
                        instance->base.callback(&instance->base, instance->base.context);
                }
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
                break;
            }
            else
            {
                instance->decoder.te_last = duration;
                instance->decoder.parser_step = HondaDecoderStepCheckDuration;
            }
        }
        else
        {
            instance->decoder.parser_step = HondaDecoderStepReset;
        }
        break;
    case HondaDecoderStepCheckDuration:
        if (!level)
        {
            if ((DURATION_DIFF(instance->decoder.te_last, subghz_protocol_honda_const.te_short) < subghz_protocol_honda_const.te_delta) &&
                (DURATION_DIFF(duration, subghz_protocol_honda_const.te_short) < subghz_protocol_honda_const.te_delta))
            {
                subghz_protocol_blocks_add_bit(&instance->decoder, 0);
                instance->decoder.parser_step = HondaDecoderStepSaveDuration;
            }
            else if (
                (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_honda_const.te_long) < subghz_protocol_honda_const.te_delta) &&
                (DURATION_DIFF(duration, subghz_protocol_honda_const.te_long) < subghz_protocol_honda_const.te_delta))
            {
                subghz_protocol_blocks_add_bit(&instance->decoder, 1);
                instance->decoder.parser_step = HondaDecoderStepSaveDuration;
            }
            else
            {
                instance->decoder.parser_step = HondaDecoderStepReset;
            }
        }
        else
        {
            instance->decoder.parser_step = HondaDecoderStepReset;
        }
        break;
    }
}

static void subghz_protocol_honda_check_remote_controller(SubGhzBlockGeneric *instance)
{
    instance->serial = (uint32_t)((instance->data >> 12) & 0x0FFFFFFF);
    instance->btn = (instance->data >> 8) & 0x0F;
    instance->cnt = (instance->data >> 40) & 0xFFFF;
}

uint8_t subghz_protocol_decoder_honda_get_hash_data(void *context)
{
    furi_assert(context);
    SubGhzProtocolDecoderHonda *instance = context;
    return subghz_protocol_blocks_get_hash_data(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus subghz_protocol_decoder_honda_serialize(
    void *context,
    FlipperFormat *flipper_format,
    SubGhzRadioPreset *preset)
{
    furi_assert(context);
    SubGhzProtocolDecoderHonda *instance = context;

    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;

    ret = subghz_block_generic_serialize(&instance->generic, flipper_format, preset);

    if (ret == SubGhzProtocolStatusOk)
    {
        // Ensure fields are extracted
        subghz_protocol_honda_check_remote_controller(&instance->generic);

        // Save decoded fields
        flipper_format_write_uint32(flipper_format, "Serial", &instance->generic.serial, 1);

        uint32_t temp = instance->generic.btn;
        flipper_format_write_uint32(flipper_format, "Btn", &temp, 1);

        flipper_format_write_uint32(flipper_format, "Cnt", &instance->generic.cnt, 1);
    }

    return ret;
}

SubGhzProtocolStatus
subghz_protocol_decoder_honda_deserialize(void *context, FlipperFormat *flipper_format)
{
    furi_assert(context);
    SubGhzProtocolDecoderHonda *instance = context;
    return subghz_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, subghz_protocol_honda_const.min_count_bit_for_found);
}

void subghz_protocol_decoder_honda_get_string(void *context, FuriString *output)
{
    furi_assert(context);
    SubGhzProtocolDecoderHonda *instance = context;

    subghz_protocol_honda_check_remote_controller(&instance->generic);
    uint32_t code_found_hi = instance->generic.data >> 32;
    uint32_t code_found_lo = instance->generic.data & 0x00000000ffffffff;

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%08lX%08lX\r\n"
        "Sn:%07lX Btn:%X Cnt:%04lX\r\n",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        code_found_hi,
        code_found_lo,
        instance->generic.serial,
        instance->generic.btn,
        instance->generic.cnt);
}
