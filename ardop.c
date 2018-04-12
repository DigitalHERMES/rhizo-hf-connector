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

/**
 * @file ardop.c
 * @author Rafael Diniz
 * @date 12 Apr 2018
 * @brief Ardop modem support functions
 *
 * All the specific code for supporting Ardop.
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


#include "ardop.h"
#include "net.h"

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
        if (connector->connected == false){
            sleep(1);
            continue;
        }

        memset(buffer, 0, sizeof(buffer));
        // read header
        read_buffer(&connector->in_buffer, (uint8_t *) &buf_size, sizeof(buf_size)); // TODO: if the two parties in a connection have different endianess, we are in trouble

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

// decreate this... make a loop which allows a smaller recv size
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

void *ardop_control_worker_thread_rx(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    size_t len;
    uint8_t rcv_byte;
    uint8_t buffer[1024];
    int counter = 0;
    bool new_cmd = false;

    while(true){
        len = recv(connector->control_socket, &rcv_byte, 1, 0);
        if (len < 1){
            fprintf(stderr, "control_worker_thread_rx: socket read error.\n");
            // goto die; // ?
        }

        if (rcv_byte == '\r'){
            buffer[counter] = 0;
            counter = 0;
            new_cmd = true;
        }
        else{
            buffer[counter] = rcv_byte;
            counter++;
            new_cmd = false;
        }
// treat "STATUS CONNECT TO PP2UIT FAILED!" 
// and reset waiting for connection!
        if (new_cmd){
            if (!strcmp((char *) buffer, "DISCONNECTED")){
                fprintf(stderr, "TNC: %s\n", buffer);
                connector->connected = false;
                connector->waiting_for_connection = false;
            } else
            // other commands here
            if (!memcmp(buffer, "CONNECTED", strlen("CONNECTED"))){
                fprintf(stderr, "TNC: %s\n", buffer);
                connector->connected = true;
            } else
            if (!memcmp(buffer, "PTT", strlen("PTT"))){
                // supressed output
                // fprintf(stderr, "%s -- CMD NOT CONSIDERED!!\n", buffer);
            } else
            if (!memcmp(buffer, "INPUTPEAKS", strlen("INPUTPEAKS"))){

            } else{
                fprintf(stderr, "%s -- CMD NOT CONSIDERED!!\n", buffer);
            }
        }
    }

    return EXIT_SUCCESS;
}

void *ardop_control_worker_thread_tx(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    char buffer[1024];

    // some initialization

    memset(buffer,0,sizeof(buffer));
    sprintf(buffer, "INITIALIZE\r");
    send(connector->control_socket, buffer, strlen(buffer), 0);

    // We set a call sign
    memset(buffer,0,sizeof(buffer));
    sprintf(buffer, "MYCALL %s\r", connector->call_sign);
    send(connector->control_socket, buffer, strlen(buffer), 0);

    // for initial development, set arq timeout to 4min
    memset(buffer,0,sizeof(buffer));
    sprintf(buffer, "ARQTIMEOUT 240\r");
    send(connector->control_socket, buffer, strlen(buffer), 0);

    memset(buffer,0,sizeof(buffer));
    strcpy(buffer,"LISTEN True\r");
    send(connector->control_socket,buffer,strlen(buffer),0);

    // 1Hz function
    while(true){

        // condition for connection: no connection AND something to transmitt
        if (connector->connected == false &&
            ring_buffer_count_bytes(&connector->in_buffer.buf) > 0 &&
            !connector->waiting_for_connection){

            // TODO: try to add some entropy in order to avoid on air clashes
            memset(buffer,0,sizeof(buffer));
            sprintf(buffer,"ARQCALL %s 5\r", connector->remote_call_sign);
            send(connector->control_socket,buffer,strlen(buffer),0);
            connector->waiting_for_connection = true;
        }


        sleep(1);

    }

    return EXIT_SUCCESS;
}

bool initialize_modem_ardop(rhizo_conn *connector){
    tcp_connect(connector->ip_address, connector->tcp_base_port, &connector->control_socket);
    tcp_connect(connector->ip_address, connector->tcp_base_port+1, &connector->data_socket);

    // we start our control rx thread
    pthread_t tid1;
    pthread_create(&tid1, NULL, ardop_control_worker_thread_rx, (void *) connector);

    // we start our control tx thread
    pthread_t tid2;
    pthread_create(&tid2, NULL, ardop_control_worker_thread_tx, (void *) connector);


    // and run the two workers for the data channel
    pthread_t tid3;
    pthread_create(&tid3, NULL, ardop_data_worker_thread_tx, (void *) connector);

    // just to block, we dont create a new thread here
    ardop_data_worker_thread_rx((void *) connector);

    return true;
}
