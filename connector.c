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
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <dirent.h>

#include "connector.h"
#include "spool.h"
#include "vara.h"
#include "dstar.h"
#include "ardop.h"

// temporary global variable to enable sockets closure
rhizo_conn *tmp_conn = NULL;

void finish(int s){
    fprintf(stderr, "\nExiting...\n");

    /* Do house cleaning work here */
    if (tmp_conn){
        if (tmp_conn->data_socket){
            shutdown(tmp_conn->data_socket, SHUT_RDWR);
            close (tmp_conn->data_socket);
        }
        if (tmp_conn->control_socket){
            shutdown(tmp_conn->control_socket, SHUT_RDWR);
            close (tmp_conn->control_socket);
        }
    }

    exit(EXIT_SUCCESS);
}

void *modem_thread(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;

    if (!strcmp("vara", connector->modem_type)){
        initialize_modem_vara(connector);
    }

    if (!strcmp("ardop", connector->modem_type)){
        initialize_modem_ardop(connector);
    }

    if (!strcmp("dstar", connector->modem_type)){
        initialize_modem_dstar(connector);
    }

    return NULL;
}


int main (int argc, char *argv[])
{
    rhizo_conn connector;

    tmp_conn = &connector;

    // initialize our buffers
    initialize_buffer(&connector.in_buffer, 20); // 1MB
    initialize_buffer(&connector.out_buffer, 20); // 1MB

    // Catch Ctrl+C
    signal (SIGINT,finish);

    fprintf(stderr, "Rhizo-connector by Rafael Diniz -  rafael (AT) rhizomatica (DOT) org\n");
    fprintf(stderr, "License: GPLv3+\n\n");

    if (argc < 7)
    {
    manual:
        fprintf(stderr, "Usage modes: \n%s -r radio_modem_type -i input_spool_directory -o output_spool_directory -c callsign -d remote_callsign -s RX -a tnc_ip_address -p tcp_base_port\n", argv[0]);
        fprintf(stderr, "%s -h\n", argv[0]);
        fprintf(stderr, "\nOptions:\n");
        fprintf(stderr, " -r [ardop,dstar,vara]       Choose modem/radio type.\n");
        fprintf(stderr, " -i input_spool_directory    Input spool directory (Messages to send).\n");
        fprintf(stderr, " -o output_spool_directory    Output spool directory (Received messages).\n");
        fprintf(stderr, " -c callsign                        Station Callsign (Eg: PU2HFF).\n");
        fprintf(stderr, " -d remote_callsign           Remote Station Callsign.\n");
        fprintf(stderr, " -a tnc_ip_address            IP address of the TNC,\n");
        fprintf(stderr, " -p tcp_base_port              TCP base port of the TNC. For VARA and ARDOP ports tcp_base_port and tcp_base_port+1 are used,\n");
        fprintf(stderr, " -s initial_state                 Initial modem state: RX, TX.");
        fprintf(stderr, " -h                          Prints this help.\n");
        exit(EXIT_FAILURE);
    }

    int opt;
    while ((opt = getopt(argc, argv, "hr:i:o:c:d:p:a:s:")) != -1)
    {
        switch (opt)
        {
        case 'h':
            goto manual;
            break;
        case 'c':
            strcpy(connector.call_sign, optarg);
            break;
        case 'd':
            strcpy(connector.remote_call_sign, optarg);
            break;
        case 's':
            if (!strcmp("rx", optarg) || !strcmp("RX", optarg))
                connector.mode = MODE_RX;
            if (!strcmp("tx", optarg) || !strcmp("TX", optarg))
                connector.mode = MODE_TX;
            break;
        case 'p':
            connector.tcp_base_port = atoi(optarg);
            break;
        case 'a':
            strcpy(connector.ip_address, optarg);
            break;
        case 'r':
            strcpy(connector.modem_type, optarg);
            break;
        case 'i':
            strcpy(connector.input_directory, optarg);
            break;
        case 'o':
            strcpy(connector.output_directory, optarg);
            break;
        default:
            goto manual;
        }
    }

    connector.connected = false;
    connector.waiting_for_connection = false;

    pthread_t tid[3];

    pthread_create(&tid[0], NULL, spool_input_directory_thread, (void *) &connector);
    pthread_create(&tid[1], NULL, spool_output_directory_thread, (void *) &connector);

    // pthread_create(&tid[2], NULL, modem_thread, (void *) &connector);
    modem_thread((void *) &connector);

    return EXIT_SUCCESS;
}
