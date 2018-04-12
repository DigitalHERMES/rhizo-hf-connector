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
 * Network related functions
 */

/**
 * @file net.h
 * @author Rafael Diniz
 * @date 12 Apr 2018
 * @brief File containing network related functions
 *
 * For the sake of reusable and clean code, some network related functions.
 *
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

bool tcp_connect(char *ip, int port, int *sockt){
    struct sockaddr_in ardop_addr;

    *sockt = socket(PF_INET, SOCK_STREAM, 0);

    ardop_addr.sin_family = AF_INET;
    ardop_addr.sin_port = htons(port);
    ardop_addr.sin_addr.s_addr = inet_addr(ip);
    if (ardop_addr.sin_addr.s_addr == INADDR_NONE){
        fprintf(stderr, "Invalid IP address.\n");
        return false;
    }

    memset(ardop_addr.sin_zero, 0, sizeof(ardop_addr.sin_zero));

    connect(*sockt, (struct sockaddr *) &ardop_addr, sizeof(ardop_addr));

    return true;
}
