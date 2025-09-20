/**
 * @file    compatibility.c
 * @brief   Compatibility functions for micro:bit boards.
 *
 * DAPLink Interface Firmware
 * Copyright 2025 Micro:bit Educational Foundation
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

#include "compatibility.h"
#include "daplink.h"
#include "target_board.h"
#include "uf2.h"

#define MB_COMPAT_DBG 1
#if MB_COMPAT_DBG
#include "daplink_debug.h"
#define mb_compat_printf    debug_msg
#else
#define mb_compat_printf(...)
#endif

static uint32_t family_id_locked = 0;
#if defined(DAPLINK_BL)
static uint32_t uf2_family_id_default_id = UF2_DAPLINK_IF_FAMILY_RANGE;
static uint32_t uf2_family_id_board_id = UF2_DAPLINK_IF_FAMILY_RANGE;
#else
static uint32_t uf2_family_id_default_id = UF2_DAPLINK_TARGET_FAMILY_RANGE;
static uint32_t uf2_family_id_board_id = UF2_DAPLINK_TARGET_FAMILY_RANGE;
#endif

void compat_uf2_clear_locked_id(void) {
    mb_compat_printf("compat_uf2_clear_locked_id\n");
    family_id_locked = 0;
}

void compat_uf2_set_family_ids(uint16_t default_id, uint16_t board_id) {
    if (daplink_is_bootloader()) {
        uf2_family_id_default_id = UF2_DAPLINK_IF_FAMILY_RANGE | default_id;
        uf2_family_id_board_id = UF2_DAPLINK_IF_FAMILY_RANGE | board_id;
    } else {
        uf2_family_id_default_id = UF2_DAPLINK_TARGET_FAMILY_RANGE | default_id;
        uf2_family_id_board_id = UF2_DAPLINK_TARGET_FAMILY_RANGE | board_id;
    }
}

uint8_t compat_uf2_block_compatible(const uint8_t *buf, uint32_t size) {
    const UF2_Block *block = (const UF2_Block *)buf;
    mb_compat_printf("compat_uf2_block_compatible: family_id_locked=0x%08x, uf2_family_id_default_id=0x%08x, uf2_family_id_board_id=0x%08x\n",
        family_id_locked, uf2_family_id_default_id, uf2_family_id_board_id);

    if (!(block->flags & UF2_FLAG_FAMILY_ID)) {
        return 1;
    }
    if (family_id_locked) {
        return UF2_BLOCK_FAMILY_ID(block) == family_id_locked;
    }

    uint8_t compatible = false;
    if (daplink_is_bootloader()) {
        mb_compat_printf("compat_uf2_block_compatible: BOOTLOADER mode 0x%08x\n", uf2_family_id_board_id);
        // Only accept DAPLink interface Family ID, as done in file_stream.c
        compatible = UF2_BLOCK_FAMILY_ID(block) == uf2_family_id_board_id;
    } else {
        mb_compat_printf("compat_uf2_block_compatible: TARGET mode 0x%08x\n", uf2_family_id_board_id);
        // Family IDs to check:
        // 1. Generic micro:bit V2 Board ID (compatible with all V2 versions)
        // 2. UF2 official family ID for nRF52833
        // 3. Family ID for DAPLink Board ID for this specific micro:bit V2 version
        compatible = UF2_BLOCK_FAMILY_ID(block) == uf2_family_id_default_id
            || UF2_BLOCK_FAMILY_ID(block) == uf2_family_id_board_id
            || (g_board_info.target_cfg->uf2_family_id &&
                (UF2_BLOCK_FAMILY_ID(block) == g_board_info.target_cfg->uf2_family_id));
    }
    if (compatible) {
        family_id_locked = UF2_BLOCK_FAMILY_ID(block);
    }
    return compatible;
}
