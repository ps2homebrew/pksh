/*
 * Copyright (c) Tord Lindström and Khaled Daham
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author(s) may not be used to endorse or promote products 
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include "common.h"
#include "list.h"

#define PKO_SRV_PORT    0x4711
#define PKO_CMD_PORT    0x4712
#define PKO_LOG_PORT    0x4712

#define PKO_OPEN_CMD    0xbabe0111
#define PKO_OPEN_RLY    0xbabe0112
#define PKO_CLOSE_CMD   0xbabe0121
#define PKO_CLOSE_RLY   0xbabe0122
#define PKO_READ_CMD    0xbabe0131
#define PKO_READ_RLY    0xbabe0132
#define PKO_WRITE_CMD   0xbabe0141
#define PKO_WRITE_RLY   0xbabe0142
#define PKO_LSEEK_CMD   0xbabe0151
#define PKO_LSEEK_RLY   0xbabe0152

#define PKO_MAX_PATH    256
#define PACKET_MAXSIZE  4096
#define PS2_O_RDONLY	0x0001
#define PS2_O_WRONLY	0x0002
#define PS2_O_RDWR		0x0003
#define PS2_O_NBLOCK	0x0010
#define PS2_O_APPEND	0x0100
#define PS2_O_CREAT		0x0200
#define PS2_O_TRUNC		0x0400

#pragma pack(1)

char send_packet[PACKET_MAXSIZE] __attribute__((aligned(16)));
char recv_packet[PACKET_MAXSIZE] __attribute__((aligned(16)));

int pko_srv_fd;
int pksh_srv_fd;
int pko_cmd_fd;
int pko_log_fd;

/*! Function for reading data from clients and ps2link
 * @param sock The socket to connect to
 * @param buf Pointer for data
 * @param bytes Number of bytes to read
 * @return Number of bytes received
 */
int pko_recv_bytes(int s, char *buf, int bytes);
/*! Function for sending data to ps2link
 * @param sock The socket to connect to
 * @param buf Pointer for data
 * @param bytes Number of bytes to send
 * @return Number of bytes sent
 */
int pko_send_bytes(int s, char *buf, int bytes);
/*! Function for establishing a command sender ( will be removed )
 * @param ip a char pointer to ipaddress
 * @param port command port
 * @return filedescriptor or -1 upon failure.
 */
int pko_cmd_con(char *ip, int port);
/*! Function for establishing a command listener.
 * @param ip a char pointer to ipaddress
 * @param port command port
 * @return filedescriptor or -1 upon failure.
 */
int pko_cmd_setup(char *dst_ip, int port, int timeout);
/*! Function for accepting commands from clients
 * @param ip The socket to connect to
 * @param buf Pointer for data
 * @param bytes Number of bytes to send
 * @return Number of bytes sent
 */
int pko_srv_setup(char *src_ip, int port);
void fileio();
int pko_debug(void);
/*! Function for replying on file open request
 * @param buf Pointer for request data
 * @return 0 on Success.
 */
int pko_open_file(char *buf);
/*! Function for replying on file close request
 * @param buf Pointer for request data
 * @return 0 on Success.
 */
int pko_close_file(char *buf);
/*! Function for replying on file read request
 * @param buf Pointer for request data
 * @return 0 on Success.
 */
int pko_read_file(char *buf);
/*! Function for replying on file write request
 * @param buf Pointer for request data
 * @return 0 on Success.
 */
int pko_write_file(char *buf);
/*! Function for replying on file lseek request
 * @param buf Pointer for request data
 * @return 0 on Success.
 */
int pko_lseek_file(char *buf);
/*! Function for setting up a log listener
 * @param src_ip Pointer for local ip that we should bind to.
 * @param port Port number to bind to.
 * @return filedescriptor or -1 upon failure.
 */
int pko_log_setup(char *src_ip, int port);
/*! Function for replying on file open request
 * @param buf Pointer for request data
 * @return filedescriptor or -1 upon failure.
 */
int pko_fix_flags(int flags);
void pko_set_debug(int);
void pko_set_root(char *);
void pko_fix_path(char *);

llist pko_get_path(void);
void pko_set_path(llist); 

// packet types
typedef struct
{
    unsigned int cmd;
    unsigned short len;
} pko_pkt_hdr;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    unsigned int retval;
} pko_pkt_file_rly;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int flags;
    char path[PKO_MAX_PATH];
} pko_pkt_open_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int fd;
} pko_pkt_close_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int fd;
    int nbytes;
} pko_pkt_read_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int retval;
    int nbytes;
} pko_pkt_read_rly;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int fd;
    int nbytes;
} pko_pkt_write_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int fd;
    int offset;
    int whence;
} pko_pkt_lseek_req;
