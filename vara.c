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
 * Vara support routines
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "vara.h"

// transfering 1 byte at time
#define TRX_BLK_SIZE 1

void *vara_data_worker_thread_tx(void *conn)
{
   rhizo_conn *connector = (rhizo_conn *) conn;
    size_t len;
    uint8_t buffer[TRX_BLK_SIZE];

    while(true){
        memset(buffer, 0, sizeof(buffer));
        read_buffer(&connector->in_buffer, buffer, TRX_BLK_SIZE);
        len = send(connector->data_socket, buffer, TRX_BLK_SIZE, 0);
        if (len != TRX_BLK_SIZE)
            fprintf(stderr, "data_worker_thread: socket write error.\n");
    }

}

void *vara_data_worker_thread_rx(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    size_t len;
    uint8_t buffer[TRX_BLK_SIZE];

    while(true){
        len = recv(connector->data_socket, buffer, TRX_BLK_SIZE, 0);
        // write to buffer
        if (len > 0)
            write_buffer(&connector->out_buffer, buffer, len);
        else
            fprintf(stderr, "data_worker_thread: socket read error.\n");
    }

}

void *vara_control_worker_thread(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    size_t len;
    char buffer[1024];

    while(true){
        len = recv(connector->control_socket, buffer, TRX_BLK_SIZE, 0);

        if (len > 0){
            if (buffer[0] == '\r'){
                fprintf(stderr, "\n");
            }
            else{
                fprintf(stderr, "%c", buffer[0]);
            }
        }
        else
            fprintf(stderr, "control_worker_thread: read error.\n");
    }

}

bool initialize_modem_vara(rhizo_conn *connector){
    char buffer[1024];
    struct sockaddr_in vara_addr;
    socklen_t addr_size;

    connector->control_socket = socket(PF_INET, SOCK_STREAM, 0);

    vara_addr.sin_family = AF_INET;
    vara_addr.sin_port = htons(connector->tcp_base_port);
    vara_addr.sin_addr.s_addr = inet_addr(connector->ip_address);

    memset(vara_addr.sin_zero, 0, sizeof(vara_addr.sin_zero));

    addr_size = sizeof vara_addr;
    connect(connector->control_socket, (struct sockaddr *) &vara_addr, addr_size);

    // we start our control thread
    pthread_t tid1;
    pthread_create(&tid1, NULL, vara_control_worker_thread, (void *) connector);

    // We set a call sign
    memset(buffer,0,sizeof(buffer));
    sprintf(buffer, "MYCALL %s\r", connector->call_sign);
    send(connector->control_socket, buffer, strlen(buffer), 0);

    if (connector->mode == MODE_RX){
        memset(buffer,0,sizeof(buffer));
        strcpy(buffer,"LISTEN ON\r");
        send(connector->control_socket,buffer,strlen(buffer),0);
    }

    if (connector->mode == MODE_TX){
        memset(buffer,0,sizeof(buffer));
        sprintf(buffer,"CONNECT %s %s\r", connector->call_sign,
               connector->remote_call_sign);
        send(connector->control_socket,buffer,strlen(buffer),0);
    }

    // now lets initialize the data port connection
    connector->data_socket = socket(PF_INET, SOCK_STREAM, 0);

    vara_addr.sin_family = AF_INET;
    vara_addr.sin_port = htons(connector->tcp_base_port+1);
    vara_addr.sin_addr.s_addr = inet_addr(connector->ip_address);

    memset(vara_addr.sin_zero, 0, sizeof(vara_addr.sin_zero));

    addr_size = sizeof vara_addr;
    connect(connector->data_socket, (struct sockaddr *) &vara_addr, addr_size);

    pthread_t tid2;
    pthread_create(&tid2, NULL, vara_data_worker_thread_tx, (void *) connector);

    // just to block, we dont create a new thread here
    vara_data_worker_thread_rx((void *) connector);


    return true;
}
