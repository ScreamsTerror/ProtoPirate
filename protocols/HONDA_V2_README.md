# Honda V2 Protocol Implementation

This document describes the Honda V2 protocol implementation for ProtoPirate, specifically designed for Honda keyfobs with FCC ID KR5V2X that are vulnerable to CVE-2022-27254.

## Overview

The Honda V2 protocol implements support for:
- **Target**: Honda keyfobs with FCC ID KR5V2X
- **Vulnerability**: CVE-2022-27254 (Rolling code vulnerability)
- **Frequency**: 433.657MHz (primary) and 434.176MHz (secondary)
- **Modulation**: Frequency Modulation (FM) with custom presets

## Technical Details

### Protocol Characteristics
- **Encoding**: Manchester encoding
- **Data Length**: 64 bits
- **Modulation**: FM with custom CC1101 presets
- **Preamble**: 12 pairs of long pulses
- **Sync**: Long pulse followed by short pulse

### Custom Presets

Based on the Honda firmware analysis, two custom presets are used:

#### Honda1 (Primary)
- More stable, less sensitive
- Preferred for general use
- CC1101 register configuration optimized for 433.657MHz

#### Honda2 (Secondary)
- Higher sensitivity but more noise
- Alternative frequency at 434.176MHz
- Useful in challenging RF environments

### Data Structure

```
64-bit packet structure:
Bits 63-32: Serial number (32 bits)
Bits 31-24: Button code (8 bits)
Bits 23-8:  Reserved/unknown (16 bits)
Bits 7-0:   Counter/rolling code (8 bits)
```

### Button Codes
- **0x01**: Lock
- **0x02**: Unlock
- **0x04**: Trunk
- **0x08**: Panic

## Implementation Details

### Files Created
- `protocols/honda_v2.h` - Header file with declarations
- `protocols/honda_v2.c` - Main implementation with decoder/encoder

### Key Features
- **Manchester Decoding**: Robust bit extraction with alignment checking
- **Preamble Detection**: Reliable signal identification
- **Rolling Code Support**: Extracts and displays counter values
- **Transmission**: Full encode/decode capability
- **Storage**: Complete serialization/deserialization support

### Decoder States
1. **Reset**: Initial state waiting for signal
2. **CheckPreamble**: Validating preamble pattern
3. **CollectRawBits**: Capturing and decoding Manchester data

### Timing Constants
- `te_short`: 200 microseconds
- `te_long`: 400 microseconds  
- `te_delta`: 60 microseconds tolerance
- `min_count_bit_for_found`: 64 bits

## Usage

### Reception (Decoding)
1. Set frequency to 433.657MHz or 434.176MHz
2. Use custom preset "Honda1" or "Honda2"
3. Press buttons on Honda keyfob
4. Protocol will automatically decode and display:
   - Full 64-bit key
   - Serial number
   - Button pressed
   - Rolling counter

### Transmission (Encoding)
1. Load previously saved Honda signal
2. Protocol will reconstruct the signal
3. Transmit using same frequency and preset
4. Works due to CVE-2022-27254 vulnerability

## CVE-2022-27254 Details

This vulnerability affects Honda KR5V2X keyfobs where:
- Rolling code implementation is weak
- Replay attacks are possible
- No proper authentication of signals
- Same counter can be reused

**Disclaimer**: This implementation is for educational and research purposes only.

## Testing

### Required Equipment
- Honda keyfob with FCC ID KR5V2X
- Flipper Zero with ProtoPirate firmware
- RF analysis tools (optional)

### Test Procedure
1. Flash ProtoPirate with Honda V2 support
2. Configure frequency and custom preset
3. Test reception with real keyfob
4. Verify decoded data matches expected values
5. Test transmission/replay functionality

## Integration

The protocol is automatically registered in:
- `protocols/protocol_items.h` (include)
- `protocols/protocol_items.c` (registry)

No additional configuration required.

## Troubleshooting

### Common Issues
- **No Signal Detected**: Check frequency and custom preset
- **Decoding Errors**: Verify keyfob model (KR5V2X required)
- **Transmission Fails**: Ensure proper custom preset configuration

### Debug Information
Protocol provides detailed logging via FURI_LOG with TAG "HondaV2".

## References

- Original Honda firmware: https://github.com/nonamecoder/FlipperZeroHondaFirmware
- CVE-2022-27254 documentation
- DEFCON 30 Car Hacking Village presentation

## Credits

Implementation based on research by:
- SkorP (Sub-GHz architect for Flipper Zero)
- Nonamecoder (Honda vulnerability research)
- Car Hacking Village community

---
**Educational Use Only**: This software is intended for security research and educational purposes. Users are responsible for compliance with applicable laws and regulations.
