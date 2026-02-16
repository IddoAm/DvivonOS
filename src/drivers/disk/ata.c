#include <drivers/disk/ata.h>
#include <drivers/block_device.h>
#include <arch/i686/io.h>
#include <arch/i686/idt.h>
#include <arch/i686/pic.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


/* ========================================================================
 * Internal State
 * ======================================================================== */

/**
 * Interrupt synchronization flag.
 * Set to 1 by the ATA ISR, cleared before issuing a command.
 */
static volatile int ata_irq_fired = 0;

/* ========================================================================
 * Static Helpers
 * ======================================================================== */

/**
 * ata_setup_lba - Program the LBA registers and sector count
 * @lba: 28-bit Logical Block Address
 * @count: Number of sectors (1-255)
 *
 * Programs sector count, LBA low/mid/high, and the upper 4 LBA bits
 * in the drive/head register for a 28-bit LBA transfer on the master drive.
 */
static void ata_setup_lba(uint32_t lba, uint8_t count) {
    outb(ATA_SECTOR_COUNT, count);
    outb(ATA_LBA_LOW, (uint8_t)(lba & 0xFF));
    outb(ATA_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HIGH, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_DRIVE_SELECT, 0xE0 | ((lba >> 24) & 0x0F));
}

/**
 * ata_poll_drq - Poll briefly for the DRQ bit
 *
 * Used only where an interrupt is not expected, such as the first
 * DRQ after a WRITE SECTORS command.
 * Returns ATA_ERROR_NONE when DRQ is set, or an error code.
 */
static int ata_poll_drq(void) {
    uint8_t status;
    int i;

    for (i = 0; i < 30000; i++) {
        status = inb(ATA_STATUS_PORT);
        if (status & ATA_STATUS_ERROR)
            return ATA_ERROR_FAULT;
        if (status & ATA_STATUS_DRQ)
            return ATA_ERROR_NONE;
    }

    return ATA_ERROR_TIMEOUT;
}

/**
 * ata_read_pio_data - Read one sector of PIO data from the data port
 * @buffer: Destination buffer (must hold ATA_SECTOR_WORDS words)
 */
static void ata_read_pio_data(uint16_t *buffer) {
    int i;

    for (i = 0; i < ATA_SECTOR_WORDS; i++)
        buffer[i] = inw(ATA_DATA_PORT);
}

/**
 * ata_write_pio_data - Write one sector of PIO data to the data port
 * @buffer: Source buffer (must hold ATA_SECTOR_WORDS words)
 */
static void ata_write_pio_data(const uint16_t *buffer) {
    int i;

    for (i = 0; i < ATA_SECTOR_WORDS; i++)
        outw(ATA_DATA_PORT, buffer[i]);
}

/**
 * ata_prepare_irq - Reset the interrupt flag before issuing a command
 *
 * Must be called before any operation that will trigger an IRQ,
 * and before the command or data transfer that produces the IRQ.
 */
static void ata_prepare_irq(void) {
    ata_irq_fired = 0;
}

/**
 * ata_wait_irq - Block until the ATA interrupt fires
 *
 * Halts the CPU between checks so the processor is not spinning
 * on a tight loop.  Any interrupt (timer, keyboard, etc.) wakes
 * the CPU; the loop rechecks the flag and halts again if it was
 * not the ATA IRQ.
 *
 * TODO: Replace with yield() once a proper scheduler is available.
 */
static void ata_wait_irq(void) {
    while (!ata_irq_fired)
    {
        __asm__ volatile("hlt");
    }
}

/* ========================================================================
 * Interrupt Handlers
 * ======================================================================== */

/**
 * ata_primary_irq_handler - ISR for IRQ 14 (Primary ATA channel)
 *
 * Acknowledges the interrupt by reading the status register
 * (which de-asserts the IRQ line on the drive) and signals the
 * driver through the volatile flag.
 *
 * EOI is sent by the common interrupt dispatcher in idt.c.
 */
static void ata_primary_irq_handler(interrupt_frame_t *frame) {
    (void)frame;

    /* Read status to acknowledge the interrupt on the drive */
    inb(ATA_STATUS_PORT);

    ata_irq_fired = 1;
}

/**
 * ata_secondary_irq_handler - ISR for IRQ 15 (Secondary ATA channel)
 */
static void ata_secondary_irq_handler(interrupt_frame_t *frame) {
    (void)frame;

    /* Secondary channel status port is at 0x177 */
    inb(0x177);

    ata_irq_fired = 1;
}

/* ========================================================================
 * Public API
 * ======================================================================== */

/**
 * ata_select_drive - Select master/slave drive
 * @drive: Drive number (0=master, 1=slave)
 *
 * Writes the appropriate value to the drive select register and
 * performs an I/O wait to let the bus settle.
 */
void ata_select_drive(int drive) {
    uint8_t sel = (drive == 0) ? ATA_MASTER : ATA_SLAVE;

    outb(ATA_DRIVE_SELECT, sel);
    io_wait();
}

/**
 * ata_wait_ready - Wait for drive to become ready
 * @timeout_ms: Approximate timeout in milliseconds
 *
 * Polls the status register until BSY clears and DRDY sets.
 * The iteration count is a rough approximation since no precise
 * timer is used inside this loop.
 *
 * Returns ATA_ERROR_NONE when ready, or an error code.
 */
int ata_wait_ready(int timeout_ms) {
    uint8_t status;
    int iterations = timeout_ms * 1000;
    int i;

    for (i = 0; i < iterations; i++) {
        status = inb(ATA_STATUS_PORT);

        if (status & ATA_STATUS_ERROR)
            return ATA_ERROR_FAULT;
        if (!(status & ATA_STATUS_BUSY) && (status & ATA_STATUS_READY))
            return ATA_ERROR_NONE;
    }

    return ATA_ERROR_TIMEOUT;
}

/**
 * ata_init - Initialize ATA driver and enable interrupt-driven I/O
 *
 * Registers IRQ handlers for the primary and secondary ATA channels,
 * unmasks the corresponding PIC lines, and clears the nIEN bit on
 * the device control register so the drive will fire interrupts.
 *
 * Returns 0 on success.
 */
int ata_init(void) {
    /* Register IRQ handlers with the IDT */
    isr_register_handler(irq_to_vector(ATA_IRQ_PRIMARY),
                         ata_primary_irq_handler);
    isr_register_handler(irq_to_vector(ATA_IRQ_SECONDARY),
                         ata_secondary_irq_handler);

    /* Unmask ATA IRQs on the PIC.*/
    pic_clear_mask(2);
    pic_clear_mask(ATA_IRQ_PRIMARY);
    pic_clear_mask(ATA_IRQ_SECONDARY);

    /* Enable interrupts on the primary ATA device (clear nIEN bit) */
    outb(ATA_CONTROL_PORT, 0x00);

    return ATA_ERROR_NONE;
}

/* ------------------------------------------------------------------------ */

/**
 * ata_read_sector - Read a single sector from disk
 * @lba: Logical Block Address (28-bit)
 * @buffer: uint16_t[256] destination buffer (512 bytes)
 *
 * Thin wrapper around ata_read_sectors for convenience.
 * Returns 0 on success, error code on failure.
 */
int ata_read_sector(uint32_t lba, uint16_t *buffer) {
    return ata_read_sectors(lba, 1, buffer);
}

/**
 * ata_write_sector - Write a single sector to disk
 * @lba: Logical Block Address (28-bit)
 * @buffer: uint16_t[256] source buffer (512 bytes)
 *
 * Thin wrapper around ata_write_sectors for convenience.
 * Returns 0 on success, error code on failure.
 */
int ata_write_sector(uint32_t lba, const uint16_t *buffer) {
    return ata_write_sectors(lba, 1, buffer);
}

/* ------------------------------------------------------------------------ */

/**
 * ata_read_sectors - Read multiple sectors from disk
 * @lba: Starting Logical Block Address
 * @count: Number of sectors to read
 * @buffer: Destination buffer (count * 256 words)
 *
 * Issues READ SECTORS commands in chunks of up to 255 sectors.
 * Each sector transfer is synchronised via IRQ 14.
 *
 * Protocol (per chunk):
 *   1. Prepare IRQ flag
 *   2. Issue READ SECTORS command
 *   3. For each sector: wait IRQ -> read PIO data -> prepare next IRQ
 *
 * Returns 0 on success, error code on failure.
 */
int ata_read_sectors(uint32_t lba, uint32_t count, uint16_t *buffer) {
    uint32_t chunk;
    uint32_t i;

    if (!buffer || count == 0)
        return ATA_ERROR_UNKNOWN;

    while (count > 0) {
        chunk = (count > ATA_MAX_SECTORS_PER_CMD)
              ? ATA_MAX_SECTORS_PER_CMD : count;

        ata_select_drive(0);
        if (ata_wait_ready(1000) != ATA_ERROR_NONE)
        return ATA_ERROR_TIMEOUT;
    
        ata_setup_lba(lba, (uint8_t)chunk);
        
        /* Arm the flag before the command so we catch the first IRQ */
        ata_prepare_irq();

        outb(ATA_COMMAND_PORT, ATA_CMD_READ_SECTORS);
        
        for (i = 0; i < chunk; i++) {
            ata_wait_irq();
            ata_read_pio_data(buffer);
            buffer += ATA_SECTOR_WORDS;
            
            /* Arm for the next sector's IRQ (skip after the last one) */
            if (i + 1 < chunk)
            ata_prepare_irq();
        }

        count -= chunk;
        lba += chunk;
    }

    return ATA_ERROR_NONE;
}

/**
 * ata_write_sectors - Write multiple sectors to disk
 * @lba: Starting Logical Block Address
 * @count: Number of sectors to write
 * @buffer: Source buffer (count * 256 words)
 *
 * Issues WRITE SECTORS commands in chunks of up to 255 sectors.
 *
 * Protocol (per chunk):
 *   1. Issue WRITE SECTORS command
 *   2. Poll for the initial DRQ (no interrupt on first DRQ for writes)
 *   3. For each sector: prepare IRQ -> write PIO data -> wait IRQ
 *
 * The IRQ after each sector signals either "ready for the next sector"
 * or "final write completed" for the last sector.
 *
 * Returns 0 on success, error code on failure.
 */
int ata_write_sectors(uint32_t lba, uint32_t count, const uint16_t *buffer) {
    uint32_t chunk;
    uint32_t i;
    int result;

    if (!buffer || count == 0)
        return ATA_ERROR_UNKNOWN;

    while (count > 0) {
        chunk = (count > ATA_MAX_SECTORS_PER_CMD)
              ? ATA_MAX_SECTORS_PER_CMD : count;

        ata_select_drive(0);

        if (ata_wait_ready(1000) != ATA_ERROR_NONE)
            return ATA_ERROR_TIMEOUT;

        ata_setup_lba(lba, (uint8_t)chunk);
        outb(ATA_COMMAND_PORT, ATA_CMD_WRITE_SECTORS);

        /* The first DRQ after a WRITE command does not generate an IRQ */
        result = ata_poll_drq();
        if (result != ATA_ERROR_NONE)
            return result;

        for (i = 0; i < chunk; i++) {
            ata_prepare_irq();
            ata_write_pio_data(buffer);
            buffer += ATA_SECTOR_WORDS;
            ata_wait_irq();
        }

        count -= chunk;
        lba += chunk;
    }

    return ATA_ERROR_NONE;
}

/* ------------------------------------------------------------------------ */

/**
 * ata_identify_drive - Identify drive parameters
 * @drive: Drive number (0=master, 1=slave)
 * @buffer: Buffer to store IDENTIFY data (512 bytes / 256 words)
 *
 * Sends the IDENTIFY command and reads the resulting 512-byte
 * information block via interrupt-driven PIO.
 *
 * Returns 0 on success, ATA_ERROR_FAULT if the drive does not exist,
 * or another error code on failure.
 */
int ata_identify_drive(int drive, uint16_t *buffer) {
    uint8_t status;

    if (!buffer)
        return ATA_ERROR_UNKNOWN;

    ata_select_drive(drive);

    if (ata_wait_ready(1000) != ATA_ERROR_NONE)
        return ATA_ERROR_TIMEOUT;

    /* Clear sector count and LBA registers (required by ATA spec) */
    outb(ATA_SECTOR_COUNT, 0);
    outb(ATA_LBA_LOW, 0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HIGH, 0);

    ata_prepare_irq();
    outb(ATA_COMMAND_PORT, ATA_CMD_IDENTIFY);

    /* Read alternate status (does not acknowledge IRQ) to check presence */
    status = inb(ATA_CONTROL_PORT);
    if (status == 0)
        return ATA_ERROR_FAULT; /* No drive on this channel */

    /* Wait for the drive to present the IDENTIFY data */
    ata_wait_irq();
    ata_read_pio_data(buffer);

    return ATA_ERROR_NONE;
}

/* ========================================================================
 * Block Device Adapter
 *
 * Wraps ATA PIO sector I/O behind the generic block_device_ops_t
 * interface.  The device's block_size field controls how many 512-byte
 * sectors are read per "block" (e.g. block_size=1024 => 2 sectors/block).
 * ======================================================================== */

/* Static instance – only one primary-master device is supported. */
static block_device_t ata_block_dev;

/**
 * ata_blk_read_block – Read one logical block via ATA PIO.
 * Translates the block number to an LBA using dev->block_size.
 */
static int ata_blk_read_block(block_device_t *dev, uint32_t block, void *buf)
{
    uint32_t sectors_per_block = dev->block_size / ATA_SECTOR_SIZE;
    uint32_t lba = block * sectors_per_block;
    return ata_read_sectors(lba, sectors_per_block, (uint16_t *)buf);
}

/**
 * ata_blk_write_block – Write one logical block via ATA PIO.
 */
static int ata_blk_write_block(block_device_t *dev, uint32_t block, const void *buf)
{
    uint32_t sectors_per_block = dev->block_size / ATA_SECTOR_SIZE;
    uint32_t lba = block * sectors_per_block;
    return ata_write_sectors(lba, sectors_per_block, (const uint16_t *)buf);
}

/**
 * ata_blk_read_blocks – Read multiple consecutive logical blocks.
 */
static int ata_blk_read_blocks(block_device_t *dev, uint32_t start,
                               uint32_t count, void *buf)
{
    uint32_t sectors_per_block = dev->block_size / ATA_SECTOR_SIZE;
    uint32_t lba = start * sectors_per_block;
    uint32_t total_sectors = count * sectors_per_block;
    return ata_read_sectors(lba, total_sectors, (uint16_t *)buf);
}

/**
 * ata_blk_write_blocks – Write multiple consecutive logical blocks.
 */
static int ata_blk_write_blocks(block_device_t *dev, uint32_t start,
                                uint32_t count, const void *buf)
{
    uint32_t sectors_per_block = dev->block_size / ATA_SECTOR_SIZE;
    uint32_t lba = start * sectors_per_block;
    uint32_t total_sectors = count * sectors_per_block;
    return ata_write_sectors(lba, total_sectors, (const uint16_t *)buf);
}

static const block_device_ops_t ata_blk_ops = {
    .read_block   = ata_blk_read_block,
    .write_block  = ata_blk_write_block,
    .read_blocks  = ata_blk_read_blocks,
    .write_blocks = ata_blk_write_blocks,
    .flush        = NULL,
    .ioctl        = NULL,
    .release      = NULL,
};

/**
 * ata_drive_exists – Quick probe: write known values to the sector-count
 * and LBA-low registers, then read them back.  If the bus is floating
 * (no drive), the read returns 0xFF instead of the written value.
 * Also rejects status == 0x00 (disconnected) and 0xFF (floating bus).
 */
static int ata_drive_exists(void)
{
    ata_select_drive(0);

    uint8_t status = inb(ATA_STATUS_PORT);
    if (status == 0x00 || status == 0xFF)
        return 0;

    /* Write / read-back test on scratch registers */
    outb(ATA_SECTOR_COUNT, 0x55);
    outb(ATA_LBA_LOW,      0xAA);
    if (inb(ATA_SECTOR_COUNT) != 0x55 || inb(ATA_LBA_LOW) != 0xAA)
        return 0;

    return 1;   /* drive present */
}

/**
 * ata_create_block_device – Probe for the primary-master ATA drive
 * and, if one exists, build and register a generic block device.
 */
block_device_t *ata_create_block_device(void)
{
    if (!ata_drive_exists())
        return NULL;

    ata_block_dev.major      = 3;
    ata_block_dev.minor      = 0;
    ata_block_dev.block_size = ATA_SECTOR_SIZE;   /* updated by FS mount */
    ata_block_dev.capacity   = 0;
    ata_block_dev.base_port  = ATA_DATA_PORT;
    ata_block_dev.drive_type = 0;                 /* master */
    ata_block_dev.is_exists  = true;
    ata_block_dev.ops        = &ata_blk_ops;
    ata_block_dev.name       = "hda";
    ata_block_dev.driver_data = NULL;

    if (block_device_register(&ata_block_dev) != 0)
        return NULL;

    return &ata_block_dev;
}
