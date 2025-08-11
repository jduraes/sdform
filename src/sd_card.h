#ifndef SD_CARD_H
#define SD_CARD_H

#include "pico/stdlib.h"
#include "hardware/spi.h"

// SD card types
#define SD_CARD_TYPE_SD1 1
#define SD_CARD_TYPE_SD2 2
#define SD_CARD_TYPE_SDHC 3

// SD card commands
#define CMD0 (0x40 | 0)
#define CMD8 (0x40 | 8)
#define CMD55 (0x40 | 55)
#define CMD58 (0x40 | 58)
#define ACMD41 (0x40 | 41)
#define READ_SINGLE_BLOCK (0x40 | 17)

typedef struct {
    uint8_t type;
    uint32_t blocks;
    uint16_t block_size;
} sd_card_info_t;

int sd_init(spi_inst_t *spi, uint sck, uint mosi, uint miso, uint cs);
int sd_get_info(sd_card_info_t *info);
int sd_read_block(uint32_t block, uint8_t *buffer);

#endif

