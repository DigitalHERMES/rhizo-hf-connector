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
 * Ardop support routines
 */

#ifndef HAVE_ARDOP_H__
#define HAVE_ARDOP_H__

#include <stdint.h>
#include <pthread.h>

#include "connector.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_ARDOP_PACKET 65535

bool initialize_modem_ardop(rhizo_conn *connector);

#ifdef __cplusplus
};
#endif

#endif /* HAVE_ARDOP_H__ */
