/* Rhizo-connector: A connector to different HF modems
 * Copyright (C) 2018 Rhizomatica
 * Author: Rafael Diniz <rafael@riseup.net>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 *
 * D-Star support routines
 */

/**
 * @file dstar.h
 * @author Rafael Diniz
 * @date 12 Apr 2018
 * @brief D-Star specific support functions
 *
 * D-Star specific support functions
 *
 */

#ifndef HAVE_DSTAR_H__
#define HAVE_DSTAR_H__

#include <stdint.h>
#include <pthread.h>
#include "connector.h"

#ifdef __cplusplus
extern "C" {
#endif

bool initialize_modem_dstar(rhizo_conn *connector);
bool write_dstar_bitstream(uint8_t *buffer, size_t read_count);

#ifdef __cplusplus
};
#endif

#endif /* HAVE_DSTAR_H__ */
