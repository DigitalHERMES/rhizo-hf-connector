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
#include "spool.h"
#include "net.h"

// TODO: implement a mechanism which verifies when messages are send (using BUFFER cmd)?

void *ardop_data_worker_thread_tx(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    uint8_t *buffer;
    uint32_t buf_size; // our header is 4 bytes long
    uint8_t ardop_size[2];
    uint32_t packet_size;

    while(true){
        // check if we are connected, otherwise, wait
        while (connector->connected == false || ring_buffer_count_bytes(&connector->in_buffer.buf) == 0){
            sleep(1);
        }
        // read header
        read_buffer(&connector->in_buffer, (uint8_t *) &buf_size, sizeof(buf_size)); // TODO: if the two parties in a connection have different endianess, we are in trouble

        fprintf(stderr, "ardop_data_worker_thread_tx: Tx msg size: %u\n", buf_size);

        buffer = (uint8_t *) malloc(buf_size);
        memset(buffer, 0, buf_size);
        read_buffer(&connector->in_buffer, buffer, buf_size);

        fprintf(stderr, "ardop_data_worker_thread_tx: After read buffer\n");

        packet_size = buf_size + 4; // added our 4 bytes header with length

        uint32_t counter = 0;
        uint32_t tx_size = packet_size;
        while (tx_size != 0){
        // max size here is circa 8182 counting the 4byte header
           if (tx_size > MAX_ARDOP_PACKET){
               ardop_size[0] = (uint8_t) (MAX_ARDOP_PACKET >> 8);
               ardop_size[1] = (uint8_t) (MAX_ARDOP_PACKET & 255);
           }
           else{
               ardop_size[0] = (uint8_t) (tx_size >> 8);
               ardop_size[1] = (uint8_t) (tx_size & 255);
           }

           // ardop header
           tcp_write(connector->data_socket, ardop_size, sizeof(ardop_size));

           fprintf(stderr, "ardop_data_worker_thread_tx: After ardop header tcp_write\n");

           if (tx_size == packet_size) { // first pass, we send our size header
               tcp_write(connector->data_socket, (uint8_t *) &buf_size, sizeof(buf_size) );
               if (tx_size > MAX_ARDOP_PACKET){
                   tcp_write(connector->data_socket, buffer , MAX_ARDOP_PACKET - 4);
                   counter += MAX_ARDOP_PACKET - 4;
                   tx_size -= MAX_ARDOP_PACKET;
               }
               else{
                   tcp_write(connector->data_socket, buffer, tx_size - 4);
                   counter += tx_size - 4;
                   tx_size -= tx_size;
               }
           }
           else{ // not first pass
               if (tx_size > MAX_ARDOP_PACKET){
                   tcp_write(connector->data_socket, &buffer[counter] , MAX_ARDOP_PACKET);
                   counter += MAX_ARDOP_PACKET;
                   tx_size -= MAX_ARDOP_PACKET;
               }
               else{
                   tcp_write(connector->data_socket, &buffer[counter], tx_size);
                   counter += tx_size;
                   tx_size -= tx_size;
               }
           }
           fprintf(stderr, "ardop_data_worker_thread_tx: tcp_write %u\n", counter);
        }

        free(buffer);
    }

}

void *ardop_data_worker_thread_rx(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    uint8_t buffer[MAX_ARDOP_PACKET_SAFE];
    uint32_t buf_size; // our header is 4 bytes long
    uint8_t ardop_size[2];

    while(true){
        while (connector->connected == false){
            sleep(1);
        }

        tcp_read(connector->data_socket, ardop_size, 2);

        // ARDOP TNC data format: length 2 bytes | payload
        buf_size = 0;
        buf_size = ardop_size[0];
        buf_size <<= 8;
        buf_size |= ardop_size[1];

        tcp_read(connector->data_socket, buffer, buf_size);

        fprintf(stderr,"Ardop message of size: %u received.\n", buf_size);

        if (buf_size > 3 && !memcmp("ARQ", buffer,  3)){
            buf_size -= 3;
            write_buffer(&connector->out_buffer, buffer + 3, buf_size);
        }
        else{
            fprintf(stderr, "Ardop non-payload data rx: %s\n", buffer);
        }

    }

    return EXIT_SUCCESS;
}

void *ardop_control_worker_thread_rx(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    uint8_t rcv_byte;
    uint8_t buffer[1024];
    int counter = 0;
    bool new_cmd = false;

    while(true){
        tcp_read(connector->control_socket, &rcv_byte, 1);

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

// treat "STATUS CONNECT TO PP2UIT FAILED!" ?
// and reset waiting for connection!

        if (new_cmd){
            if (!memcmp(buffer, "DISCONNECTED", strlen("DISCONNECTED"))){
                fprintf(stderr, "TNC: %s\n", buffer);
                connector->connected = false;
                connector->waiting_for_connection = false;
            } else
            if (!memcmp(buffer, "NEWSTATE DISC", strlen("NEWSTATE DISC"))){
                fprintf(stderr, "TNC: %s\n", buffer);
                connector->connected = false;
                connector->waiting_for_connection = false;
            } else
            if (!memcmp(buffer, "CONNECTED", strlen("CONNECTED"))){
                fprintf(stderr, "TNC: %s\n", buffer);
                connector->connected = true;
                connector->waiting_for_connection = false;
            } else
            if (!memcmp(buffer, "PTT", strlen("PTT"))){
                // supressed output
                // fprintf(stderr, "%s -- CMD NOT CONSIDERED!!\n", buffer);
            } else
            if (!memcmp(buffer, "BUFFER", strlen("BUFFER"))){
                uint32_t buf_size;
                sscanf( (char *) buffer, "BUFFER %u", &buf_size);
                fprintf(stderr, "BUFFER: %u\n", buf_size);

                // our delete messages mechanism
                if (buf_size == 0 &&
                    ring_buffer_count_bytes(&connector->in_buffer.buf) == 0 &&
                    connector->connected == true){
                    fprintf(stderr, "Shoud we call now to erase messages?\n");
                    remove_all_msg_path_queue(connector);
                }

            } else
            if (!memcmp(buffer, "INPUTPEAKS", strlen("INPUTPEAKS"))){
                // suppressed output
            } else {
                fprintf(stderr, "%s\n", buffer);
            }
        }
    }

    return EXIT_SUCCESS;
}

void *ardop_control_worker_thread_tx(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    char buffer[1024];

    // initialize
    memset(buffer,0,sizeof(buffer));
    sprintf(buffer, "INITIALIZE\r");
    tcp_write(connector->control_socket, buffer, strlen(buffer));

    // We set a call sign
    memset(buffer,0,sizeof(buffer));
    sprintf(buffer, "MYCALL %s\r", connector->call_sign);
    tcp_write(connector->control_socket, buffer, strlen(buffer));

    // for initial development, set arq timeout to 4min
    memset(buffer,0,sizeof(buffer));
    sprintf(buffer, "ARQTIMEOUT 240\r");
    tcp_write(connector->control_socket, buffer, strlen(buffer));

    memset(buffer,0,sizeof(buffer));
    strcpy(buffer,"LISTEN True\r");
    tcp_write(connector->control_socket, buffer, strlen(buffer));

    // 1Hz function
    while(true){

        // condition for connection: no connection AND have something to transmit
//        fprintf(stderr, "Connection loop conn: %d buf_cnt: %ld wait_conn %d \n", connector->connected, ring_buffer_count_bytes(&connector->in_buffer.buf), connector->waiting_for_connection);
        if (connector->connected == false &&
            ring_buffer_count_bytes(&connector->in_buffer.buf) > 0 &&
            !connector->waiting_for_connection){

            memset(buffer,0,sizeof(buffer));
            sprintf(buffer,"ARQCALL %s 5\r", connector->remote_call_sign);
            tcp_write(connector->control_socket, (uint8_t *)buffer, strlen(buffer));

            fprintf(stderr, "CONNECTING... %s\n", buffer);

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
