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

#define PKO_RESET_CMD       0xbabe0201
#define PKO_EXECIOP_CMD     0xbabe0202
#define PKO_EXECEE_CMD      0xbabe0203
#define PKO_POWEROFF_CMD    0xbabe0204
#define PKO_SCRDUMP_CMD     0xbabe205
#define PKO_NETDUMP_CMD     0xbabe206

#define PKO_DUMPMEM_CMD     0xbabe207
#define PKO_STARTVU_CMD     0xbabe208
#define PKO_STOPVU_CMD      0xbabe209
#define PKO_DUMPREGS_CMD    0xbabe2010
#define PKO_GSEXEC_CMD      0xbabe2011

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

// int link_debug_stub(void);
// function declarations
/*! Tells ps2link to dump exception output to the screen
 * @param sock The socket to connect to
 */
int pko_dump2screen_req(int sock);
/*! Tells ps2link to dump exception output to the client
 * @param sock The socket to connect to
 */
int pko_dump2pc_req(int sock);
/*! Tells ps2link to dump memory 
 * @param sock The socket to connect to
 * @param file File to dump to
 * @param offset Memory start adress
 * @param size Size to dump
 */
int pko_dumpmemory_req(int sock, char *file, unsigned int offset, unsigned int size);
/*! Tells ps2link to dump given registers
 * @param sock The socket to connect to
 * @param file File to dump to
 * @param regs Type of registers to dump
 */
int pko_dumpregs_req(int sock, char *file, unsigned int regs);
/*! Tells ps2link to dump given registers
 * @param sock The socket to connect to
 * @param argv Argument array where the first one is the path to the executable
 * @param argvlen Length of argv
 * @param argc Number of arguments in argv
 */
int pko_execiop_req(int sock, char *argv, unsigned int argvlen, unsigned int argc);
/*! Tells ps2link to execute given file on EE
 * @param sock The socket to connect to
 * @param argv Argument array where the first one is the path to the executable
 * @param argvlen Length of argv
 * @param argc Number of arguments in argv
 */
int pko_execee_req(int sock, char *argv, unsigned int argvlen, unsigned int argc);
/*! Tells ps2link to send data from given file to GS
 * @param sock The socket to connect to
 * @param file File that should be sent to GS
 * @param size Size of the file
 */
int pko_gsexec_req(int sock, char *file, unsigned int size);
/*! Tells ps2link to poweroff PS2
 * @param sock The socket to connect to
 */
int pko_poweroff_req(int sock);
/*! Tells ps2link to reset PS2
 * @param sock The socket to connect to
 */
int pko_reset_req(int sock);
/*! Tells ps2link to stop given VPU
 * @param sock The socket to connect to
 * @param vpu 0 for VPU0 or 1 for VPU1
 */
int pko_stop_vu(int sock, unsigned int vpu);
/*! Tells ps2link to start given VPU
 * @param sock The socket to connect to
 * @param vpu 0 for VPU0 and 1 for VPU1
 */
int pko_start_vu(int sock, unsigned int vpu);

// packet types
typedef struct
{
    unsigned int cmd;
    unsigned short len;
} pko_pkt_reset_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int  argc;
    char argv[PKO_MAX_PATH];
} pko_pkt_execee_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    unsigned short size;
    unsigned char file[PKO_MAX_PATH];
} pko_pkt_gsexec_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int  argc;
    char argv[PKO_MAX_PATH];
} pko_pkt_execiop_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
} pko_pkt_poweroff_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
} pko_pkt_dump2screen_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
} pko_pkt_dump2pc_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    unsigned int offset;
    unsigned int size;
    char argv[PKO_MAX_PATH];
} pko_pkt_dumpmem_req;

typedef struct __attribute__((packed))
{
    unsigned int cmd;
    unsigned short len;
    int vpu;
} pko_pkt_stopvu_req;

typedef struct __attribute__((packed))
{
    unsigned int cmd;
    unsigned short len;
    int vpu;
} pko_pkt_startvu_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    unsigned int regs;
    char argv[PKO_MAX_PATH];
} pko_pkt_dumpregs_req;
