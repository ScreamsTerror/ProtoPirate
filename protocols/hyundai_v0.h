#pragma once

#include <furi.h>
#include <lib/subghz/protocols/base.h>
#include <lib/subghz/types.h>
#include <lib/subghz/blocks/const.h>
#include <lib/subghz/blocks/decoder.h>
#include <lib/subghz/blocks/encoder.h>
#include <lib/subghz/blocks/generic.h>
#include <lib/subghz/blocks/math.h>
#include <flipper_format/flipper_format.h>

#define HYUNDAI_PROTOCOL_V0_NAME "Hyundai V0"

typedef struct SubGhzProtocolDecoderHyundai SubGhzProtocolDecoderHyundai;
typedef struct SubGhzProtocolEncoderHyundai SubGhzProtocolEncoderHyundai;

extern const SubGhzProtocolDecoder subghz_protocol_hyundai_decoder;
extern const SubGhzProtocolEncoder subghz_protocol_hyundai_encoder;
extern const SubGhzProtocol hyundai_protocol_v0;

void* subghz_protocol_decoder_hyundai_alloc(SubGhzEnvironment* environment);
void subghz_protocol_decoder_hyundai_free(void* context);
void subghz_protocol_decoder_hyundai_reset(void* context);
void subghz_protocol_decoder_hyundai_feed(void* context, bool level, uint32_t duration);
uint8_t subghz_protocol_decoder_hyundai_get_hash_data(void* context);
SubGhzProtocolStatus subghz_protocol_decoder_hyundai_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);
SubGhzProtocolStatus subghz_protocol_decoder_hyundai_deserialize(void* context, FlipperFormat* flipper_format);
void subghz_protocol_decoder_hyundai_get_string(void* context, FuriString* output);

// Additional encoder functions
void subghz_protocol_encoder_hyundai_set_button(void* context, uint8_t button);
void subghz_protocol_encoder_hyundai_set_counter(void* context, uint16_t counter);
