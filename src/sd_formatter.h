#ifndef SD_FORMATTER_H
#define SD_FORMATTER_H

#include "sd_analyzer.h"

// Partition table types
typedef enum {
    PARTITION_TABLE_MBR = 0,
    PARTITION_TABLE_GPT = 1
} partition_table_type_t;

// Filesystem types
typedef enum {
    FILESYSTEM_FAT12 = 0,
    FILESYSTEM_FAT16 = 1,
    FILESYSTEM_FAT32 = 2,
    FILESYSTEM_EXFAT = 3
} filesystem_type_t;

// Formatting options
typedef struct {
    partition_table_type_t partition_table;
    filesystem_type_t filesystem;
    char volume_label[12];
    bool quick_format;
    bool confirm_format;
} format_options_t;

// SD formatter functions
int sd_formatter_show_card_content(void);
bool sd_formatter_confirm_format(const sd_analysis_t* analysis);
int sd_formatter_get_format_options(format_options_t* options);
int sd_formatter_wipe_card(void);
int sd_formatter_create_partition_table(partition_table_type_t type, uint32_t total_sectors);
int sd_formatter_format_partition(uint32_t start_lba, uint32_t size_sectors, 
                                 filesystem_type_t fs_type, const char* volume_label);

// Utility functions
const char* sd_formatter_get_partition_table_name(partition_table_type_t type);
const char* sd_formatter_get_filesystem_name(filesystem_type_t type);
void sd_formatter_print_format_summary(const format_options_t* options, 
                                       const sd_analysis_t* analysis);

#endif // SD_FORMATTER_H
