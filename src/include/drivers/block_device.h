#ifndef DRIVERS_BLOCK_DEVICE_H
#define DRIVERS_BLOCK_DEVICE_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declarations */
typedef struct block_device block_device_t;
typedef struct block_device_ops block_device_ops_t;

/* Block device structure */
struct block_device {
    uint32_t major;                 /* Major device number */
    uint32_t minor;                 /* Minor device number */
    uint32_t block_size;            /* Block size in bytes */
    uint32_t capacity;              /* Total blocks on device */
    uint16_t base_port;             /* Base I/O port for device (if applicable) */
    uint8_t  drive_type;            /* Drive type (e.g., 0 for Master, 1 for Slave) */
    bool     is_exists;             /* Does this device exist? */
    const block_device_ops_t *ops;  /* Device operations */
    const char *name;               /* Device name (e.g., "hda", "sda") */
    void *driver_data;              /* Driver-specific data */
};

/* Block device operations */
typedef struct block_device_ops {
    /* Read/write operations */
    int (*read_block)(block_device_t *dev, uint32_t block, void *buf);
    int (*write_block)(block_device_t *dev, uint32_t block, const void *buf);
    int (*read_blocks)(block_device_t *dev, uint32_t block, uint32_t count, void *buf);
    int (*write_blocks)(block_device_t *dev, uint32_t block, uint32_t count, const void *buf);
    
    /* Device control */
    int (*flush)(block_device_t *dev);
    int (*ioctl)(block_device_t *dev, uint32_t cmd, void *arg);
    
    /* Cleanup */
    void (*release)(block_device_t *dev);
} block_device_ops_t;

/* Block device registration and management */
int block_device_register(block_device_t *dev);
int block_device_unregister(block_device_t *dev);
block_device_t *block_device_get(uint32_t major, uint32_t minor);
void block_device_put(block_device_t *dev);

#endif /* DRIVERS_BLOCK_DEVICE_H */
