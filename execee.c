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

#include "linkproto_stub.h"
#include "common.h"

int
main(int argc, char **argv)
{
    int sock, ret, i;
    struct sockaddr_in addr;
    unsigned char file[PKO_MAX_PATH];
    memset(file, 0, PKO_MAX_PATH);

    if (argc < 2) {
        printf("Usage:\n"
               " %s <EE elf> arg1 .. argN\n\n", argv[0]);
        return 0;
    }

    bzero((void *)&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PKO_SRV_PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    memset(&(addr.sin_zero), '\0', 8);
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1 ) {
        perror("socket");
    }
    if((ret = connect(sock, (struct sockaddr *)&addr,
                sizeof(struct sockaddr_in))) < 0) {
        perror("");
    }
    // if we have device its most likely a full path
    if ( !arg_device_check(argv[1]) ) {
        strcpy(file, getcwd(NULL, 0));
        strcat(file, "/");
        strcat(file, argv[1]);
    } else {
        strcpy(file, argv[1]);
    }

    // prepend arguments
    for ( i = 2; i < argc; i++ ) {
        strcat(file, " ");
        strcat(file, argv[i]);
    }

    ret = pko_execee_req(sock, file, strlen(file), 1);
    if (ret < 0) {
        printf("Sending execee req failed\n");
    }
    return 0;
}
