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
 * Spool directory routines
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
#include <dirent.h>
#include <sys/inotify.h>

#include "spool.h"
#include "connector.h"


bool write_message_to_buffer(char *msg_path, rhizo_conn *connector){
    uint8_t buffer[BUFFER_SIZE];
    FILE *f_in;
    uint32_t msg_size;

    f_in = fopen(msg_path,"r");
    if (f_in == NULL){
        fprintf(stderr, "write_message_to_buffer: Message %s could not be opened.\n", msg_path);
        return false;
    }

    struct stat st;
    stat(msg_path, &st);
    msg_size = (uint32_t) st.st_size;

    // our 4 byte header which contains the size of the message
    write_buffer(&connector->in_buffer, (uint8_t *) &msg_size, sizeof(msg_size));

    size_t read_count = 0;
    uint32_t total_read = 0;
    while ((read_count = fread(buffer, 1, sizeof(buffer), f_in)) > 0){
        total_read += read_count;

       fprintf(stderr, "writing to buffer msg of size %u tx now: %lu\n", msg_size, read_count);
       write_buffer(&connector->in_buffer, buffer, read_count);
    }

    if (total_read != msg_size){
        fprintf(stderr, "Warning: possible truncated message. FIXME! total_read = %u msg_size = %u\n", total_read, msg_size);
    }

    fclose(f_in);

    return true;
}

bool read_message_from_buffer(char *msg_path, rhizo_conn *connector){
        uint32_t msg_size = 0;
        uint8_t *buffer;

        read_buffer(&connector->out_buffer, (uint8_t *) &msg_size, sizeof(msg_size));

        fprintf(stderr, "Incoming message of size: %u\n", msg_size);

        FILE *fp = fopen (msg_path, "w");
        if (fp == NULL){
            fprintf(stderr, "read_message_from_buffer: Message %s could not be opened.\n", msg_path);
            return false;
        }

        fprintf(stderr, "writing file\n");

        buffer = (uint8_t *) malloc(msg_size);
        read_buffer(&connector->out_buffer, buffer, msg_size);
        fwrite(buffer, 1, msg_size, fp);
        free(buffer);

        fclose(fp);

        return true;
}

void *spool_output_directory_thread(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    struct dirent *dp;
    char msg_path[1024];
    uint32_t msg_counter = 0;

    DIR *dirp = opendir(connector->output_directory);
    if (dirp == NULL){
        fprintf(stderr, "Directory \"%s\" could not be opened.\n", connector->output_directory);
        return NULL;
    }

    uint32_t higher_previous_message = 0;
    while ((dp = readdir(dirp)) != NULL){
        if (dp->d_type == DT_REG){
            uint32_t curr_msg;
            sscanf(dp->d_name, "msg-%u.txt", &curr_msg);
            if (curr_msg > higher_previous_message)
                higher_previous_message = curr_msg;
        }
    }
    (void)closedir(dirp);

    msg_counter = higher_previous_message + 1;

    while (true){
        strcpy(msg_path, connector->output_directory);
        sprintf(msg_path+strlen(msg_path), "msg-%010u.txt", msg_counter);
        msg_counter++;

        read_message_from_buffer(msg_path, connector);
    }

    return NULL;
}

void *spool_input_directory_thread(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    struct dirent *dp;
    char msg_path[1024];

    DIR *dirp = opendir(connector->input_directory);
    if (dirp == NULL){
        fprintf(stderr, "Directory \"%s\" could not be opened.\n", connector->input_directory);
        return NULL;
    }
    while ((dp = readdir(dirp)) != NULL){
        if (dp->d_type == DT_REG){
            strcpy(msg_path, connector->input_directory);
            strcat(msg_path, dp->d_name);
            write_message_to_buffer(msg_path, connector);
        }
    }
    (void)closedir(dirp);


    // now starts inotify marvelous...
#define EVENT_SIZE  (sizeof(struct inotify_event))
#define BUF_LEN     (1024 * (EVENT_SIZE + 16))

    int length, i = 0;
    int fd;
    int wd;
    char buffer_inot[BUF_LEN];

    fd = inotify_init();

    if (fd < 0) {
        perror("inotify_init");
    }

    wd = inotify_add_watch(fd, connector->input_directory,
                           IN_MOVED_TO | IN_CLOSE_WRITE);

    while(true){
        i = 0;
        length = read(fd, buffer_inot, BUF_LEN);

        if (length < 0) {
            perror("read");
        }

        while (i < length) {
            struct inotify_event *event =
                (struct inotify_event *) &buffer_inot[i];
            if (event->len) {
                if ((event->mask & IN_CLOSE_WRITE) || (event->mask & IN_MOVED_TO)) {
                    strcpy(msg_path, connector->input_directory);
                    strcat(msg_path, event->name);
                    printf("Writing message to buffer %s to buffer.\n", msg_path);
                    write_message_to_buffer(msg_path, connector);
                }


            }
            i += EVENT_SIZE + event->len;
        }

    }
    inotify_rm_watch(fd, wd);
    close(fd);

    fprintf(stderr, "Spool is finishing...\n");

    return NULL;
}
