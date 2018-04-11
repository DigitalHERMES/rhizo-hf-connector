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

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include "ardop.h"

// transfering 1 byte at time  - please don't change, that are some hardcoded stuff
#define TRX_BLK_SIZE 1

// TODO: implement a mechanism which verifies when messages are send (using BUFFER cmd)?

void *ardop_data_worker_thread_tx(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    size_t len;
    uint8_t buffer[MAX_ARDOP_PACKET];
    uint32_t buf_size; // our header is 4 bytes long
    uint8_t ardop_size[2];

    uint32_t counter;
    while(true){
        memset(buffer, 0, sizeof(buffer));

// TODO: if the two parties in a connection have different endianess, we are in trouble
        // read header 
        read_buffer(&connector->in_buffer, (uint8_t *) &buf_size, sizeof(buf_size));

        counter = buf_size;
        while (counter > MAX_ARDOP_PACKET){
            read_buffer(&connector->in_buffer, buffer, MAX_ARDOP_PACKET);

            ardop_size[0] = 255;
            ardop_size[1] = 255;

            len = send(connector->data_socket, ardop_size, sizeof(ardop_size), 0);
            if (len != sizeof(ardop_size))
                fprintf(stderr, "data_worker_thread_tx: socket write error.\n");

            len = send(connector->data_socket, buffer, MAX_ARDOP_PACKET, 0);
            if (len != MAX_ARDOP_PACKET)
                fprintf(stderr, "data_worker_thread_tx: socket write error.\n");

            counter -= MAX_ARDOP_PACKET;

            fprintf(stderr, "Transmitting Data!");
        }

        read_buffer(&connector->in_buffer, buffer, counter);

        ardop_size[0] = (uint8_t) (counter >> 8);
        ardop_size[1] = (uint8_t) (counter & 255);

        len = send(connector->data_socket, ardop_size, sizeof(ardop_size), 0);
        if (len != sizeof(ardop_size))
            fprintf(stderr, "data_worker_thread_tx: socket write error.\n");

        len = send(connector->data_socket, buffer, counter, 0);
        if (len != counter)
                fprintf(stderr, "data_worker_thread_tx: socket write error.\n");

    }

}

void *ardop_data_worker_thread_rx(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    size_t len;
    uint8_t buffer[MAX_ARDOP_PACKET];
    uint32_t buf_size; // our header is 4 bytes long
    uint8_t ardop_size[2];

    while(true){
        len = recv(connector->data_socket, &ardop_size, sizeof(ardop_size), 0);
        if (len != sizeof(ardop_size))
            fprintf(stderr, "data_worker_thread_rx: socket read error.\n");

// ARDOP frame TNC data format
        buf_size = ardop_size[0];
        buf_size <<= 8;
        buf_size |= ardop_size[1];

        len = recv(connector->data_socket, buffer, buf_size, 0);
        if (len != buf_size)
            fprintf(stderr, "data_worker_thread_rx: socket read error.\n");

//        fprintf(stderr, "received from ardop: %s\n", (char *) buffer);
        // write header
        write_buffer(&connector->out_buffer, (uint8_t *) &buf_size, sizeof(buf_size));
        // write to buffer
        write_buffer(&connector->out_buffer, buffer, len);
    }

    return EXIT_SUCCESS;
}

void *ardop_control_worker_thread(void *conn)
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


    return EXIT_SUCCESS;
}


bool initialize_modem_ardop(rhizo_conn *connector){
    char buffer[1024];
    struct sockaddr_in ardop_addr;
    socklen_t addr_size;

    connector->control_socket = socket(PF_INET, SOCK_STREAM, 0);

    ardop_addr.sin_family = AF_INET;
    ardop_addr.sin_port = htons(connector->tcp_base_port);
    ardop_addr.sin_addr.s_addr = inet_addr(connector->ip_address);

    memset(ardop_addr.sin_zero, 0, sizeof(ardop_addr.sin_zero));

    addr_size = sizeof ardop_addr;
    connect(connector->control_socket, (struct sockaddr *) &ardop_addr, addr_size);

    // we start our control thread
    pthread_t tid1;
    pthread_create(&tid1, NULL, ardop_control_worker_thread, (void *) connector);

    // a initialize first
    memset(buffer,0,sizeof(buffer));
    sprintf(buffer, "INITIALIZE\r");
    send(connector->control_socket, buffer, strlen(buffer), 0);

    // We set a call sign
    memset(buffer,0,sizeof(buffer));
    sprintf(buffer, "MYCALL %s\r", connector->call_sign);
    send(connector->control_socket, buffer, strlen(buffer), 0);

    // for initial development, set arq timeout very high (with ardopc it doesnt work :/ )
//    memset(buffer,0,sizeof(buffer));
//    sprintf(buffer, "ARQTIMEOUT 100\r");
//    send(connector->control_socket, buffer, strlen(buffer), 0);

    if (connector->mode == MODE_RX){
        memset(buffer,0,sizeof(buffer));
        strcpy(buffer,"LISTEN True\r");
        send(connector->control_socket,buffer,strlen(buffer),0);
    }

    if (connector->mode == MODE_TX){
        memset(buffer,0,sizeof(buffer));
        sprintf(buffer,"ARQCALL %s 5\r", connector->remote_call_sign);
        send(connector->control_socket,buffer,strlen(buffer),0);
    }

    // now lets initialize the data port connection
    connector->data_socket = socket(PF_INET, SOCK_STREAM, 0);

    ardop_addr.sin_family = AF_INET;
    ardop_addr.sin_port = htons(connector->tcp_base_port+1);
    ardop_addr.sin_addr.s_addr = inet_addr(connector->ip_address);

    memset(ardop_addr.sin_zero, 0, sizeof(ardop_addr.sin_zero));

    addr_size = sizeof ardop_addr;
    connect(connector->data_socket, (struct sockaddr *) &ardop_addr, addr_size);

    pthread_t tid2;
    pthread_create(&tid2, NULL, ardop_data_worker_thread_tx, (void *) connector);

    // just to block, we dont create a new thread here
    ardop_data_worker_thread_rx((void *) connector);

    return true;
}
