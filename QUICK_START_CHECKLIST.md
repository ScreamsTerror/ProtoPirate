# Quick Start: Adding New Keyfob Protocols

## 🎯 TL;DR Process

**YES, you need keyfob signal files (.sub) to add new manufacturers!**

## 📋 Required Materials

### Hardware
- [ ] Flipper Zero OR SDR dongle ($20-30 RTL-SDR)
- [ ] Target car keyfob

### Software
- [ ] Universal Radio Hacker (URH) - Free download
- [ ] ProtoPirate source code

### Signal Files You Need to Capture
- [ ] Lock button presses (3+ times)
- [ ] Unlock button presses (3+ times) 
- [ ] Trunk button (if present)
- [ ] Panic button (if present)
- [ ] Multiple keyfobs (if possible)

## 🚀 Step-by-Step Process

### Step 1: Capture Signals (30 minutes)

**Using Flipper Zero:**
1. Open ProtoPirate → Receiver
2. Set frequency: 433.92MHz (most common) or 315MHz
3. Press keyfob buttons near Flipper
4. Save as `.sub` files

**Using SDR (better for analysis):**
1. Connect RTL-SDR to computer
2. Open URH
3. Record at 433.92MHz
4. Press buttons multiple times
5. Save recordings

### Step 2: Analyze in URH (1-2 hours)

1. **Load signal file** in URH
2. **Set modulation** (ASK/OOK or FSK)
3. **Adjust symbol rate** until you see clean bits
4. **Export binary data** for analysis

**Look for patterns:**
```
Same button pressed multiple times:
AAAA BBBB CCCC DDDD  ← Changes each press (counter)
AAAA BBBB CCCC EEEE  ← Changes each press (counter)

Different buttons:
AAAA BBBB 0001 CCCC  ← Lock button
AAAA BBBB 0002 CCCC  ← Unlock button
AAAA BBBB 0004 CCCC  ← Trunk button
```

### Step 3: Extract Protocol Info (1 hour)

**From your analysis, identify:**

1. **Timing:**
   - Short pulse: ~250µs
   - Long pulse: ~500µs  
   - Tolerance: ±80µs

2. **Data Structure:**
   - Total bits: 64
   - Serial location: bits 16-47
   - Button location: bits 8-15
   - Counter location: bits 48-63

3. **Encoding:**
   - Manchester: 0=short+long, 1=long+short
   - PWM: 0=short, 1=long
   - PPM: 0=short gap, 1=long gap

### Step 4: Create Protocol Files (2-4 hours)

**Create two files:**

1. `protocols/newmanufacturer_v0.h` (copy from existing protocol)
2. `protocols/newmanufacturer_v0.c` (copy from existing protocol)

**Key changes to make:**
```c
// Change timing constants
static const SubGhzBlockConst subghz_protocol_newmanufacturer_const = {
    .te_short = 250,    // Your measured value
    .te_long = 500,     // Your measured value
    .te_delta = 80,     // 20% tolerance
    .min_count_bit_for_found = 64,  // Your measured bit count
};

// Change protocol name
#define NEWMANUFACTURER_PROTOCOL_V0_NAME "NewManufacturer V0"
```

### Step 5: Register Protocol (10 minutes)

**Edit `protocols/protocol_items.h`:**
```c
#include "newmanufacturer_v0.h"  // Add this line
```

**Edit `protocols/protocol_items.c`:**
```c
const SubGhzProtocol* protopirate_protocol_registry_items[] = {
    // ... existing protocols ...
    &newmanufacturer_protocol_v0,  // Add this line
};
```

### Step 6: Test (30 minutes)

1. **Build and flash** to Flipper Zero
2. **Load your .sub files** 
3. **Verify decoding** shows correct serial/button/counter
4. **Test with live signals** from keyfob

## 📁 File Templates

### Minimum Working Decoder (Copy & Modify)

Start with the simplest working protocol like `subaru.c` and modify:

```c
// 1. Change all "subaru" to "newmanufacturer"
// 2. Update timing constants from your analysis
// 3. Modify bit extraction for your protocol
// 4. Test decoder first, add encoder later
```

## 🎯 Success Criteria

Your protocol works when:
- [ ] Decoder shows consistent serial number across presses
- [ ] Button codes match what you pressed
- [ ] Counter increments with each press
- [ ] No crashes or errors in ProtoPirate

## 🆘 Common Issues & Solutions

### "Signal not detected"
- Try 315MHz instead of 433.92MHz
- Check keyfob battery
- Get closer to antenna

### "Decoding fails"  
- Adjust timing tolerance (`te_delta`)
- Check bit count (`min_count_bit_for_found`)
- Verify encoding scheme

### "Wrong button codes"
- Check bit order (MSB vs LSB)
- Verify button bit positions
- Look for button-specific patterns

## 📚 Learn from Examples

**Study these working protocols:**
- `subaru.c` - Simple, clean example
- `kia_v0.c` - More complex with rolling codes
- `hyundai_v0.c` - Good encoder example

## 🤝 Get Help

**Resources:**
- **Discord**: Flipper Zero community
- **Reddit**: r/flipperzero, r/RTLSDR  
- **GitHub**: Issues in ProtoPirate repo
- **URH Documentation**: For signal analysis

## ⏱️ Time Estimate

| Step | Time |
|------|------|
| Signal Capture | 30 min |
| URH Analysis | 1-2 hours |
| Protocol Extraction | 1 hour |
| Code Implementation | 2-4 hours |
| Testing & Debug | 30 min |
| **Total** | **5-8 hours** |

## 🎉 You're Ready!

Follow this checklist and you'll successfully add a new keyfob protocol to ProtoPirate!

**Remember:** Start with decoder only, test thoroughly, then add encoder if needed.
