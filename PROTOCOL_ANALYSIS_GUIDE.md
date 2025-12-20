# Practical Guide to Adding New Keyfob Protocols

## Overview

To add a new car manufacturer protocol to ProtoPirate, you need to analyze real keyfob signals to understand the protocol structure. This guide covers the entire process from signal capture to implementation.

## What You Need

### Hardware Requirements
1. **Flipper Zero** with SubGHz capabilities
2. **OR** Software-Defined Radio (SDR) like:
   - RTL-SDR dongle (~$20-30)
   - HackRF One
   - LimeSDR

### Software Requirements
1. **Signal Analysis Tools:**
   - Universal Radio Hacker (URH) - Free, cross-platform
   - Inspectrum - Open source
   - GQRX or SDR# (for live signal capture)

2. **Development Tools:**
   - Flipper Zero development environment
   - VS Code with C/C++ extensions
   - Git for version control

## Step 1: Signal Capture

### Method A: Using Flipper Zero
1. Open ProtoPirate on Flipper Zero
2. Go to Receiver mode
3. Set frequency to 433.92MHz (most common) or 315MHz
4. Press buttons on the target keyfob
5. Save captured signals as `.sub` files

### Method B: Using SDR (Recommended for Analysis)
1. Connect SDR to computer
2. Open URH or Inspectrum
3. Set frequency to 433.92MHz or 315MHz
4. Record multiple button presses from the keyfob
5. Save recordings for analysis

## Step 2: Signal Analysis

### Analyze Signal Characteristics
Using URH or Inspectrum, examine the captured signals:

1. **Frequency**: 433.92MHz (most common) or 315MHz
2. **Modulation**: AM or FM
3. **Data Rate**: Usually 1-10 kbps
4. **Signal Structure**:
   ```
   [Preamble][Sync][Data][Checksum]
   ```

### Extract Protocol Parameters

#### Timing Analysis
- **Short Pulse Duration** (te_short): Usually 200-500 microseconds
- **Long Pulse Duration** (te_long): Usually 2x te_short
- **Timing Tolerance** (te_delta): Usually 10-20% of te_short

#### Example Analysis
```
Signal:  _-_-_-_--__-_-_-_-__--__--__--__--__--__--__--__
        S L S L S L  L  S L S L S L  L  S  S  S  S  S  S  S
        
Where:
S = Short pulse (250µs)
L = Long pulse (500µs)
_ = HIGH signal
- = LOW signal
```

### Determine Encoding Scheme

#### Common Patterns:
1. **Manchester Encoding:**
   - 0 = Short HIGH + Long LOW
   - 1 = Long HIGH + Short LOW

2. **PWM (Pulse Width Modulation):**
   - 0 = Short pulse
   - 1 = Long pulse

3. **PPM (Pulse Position Modulation):**
   - 0 = Short gap between pulses
   - 1 = Long gap between pulses

## Step 3: Data Structure Analysis

### Typical Keyfob Data Format
```
Bits 0-15:   Header/Preamble
Bits 16-31:  Serial Number (unique to keyfob)
Bits 32-39:  Button Code
Bits 40-55:  Rolling Counter
Bits 56-63:  Checksum/Signature
```

### Analyze Multiple Captures
1. Capture the same button pressed multiple times
2. Identify which parts change (rolling counter) vs. stay constant (serial)
3. Map button codes:
   - Lock: 0x01
   - Unlock: 0x02
   - Trunk: 0x04
   - Panic: 0x08

### Example Analysis Workflow
```bash
# Using URH command line
urh -f captured_signal.complex

# In URH:
# 1. Load signal file
# 2. Set modulation type (ASK/OOK, FSK, etc.)
# 3. Adjust symbol rate
# 4. Decode as binary
# 5. Export bits for analysis
```

## Step 4: Protocol Implementation

### Create Protocol Files

#### Header File: `protocols/newmanufacturer_v0.h`
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

#define NEWMANUFACTURER_PROTOCOL_V0_NAME "NewManufacturer V0"

typedef struct SubGhzProtocolDecoderNewManufacturer SubGhzProtocolDecoderNewManufacturer;
typedef struct SubGhzProtocolEncoderNewManufacturer SubGhzProtocolEncoderNewManufacturer;

extern const SubGhzProtocolDecoder subghz_protocol_newmanufacturer_decoder;
extern const SubGhzProtocolEncoder subghz_protocol_newmanufacturer_encoder;
extern const SubGhzProtocol newmanufacturer_protocol_v0;

// Standard decoder functions
void* subghz_protocol_decoder_newmanufacturer_alloc(SubGhzEnvironment* environment);
void subghz_protocol_decoder_newmanufacturer_free(void* context);
void subghz_protocol_decoder_newmanufacturer_reset(void* context);
void subghz_protocol_decoder_newmanufacturer_feed(void* context, bool level, uint32_t duration);
uint8_t subghz_protocol_decoder_newmanufacturer_get_hash_data(void* context);
SubGhzProtocolStatus subghz_protocol_decoder_newmanufacturer_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);
SubGhzProtocolStatus subghz_protocol_decoder_newmanufacturer_deserialize(void* context, FlipperFormat* flipper_format);
void subghz_protocol_decoder_newmanufacturer_get_string(void* context, FuriString* output);

// Optional encoder functions
void subghz_protocol_encoder_newmanufacturer_set_button(void* context, uint8_t button);
void subghz_protocol_encoder_newmanufacturer_set_counter(void* context, uint16_t counter);
```

#### Implementation File: `protocols/newmanufacturer_v0.c`

```c
#include "newmanufacturer_v0.h"

#define TAG "NewManufacturerProtocolV0"

// Timing constants from your analysis
static const SubGhzBlockConst subghz_protocol_newmanufacturer_const = {
    .te_short = 250,    // From signal analysis
    .te_long = 500,     // From signal analysis
    .te_delta = 80,     // 20% tolerance
    .min_count_bit_for_found = 64,  // From protocol analysis
};

// Decoder structure
struct SubGhzProtocolDecoderNewManufacturer
{
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;
    uint16_t header_count;
};

// Encoder structure
struct SubGhzProtocolEncoderNewManufacturer
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
    NewManufacturerDecoderStepReset = 0,
    NewManufacturerDecoderStepCheckPreambula,
    NewManufacturerDecoderStepSaveDuration,
    NewManufacturerDecoderStepCheckDuration,
} NewManufacturerDecoderStep;

// Protocol registry declarations
const SubGhzProtocolDecoder subghz_protocol_newmanufacturer_decoder = {
    .alloc = subghz_protocol_decoder_newmanufacturer_alloc,
    .free = subghz_protocol_decoder_newmanufacturer_free,
    .feed = subghz_protocol_decoder_newmanufacturer_feed,
    .reset = subghz_protocol_decoder_newmanufacturer_reset,
    .get_hash_data = subghz_protocol_decoder_newmanufacturer_get_hash_data,
    .serialize = subghz_protocol_decoder_newmanufacturer_serialize,
    .deserialize = subghz_protocol_decoder_newmanufacturer_deserialize,
    .get_string = subghz_protocol_decoder_newmanufacturer_get_string,
};

const SubGhzProtocolEncoder subghz_protocol_newmanufacturer_encoder = {
    .alloc = subghz_protocol_encoder_newmanufacturer_alloc,
    .free = subghz_protocol_encoder_newmanufacturer_free,
    .deserialize = subghz_protocol_encoder_newmanufacturer_deserialize,
    .stop = subghz_protocol_encoder_newmanufacturer_stop,
    .yield = subghz_protocol_encoder_newmanufacturer_yield,
};

const SubGhzProtocol newmanufacturer_protocol_v0 = {
    .name = NEWMANUFACTURER_PROTOCOL_V0_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_FM | SubGhzProtocolFlag_Decodable |
            SubGhzProtocolFlag_Save | SubGhzProtocolFlag_Send,
    .decoder = &subghz_protocol_newmanufacturer_decoder,
    .encoder = &subghz_protocol_newmanufacturer_encoder,
};
```

## Step 5: Implementation Strategy

### Start Simple - Decoder Only
1. First implement just the decoder functionality
2. Test with captured `.sub` files
3. Verify decoding produces correct serial, button, counter

### Add Encoder Later
1. Once decoder works, implement encoder
2. Test transmission capability
3. Verify it works with actual vehicle

### Use Existing Protocols as Templates
Study similar working protocols:
- **Simple protocols**: Look at Subaru, Citroen
- **Complex protocols**: Look at KIA versions, Hyundai
- **Similar encoding**: Match pulse patterns

## Step 6: Testing Process

### Unit Testing
```bash
# Build and flash to Flipper Zero
fbt fap_proto_pirate

# Test with captured signals
# Load .sub files and verify decoding
```

### Real-World Testing
1. Test with multiple keyfobs of same manufacturer
2. Test different models/years
3. Verify all buttons decode correctly
4. Test transmission if implemented

## Step 7: Integration

### Register Protocol
1. Add to `protocols/protocol_items.h`:
```c
#include "newmanufacturer_v0.h"
```

2. Add to `protocols/protocol_items.c`:
```c
const SubGhzProtocol* protopirate_protocol_registry_items[] = {
    // ... existing protocols ...
    &newmanufacturer_protocol_v0,
};
```

### Documentation
1. Update README.md with new protocol
2. Add protocol notes to guide
3. Document any special requirements

## Required Signal Files

### What You Need:
1. **Multiple captures** of the same button (to identify rolling counter)
2. **All button types** (lock, unlock, trunk, panic)
3. **Different keyfobs** if possible (to verify serial extraction)
4. **Raw signal files** (.sub, .complex, or .wav format)

### Capture Checklist:
- [ ] Lock button (3+ captures)
- [ ] Unlock button (3+ captures)
- [ ] Trunk button (if present)
- [ ] Panic button (if present)
- [ ] Different keyfob (if available)

## Common Pitfalls to Avoid

### Timing Issues
- Wrong `te_short`/`te_long` values
- Too strict `te_delta` tolerance
- Incorrect bit rate assumption

### Data Structure Errors
- Wrong bit positions for serial/button/counter
- Incorrect bit order (MSB vs LSB)
- Missing checksum validation

### Encoding Mistakes
- Wrong encoding scheme (Manchester vs PWM)
- Incorrect preamble detection
- Missing sync pulse handling

## Resources for Protocol Research

### Online Resources
- **RF hacking forums**: r/flipperzero, r/RTLSDR
- **Automotive security research papers**
- **Patent databases** for keyfob protocols
- **GitHub repositories** for similar projects

### Tools and Software
- **Universal Radio Hacker**: Best for protocol analysis
- **Inspectrum**: Good for visual signal analysis
- **GNU Radio**: Advanced signal processing
- **Audacity**: Basic audio signal analysis

## Troubleshooting

### Signal Not Detected
- Check frequency (433 vs 315 MHz)
- Verify modulation type (AM vs FM)
- Adjust timing tolerance

### Decoding Fails
- Verify timing constants
- Check encoding scheme
- Ensure correct bit length

### Transmission Fails
- Verify encoder implementation
- Check power output settings
- Ensure correct frequency and modulation

## Next Steps

1. **Start signal collection** for your target manufacturer
2. **Analyze protocols** using URH or similar tools
3. **Implement decoder** first, test thoroughly
4. **Add encoder** once decoder is working
5. **Submit pull request** to share with community

Remember: Protocol reverse engineering is iterative. Start simple, test thoroughly, and build complexity gradually!
