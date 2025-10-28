# Sound Board System - Planning Summary

## Project Overview

A distributed sound board system using 5 ESP32-C3 boards that can:

- Play sounds locally via button presses
- Trigger remote sound playback on other boards wirelessly
- Operate without MAC address management using ESP-NOW broadcast

## Key Design Decisions

### 1. ESP-NOW Broadcast Architecture ✅

**Decision**: Use broadcast mode instead of peer-to-peer
**Rationale**:

- Eliminates MAC address management complexity
- Simplifies configuration (only Board ID needed)
- Makes system more maintainable
- Easier to add/remove boards

### 2. WAV-Only Audio Format ✅

**Decision**: Support only WAV files (16-bit, 44.1kHz stereo)
**Rationale**:

- Existing I2S implementation works perfectly with WAV
- No additional decoding libraries needed
- Better performance and reliability
- Simple ffmpeg conversion from MP3

### 3. Message Filtering by Board ID ✅

**Decision**: All boards receive broadcasts, filter by target ID
**Rationale**:

- Simpler than managing peer lists
- More reliable than unicast
- Natural fit for ESP-NOW broadcast
- Easy to debug (all boards see all messages)

### 4. Button Priority Handling ✅

**Decision**: Dual press > Red button > Single press
**Rationale**:

- Dual press is most specific action
- Red button is special (remote trigger)
- Single presses are most common

## System Architecture

### Hardware per Board

- Seeed Studio XIAO ESP32-C3
- MAX98357A I2S Amplifier
- MicroSD Card Breakout
- 4 Momentary Buttons
- 3W 4Ω Speaker

### Software Components

1. **Button Management**: Debouncing, multi-press detection
2. **Sound Management**: File discovery, random selection
3. **ESP-NOW Communication**: Broadcast messaging with filtering
4. **Audio Playback**: Existing I2S implementation (proven working)

### Communication Protocol

```cpp
struct ESPNowMessage {
    uint8_t senderBoardId;    // 1-5
    uint8_t targetBoardId;    // 1-5
    char soundFile[64];       // Filename
    uint32_t timestamp;       // For deduplication
    uint8_t checksum;         // Validation
};
```

## Button Functionality

| Button(s)           | Action                                   |
| ------------------- | ---------------------------------------- |
| Green               | Play static sound (configured per board) |
| Blue                | Play static sound (configured per board) |
| Yellow              | Play static sound (configured per board) |
| Green + Blue/Yellow | Play random sound locally                |
| Blue + Yellow       | Play random sound locally                |
| Red                 | Broadcast message to random board        |

## Configuration Strategy

### PlatformIO Environments

5 separate build environments (board1-board5) with:

- Unique `BOARD_ID` (1-5)
- 3 static sound file mappings
- Identical base configuration

### Example Configuration

```ini
[env:board1]
build_flags =
    -DBOARD_ID=1
    -DGREEN_SOUND=\"sound1.wav\"
    -DBLUE_SOUND=\"sound2.wav\"
    -DYELLOW_SOUND=\"sound3.wav\"
```

## Implementation Approach

### Phase 1: Core Functionality

1. Update PlatformIO configuration
2. Add data structures and constants
3. Implement sound file discovery
4. Implement button debouncing
5. Implement ESP-NOW communication

### Phase 2: Integration

6. Implement button handlers
7. Update setup() function
8. Update loop() function
9. Add playback state tracking

### Phase 3: Testing

10. Individual board testing
11. Multi-board communication testing
12. Stress testing
13. Error scenario testing

## Key Features

### Advantages

✅ No MAC address management
✅ Simple configuration (Board ID only)
✅ Proven audio implementation
✅ Robust error handling
✅ Easy to debug (comprehensive logging)
✅ Scalable (easy to add more boards)

### Technical Highlights

- **Broadcast Communication**: All boards receive, filter by ID
- **Random Selection**: True random using `esp_random()`
- **Debouncing**: 50ms debounce, 100ms dual-press window
- **Volume Control**: Existing soft-limiting algorithm
- **Error Handling**: Comprehensive logging and validation

## File Structure

```
sound_boards/
├── platformio.ini              # 5 board configurations
├── src/
│   └── main.cpp               # Complete implementation
├── ARCHITECTURE.md            # Detailed system design
├── IMPLEMENTATION_PLAN.md     # Step-by-step coding guide
├── QUICK_START.md             # Quick reference
├── PLANNING_SUMMARY.md        # This file
└── README.md                  # Hardware documentation
```

## Documentation Deliverables

1. **ARCHITECTURE.md** (598 lines)

   - Complete system design
   - Communication protocol
   - Hardware specifications
   - Software architecture
   - Error handling strategy
   - Testing strategy

2. **IMPLEMENTATION_PLAN.md** (738 lines)

   - Step-by-step implementation guide
   - Code snippets for each component
   - Testing procedures
   - Troubleshooting guide
   - Deployment checklist

3. **QUICK_START.md** (234 lines)
   - Quick reference guide
   - Setup instructions
   - Button functions
   - Common issues and solutions

## Next Steps

### Ready for Implementation

The planning phase is complete. You can now:

1. **Review Documentation**

   - Read through ARCHITECTURE.md for system understanding
   - Review IMPLEMENTATION_PLAN.md for coding steps
   - Reference QUICK_START.md for quick lookups

2. **Switch to Code Mode**

   - Implement the system following IMPLEMENTATION_PLAN.md
   - Start with PlatformIO configuration
   - Add components incrementally
   - Test after each major component

3. **Prepare Hardware**
   - Wire all 5 boards according to specifications
   - Format SD cards as FAT32
   - Convert audio files to WAV format
   - Prepare for testing

## Success Criteria

The system will be complete when:

- ✅ All 5 boards have unique configurations
- ✅ Single button presses play static sounds
- ✅ Dual button presses play random sounds
- ✅ Red button triggers remote playback
- ✅ Only target boards respond to messages
- ✅ System operates reliably
- ✅ Error handling works correctly

## Estimated Implementation Time

- **PlatformIO Configuration**: 15 minutes
- **Core Components**: 2-3 hours
- **Integration**: 1-2 hours
- **Testing**: 2-3 hours
- **Total**: 5-8 hours

## Questions Answered

✅ How to avoid MAC address management? → Use broadcast with Board ID filtering
✅ What audio format? → WAV only (16-bit, 44.1kHz stereo)
✅ How many sounds per board? → ~15-20 WAV files
✅ Button functionality? → Static sounds + random sounds + remote trigger
✅ How to handle dual button press? → 100ms detection window, plays random sound

## Ready to Proceed?

All planning is complete. The system architecture is well-defined, and detailed implementation instructions are available. You can now:

1. **Switch to Code Mode** to implement the solution
2. **Ask clarifying questions** if anything is unclear
3. **Request modifications** to the architecture if needed

The implementation will follow the step-by-step guide in IMPLEMENTATION_PLAN.md, which provides complete code snippets for each component.
