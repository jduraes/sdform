#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "sd_analyzer.h"
#include "sd_formatter.h"

#define VERSION "1.0.0"

int main() {
    stdio_init_all();
    
    // Wait for USB serial to be ready
    sleep_ms(2000);
    
    // Display startup banner
    sd_analyzer_print_banner("SD Card Formatter", VERSION);
    
    // Initialize SD card
    if (sd_analyzer_init() != 0) {
        printf("Cannot proceed without SD card initialization\\n");
        while (1) sleep_ms(1000);
    }
    
    // Show current card content
    printf("\\nAnalyzing current SD card content...\\n");
    int partition_count = sd_formatter_show_card_content();
    
    if (partition_count < 0) {
        printf("Failed to read SD card content\\n");
        while (1) sleep_ms(1000);
    }
    
    // Get SD card analysis for confirmation
    sd_analysis_t analysis;
    if (sd_analyzer_get_info(&analysis) != 0) {
        printf("Failed to get SD card information\\n");
        while (1) sleep_ms(1000);
    }
    
    // Ask for format confirmation
    if (!sd_formatter_confirm_format(&analysis)) {
        printf("\\nFormat operation cancelled by user\\n");
        printf("Exiting safely...\\n");
        while (1) sleep_ms(1000);
    }
    
    // Get format options
    format_options_t options;
    if (sd_formatter_get_format_options(&options) != 0) {
        printf("Failed to get format options\\n");
        while (1) sleep_ms(1000);
    }
    
    // Print format summary
    sd_formatter_print_format_summary(&options, &analysis);
    
    // Perform the format operation (simulated for safety)
    printf("\\n=== BEGINNING FORMAT OPERATION ===\\n");
    
    printf("Step 1: Wiping existing data...\\n");
    if (sd_formatter_wipe_card() != 0) {
        printf("Failed to wipe SD card\\n");
        while (1) sleep_ms(1000);
    }
    
    printf("\\nStep 2: Creating partition table...\\n");
    if (sd_formatter_create_partition_table(options.partition_table, analysis.card_info.blocks) != 0) {
        printf("Failed to create partition table\\n");
        while (1) sleep_ms(1000);
    }
    
    printf("\\nStep 3: Formatting filesystem...\\n");
    uint32_t partition_start = (options.partition_table == PARTITION_TABLE_GPT) ? 2048 : 2048; // Standard alignment
    uint32_t partition_size = analysis.card_info.blocks - partition_start - 1024; // Leave some space at end
    
    if (sd_formatter_format_partition(partition_start, partition_size, 
                                     options.filesystem, options.volume_label) != 0) {
        printf("Failed to format partition\\n");
        while (1) sleep_ms(1000);
    }
    
    printf("\\n=== FORMAT COMPLETE ===\\n");
    printf("\\n*** IMPORTANT NOTE ***\\n");
    printf("This is a SIMULATION for safety. To enable actual formatting:\\n");
    printf("1. Implement sd_write_block() function in sd_card.c\\n");
    printf("2. Add proper MBR/GPT creation logic\\n");
    printf("3. Add FAT filesystem creation logic\\n");
    printf("4. Enable confirmation in sd_formatter_confirm_format()\\n");
    printf("5. Test thoroughly with non-important SD cards first!\\n");
    
    printf("\\nFormatter ready for development. System will now idle.\\n");
    
    // Keep the program running
    while (1) {
        sleep_ms(1000);
    }
    
    return 0;
}
