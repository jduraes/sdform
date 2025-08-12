#include "sd_formatter.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

int sd_formatter_show_card_content(void) {
    sd_analysis_t analysis;
    if (sd_analyzer_get_info(&analysis) != 0) {
        printf("Failed to read SD card information\n");
        return -1;
    }
    
    printf("\n=== CURRENT SD CARD CONTENT ===\n");
    printf("Card: %s, %.2f MB (%u blocks)\n", 
           analysis.card_info.type == SD_CARD_TYPE_SDHC ? "SDHC" : "SD",
           (analysis.card_info.blocks * 512.0) / (1024 * 1024),
           analysis.card_info.blocks);
    
    // Show partition information
    partition_info_t partitions[8];
    int partition_count = 0;
    
    if (analysis.has_gpt) {
        printf("Partition table: GPT\n");
        partition_count = sd_analyzer_parse_gpt(partitions, 8);
    } else if (analysis.has_mbr) {
        printf("Partition table: MBR\n");
        partition_count = sd_analyzer_parse_mbr(partitions, 8);
    } else {
        printf("Partition table: None\n");
    }
    
    if (partition_count > 0) {
        printf("\n+-----+-------------+---------+-----------+\n");
        printf("| #   | Name        | Type    | Size      |\n");
        printf("+-----+-------------+---------+-----------+\n");
        
        for (int i = 0; i < partition_count; i++) {
            char name[13];
            if (strlen(partitions[i].name) == 0) {
                snprintf(name, sizeof(name), "Partition %d", i + 1);
            } else {
                snprintf(name, sizeof(name), "%.11s", partitions[i].name);
            }
            
            printf("| %-3d | %-11s | %-7s | %6.1f MB |\n", 
                   i + 1,
                   name,
                   partitions[i].filesystem,
                   (partitions[i].size_sectors * 512.0) / (1024 * 1024));
        }
        printf("+-----+-------------+---------+-----------+\n");
        
        // Show contents of ALL partitions
        printf("\n=== ALL PARTITION CONTENTS ===\n");
        for (int i = 0; i < partition_count; i++) {
            printf("\n--- PARTITION %d: %s (%.1f MB) ---\n", 
                   i + 1, 
                   partitions[i].filesystem,
                   (partitions[i].size_sectors * 512.0) / (1024 * 1024));
            
            if (strcmp(partitions[i].filesystem, "FAT32") == 0 ||
                strcmp(partitions[i].filesystem, "FAT16") == 0 ||
                strcmp(partitions[i].filesystem, "FAT12") == 0) {
                
                // Calculate actual root directory location for FAT
                uint32_t fat_start_lba = partitions[i].start_lba;
                
                // Read the boot sector to get proper FAT parameters
                uint8_t boot_sector[512];
                if (sd_read_block(fat_start_lba, boot_sector) == 0) {
                    // Parse FAT boot sector to find root directory
                    uint16_t reserved_sectors = *(uint16_t*)(boot_sector + 14);
                    uint8_t num_fats = boot_sector[16];
                    uint32_t fat_size = *(uint32_t*)(boot_sector + 36); // FAT32
                    
                    uint32_t root_dir_lba = fat_start_lba + reserved_sectors + (num_fats * fat_size);
                    
                    printf("Root directory at LBA %u:\n", root_dir_lba);
                    sd_analyzer_list_fat_directory(root_dir_lba, "/");
                } else {
                    printf("Could not read boot sector for partition %d\n", i + 1);
                }
                
            } else if (strcmp(partitions[i].filesystem, "exFAT") == 0) {
                printf("exFAT partition - contents listing not implemented yet\n");
            } else if (strncmp(partitions[i].filesystem, "ext", 3) == 0) {
                printf("Linux ext filesystem - contents listing not implemented yet\n");
            } else {
                printf("Unknown filesystem type - cannot list contents\n");
            }
        }
        
    } else {
        printf("No partitions found.\n");
    }
    
    return partition_count;
}

bool sd_formatter_confirm_format(const sd_analysis_t* analysis) {
    printf("\n==================== WARNING ====================\n");
    printf("This will PERMANENTLY ERASE ALL DATA on the SD card!\n");
    printf("Card capacity: %.2f MB\n", 
           (analysis->card_info.blocks * 512.0) / (1024 * 1024));
    printf("==================================================\n");
    printf("\nDo you want to continue with formatting? (y/N): ");
    
    // In a real implementation, this would wait for UART input
    // For now, we'll auto-confirm after showing the warning
    printf("(Auto-declining for safety - modify code to enable)\n");
    return false; // Safe default - prevents accidental formatting
}

int sd_formatter_get_format_options(format_options_t* options) {
    // Set default options
    options->partition_table = PARTITION_TABLE_MBR;
    options->filesystem = FILESYSTEM_FAT32;
    strcpy(options->volume_label, "SDCARD");
    options->quick_format = true;
    options->confirm_format = false;
    
    printf("\n=== FORMAT OPTIONS ===\n");
    printf("Select partition table type:\n");
    printf("  1. MBR (Master Boot Record) [default]\n");
    printf("  2. GPT (GUID Partition Table)\n");
    printf("Choice (1-2): ");
    
    // Auto-select MBR for embedded system
    printf("1 (MBR selected)\n");
    
    printf("\nSelect filesystem type:\n");
    printf("  1. FAT12 (for small cards < 16MB)\n");
    printf("  2. FAT16 (for cards 16MB-2GB)\n");
    printf("  3. FAT32 (for cards > 32MB) [default]\n");
    printf("  4. exFAT (for cards > 32GB)\n");
    printf("Choice (1-4): ");
    
    // Auto-select FAT32 for embedded system
    printf("3 (FAT32 selected)\n");
    
    printf("\nVolume label [%s]: ", options->volume_label);
    printf("(Using default: %s)\n", options->volume_label);
    
    return 0;
}

int sd_formatter_wipe_card(void) {
    printf("\nWiping SD card...\n");
    
    // Get card info first
    sd_analysis_t analysis;
    if (sd_analyzer_get_info(&analysis) != 0) {
        return -1;
    }
    
    // Wipe first few sectors (MBR, GPT header, etc.)
    uint8_t zero_buffer[512] = {0};
    
    printf("Clearing partition tables and boot sectors...\n");
    
    // Write functionality not implemented yet
    printf("Warning: sd_write_block not implemented yet\n");
    
    // For now, just simulate the process
    printf("Simulating wipe of first 64 sectors...\n");
    for (int i = 0; i < 64; i++) {
        printf("\rClearing sector %d/64", i + 1);
        sleep_ms(10); // Simulate write time
    }
    printf("\nWipe simulation complete\n");
    
    return 0;
}

int sd_formatter_create_partition_table(partition_table_type_t type, uint32_t total_sectors) {
    printf("\nCreating %s partition table...\n", 
           sd_formatter_get_partition_table_name(type));
    
    if (type == PARTITION_TABLE_MBR) {
        printf("Creating MBR with single partition covering full card\n");
        // MBR creation logic would go here
        
    } else if (type == PARTITION_TABLE_GPT) {
        printf("Creating GPT with single partition covering full card\n");
        // GPT creation logic would go here
        
    } else {
        return -1;
    }
    
    printf("Partition table creation simulated\n");
    return 0;
}

int sd_formatter_format_partition(uint32_t start_lba, uint32_t size_sectors, 
                                 filesystem_type_t fs_type, const char* volume_label) {
    printf("\nFormatting partition at LBA %u (%.2f MB) as %s...\n",
           start_lba, 
           (size_sectors * 512.0) / (1024 * 1024),
           sd_formatter_get_filesystem_name(fs_type));
    
    printf("Volume label: %s\n", volume_label);
    
    if (fs_type == FILESYSTEM_FAT32) {
        printf("Creating FAT32 boot sector...\n");
        printf("Initializing File Allocation Tables...\n");
        printf("Creating root directory...\n");
        
    } else if (fs_type == FILESYSTEM_FAT16) {
        printf("Creating FAT16 boot sector...\n");
        printf("Initializing File Allocation Tables...\n");
        printf("Creating root directory...\n");
        
    } else if (fs_type == FILESYSTEM_EXFAT) {
        printf("Creating exFAT boot sector...\n");
        printf("Initializing File Allocation Table...\n");
        printf("Creating root directory cluster...\n");
        
    } else {
        return -1;
    }
    
    printf("Filesystem creation simulated\n");
    return 0;
}

const char* sd_formatter_get_partition_table_name(partition_table_type_t type) {
    switch (type) {
        case PARTITION_TABLE_MBR: return "MBR";
        case PARTITION_TABLE_GPT: return "GPT";
        default: return "Unknown";
    }
}

const char* sd_formatter_get_filesystem_name(filesystem_type_t type) {
    switch (type) {
        case FILESYSTEM_FAT12: return "FAT12";
        case FILESYSTEM_FAT16: return "FAT16";
        case FILESYSTEM_FAT32: return "FAT32";
        case FILESYSTEM_EXFAT: return "exFAT";
        default: return "Unknown";
    }
}

void sd_formatter_print_format_summary(const format_options_t* options, 
                                       const sd_analysis_t* analysis) {
    printf("\n=== FORMAT SUMMARY ===\n");
    printf("SD Card: %.2f MB\n", 
           (analysis->card_info.blocks * 512.0) / (1024 * 1024));
    printf("Partition table: %s\n", 
           sd_formatter_get_partition_table_name(options->partition_table));
    printf("Filesystem: %s\n", 
           sd_formatter_get_filesystem_name(options->filesystem));
    printf("Volume label: %s\n", options->volume_label);
    printf("Quick format: %s\n", options->quick_format ? "Yes" : "No");
    printf("======================\n");
}
