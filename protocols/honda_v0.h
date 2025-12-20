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

#define HONDA_PROTOCOL_V0_NAME "Honda V0"

typedef struct SubGhzProtocolDecoderHondaV0 SubGhzProtocolDecoderHondaV0;
typedef struct SubGhzProtocolEncoderHondaV0 SubGhzProtocolEncoderHondaV0;

extern const SubGhzProtocolDecoder honda_protocol_v0_decoder;
extern const SubGhzProtocolEncoder honda_protocol_v0_encoder;
extern const SubGhzProtocol honda_protocol_v0;

void* honda_protocol_decoder_v0_alloc(SubGhzEnvironment* environment);
void honda_protocol_decoder_v0_free(void* context);
void honda_protocol_decoder_v0_reset(void* context);
void honda_protocol_decoder_v0_feed(void* context, bool level, uint32_t duration);
uint8_t honda_protocol_decoder_v0_get_hash_data(void* context);
SubGhzProtocolStatus honda_protocol_decoder_v0_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);
SubGhzProtocolStatus
honda_protocol_decoder_v0_deserialize(void* context, FlipperFormat* flipper_format);
void honda_protocol_decoder_v0_get_string(void* context, FuriString* output);

// Optional: Encoder functions if transmission is supported
void* honda_protocol_encoder_v0_alloc(SubGhzEnvironment* environment);
void honda_protocol_encoder_v0_free(void* context);
void honda_protocol_encoder_v0_stop(void* context);
LevelDuration honda_protocol_encoder_v0_yield(void* context);
SubGhzProtocolStatus
honda_protocol_encoder_v0_deserialize(void* context, FlipperFormat* flipper_format);
