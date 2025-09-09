/* Auto-generated header file for VFS tests */
/* Generated for: KL27Z_IF */
#ifndef VFS_TESTS_TYPES_H
#define VFS_TESTS_TYPES_H

#include <stdbool.h>

typedef enum {
    TARGET_FLASH, INTF_FLASH_BL, INTF_FLASH_IF,
} flash_location_t;

typedef struct {
    char *input_file;
    char *ref_file;
    bool out_of_order;
    flash_location_t location;
} vfs_tests_t;

#endif /* VFS_TESTS_TYPES_H */
