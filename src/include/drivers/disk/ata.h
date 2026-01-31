#ifndef DRIVERS_DISK_ATA_H
#define DRIVERS_DISK_ATA_H

#include <stdint.h>

/* ATA/IDE Controller Registers */
#define ATA_DATA_PORT        0x1F0
#define ATA_ERROR_PORT       0x1F1
#define ATA_SECTOR_COUNT     0x1F2
#define ATA_LBA_LOW          0x1F3
#define ATA_LBA_MID          0x1F4
#define ATA_LBA_HIGH         0x1F5
#define ATA_DRIVE_SELECT     0x1F6
#define ATA_COMMAND_PORT     0x1F7
#define ATA_STATUS_PORT      0x1F7

/* ATA Commands */
#define ATA_CMD_READ_SECTORS  0x20
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_IDENTIFY      0xEC

/* ATA Status Bits */
#define ATA_STATUS_BUSY       0x80
#define ATA_STATUS_READY      0x40
#define ATA_STATUS_FAULT      0x20
#define ATA_STATUS_SEEK       0x10
#define ATA_STATUS_DRQ        0x08
#define ATA_STATUS_CORRECTED  0x04
#define ATA_STATUS_INDEX      0x02
#define ATA_STATUS_ERROR      0x01

/* Drive Selection */
#define ATA_MASTER            0xA0
#define ATA_SLAVE             0xB0

/* Error Codes */
#define ATA_ERROR_NONE        0
#define ATA_ERROR_BUSY       -1
#define ATA_ERROR_TIMEOUT    -2
#define ATA_ERROR_FAULT      -3
#define ATA_ERROR_NOT_READY  -4

/* Function Declarations */

/**
 * ata_init - Initialize ATA driver
 *
 * TODO: Initialize ATA controller, detect drives, register block devices
 */
int ata_init(void);

/**
 * ata_read_sector - Read a single sector from disk
 * @lba: Logical Block Address (28-bit)
 * @buffer: Buffer to store sector data (512 bytes)
 *
 * TODO: Implement sector reading with proper error handling
 */
int ata_read_sector(uint32_t lba, void *buffer);

/**
 * ata_write_sector - Write a single sector to disk
 * @lba: Logical Block Address (28-bit)
 * @buffer: Buffer containing sector data (512 bytes)
 *
 * TODO: Implement sector writing with proper error handling
 */
int ata_write_sector(uint32_t lba, const void *buffer);

/**
 * ata_read_sectors - Read multiple sectors from disk
 * @lba: Starting Logical Block Address
 * @count: Number of sectors to read
 * @buffer: Buffer to store sector data
 *
 * TODO: Implement multi-sector reading for efficiency
 */
int ata_read_sectors(uint32_t lba, uint32_t count, void *buffer);

/**
 * ata_write_sectors - Write multiple sectors to disk
 * @lba: Starting Logical Block Address
 * @count: Number of sectors to write
 * @buffer: Buffer containing sector data
 *
 * TODO: Implement multi-sector writing for efficiency
 */
int ata_write_sectors(uint32_t lba, uint32_t count, const void *buffer);

/**
 * ata_identify_drive - Identify drive parameters
 * @drive: Drive number (0=master, 1=slave)
 * @buffer: Buffer to store IDENTIFY data (512 bytes)
 *
 * TODO: Send IDENTIFY command and parse drive information
 */
int ata_identify_drive(int drive, void *buffer);

/**
 * ata_wait_ready - Wait for drive to be ready
 * @timeout_ms: Timeout in milliseconds
 *
 * TODO: Poll status register until drive is ready or timeout
 */
int ata_wait_ready(int timeout_ms);

/**
 * ata_select_drive - Select master/slave drive
 * @drive: Drive number (0=master, 1=slave)
 *
 * TODO: Send drive select command
 */
void ata_select_drive(int drive);

#endif /* DRIVERS_DISK_ATA_H */