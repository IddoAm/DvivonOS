#ifndef DRIVERS_DISK_ATA_H
#define DRIVERS_DISK_ATA_H

#include <stdint.h>

/* ========================================================================
 * ATA/IDE Controller Registers (Primary Channel)
 * ======================================================================== */
#define ATA_DATA_PORT        0x1F0
#define ATA_ERROR_PORT       0x1F1
#define ATA_SECTOR_COUNT     0x1F2
#define ATA_LBA_LOW          0x1F3
#define ATA_LBA_MID          0x1F4
#define ATA_LBA_HIGH         0x1F5
#define ATA_DRIVE_SELECT     0x1F6
#define ATA_COMMAND_PORT     0x1F7
#define ATA_STATUS_PORT      0x1F7

/* Device Control / Alternate Status Register (Primary Channel)
 * Write: Device Control Register   Read: Alternate Status (no IRQ ack) */
#define ATA_CONTROL_PORT     0x3F6

/* ========================================================================
 * ATA Commands
 * ======================================================================== */
#define ATA_CMD_READ_SECTORS  0x20
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_IDENTIFY      0xEC

/* ========================================================================
 * ATA Status Bits (read from ATA_STATUS_PORT or ATA_CONTROL_PORT)
 * ======================================================================== */
#define ATA_STATUS_BUSY       0x80
#define ATA_STATUS_READY      0x40
#define ATA_STATUS_FAULT      0x20
#define ATA_STATUS_SEEK       0x10
#define ATA_STATUS_DRQ        0x08
#define ATA_STATUS_CORRECTED  0x04
#define ATA_STATUS_INDEX      0x02
#define ATA_STATUS_ERROR      0x01

/* ========================================================================
 * Device Control Register Bits (write to ATA_CONTROL_PORT)
 * ======================================================================== */
#define ATA_DCR_NIEN          0x02    /* Disable (negate) interrupts */
#define ATA_DCR_SRST          0x04    /* Software reset */

/* ========================================================================
 * Drive Selection
 * ======================================================================== */
#define ATA_MASTER            0xA0
#define ATA_SLAVE             0xB0

/* ========================================================================
 * IRQ Lines
 * ======================================================================== */
#define ATA_IRQ_PRIMARY       14
#define ATA_IRQ_SECONDARY     15

/* ========================================================================
 * Constants
 * ======================================================================== */
#define ATA_SECTOR_SIZE           512
#define ATA_SECTOR_WORDS          256   /* 512 bytes / 2 bytes per word */
#define ATA_MAX_SECTORS_PER_CMD   255   /* ATA PIO command limit */

/* ========================================================================
 * Error Codes
 * ======================================================================== */
#define ATA_ERROR_NONE        0
#define ATA_ERROR_BUSY       -1
#define ATA_ERROR_TIMEOUT    -2
#define ATA_ERROR_FAULT      -3
#define ATA_ERROR_NOT_READY  -4
#define ATA_ERROR_UNKNOWN    -5

/* ========================================================================
 * Function Declarations
 * ======================================================================== */

/**
 * ata_init - Initialize ATA driver and enable interrupt-driven I/O
 *
 * Registers IRQ handlers for primary and secondary ATA channels,
 * unmasks PIC lines, and enables device interrupts.
 * Returns 0 on success.
 */
int ata_init(void);

/**
 * ata_read_sector - Read a single sector from disk
 * @lba: Logical Block Address (28-bit)
 * @buffer: Buffer to store sector data (512 bytes / 256 words)
 *
 * Reads one 512-byte sector using interrupt-driven PIO.
 * Returns 0 on success, error code on failure.
 */
int ata_read_sector(uint32_t lba, uint16_t *buffer);

/**
 * ata_write_sector - Write a single sector to disk
 * @lba: Logical Block Address (28-bit)
 * @buffer: Buffer containing sector data (512 bytes / 256 words)
 *
 * Writes one 512-byte sector using interrupt-driven PIO.
 * Returns 0 on success, error code on failure.
 */
int ata_write_sector(uint32_t lba, const uint16_t *buffer);

/**
 * ata_read_sectors - Read multiple sectors from disk
 * @lba: Starting Logical Block Address
 * @count: Number of sectors to read
 * @buffer: Buffer to store sector data
 *
 * Reads multiple 512-byte sectors using interrupt-driven PIO.
 * Handles chunking for the ATA 255-sector command limit.
 * Returns 0 on success, error code on failure.
 */
int ata_read_sectors(uint32_t lba, uint32_t count, uint16_t *buffer);

/**
 * ata_write_sectors - Write multiple sectors to disk
 * @lba: Starting Logical Block Address
 * @count: Number of sectors to write
 * @buffer: Buffer containing sector data
 *
 * Writes multiple 512-byte sectors using interrupt-driven PIO.
 * Handles chunking for the ATA 255-sector command limit.
 * Returns 0 on success, error code on failure.
 */
int ata_write_sectors(uint32_t lba, uint32_t count, const uint16_t *buffer);

/**
 * ata_identify_drive - Identify drive parameters
 * @drive: Drive number (0=master, 1=slave)
 * @buffer: Buffer to store IDENTIFY data (512 bytes / 256 words)
 *
 * Sends IDENTIFY command and reads drive information.
 * Returns 0 on success, error code on failure.
 */
int ata_identify_drive(int drive, uint16_t *buffer);

/**
 * ata_wait_ready - Wait for drive to be ready
 * @timeout_ms: Approximate timeout in milliseconds
 *
 * Polls the status register until BSY clears and DRDY sets.
 * Returns 0 if ready, error code on timeout or fault.
 */
int ata_wait_ready(int timeout_ms);

/**
 * ata_select_drive - Select master/slave drive
 * @drive: Drive number (0=master, 1=slave)
 *
 * Selects the specified drive and waits for the bus to settle.
 */
void ata_select_drive(int drive);

/* Forward declaration */
struct block_device;

/**
 * ata_create_block_device - Create an ATA-backed block device
 *
 * Input:  None (uses primary master drive)
 * Output: Pointer to a registered block_device_t backed by ATA PIO,
 *         or NULL on failure.
 *
 * Creates a block_device_t whose read/write ops translate
 * block-level I/O into ATA sector-level I/O.  The device's
 * block_size starts at 512 (sector size) and may be updated
 * by the filesystem layer after mount.
 */
struct block_device *ata_create_block_device(void);

#endif /* DRIVERS_DISK_ATA_H */
