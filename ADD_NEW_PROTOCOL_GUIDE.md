# How to Add New Keyfob Protocols to ProtoPirate

This guide will walk you through the process of adding new car keyfob protocols to the ProtoPirate Flipper Zero application.

## Overview

ProtoPirate uses a modular protocol system where each car manufacturer/protocol is implemented as a separate module. The system supports both decoding (receiving) and encoding (transmitting) functionality.

## File Structure

Each protocol consists of two files:
- `protocols/manufacturer_vX.h` - Header file with declarations
- `protocols/manufacturer_vX.c` - Implementation file

## Step-by-Step Guide

### Step 1: Create Protocol Header File

Create a new header file in the `protocols/` directory. For example, `protocols/tesla_v0.h`:

```c
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

#define TESLA_PROTOCOL_V0_NAME "Tesla V0"

typedef struct SubGhzProtocolDecoderTesla SubGhzProtocolDecoderTesla;
typedef struct SubGhzProtocolEncoderTesla SubGhzProtocolEncoderTesla;

extern const SubGhzProtocolDecoder subghz_protocol_tesla_decoder;
extern const SubGhzProtocolEncoder subghz_protocol_tesla_encoder;
extern const SubGhzProtocol tesla_protocol_v0;

void* subghz_protocol_decoder_tesla_alloc(SubGhzEnvironment* environment);
void subghz_protocol_decoder_tesla_free(void* context);
void subghz_protocol_decoder_tesla_reset(void* context);
void subghz_protocol_decoder_tesla_feed(void* context, bool level, uint32_t duration);
uint8_t subghz_protocol_decoder_tesla_get_hash_data(void* context);
SubGhzProtocolStatus subghz_protocol_decoder_tesla_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);
SubGhzProtocolStatus subghz_protocol_decoder_tesla_deserialize(void* context, FlipperFormat* flipper_format);
void subghz_protocol_decoder_tesla_get_string(void* context, FuriString* output);

// Optional: Encoder functions if transmission is supported
void subghz_protocol_encoder_tesla_set_button(void* context, uint8_t button);
void subghz_protocol_encoder_tesla_set_counter(void* context, uint16_t counter);
```

### Step 2: Create Protocol Implementation File

Create the corresponding implementation file, e.g., `protocols/tesla_v0.c`:

```c
#include "tesla_v0.h"

#define TAG "TeslaProtocolV0"

// Protocol timing constants
static const SubGhzBlockConst subghz_protocol_tesla_const = {
    .te_short = 250,    // Short pulse duration in microseconds
    .te_long = 500,     // Long pulse duration in microseconds
    .te_delta = 80,     // Timing tolerance
    .min_count_bit_for_found = 64,  // Minimum bits to detect
};

// Decoder structure
struct SubGhzProtocolDecoderTesla
{
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;
    uint16_t header_count;
};

// Encoder structure (if transmission is supported)
struct SubGhzProtocolEncoderTesla
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
    TeslaDecoderStepReset = 0,
    TeslaDecoderStepCheckPreambula,
    TeslaDecoderStepSaveDuration,
    TeslaDecoderStepCheckDuration,
} TeslaDecoderStep;

// Protocol registry declarations
const SubGhzProtocolDecoder subghz_protocol_tesla_decoder = {
    .alloc = subghz_protocol_decoder_tesla_alloc,
    .free = subghz_protocol_decoder_tesla_free,
    .feed = subghz_protocol_decoder_tesla_feed,
    .reset = subghz_protocol_decoder_tesla_reset,
    .get_hash_data = subghz_protocol_decoder_tesla_get_hash_data,
    .serialize = subghz_protocol_decoder_tesla_serialize,
    .deserialize = subghz_protocol_decoder_tesla_deserialize,
    .get_string = subghz_protocol_decoder_tesla_get_string,
};

const SubGhzProtocolEncoder subghz_protocol_tesla_encoder = {
    .alloc = subghz_protocol_encoder_tesla_alloc,
    .free = subghz_protocol_encoder_tesla_free,
    .deserialize = subghz_protocol_encoder_tesla_deserialize,
    .stop = subghz_protocol_encoder_tesla_stop,
    .yield = subghz_protocol_encoder_tesla_yield,
};

const SubGhzProtocol tesla_protocol_v0 = {
    .name = TESLA_PROTOCOL_V0_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_FM | SubGhzProtocolFlag_Decodable |
            SubGhzProtocolFlag_Save | SubGhzProtocolFlag_Send,
    .decoder = &subghz_protocol_tesla_decoder,
    .encoder = &subghz_protocol_tesla_encoder,
};
```

### Step 3: Implement Core Functions

#### Decoder Functions

1. **Alloc/Free/Reset Functions:**
```c
void* subghz_protocol_decoder_tesla_alloc(SubGhzEnvironment* environment)
{
    UNUSED(environment);
    SubGhzProtocolDecoderTesla* instance = malloc(sizeof(SubGhzProtocolDecoderTesla));
    instance->base.protocol = &tesla_protocol_v0;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void subghz_protocol_decoder_tesla_free(void* context)
{
    furi_assert(context);
    SubGhzProtocolDecoderTesla* instance = context;
    free(instance);
}

void subghz_protocol_decoder_tesla_reset(void* context)
{
    furi_assert(context);
    SubGhzProtocolDecoderTesla* instance = context;
    instance->decoder.parser_step = TeslaDecoderStepReset;
}
```

2. **Feed Function (Signal Processing):**
This is the most complex part - you need to implement the state machine that decodes the RF signal:

```c
void subghz_protocol_decoder_tesla_feed(void* context, bool level, uint32_t duration)
{
    furi_assert(context);
    SubGhzProtocolDecoderTesla* instance = context;

    switch (instance->decoder.parser_step)
    {
    case TeslaDecoderStepReset:
        // Look for preamble pattern
        if ((level) && (DURATION_DIFF(duration, subghz_protocol_tesla_const.te_short) < subghz_protocol_tesla_const.te_delta))
        {
            instance->decoder.parser_step = TeslaDecoderStepCheckPreambula;
            instance->decoder.te_last = duration;
            instance->header_count = 0;
        }
        break;
    
    case TeslaDecoderStepCheckPreambula:
        // Check for preamble pattern continuation
        // Implementation depends on your protocol's preamble
        break;
    
    // Add other decoder steps as needed
    }
}
```

3. **Serialize/Deserialize Functions:**
```c
SubGhzProtocolStatus subghz_protocol_decoder_tesla_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset)
{
    furi_assert(context);
    SubGhzProtocolDecoderTesla* instance = context;
    
    return subghz_block_generic_serialize(&instance->generic, flipper_format, preset);
}

SubGhzProtocolStatus subghz_protocol_decoder_tesla_deserialize(void* context, FlipperFormat* flipper_format)
{
    furi_assert(context);
    SubGhzProtocolDecoderTesla* instance = context;
    
    return subghz_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, subghz_protocol_tesla_const.min_count_bit_for_found);
}
```

4. **Get String Function:**
```c
void subghz_protocol_decoder_tesla_get_string(void* context, FuriString* output)
{
    furi_assert(context);
    SubGhzProtocolDecoderTesla* instance = context;
    
    uint32_t code_found_hi = instance->generic.data >> 32;
    uint32_t code_found_lo = instance->generic.data & 0x00000000ffffffff;
    
    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%08lX%08lX\r\n"
        "Sn:%08lX Btn:%02X Cnt:%04lX\r\n",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        code_found_hi,
        code_found_lo,
        instance->generic.serial,
        instance->generic.btn,
        instance->generic.cnt);
}
```

### Step 4: Register the Protocol

1. **Add include to `protocols/protocol_items.h`:**
```c
// Add this in the appropriate section
#include "tesla_v0.h"
```

2. **Add protocol to registry in `protocols/protocol_items.c`:**
```c
const SubGhzProtocol* protopirate_protocol_registry_items[] = {
    // ... existing protocols ...
    &tesla_protocol_v0,  // Add this line
};
```

## Key Components to Understand

### Timing Constants
- `te_short`: Duration of short pulses (microseconds)
- `te_long`: Duration of long pulses (microseconds)  
- `te_delta`: Timing tolerance for pulse detection
- `min_count_bit_for_found`: Minimum bits required to consider a signal valid

### Protocol Flags
- `SubGhzProtocolFlag_433`: Works on 433MHz frequency
- `SubGhzProtocolFlag_AM`: Amplitude Modulation
- `SubGhzProtocolFlag_FM`: Frequency Modulation
- `SubGhzProtocolFlag_Decodable`: Can decode signals
- `SubGhzProtocolFlag_Save`: Can save decoded signals
- `SubGhzProtocolFlag_Send`: Can transmit signals

### Common Encoding Schemes
- **Manchester Encoding**: Bit 0 = short+long, Bit 1 = long+short
- **PWM (Pulse Width Modulation)**: Different pulse widths for 0/1
- **PPM (Pulse Position Modulation)**: Position of pulse determines bit value

## Protocol Analysis Tips

1. **Capture signals** using a software-defined radio (SDR) or Flipper Zero
2. **Analyze timing** to determine te_short, te_long, and te_delta values
3. **Identify preamble pattern** - usually a repeating sequence
4. **Determine bit length** and data structure
5. **Extract serial, button, and counter** fields from the data
6. **Test with multiple presses** to understand rolling code behavior

## Common Patterns

### Preamble Detection
Most protocols start with a preamble (repeating pattern) followed by a sync pulse:

```c
case TeslaDecoderStepCheckPreambula:
    if (level && DURATION_DIFF(duration, subghz_protocol_tesla_const.te_short) < subghz_protocol_tesla_const.te_delta) {
        instance->header_count++;
    } else if (duration > expected_sync_duration) {
        // Found sync, move to data decoding
        instance->decoder.parser_step = TeslaDecoderStepSaveDuration;
        instance->decoder.decode_data = 0;
        instance->decoder.decode_count_bit = 0;
    }
    break;
```

### Data Bit Extraction
```c
case TeslaDecoderStepCheckDuration:
    if (!level) {
        if (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_tesla_const.te_short) < subghz_protocol_tesla_const.te_delta &&
            DURATION_DIFF(duration, subghz_protocol_tesla_const.te_long) < subghz_protocol_tesla_const.te_delta) {
            subghz_protocol_blocks_add_bit(&instance->decoder, 0);
        } else if (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_tesla_const.te_long) < subghz_protocol_tesla_const.te_delta &&
                   DURATION_DIFF(duration, subghz_protocol_tesla_const.te_short) < subghz_protocol_tesla_const.te_delta) {
            subghz_protocol_blocks_add_bit(&instance->decoder, 1);
        }
    }
    break;
```

## Testing Your Protocol

1. **Add to Flipper Zero build**
2. **Test with real keyfob** signals
3. **Verify decoding accuracy**
4. **Test transmission** (if implemented)
5. **Check serialization/deserialization**

## Resources

- **Flipper Zero SubGHz documentation**
- **RF analysis tools** (Universal Radio Hacker, Inspectrum)
- **Existing protocols** in the ProtoPirate project for reference
- **Automotive RF protocol research papers**

## Current Working Protocols

Currently supported protocols in ProtoPirate:
- **KIA/Hyundai**: Multiple versions (v0-v5)
- **Asian**: Ford, Subaru, Suzuki, Mazda, Honda, Mitsubishi
- **European VAG**: Volkswagen
- **European PSA**: Citroen
- **European Fiat**: Fiat
- **Japanese**: Nissan

## Removed Non-Working Protocols

The following protocols have been removed due to non-functionality:
- ~~Mercedes V0~~
- ~~BMW V0~~  
- ~~Toyota V0~~
- ~~Peugeot~~

Use this guide to add new, working protocols to expand ProtoPirate's capabilities!
