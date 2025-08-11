#include "sd_analyzer.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include <stdio.h>
#include <string.h>

static sd_analysis_t current_analysis = {0};

int sd_analyzer_init(void) {
    printf("Initializing SD card...\\n");
    printf("SPI pins: SCK=%d, MOSI=%d, MISO=%d, CS=%d\\n", 
           SD_PIN_SCK, SD_PIN_MOSI, SD_PIN_MISO, SD_PIN_CS);
    
    int result = sd_init(SD_SPI_PORT, SD_PIN_SCK, SD_PIN_MOSI, SD_PIN_MISO, SD_PIN_CS);
    if (result != 0) {
        printf("Failed to initialize SD card! Error code: %d\\n", result);
        printf("\\nTroubleshooting:\\n");
        printf("1. Check all SPI connections are secure\\n");
        printf("2. Ensure SD card is properly inserted\\n");
        printf("3. Try a different SD card\\n");
        printf("4. Check power supply (3.3V for SD card)\\n");
        printf("5. Verify pin connections match the code\\n");
        current_analysis.initialized = false;
        return result;
    }
    
    printf("SD card initialized successfully!\\n");
    current_analysis.initialized = true;
    return 0;
}

int sd_analyzer_get_info(sd_analysis_t* analysis) {
    if (!current_analysis.initialized) {
        return -1;
    }
    
    // Get card info
    if (sd_get_info(&current_analysis.card_info) != 0) {
        return -2;
    }
    
    // Check for partition table type
    uint8_t mbr[512];
    if (sd_read_block(0, mbr) != 0) {
        return -3;
    }
    
    // Check boot signature
    current_analysis.has_mbr = (mbr[510] == 0x55 && mbr[511] == 0xAA);
    
    // Check for GPT protective MBR
    current_analysis.has_gpt = false;
    if (current_analysis.has_mbr) {
        for (int i = 0; i < 4; i++) {
            uint8_t *partition = &mbr[446 + i * 16];
            if (partition[4] == 0xEE) { // GPT protective MBR
                current_analysis.has_gpt = true;
                break;
            }
        }
    }
    
    *analysis = current_analysis;
    return 0;
}

void sd_analyzer_print_card_info(const sd_card_info_t* card_info) {
    printf("\\nSD Card Information:\\n");
    printf("Type: %s\\n", card_info->type == SD_CARD_TYPE_SD1 ? "SD1" : 
                      card_info->type == SD_CARD_TYPE_SD2 ? "SD2" : 
                      card_info->type == SD_CARD_TYPE_SDHC ? "SDHC" : "Unknown");
    printf("Capacity: %.2f MB (%u blocks)\\n", 
           (card_info->blocks * 512.0) / (1024 * 1024), 
           card_info->blocks);
    printf("Block size: %u bytes\\n", card_info->block_size);
}

void sd_analyzer_print_banner(const char* app_name, const char* version) {
    printf("********************************************************************************\\n");
    printf("Raspberry Pi Pico %s\\n", app_name);
    printf("Version: %s\\n", version);
    printf("Built: %s %s\\n", __DATE__, __TIME__);
    printf("================================================================================\\n");
}

bool sd_analyzer_is_gpt_protective_mbr(void) {
    return current_analysis.has_gpt;
}

int sd_analyzer_parse_mbr(partition_info_t* partitions, uint32_t max_partitions) {
    uint8_t mbr[512];
    if (sd_read_block(0, mbr) != 0) {
        return -1;
    }
    
    if (mbr[510] != 0x55 || mbr[511] != 0xAA) {
        return -2; // Invalid boot signature
    }
    
    int partition_count = 0;
    printf("\\n=== MBR Partition Table ===\\n");
    
    for (int i = 0; i < 4 && partition_count < max_partitions; i++) {
        uint8_t *partition = &mbr[446 + i * 16];
        
        uint8_t status = partition[0];
        uint8_t type = partition[4];
        uint32_t lba_start = partition[8] | (partition[9] << 8) | 
                            (partition[10] << 16) | (partition[11] << 24);
        uint32_t lba_size = partition[12] | (partition[13] << 8) | 
                           (partition[14] << 16) | (partition[15] << 24);
        
        if (type != 0x00 && lba_start > 0 && lba_size > 0) {
            partitions[partition_count].type = type;
            partitions[partition_count].start_lba = lba_start;
            partitions[partition_count].size_sectors = lba_size;
            partitions[partition_count].bootable = (status == 0x80);
            snprintf(partitions[partition_count].name, sizeof(partitions[partition_count].name),
                    "Partition %d", i + 1);
            
            // Detect filesystem
            sd_analyzer_detect_filesystem(lba_start, partitions[partition_count].filesystem, 
                                        sizeof(partitions[partition_count].filesystem));
            
            printf("Partition %d:\\n", i + 1);
            printf("  Status: 0x%02X (%s)\\n", status, (status == 0x80) ? "Bootable" : "Not bootable");
            printf("  Type: 0x%02X", type);
            switch (type) {
                case 0x01: printf(" (FAT12)"); break;
                case 0x04: printf(" (FAT16 <32MB)"); break;
                case 0x06: printf(" (FAT16)"); break;
                case 0x0B: printf(" (FAT32)"); break;
                case 0x0C: printf(" (FAT32 LBA)"); break;
                case 0x0E: printf(" (FAT16 LBA)"); break;
                case 0x83: printf(" (Linux)"); break;
                case 0xEE: printf(" (GPT Protective MBR)"); break;
                default: printf(" (Unknown)"); break;
            }
            printf("\\n");
            printf("  LBA Start: %u\\n", lba_start);
            printf("  Size: %u sectors (%.2f MB)\\n", lba_size, (lba_size * 512.0) / (1024 * 1024));
            printf("  Filesystem: %s\\n", partitions[partition_count].filesystem);
            
            partition_count++;
        }
    }
    
    return partition_count;
}

int sd_analyzer_parse_gpt(partition_info_t* partitions, uint32_t max_partitions) {
    uint8_t gpt_header[512];
    printf("\\n=== GPT Partition Table ===\\n");
    
    if (sd_read_block(1, gpt_header) != 0) {
        return -1;
    }
    
    if (memcmp(gpt_header, "EFI PART", 8) != 0) {
        return -2; // Invalid GPT signature
    }
    
    uint32_t partition_entry_lba = gpt_header[72] | (gpt_header[73] << 8) | 
                                  (gpt_header[74] << 16) | (gpt_header[75] << 24);
    uint32_t num_partitions = gpt_header[80] | (gpt_header[81] << 8) | 
                             (gpt_header[82] << 16) | (gpt_header[83] << 24);
    uint32_t entry_size = gpt_header[84] | (gpt_header[85] << 8) | 
                         (gpt_header[86] << 16) | (gpt_header[87] << 24);
    
    printf("Number of partitions: %u\\n", num_partitions);
    printf("Partition entries start at LBA: %u\\n", partition_entry_lba);
    
    uint8_t partition_table[512];
    if (sd_read_block(partition_entry_lba, partition_table) != 0) {
        return -3;
    }
    
    int partition_count = 0;
    uint32_t partitions_per_sector = 512 / entry_size;
    uint32_t partitions_to_check = (num_partitions < partitions_per_sector) ? 
                                   num_partitions : partitions_per_sector;
    
    for (uint32_t i = 0; i < partitions_to_check && partition_count < max_partitions; i++) {
        uint8_t *entry = &partition_table[i * entry_size];
        
        // Check if partition exists
        bool empty = true;
        for (int j = 0; j < 16; j++) {
            if (entry[j] != 0) {
                empty = false;
                break;
            }
        }
        
        if (!empty) {
            uint64_t start_lba = 0, end_lba = 0;
            for (int j = 0; j < 8; j++) {
                start_lba |= ((uint64_t)entry[32 + j]) << (j * 8);
                end_lba |= ((uint64_t)entry[40 + j]) << (j * 8);
            }
            
            // Extract partition name (UTF-16, so take every other byte)
            char name[37] = {0};
            for (int j = 0, k = 0; j < 72 && k < 36; j += 2, k++) {
                name[k] = entry[56 + j];
                if (name[k] == 0) break;
            }
            
            partitions[partition_count].type = 0xEE; // GPT partition
            partitions[partition_count].start_lba = (uint32_t)start_lba;
            partitions[partition_count].size_sectors = (uint32_t)(end_lba - start_lba + 1);
            partitions[partition_count].bootable = false; // GPT doesn't use bootable flag the same way
            strncpy(partitions[partition_count].name, name[0] ? name : "(unnamed)", 
                   sizeof(partitions[partition_count].name) - 1);
            
            // Detect filesystem
            sd_analyzer_detect_filesystem((uint32_t)start_lba, partitions[partition_count].filesystem,
                                        sizeof(partitions[partition_count].filesystem));
            
            printf("\\nPartition %u:\\n", i + 1);
            printf("  Name: %s\\n", partitions[partition_count].name);
            printf("  Start LBA: %llu\\n", start_lba);
            printf("  End LBA: %llu\\n", end_lba);
            printf("  Size: %llu sectors (%.2f MB)\\n", 
                   end_lba - start_lba + 1, 
                   ((end_lba - start_lba + 1) * 512.0) / (1024 * 1024));
            printf("  Filesystem: %s\\n", partitions[partition_count].filesystem);
            
            partition_count++;
        }
    }
    
    return partition_count;
}

int sd_analyzer_detect_filesystem(uint32_t start_lba, char* fs_type, size_t fs_type_size) {
    uint8_t boot_sector[512];
    
    if (sd_read_block(start_lba, boot_sector) != 0) {
        strncpy(fs_type, "Read Error", fs_type_size - 1);
        return -1;
    }
    
    // Check for FAT filesystem signatures
    if ((boot_sector[510] == 0x55 && boot_sector[511] == 0xAA) && 
        (memcmp(&boot_sector[54], "FAT12   ", 8) == 0 || 
         memcmp(&boot_sector[54], "FAT16   ", 8) == 0 || 
         memcmp(&boot_sector[82], "FAT32   ", 8) == 0)) {
        
        if (memcmp(&boot_sector[82], "FAT32   ", 8) == 0) {
            strncpy(fs_type, "FAT32", fs_type_size - 1);
        } else if (memcmp(&boot_sector[54], "FAT16   ", 8) == 0) {
            strncpy(fs_type, "FAT16", fs_type_size - 1);
        } else {
            strncpy(fs_type, "FAT12", fs_type_size - 1);
        }
        
    } else if (memcmp(&boot_sector[3], "EXFAT   ", 8) == 0) {
        strncpy(fs_type, "exFAT", fs_type_size - 1);
        
    } else if (boot_sector[56] == 0x53 && boot_sector[57] == 0xEF) {
        strncpy(fs_type, "ext2/3/4", fs_type_size - 1);
        
    } else {
        strncpy(fs_type, "Unknown", fs_type_size - 1);
    }
    
    fs_type[fs_type_size - 1] = '\0';
    return 0;
}

void sd_analyzer_print_hex_dump(uint8_t *data, size_t len, size_t offset) {
    for (size_t i = 0; i < len; i++) {
        if (i % 16 == 0) {
            printf("%08X: ", (unsigned int)(offset + i));
        }
        printf("%02X ", data[i]);
        if (i % 16 == 15 || i == len - 1) {
            int padding = 15 - (i % 16);
            for (int p = 0; p < padding; p++) {
                printf("   ");
            }
            printf(" |");
            for (size_t j = i - (i % 16); j <= i; j++) {
                char c = (data[j] >= 32 && data[j] <= 126) ? data[j] : '.';
                printf("%c", c);
            }
            printf("|\\n");
        }
    }
}

void sd_analyzer_read_and_display_sector(uint32_t sector_num) {
    uint8_t buffer[512];
    printf("\\n--- Reading sector %u ---\\n", sector_num);
    
    if (sd_read_block(sector_num, buffer) == 0) {
        sd_analyzer_print_hex_dump(buffer, 512, sector_num * 512);
    } else {
        printf("Error reading sector %u\\n", sector_num);
    }
}

bool sd_analyzer_confirm_action(const char* prompt) {
    // Since this is embedded, we'll return true by default
    // In a real implementation, this could wait for UART input
    printf("%s [Y/n]: ", prompt);
    printf("(Auto-confirming for embedded system)\\n");
    return true;
}

void sd_analyzer_format_fat_datetime(uint16_t date, uint16_t time, char* output, size_t output_size) {
    int year = ((date >> 9) & 0x7F) + 1980;
    int month = (date >> 5) & 0x0F;
    int day = date & 0x1F;
    int hour = (time >> 11) & 0x1F;
    int minute = (time >> 5) & 0x3F;
    
    const char* months[] = {"???", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    
    if (month >= 1 && month <= 12 && day >= 1 && day <= 31) {
        snprintf(output, output_size, "%s %2d %02d:%02d", 
                months[month], day, hour, minute);
    } else {
        snprintf(output, output_size, "??? ?? ??:??");
    }
}

// Simplified FAT analysis and directory listing functions
void sd_analyzer_analyze_fat(uint32_t start_lba) {
    uint8_t boot_sector[512];
    if (sd_read_block(start_lba, boot_sector) != 0) {
        printf("  Error reading FAT boot sector\\n");
        return;
    }
    
    uint16_t bytes_per_sector = boot_sector[11] | (boot_sector[12] << 8);
    uint8_t sectors_per_cluster = boot_sector[13];
    uint16_t reserved_sectors = boot_sector[14] | (boot_sector[15] << 8);
    uint8_t num_fats = boot_sector[16];
    uint16_t root_entries = boot_sector[17] | (boot_sector[18] << 8);
    uint32_t sectors_per_fat = boot_sector[22] | (boot_sector[23] << 8);
    
    // For FAT32, sectors per FAT is at offset 36
    if (sectors_per_fat == 0) {
        sectors_per_fat = boot_sector[36] | (boot_sector[37] << 8) | 
                         (boot_sector[38] << 16) | (boot_sector[39] << 24);
    }
    
    printf("  Bytes per sector: %u\\n", bytes_per_sector);
    printf("  Sectors per cluster: %u\\n", sectors_per_cluster);
    printf("  Reserved sectors: %u\\n", reserved_sectors);
    printf("  Number of FATs: %u\\n", num_fats);
    printf("  Root entries: %u\\n", root_entries);
    printf("  Sectors per FAT: %u\\n", sectors_per_fat);
    
    uint32_t fat_start = start_lba + reserved_sectors;
    uint32_t root_dir_start = fat_start + (num_fats * sectors_per_fat);
    
    printf("  FAT starts at LBA: %u\\n", fat_start);
    printf("  Root directory at LBA: %u\\n", root_dir_start);
    
    sd_analyzer_list_fat_directory(root_dir_start, "/");
}

void sd_analyzer_list_fat_directory(uint32_t dir_start_lba, const char* path) {
    uint8_t buffer[512];
    printf("\\n  === Directory listing for %s ===\\n", path);
    
    if (sd_read_block(dir_start_lba, buffer) != 0) {
        printf("  Error reading directory sector\\n");
        return;
    }
    
    int file_count = 0;
    uint64_t total_size = 0;
    char long_filename[256] = {0};
    
    for (int i = 0; i < 16; i++) {
        uint8_t *entry = &buffer[i * 32];
        
        if (entry[0] == 0x00) break;
        if (entry[0] == 0xE5) continue;
        
        // Handle Long Filename entries
        if (entry[11] == 0x0F) {
            uint8_t sequence = entry[0] & 0x3F;
            int pos = (sequence - 1) * 13;
            
            if (pos >= 0 && pos < 243) {
                // Extract Unicode characters (simplified ASCII conversion)
                for (int j = 0; j < 5; j++) {
                    uint16_t unicode_char = entry[1 + j * 2] | (entry[2 + j * 2] << 8);
                    if (unicode_char == 0 || unicode_char == 0xFFFF) break;
                    if (pos + j < 255) long_filename[pos + j] = (unicode_char < 128) ? (char)unicode_char : '?';
                }
                for (int j = 0; j < 6; j++) {
                    uint16_t unicode_char = entry[14 + j * 2] | (entry[15 + j * 2] << 8);
                    if (unicode_char == 0 || unicode_char == 0xFFFF) break;
                    if (pos + 5 + j < 255) long_filename[pos + 5 + j] = (unicode_char < 128) ? (char)unicode_char : '?';
                }
                for (int j = 0; j < 2; j++) {
                    uint16_t unicode_char = entry[28 + j * 2] | (entry[29 + j * 2] << 8);
                    if (unicode_char == 0 || unicode_char == 0xFFFF) break;
                    if (pos + 11 + j < 255) long_filename[pos + 11 + j] = (unicode_char < 128) ? (char)unicode_char : '?';
                }
            }
            continue;
        }
        
        // Regular 8.3 entry
        char short_filename[13] = {0};
        memcpy(short_filename, entry, 8);
        short_filename[8] = 0;
        
        // Trim spaces
        for (int j = 7; j >= 0 && short_filename[j] == ' '; j--) {
            short_filename[j] = 0;
        }
        
        // Add extension
        if (entry[8] != ' ') {
            if (strlen(short_filename) > 0) strcat(short_filename, ".");
            strncat(short_filename, (char*)&entry[8], 3);
            int len = strlen(short_filename);
            for (int j = len - 1; j >= 0 && short_filename[j] == ' '; j--) {
                short_filename[j] = 0;
            }
        }
        
        uint8_t attributes = entry[11];
        uint32_t file_size = entry[28] | (entry[29] << 8) | 
                            (entry[30] << 16) | (entry[31] << 24);
        
        uint16_t mod_time = entry[22] | (entry[23] << 8);
        uint16_t mod_date = entry[24] | (entry[25] << 8);
        
        const char* display_name = (long_filename[0] != 0) ? long_filename : 
                                  (short_filename[0] != 0) ? short_filename : "<no name>";
        
        // Format like Unix 'ls -l'
        printf("  ");
        printf("%c", (attributes & 0x10) ? 'd' : '-');
        printf("r");
        printf("%c", (attributes & 0x01) ? '-' : 'w');
        printf("%c", (attributes & 0x10) ? 'x' : '-');
        printf("r");
        printf("%c", (attributes & 0x01) ? '-' : 'w');
        printf("%c", (attributes & 0x10) ? 'x' : '-');
        printf("r");
        printf("%c", (attributes & 0x01) ? '-' : 'w');
        printf("%c", (attributes & 0x10) ? 'x' : '-');
        printf(" %2d", 1);
        
        if (attributes & 0x10) {
            printf(" %10s", "<DIR>");
        } else {
            printf(" %10u", file_size);
            total_size += file_size;
        }
        
        char datetime_str[32];
        sd_analyzer_format_fat_datetime(mod_date, mod_time, datetime_str, sizeof(datetime_str));
        printf(" %s", datetime_str);
        printf(" %s", display_name);
        
        if (long_filename[0] != 0 && strcmp(long_filename, short_filename) != 0) {
            printf(" [%s]", short_filename);
        }
        
        printf("\\n");
        file_count++;
        
        memset(long_filename, 0, sizeof(long_filename));
    }
    
    printf("  total %d\\n", (int)(total_size / 1024));
    printf("  %d files and directories\\n", file_count);
}
