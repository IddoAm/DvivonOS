/*
 * vfs/filesystem.c – Filesystem type registration and lookup.
 *
 * Maintains a singly-linked list of filesystem_type_t structures.
 * filesystem_init() registers all built-in types (currently ext2).
 */

#include <fs/vfs/filesystem.h>
#include <fs/ext2/ext2.h>
#include <lib/stdio.h>
#include <lib/string.h>

/* Head of the registered-filesystem linked list. */
static filesystem_type_t *filesystem_list = NULL;

/*
 * Append a filesystem type to the list.
 * Returns -1 if a type with the same name already exists.
 */
int register_filesystem(filesystem_type_t *fs)
{
    if (!fs || !fs->name) return -1;

    /* Duplicate check */
    for (filesystem_type_t *p = filesystem_list; p; p = p->next) {
        if (strcmp(p->name, fs->name) == 0)
            return -1;
    }

    fs->next = filesystem_list;
    filesystem_list = fs;
    return 0;
}

/*
 * Remove a filesystem type from the list.
 */
int unregister_filesystem(filesystem_type_t *fs)
{
    if (!fs) return -1;

    filesystem_type_t **pp = &filesystem_list;
    while (*pp) {
        if (*pp == fs) {
            *pp = fs->next;
            fs->next = NULL;
            return 0;
        }
        pp = &(*pp)->next;
    }
    return -1;  /* not found */
}

/*
 * Look up a registered filesystem type by its name string.
 */
filesystem_type_t *get_filesystem(const char *name)
{
    if (!name) return NULL;

    for (filesystem_type_t *p = filesystem_list; p; p = p->next) {
        if (strcmp(p->name, name) == 0)
            return p;
    }
    return NULL;
}

/*
 * Initialize the filesystem subsystem: register all built-in types.
 * Called once during boot from start_the_fs().
 */
int filesystem_init(void)
{
    filesystem_list = NULL;
    printf("[vfs] initializing filesystem subsystem\n");

    /* Register the ext2 driver */
    ext2_init();

    return 0;
}
