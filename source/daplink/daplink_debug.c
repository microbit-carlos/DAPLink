/**
 * @file    daplink_debug.c
 * @brief   optional trace messages useful in development
 *
 * DAPLink Interface Firmware
 * Copyright (c) 2009-2021, Arm Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "cmsis_os2.h"
#include "rl_usb.h"
#include "util.h"

#if defined (DAPLINK_DEBUG)

#if DAPLINK_DEBUG_RTT

#include "debug/SEGGER_RTT.c"

uint32_t daplink_debug(uint8_t *buf, uint32_t size)
{
    return SEGGER_RTT_Write(0, buf, size);
}

#else

static const char error_msg[] = "\r\n<OVERFLOW>\r\n";

uint32_t daplink_debug(uint8_t *buf, uint32_t size)
{
    uint32_t total_free;
    uint32_t write_free;
    uint32_t error_len = strlen(error_msg);
    total_free = USBD_CDC_ACM_DataFree();

    if (total_free < error_len) {
        // No space
        return 0;
    }

    // Size available for writing
    write_free = total_free - error_len;
    size = MIN(write_free, size);
    USBD_CDC_ACM_DataSend(buf, size);

    if (write_free == size) {
        USBD_CDC_ACM_DataSend((uint8_t *)error_msg, error_len);
    }

    return size;
}
#endif

static char daplink_debug_buf[512] = {0};
uint32_t daplink_debug_print(const char *format, ...)
{
    uint32_t ret;
    int32_t r = 0;
    va_list arg;
    ret = 1;
    va_start(arg, format);
    r = vsnprintf(daplink_debug_buf, sizeof(daplink_debug_buf), format, arg);

    if (r >= sizeof(daplink_debug_buf)) {
        r = snprintf(daplink_debug_buf, sizeof(daplink_debug_buf), "<Error - string length %i exceeds print buffer>\r\n", r);
        ret = 0;
    }

    va_end(arg);
    daplink_debug((uint8_t *)daplink_debug_buf, r);
    return ret;
}

// Created to capture MSD sectors and print them as either:
// - ASCII when detected as being Intel/Universal Hex data
// - A block of hex data + ASCII representation on the side for non hex data
// - When the block is all zeros, a single line indicating this
//   (reduces amount of data sent via serial/RTT)
void debug_raw_data(uint8_t *buf, uint32_t size) {
    static const int BYTES_PER_LINE = 32;
    const int lines = size / BYTES_PER_LINE;
    static char decoded_buffer[33] = {0};
    decoded_buffer[BYTES_PER_LINE] = '\0';

    // 1st do a single pass to see if there is any data to print
    // TODO: This assumes size is divisible by 4
    bool all_zeros = true;
    for (int i = 0; i < size; i += 4) {
        if (*((uint32_t *)&buf[i])) {
            all_zeros = false;
            break;
        }
    }
    if (all_zeros) {
       daplink_debug_print("Block is all 0x00\n", size);
       return;
    }
    // If the first 50 bytes (a bit over a standard 16 byte record)
    // are ASCII and have a ':' then they are likely Intel/Universal hex
    bool ihex_ascii = false;
    for (int i = 0; i < 50; i++) {
        if (!( (buf[i] >= 32 && buf[i] < 127) || buf[i] == '\n')) {
            ihex_ascii = false;
            break;
        } else {
            if (buf[i] == ':') {
                ihex_ascii = true;
            }
        }
    }
    if (ihex_ascii) {
        daplink_debug("Hex:\n", 5);
        // the first 124 chars should be fine, 2 full data records + others
        daplink_debug(buf, 124);
        daplink_debug("---\n", 4);
        return;
    }

    // For data that is not hex, we'll print it in hex & ASCII
    for (int i = 0; i < lines; i++) {
        bool decode = false;
        //daplink_debug_print("L%02d:", i);
        for (int j = 0; j < BYTES_PER_LINE; j++) {
            if (j % 8 == 0) daplink_debug(" ", 1);
            uint8_t byte = buf[(i * BYTES_PER_LINE) + j];
            daplink_debug_print("%02x",byte);
            if (byte >= 32 && byte < 127) {
                decode = true;
                decoded_buffer[j] = byte;
            } else {
                decoded_buffer[j] = '.';
            }
        }
        if (decode) {
            daplink_debug_print("| |%s|\n", decoded_buffer);
        } else {
            daplink_debug("\n", 1);
        }
    }
}

#endif
