/**
 * @file   compatibility.h
 * @brief  Compatibility functions for micro:bit boards.
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
#ifndef COMPATIBILITY_H_
#define COMPATIBILITY_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void compat_uf2_set_family_ids(uint16_t default_id, uint16_t id);
void compat_uf2_clear_locked_id(void);
uint8_t compat_uf2_block_compatible(const uint8_t *buf, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* COMPATIBILITY_H_ */
