# SD Card Formatter for Raspberry Pi Pico

A Raspberry Pi Pico-based SD card formatter that can wipe and format SD cards with various partition tables and filesystems.

## Features

- **Safe Design**: All destructive operations are simulated by default to prevent accidental data loss
- **Multiple Partition Types**: Supports MBR and GPT partition tables
- **Multiple Filesystems**: Supports FAT12, FAT16, FAT32, and exFAT (planned)
- **Content Preview**: Shows current SD card content before formatting
- **Confirmation Dialog**: Asks for explicit confirmation before formatting
- **Modular Design**: Reuses SD card analysis functions from SDAnalyst project

## Hardware Requirements

- Raspberry Pi Pico or Pico W
- SD card connected via SPI:
  - GPIO 2: SCK (Clock)
  - GPIO 3: MOSI (Data Out)
  - GPIO 4: MISO (Data In)  
  - GPIO 5: CS (Chip Select)
- 3.3V power supply for SD card

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Installation

1. Hold the BOOTSEL button while connecting Pico to USB
2. Copy `sdformatter.uf2` to the RPI-RP2 drive
3. Connect to USB serial at 115200 baud to see output

## Safety Features

- **Simulation Mode**: All write operations are simulated by default
- **Auto-decline**: Format confirmation automatically declines for safety
- **Clear Warnings**: Multiple warnings about data loss before formatting
- **No Accidental Execution**: Requires code modification to enable actual formatting

## To Enable Actual Formatting (DANGEROUS!)

1. Implement `sd_write_block()` function in `sd_card.c`
2. Enable confirmation in `sd_formatter_confirm_format()`
3. Add proper MBR/GPT creation logic in `sd_formatter.c`
4. Add FAT filesystem creation logic
5. **Test thoroughly with disposable SD cards first!**

## Project Structure

```
src/
├── main.c              # Main formatter application
├── sd_card.h/.c        # Low-level SD card SPI interface
├── sd_analyzer.h/.c    # SD card analysis functions (shared with SDAnalyst)
└── sd_formatter.h/.c   # SD card formatting functions
```

## Related Projects

- [SDAnalyst](../SDAnalyst/) - SD card analysis and content viewer

## Warning

⚠️ **This tool can permanently erase all data on SD cards when fully implemented. Use with extreme caution and only on cards you can afford to lose. Always backup important data first!**
