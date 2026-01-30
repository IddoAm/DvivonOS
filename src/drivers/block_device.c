#include "drivers/block_device.h"
#include <stdlib.h>

/* Simple block device registry */
#define MAX_BLOCK_DEVICES 16
static block_device_t *block_devices[MAX_BLOCK_DEVICES] = {NULL};

/**
 * block_device_register - Register a block device
 * @dev: Block device to register
 *
 * Registers a block device in the system.
 * Returns 0 on success, or error code.
 */
int block_device_register(block_device_t *dev)
{
    if (!dev || !dev->ops)
        return -1;

    /* Find empty slot */
    for (int i = 0; i < MAX_BLOCK_DEVICES; i++) {
        if (block_devices[i] == NULL) {
            block_devices[i] = dev;
            return 0;
        }
    }

    return -1;  /* No space */
}

/**
 * block_device_unregister - Unregister a block device
 * @dev: Block device to unregister
 *
 * Unregisters a block device from the system.
 * Returns 0 on success, or error code.
 */
int block_device_unregister(block_device_t *dev)
{
    if (!dev)
        return -1;

    for (int i = 0; i < MAX_BLOCK_DEVICES; i++) {
        if (block_devices[i] == dev) {
            block_devices[i] = NULL;
            return 0;
        }
    }

    return -1;  /* Not found */
}

/**
 * block_device_get - Get block device by major/minor number
 * @major: Major device number
 * @minor: Minor device number
 *
 * Looks up and returns a registered block device.
 * Returns pointer to device, or NULL if not found.
 */
block_device_t *block_device_get(uint32_t major, uint32_t minor)
{
    for (int i = 0; i < MAX_BLOCK_DEVICES; i++) {
        if (block_devices[i] != NULL &&
            block_devices[i]->major == major &&
            block_devices[i]->minor == minor) {
            return block_devices[i];
        }
    }

    return NULL;
}

/**
 * block_device_put - Release reference to block device
 * @dev: Block device to release
 *
 * Releases a reference to a block device (placeholder for future refcounting).
 */
void block_device_put(block_device_t *dev)
{
    /* Placeholder for future refcounting implementation */
    //TODO: Implement reference counting if we want in the future
}
