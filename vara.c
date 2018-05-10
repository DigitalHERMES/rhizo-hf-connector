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

/**
 * @file vara.c
 * @author Rafael Diniz
 * @date 12 Apr 2018
 * @brief VARA modem support functions
 *
 * All the specific code for supporting VARA.
 *
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
#include <sched.h>


#include "common.h"
#include "net.h"
#include "spool.h"
#include "vara.h"

void *vara_data_worker_thread_tx(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    uint8_t *buffer;
    uint32_t buf_size;

    while(true){
        // check if we are connected, otherwise, wait
        connector->safe_state++;
        while (connector->connected == false || ring_buffer_count_bytes(&connector->in_buffer.buf) == 0){
            sleep(1);
        }
        connector->safe_state--;

        connector->timeout_counter = 0;

        // read header
        read_buffer(&connector->in_buffer, (uint8_t *) &buf_size, sizeof(buf_size)); // \todo if the two parties in a connection have different endianess, we are in trouble

        buffer = (uint8_t *) malloc (buf_size);
        memset(buffer, 0, buf_size);
        read_buffer(&connector->in_buffer, buffer, buf_size);

        tcp_write(connector->data_socket, (uint8_t *) &buf_size, sizeof(buf_size) );
        tcp_write(connector->data_socket, buffer, buf_size);

        free(buffer);
    }

}

void *vara_data_worker_thread_rx(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    uint8_t *buffer;
    uint32_t buf_size;
    fd_set read_set;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    FD_ZERO(&read_set);
    FD_SET(connector->data_socket, &read_set);

    while(true){
        connector->safe_state++;
        while (connector->connected == false || select(connector->data_socket+1, &read_set, NULL, NULL, &timeout) == 0){
            sleep(1);
        }
        connector->safe_state--;

        connector->timeout_counter = 0;

        tcp_read(connector->data_socket, (uint8_t *) &buf_size, sizeof(buf_size));

        buffer = (uint8_t *) malloc (buf_size);
        memset(buffer, 0, buf_size);
        tcp_read(connector->data_socket, buffer, buf_size);

        fprintf(stderr,"Message of size: %u received.\n", buf_size);

        write_buffer(&connector->out_buffer, (uint8_t *) &buf_size, sizeof(buf_size));
        write_buffer(&connector->out_buffer, buffer, buf_size);

        connector->timeout_counter = 0;

        free(buffer);
    }

}

void *vara_control_worker_thread_rx(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    uint8_t rcv_byte;
    uint8_t buffer[1024];
    int counter = 0;
    bool new_cmd = false;

    while(true){
        if (!tcp_read(connector->control_socket, &rcv_byte, 1)){
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
                connector->waiting_for_connection = false;
            } else
            if (!memcmp(buffer, "BUFFER", strlen("BUFFER"))){
                uint32_t buf_size;
                sscanf( (char *) buffer, "BUFFER %u", &buf_size);
                fprintf(stderr, "BUFFER: %u\n", buf_size);

                if (buf_size != 0)
                    connector->timeout_counter = 0;

                // our delete messages mechanism
                if (buf_size == 0 &&
                    ring_buffer_count_bytes(&connector->in_buffer.buf) == 0 &&
                    connector->connected == true){
                    fprintf(stderr, "Shoud we call now to erase messages?\n");
                    remove_all_msg_path_queue(connector);
                }
            } else
            if (!memcmp(buffer, "PTT", strlen("PTT"))){
                // supressed output
                // fprintf(stderr, "%s -- CMD NOT CONSIDERED!!\n", buffer);
            } else {
                fprintf(stderr, "%s\n", buffer);
            }
        }
    }

    return EXIT_SUCCESS;
}

void *vara_control_worker_thread_tx(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    char buffer[1024];

    // We set a call sign
    memset(buffer,0,sizeof(buffer));
    sprintf(buffer, "MYCALL %s\r", connector->call_sign);
    tcp_write(connector->control_socket, (uint8_t *) buffer, strlen(buffer));

    memset(buffer,0,sizeof(buffer));
    strcpy(buffer,"LISTEN ON\r");
    tcp_write(connector->control_socket, (uint8_t *) buffer, strlen(buffer));

    // 1Hz function
    while(true){


        // Logic to start a connection
        // condition for connection: no connection AND something to transmitt AND we did not issue a CONNECT recently
        if (connector->connected == false &&
            ring_buffer_count_bytes(&connector->in_buffer.buf) > 0 &&
            !connector->waiting_for_connection){

            // \todo try to add some entropy in order to avoid on air clashes
            memset(buffer,0,sizeof(buffer));
            sprintf(buffer,"CONNECT %s %s\r", connector->call_sign,
                    connector->remote_call_sign);
            tcp_write(connector->control_socket, (uint8_t *)buffer, strlen(buffer));

            fprintf(stderr, "CONNECTING... %s\n", buffer);

            connector->waiting_for_connection = true;
        }

        // Logic to disconnect on timeout
        if (connector->timeout_counter >= connector->timeout &&
            connector->connected == true){

            connector->connected = false;

            while (connector->safe_state != 2){ // this means we have both data threads in the safe zone
                sched_yield();
            }
            fprintf(stderr, "DISCONNECTING BY TIMEOUT...\n");

            memset(buffer,0,sizeof(buffer));
            sprintf(buffer,"DISCONNECT\r");
            tcp_write(connector->control_socket, (uint8_t *)buffer, strlen(buffer));

        }

        sleep(1);

    }

    return EXIT_SUCCESS;
}

bool initialize_modem_vara(rhizo_conn *connector){
    tcp_connect(connector->ip_address, connector->tcp_base_port, &connector->control_socket);
    tcp_connect(connector->ip_address, connector->tcp_base_port+1, &connector->data_socket);

    // we start our control thread
    pthread_t tid1;
    pthread_create(&tid1, NULL, vara_control_worker_thread_rx, (void *) connector);

    // we start our control tx thread
    pthread_t tid2;
    pthread_create(&tid2, NULL, vara_control_worker_thread_tx, (void *) connector);

    pthread_t tid3;
    pthread_create(&tid3, NULL, vara_data_worker_thread_tx, (void *) connector);

    pthread_t tid4;
    pthread_create(&tid4, NULL, connection_timeout_thread, (void *) connector);

    // just to block, we dont create a new thread here
    vara_data_worker_thread_rx((void *) connector);

    return true;
}
