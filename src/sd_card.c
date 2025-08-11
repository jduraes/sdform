#include "sd_card.h"
#include "hardware/gpio.h"
#include <stdio.h>
#include <string.h>

static spi_inst_t *sd_spi;
static uint sd_cs_pin;
static sd_card_info_t sd_info;

static void sd_cs_select() {
    gpio_put(sd_cs_pin, 0);
}

static void sd_cs_deselect() {
    gpio_put(sd_cs_pin, 1);
}

static uint8_t sd_spi_write(uint8_t data) {
    uint8_t rx_data;
    spi_write_read_blocking(sd_spi, &data, &rx_data, 1);
    return rx_data;
}

static void sd_wait_not_busy() {
    while (sd_spi_write(0xFF) != 0xFF);
}

static uint8_t sd_send_command(uint8_t cmd, uint32_t arg) {
    uint8_t response;
    
    sd_wait_not_busy();
    
    // Send command packet
    sd_spi_write(cmd);
    sd_spi_write((arg >> 24) & 0xFF);
    sd_spi_write((arg >> 16) & 0xFF);
    sd_spi_write((arg >> 8) & 0xFF);
    sd_spi_write(arg & 0xFF);
    
    // CRC (only matters for CMD0 and CMD8)
    if (cmd == CMD0) {
        sd_spi_write(0x95);
    } else if (cmd == CMD8) {
        sd_spi_write(0x87);
    } else {
        sd_spi_write(0x01);
    }
    
    // Wait for response
    for (int i = 0; i < 10; i++) {
        response = sd_spi_write(0xFF);
        if ((response & 0x80) == 0) break;
    }
    
    return response;
}

int sd_init(spi_inst_t *spi, uint sck, uint mosi, uint miso, uint cs) {
    sd_spi = spi;
    sd_cs_pin = cs;
    
    // Initialize SPI
    spi_init(spi, 100 * 1000); // Start at 100kHz for better compatibility
    gpio_set_function(sck, GPIO_FUNC_SPI);
    gpio_set_function(mosi, GPIO_FUNC_SPI);
    gpio_set_function(miso, GPIO_FUNC_SPI);
    
    // Initialize CS pin
    gpio_init(cs);
    gpio_set_dir(cs, GPIO_OUT);
    sd_cs_deselect();
    
    sleep_ms(10); // Longer power-up delay
    
    // Send 80 clock pulses with CS high
    for (int i = 0; i < 10; i++) {
        sd_spi_write(0xFF);
    }
    
    // Additional delay after power-up sequence
    sleep_ms(1);
    
    sd_cs_select();
    
    // CMD0: Go to idle state
    printf("Sending CMD0 (reset)...\n");
    uint8_t response = sd_send_command(CMD0, 0);
    printf("CMD0 response: 0x%02X (expected: 0x01)\n", response);
    if (response != 0x01) {
        printf("CMD0 failed - card not responding or bad connection\n");
        sd_cs_deselect();
        return -1;
    }
    
    // CMD8: Check voltage range (SD v2.0 only)
    printf("Sending CMD8 (voltage check)...\n");
    response = sd_send_command(CMD8, 0x1AA);
    printf("CMD8 response: 0x%02X\n", response);
    if (response == 0x01) {
        printf("SD v2.0 card detected\n");
        // SD v2.0
        uint32_t ocr = 0;
        for (int i = 0; i < 4; i++) {
            ocr = (ocr << 8) | sd_spi_write(0xFF);
        }
        printf("CMD8 OCR response: 0x%08X (expected: 0x??????1AA)\n", ocr);
        
        if ((ocr & 0xFFF) != 0x1AA) {
            printf("CMD8 OCR check failed\n");
            sd_cs_deselect();
            return -2;
        }
        
        // Try standard SD v2 initialization sequence
        printf("Starting SD v2.0 initialization sequence...\n");
        
        // First, try without HCS bit for compatibility
        printf("Phase 1: ACMD41 without HCS bit...\n");
        int timeout = 100;
        int attempt = 0;
        do {
            // Send CMD55 (next command is app-specific)
            uint8_t cmd55_resp = sd_send_command(CMD55, 0);
            
            // Wait a bit between CMD55 and ACMD41
            sleep_ms(1);
            
            // Send ACMD41 without HCS bit
            response = sd_send_command(ACMD41, 0x00000000);
            
            attempt++;
            if (attempt % 10 == 0) {
                printf("Attempt %d: CMD55=0x%02X, ACMD41=0x%02X\n", attempt, cmd55_resp, response);
            }
            
            // Check for success or valid responses
            if (response == 0x00) {
                printf("ACMD41 without HCS successful after %d attempts\n", attempt);
                break;
            }
            
            // Check if CMD55 failed
            if (cmd55_resp != 0x01 && cmd55_resp != 0x00) {
                printf("CMD55 failed with 0x%02X, aborting\n", cmd55_resp);
                break;
            }
            
            sleep_ms(10); // Longer delay between attempts
        } while (--timeout > 0);
        
        // If phase 1 failed, try with HCS bit
        if (response != 0x00) {
            printf("Phase 2: ACMD41 with HCS bit...\n");
            timeout = 100;
            attempt = 0;
            do {
                uint8_t cmd55_resp = sd_send_command(CMD55, 0);
                sleep_ms(1);
                response = sd_send_command(ACMD41, 0x40000000);
                
                attempt++;
                if (attempt % 10 == 0) {
                    printf("Attempt %d: CMD55=0x%02X, ACMD41=0x%02X\n", attempt, cmd55_resp, response);
                }
                
                if (response == 0x00) {
                    printf("ACMD41 with HCS successful after %d attempts\n", attempt);
                    break;
                }
                
                if (cmd55_resp != 0x01 && cmd55_resp != 0x00) {
                    printf("CMD55 failed with 0x%02X, aborting\n", cmd55_resp);
                    break;
                }
                
                sleep_ms(10);
            } while (--timeout > 0);
        }
        
        if (timeout == 0) {
            printf("ACMD41 timeout - card not ready\n");
            sd_cs_deselect();
            return -3;
        }
        printf("ACMD41 successful after %d tries\n", 1000 - timeout);
        
        // Check CCS bit in OCR
        response = sd_send_command(CMD58, 0);
        if (response != 0x00) {
            sd_cs_deselect();
            return -1;
        }
        
        uint32_t ocr_resp = 0;
        for (int i = 0; i < 4; i++) {
            ocr_resp = (ocr_resp << 8) | sd_spi_write(0xFF);
        }
        
        if (ocr_resp & 0x40000000) {
            sd_info.type = SD_CARD_TYPE_SDHC;
        } else {
            sd_info.type = SD_CARD_TYPE_SD2;
        }
        
    } else if (response == 0x05) {
        printf("SD v1.0 or MMC card detected\n");
        // SD v1.0 or MMC
        sd_info.type = SD_CARD_TYPE_SD1;
        
        printf("Sending ACMD41 for SD v1.0...\n");
        int timeout = 1000;
        do {
            sd_send_command(CMD55, 0);
            response = sd_send_command(ACMD41, 0);
            if (timeout % 100 == 0) printf("ACMD41 v1 response: 0x%02X, timeout left: %d\n", response, timeout);
            sleep_ms(1);
        } while (response != 0x00 && --timeout > 0);
        
        if (timeout == 0) {
            printf("ACMD41 v1 timeout\n");
            sd_cs_deselect();
            return -4;
        }
        printf("ACMD41 v1 successful\n");
    } else {
        printf("Unknown CMD8 response: 0x%02X\n", response);
        printf("This may be an older card or unsupported type\n");
        sd_cs_deselect();
        return -5;
    }
    
    sd_cs_deselect();
    
    printf("SD card initialization complete!\n");
    
    // Keep slower speed for more reliable reading
    // spi_set_baudrate(spi, 1 * 1000 * 1000); // 1 MHz for stable reading
    
    // Get card size info
    sd_info.block_size = 512;
    sd_info.blocks = 1024 * 1024; // Default, should read from CSD
    
    return 0;
}

int sd_get_info(sd_card_info_t *info) {
    *info = sd_info;
    return 0;
}

int sd_read_block(uint32_t block, uint8_t *buffer) {
    uint8_t response;
    
    printf("Reading block %u...\n", block);
    
    sd_cs_select();
    
    // For SDHC cards, use block address directly
    // For SD cards, convert to byte address
    uint32_t address = (sd_info.type == SD_CARD_TYPE_SDHC) ? block : block * 512;
    printf("Address: %u, Card type: %s\n", address, 
           (sd_info.type == SD_CARD_TYPE_SDHC) ? "SDHC" : "SD");
    
    response = sd_send_command(READ_SINGLE_BLOCK, address);
    printf("CMD17 response: 0x%02X\n", response);
    if (response != 0x00) {
        printf("CMD17 failed with response: 0x%02X\n", response);
        sd_cs_deselect();
        return -1;
    }
    
    // Wait for data token
    int timeout = 1000;
    do {
        response = sd_spi_write(0xFF);
        timeout--;
    } while (response != 0xFE && timeout > 0);
    
    if (timeout == 0) {
        sd_cs_deselect();
        return -1;
    }
    
    // Read data
    for (int i = 0; i < 512; i++) {
        buffer[i] = sd_spi_write(0xFF);
    }
    
    // Read CRC (ignore)
    sd_spi_write(0xFF);
    sd_spi_write(0xFF);
    
    sd_cs_deselect();
    return 0;
}
