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

#define HONDA_PROTOCOL_V2_NAME "Honda V2"

typedef struct SubGhzProtocolDecoderHondaV2 SubGhzProtocolDecoderHondaV2;
typedef struct SubGhzProtocolEncoderHondaV2 SubGhzProtocolEncoderHondaV2;

extern const SubGhzProtocolDecoder honda_protocol_v2_decoder;
extern const SubGhzProtocolEncoder honda_protocol_v2_encoder;
extern const SubGhzProtocol honda_protocol_v2;

void* honda_protocol_decoder_v2_alloc(SubGhzEnvironment* environment);
void honda_protocol_decoder_v2_free(void* context);
void honda_protocol_decoder_v2_reset(void* context);
void honda_protocol_decoder_v2_feed(void* context, bool level, uint32_t duration);
uint8_t honda_protocol_decoder_v2_get_hash_data(void* context);
SubGhzProtocolStatus honda_protocol_decoder_v2_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);
SubGhzProtocolStatus
honda_protocol_decoder_v2_deserialize(void* context, FlipperFormat* flipper_format);
void honda_protocol_decoder_v2_get_string(void* context, FuriString* output);

// Optional: Encoder functions if transmission is supported
void* honda_protocol_encoder_v2_alloc(SubGhzEnvironment* environment);
void honda_protocol_encoder_v2_free(void* context);
void honda_protocol_encoder_v2_stop(void* context);
LevelDuration honda_protocol_encoder_v2_yield(void* context);
SubGhzProtocolStatus
honda_protocol_encoder_v2_deserialize(void* context, FlipperFormat* flipper_format);
