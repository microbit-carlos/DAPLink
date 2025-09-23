#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "error.h"
#include "file_stream.h"
#include "daplink_addr.h"
#include "daplink_debug.h"
#include "util.h"
#include "vfs_test_types.h"
#include "vfs_tests.h"


#define BLOCK_SIZE 512
#define SEPARATOR "================================================================================"

__WEAK void reset_test_state(void) {
    // Default implementation does nothing
    // Board specific implementation can be set in the custom target file
}

extern uint32_t target_flash_addr;
extern uint32_t target_flash_size;
extern uint8_t target_flash_byte[];

extern uint32_t intf_flash_addr;
extern uint32_t intf_flash_size;
extern uint8_t intf_flash_byte[];

void shuffle_indices(size_t *array, size_t n) {
    static bool seeded = false;
    if (!seeded) {
        srand(42); // Fixed seed for reproducible results
        seeded = true;
    }

    for (size_t i = n - 1; i > 0; i--) {
        size_t j = rand() % (i + 1);
        // Swap array[i] and array[j]
        size_t temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

int main(int argc, char **argv)
{
    for (uint32_t t = 0; t < sizeof(tests) / sizeof(vfs_tests_t); t++) {
        reset_test_state();

        fprintf(stderr, SEPARATOR "\nTest %d: %s\r\n" SEPARATOR "\r\n", t, tests[t].input_file);
        FILE *f = fopen(tests[t].input_file, "rb");
        if (f == NULL) {
            fprintf(stderr, "File not '%s' found\r\n", tests[t].input_file);
            exit(-1);
        }

        // First read the file into an array of 512 byte blocks, which will later be randomised
        fseek(f, 0, SEEK_END);
        size_t file_size = ftell(f);
        // only the out of order files have to be divisible by 512
        if (tests[t].out_of_order && file_size % BLOCK_SIZE != 0) {
            fprintf(stderr, "File '%s' size is not a multiple of %d\r\n", tests[t].input_file, BLOCK_SIZE);
            //exit(-1);
        }
        size_t num_blocks = (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
        uint8_t *file_buffer = malloc(file_size);
        if (!file_buffer) {
            fprintf(stderr, "Failed to allocate memory\r\n");
            exit(-1);
        }
        rewind(f);
        size_t read_bytes = fread(file_buffer, 1, file_size, f);
        if (read_bytes != file_size) {
            fprintf(stderr, "Failed to read entire file\r\n");
            exit(-1);
        }
        fclose(f);

        // Create an array of block indices for randomization (if out_of_order file)
        size_t block_indices[num_blocks];
        for (size_t i = 0; i < num_blocks; i++) {
            block_indices[i] = i;
        }
        if (tests[t].out_of_order) {
            shuffle_indices(block_indices, num_blocks);
        }

        error_t status;
        bool first_block = true;
        fprintf(stdout, "Start sending %zu blocks for file '%s'\r\n", num_blocks, tests[t].input_file);
        for (size_t i = 0; i < num_blocks; i++) {
            size_t this_block_size = BLOCK_SIZE;
            if (i == (num_blocks - 1) && file_size % BLOCK_SIZE != 0) {
                this_block_size = file_size % BLOCK_SIZE;
            }
            fprintf(stdout, "Sending block %zu (file block %zu), size=%zu\r\n", i, block_indices[i], this_block_size);
            uint8_t *block = &file_buffer[block_indices[i] * BLOCK_SIZE];
            if (first_block) {
                stream_type_t stream = stream_start_identify(block, this_block_size);
                fprintf(stderr, "Stream type %d\r\n", stream);
                status = stream_open(stream);
                if (status != ERROR_SUCCESS) {
                    fprintf(stderr, "Error opening stream (%i): %s\r\n", status, error_get_string(status));
                    exit(-1);
                }
                first_block = false;
            }
            status = stream_write(block, this_block_size);
            if(status != ERROR_SUCCESS_DONE && status != ERROR_SUCCESS_DONE_OR_CONTINUE && status != ERROR_SUCCESS) {
                fprintf(stderr, "Error (%i): %s\r\n", status, error_get_string(status));
                exit(-1);
                break;
            } else if (ERROR_SUCCESS_DONE == status) {
                break;
            }
        }

        status = stream_close();
        free(file_buffer);

        if (status == ERROR_SUCCESS) {
            FILE *f = fopen(tests[t].ref_file, "rb");
            if (f == NULL) {
                fprintf(stderr, "File not '%s' found\r\n", tests[t].ref_file);
                exit(-1);
            }

            uint32_t i = 0, l = 0, to_read = BLOCK_SIZE;
            uint8_t buffer[BLOCK_SIZE];
            int pos = 0, check = 0;
            while ((l = fread(buffer, 1, to_read, f)) > 0) {
                fprintf(stderr, "Check block %i, size=%i\r\n", i, l);
                uint8_t *ptr = 0;
                switch (tests[t].location) {
#if defined(DAPLINK_IF)
                case TARGET_FLASH:
                    ptr = &(target_flash_byte[pos]);
                    break;
#endif
                case INTF_FLASH_BL:
                    util_assert((pos + l) <= DAPLINK_ROM_BL_SIZE);
                    ptr = &(intf_flash_byte[(DAPLINK_ROM_BL_START - DAPLINK_ROM_START) + pos]);
                    break;
                case INTF_FLASH_IF:
                    util_assert((pos + l) <= DAPLINK_ROM_IF_SIZE);
                    ptr = &(intf_flash_byte[(DAPLINK_ROM_IF_SIZE - DAPLINK_ROM_START) + pos]);
                    break;
                default:
                    fprintf(stderr, "Unexpected");
                    exit(-1);
                }
                check = memcmp(ptr, buffer, l);
                if (check != 0) {
                    fprintf(stderr, "Mismatch at offset 0x%08x\r\n", pos + (check < 0 ? -check : check));
                }
                pos += l;
                i++;
            }
            fclose(f);
            fprintf(stderr, "Success '%s' (checked %d bytes)\r\n", tests[t].input_file, pos);
        } else {
            fprintf(stderr, "Error '%s' closing stream: %s\r\n", tests[t].input_file, error_get_string(status));
            exit(-1);
        }
    }
    fprintf(stdout, "Success\r\n");

    return 0;
}
