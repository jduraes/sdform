#ifndef SD_ANALYZER_H
#define SD_ANALYZER_H

#include "pico/stdlib.h"
#include "sd_card.h"

// Structure to hold SD card analysis results
typedef struct {
    sd_card_info_t card_info;
    bool has_mbr;
    bool has_gpt;
    uint32_t partition_count;
    bool initialized;
} sd_analysis_t;

// Partition information structure
typedef struct {
    uint8_t type;
    uint32_t start_lba;
    uint32_t size_sectors;
    char name[64];
    char filesystem[32];
    bool bootable;
} partition_info_t;

// Core SD card functions
int sd_analyzer_init(void);
int sd_analyzer_get_info(sd_analysis_t* analysis);
void sd_analyzer_print_card_info(const sd_card_info_t* card_info);
void sd_analyzer_print_banner(const char* app_name, const char* version);

// Partition analysis functions
int sd_analyzer_scan_partitions(sd_analysis_t* analysis, partition_info_t* partitions, uint32_t max_partitions);
bool sd_analyzer_is_gpt_protective_mbr(void);
int sd_analyzer_parse_gpt(partition_info_t* partitions, uint32_t max_partitions);
int sd_analyzer_parse_mbr(partition_info_t* partitions, uint32_t max_partitions);

// Filesystem analysis functions
int sd_analyzer_detect_filesystem(uint32_t start_lba, char* fs_type, size_t fs_type_size);
void sd_analyzer_analyze_fat(uint32_t start_lba);
void sd_analyzer_list_fat_directory(uint32_t dir_start_lba, const char* path);

// Utility functions
void sd_analyzer_print_hex_dump(uint8_t *data, size_t len, size_t offset);
void sd_analyzer_read_and_display_sector(uint32_t sector_num);
void sd_analyzer_format_fat_datetime(uint16_t date, uint16_t time, char* output, size_t output_size);
bool sd_analyzer_confirm_action(const char* prompt);

// SD card SPI configuration
#define SD_SPI_PORT spi0
#define SD_PIN_MISO 4
#define SD_PIN_CS   5
#define SD_PIN_SCK  2
#define SD_PIN_MOSI 3

#endif // SD_ANALYZER_H
