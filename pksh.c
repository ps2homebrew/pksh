/*
 * pukklink Shell v1.0
 *
 * Copyright (c) Khaled Daham
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
#include "pksh.h"

static char dst_ip[16] = "192.168.0.10";
static char src_ip[16] = "0.0.0.0";
static char pksh_history[MAXPATHLEN];
static int time1, time2, time_base = 0;
static struct timeval benchtime;
int prompt = 1;

int
main(int argc, char* argv[])
{
    int ret;
    int count = 0;
    // poll client stuff
    int i, j, maxi, connfd, sockfd;
    int maxfd;
    struct sockaddr_in cliaddr;
    struct timeval clienttimeout;
    int nready;
    socklen_t clilen;
    //
    int client[MAX_CLIENTS];
    fd_set master_set, readset, clientset;
    VERBOSE = 0;
    printf("pksh version 2.0\n");
    read_config();
    if (argc == 2) {
        strcpy(dst_ip, argv[1]);
        dst_ip[strlen(argv[1])] = '\0';
    }
    clienttimeout.tv_sec = 0;
    clienttimeout.tv_usec = USEC;

    printf(" Connecting to %s", dst_ip);
    fflush(stdout);
    pko_srv_fd = pko_cmd_setup(dst_ip, PKO_SRV_PORT, 60);
    if (pko_srv_fd < 0) {
        printf(", failed\n");
        return 1;
    }

    // client stuff
    for(i = 0; i < MAX_CLIENTS; i++) {
        client[i] = -1;
    }
    FD_ZERO(&clientset);
    // end client stuff

    printf("\n");

    if (DUMPSCREEN) {
        pko_cmd_con(dst_ip, PKO_CMD_PORT);
        pko_dump2screen_req(pko_cmd_fd);
    }

    pko_log_fd = pko_log_setup(src_ip, PKO_LOG_PORT);
    pksh_srv_fd = pko_srv_setup(src_ip, PKO_SRV_PORT);
    FD_ZERO(&master_set);
    FD_SET(0, &master_set);
    FD_SET(pko_srv_fd, &master_set);
    FD_SET(pko_log_fd, &master_set);
    FD_SET(pksh_srv_fd, &master_set);
    client[0] = 0;
    client[1] = pko_srv_fd;
    client[2] = pko_log_fd;
    client[3] = pksh_srv_fd;
    maxfd = pksh_srv_fd;
    maxi = 3;

    initialize_readline();

    while(doloop)
    {
        readset = master_set;
        ret = select(maxfd+1, &readset, NULL, NULL, NULL);
        if ( ret < 0 )
        {
            perror("select");
            return 0;
        } else if (ret == 0) {
            /* no file desc are ready, lets move on in life */
        } else {
            for(i = 0; i <= maxi; i++) {
                if ( (sockfd = client[i]) < 0) {
                    continue;
                }
                if ( !FD_ISSET(sockfd, &readset) ) {
                    continue;
                }
                if ( sockfd == pko_log_fd ) {
                    if(prompt)
                        fprintf(stdout, "\n");
                    pko_log_read(pko_log_fd);
                    prompt = 0;
                } else if (sockfd == 0) {
                    prompt = 1;
                    rl_callback_read_char();
                } else if(sockfd == pko_srv_fd) {
                    if(prompt)
                        fprintf(stdout, "\n");
                    pko_srv_read(pko_srv_fd);
                } else if (sockfd == pksh_srv_fd) {
                    clilen = sizeof(cliaddr);
                    connfd = accept(pksh_srv_fd, (struct sockaddr *)&cliaddr, &clilen);
                    for(j = 0; i<FD_SETSIZE; j++) {
                        if(client[j] < 0) {
                            client[j] = connfd;
                            break;
                        }
                    }
                    FD_SET(connfd, &master_set);
                    if(connfd > maxfd) {
                        maxfd = connfd;
                    }
                    if ( j > maxi ) {
                        maxi = j;
                    }
                    if (--nready <= 0) {
                        continue;
                    }
                } else {
                    if ( pko_srv_read(sockfd) < 0 ) {
                        close(sockfd);
                        FD_CLR(sockfd, &master_set);
                        client[i] = -1;
                        maxi--;
                    }
                }
                count = 0;
            }
        }

        if( (prompt == 0) && (count > MAX_NO_PROMPT_COUNT)) {
            rl_forced_update_display();
            count = 0;
            prompt = 1;
        }
        count++;
    }

    rl_callback_handler_remove();
    if ( (ret = close(pko_srv_fd)) == -1 ) {
        perror("pko_srv_fd");
    }
    if ( (ret = close(pko_log_fd)) == -1 ) {
        perror("pko_log_fd");
    }
    if ( log_to_file ) {
        if ((ret = close(log_f_fd)) == -1)
            perror("log_f_fd");
    }
    if (strcmp(pksh_history, "") != 0 ) {
        if ( (ret = write_history(pksh_history)) != 0) 
            perror("write_history");
    }
    return(0);
}

int
pko_log_read(int fd)
{
    int ret;
    int size = sizeof(struct sockaddr_in);
    static char buf[1024];
    struct sockaddr_in dest_addr;
    memset(buf, 0x0, sizeof(buf));
    memset(&(dest_addr), '\0', size);
    ret = recvfrom(
        fd, buf, sizeof(buf), 0,
        (struct sockaddr *)&dest_addr, &size
        );

    if (ret == -1) {
        perror("read");
    }
    if (log_to_file) {
        write(log_f_fd, buf, strlen(buf));
    } else {
        fprintf(stdout, "%s", buf);
        fflush(stdout);
    }
    return ret;
}

int
pko_srv_read(int fd) {
    int length = 0;
    int ret = 0;
    unsigned int cmd;
    unsigned short hlen;
    pko_pkt_hdr *header;
    
    length = pko_recv_bytes(fd, &recv_packet[0], sizeof(pko_pkt_hdr));
    if ( length < 0 ) {
        return length;
    } else if ( length == 0 ) {
        if ( fd == pko_srv_fd ) {
            close(fd);
            change_prompt();
            while(1) {
                sleep(1);
                pko_srv_fd = pko_cmd_setup(dst_ip, PKO_SRV_PORT, 1);
                if (pko_srv_fd > 0) {
                    break;
                }
            }
            change_prompt();
        } else {
            return -1;
        }
    } else {
        if(prompt)
            fprintf(stdout, "\n");
        prompt = 0;

        header = (pko_pkt_hdr *)&recv_packet[0];
        cmd = ntohl(header->cmd);
        hlen = ntohs(header->len);
        ret = pko_recv_bytes(fd, &recv_packet[length],
            hlen - sizeof(pko_pkt_hdr));

        switch (cmd) {
            case PKO_OPEN_CMD:
                if (VERBOSE) {
                    gettimeofday(&benchtime, NULL);
                    time1=(benchtime.tv_sec - time_base)*USEC+benchtime.tv_usec;
                }
                pko_open_file(&recv_packet[0]);
                break;
            case PKO_CLOSE_CMD:
                if (VERBOSE)
                {
                    gettimeofday(&benchtime, NULL);
                    time2=(benchtime.tv_sec - time_base)*USEC+benchtime.tv_usec;
                    printf(" took %2.3fs\n", ((float)(time2-time1)/(float)USEC));
                }
                pko_close_file(&recv_packet[0]);
                break;
            case PKO_READ_CMD:
                pko_read_file(&recv_packet[0]);
                break;
            case PKO_WRITE_CMD:
                pko_write_file(&recv_packet[0]);
                break;
            case PKO_LSEEK_CMD:
                pko_lseek_file(&recv_packet[0]);
                break;
            case PKO_DOPEN_CMD:
                pko_open_dir(&recv_packet[0]);
                break;
            case PKO_DREAD_CMD:
                pko_read_dir(&recv_packet[0]);
                break;
            case PKO_DCLOSE_CMD:
                pko_close_dir(&recv_packet[0]);
                break;
            case PKO_DUMPMEM_CMD:
                dumpmem( 
                    ((pko_pkt_dumpmem_req*)
                        &recv_packet[0])->argv,
                    (unsigned int)ntohl(((pko_pkt_dumpmem_req*)
                            &recv_packet[0])->offset),
                    (unsigned int)ntohl(((pko_pkt_dumpmem_req*)
                            &recv_packet[0])->size)
                    );
                break;
            case PKO_DUMPREGS_CMD:
                dumpregs(
                    ((pko_pkt_dumpregs_req*)&recv_packet[0])->argv,
                    ntohl(((pko_pkt_dumpregs_req*)&recv_packet[0])->regs)
                    );
                break;
            case PKO_GSEXEC_CMD:
                cli_gsexec(
                    ((pko_pkt_gsexec_req*)&recv_packet[0])->file
                    );
            case PKO_STOPVU_CMD:
                pko_cmd_con(dst_ip, PKO_CMD_PORT);
                pko_stop_vu(pko_cmd_fd, 
                    ntohs(((pko_pkt_stopvu_req *)
                            &recv_packet[0])->vpu)
                    );
                break;
            case PKO_STARTVU_CMD:
                pko_cmd_con(dst_ip, PKO_CMD_PORT);
                pko_start_vu(pko_cmd_fd, 
                    ntohs(((pko_pkt_startvu_req *)
                            &recv_packet[0])->vpu)
                    );
                break;
            case PKO_EXECEE_CMD:
                cli_execee(
                    ((pko_pkt_execee_req *)&recv_packet[0])->argv
                    );
                break;
            case PKO_EXECIOP_CMD:
                cli_execeiop(
                    ((pko_pkt_execiop_req*)&recv_packet[0])->argv
                    );
                break;
            case PKO_RESET_CMD:
                cli_reset();
                break;
            default:
                printf(" unknown fileio command received (%x)\n", header->cmd);
                break;
        }
    }
    return 0;
}

/* 
 *      CLI commands
 */
static char clicom[1024];

int
execute_line(line)
    char *line;
{
    int i;
    COMMAND *command;
    char *word;

    i = 0;
    while (line[i] && whitespace(line[i]))
        i++;
    word = line + i;

    while (line[i] && !whitespace(line[i]))
        i++;

    if (line[i])
        line[i++] = '\0';

    command = find_command(word);
    if (!command) {
        fprintf(stderr, "%s: No such command\n", word);
        return(-1);
    }

    while (whitespace(line[i]))
        i++;

    word = line + i;
    return ((*(command->func))(word));
}

COMMAND *
find_command(name)
    char *name;
{
    int i = 0;
    for (i = 0; commands[i].name; i++)
        if (strcmp(name, commands[i].name) == 0)
            return(&commands[i]);

    return ((COMMAND *)NULL);
}


int
cli_make(arg)
    char *arg;
{
    if (!arg)
        return 0;
    sprintf(clicom, "make %s", arg);
    return (system(clicom));
}


int
cli_list(arg)
    char *arg;
{
    if (!arg)
        arg = "";

    sprintf(clicom, "ls -l %s", arg);
    return (system(clicom));
}

int
cli_cd(arg)
    char *arg;
{
    char line[MAXPATHLEN];
    char key[MAXPATHLEN];
    char value[MAXPATHLEN];
    if (strlen(arg) == 0) {
        get_home(line);
        read_pair(line, key, value);
        chdir(value);
        cli_pwd();
        return 0;
    }
    if (chdir(arg) == -1) {
        perror(arg);
        return 1;
    }
    cli_pwd();
    return (0);
}

int
cli_gsexec(arg)
    char *arg;
{
    int size, ret, argvlen;
    unsigned char argv[PKO_MAX_PATH];
    trim(arg);
    size = size_file(arg);
    if ( size > 16384 ) {
        printf("Data file too big\n");
        return -1;
    } else if ( size == 0 ) {
        return -1;
    }

    fix_cmd_arg(argv, arg, &argvlen);

    pko_cmd_con(dst_ip, PKO_CMD_PORT);
    ret = pko_gsexec_req(pko_cmd_fd, argv, size);
    if ( ret < 0) {
        perror(" gsexec failed");
    }
    return 0;
}

int
cli_help(arg)
    char *arg;
{
    int i = 0;
    int printed = 0;
    for(i = 0; commands[i].name; i++) {
        if (!*arg || (strcmp(arg, commands[i].name) == 0)) {
            printf("%-10s  %s.\n", commands[i].name, commands[i].doc);
            printed++;
        }
    }
    if (!printed) {
        printf(" No commands match '%s'. Available commands are:\n", arg);
        for (i = 0; commands[i].name; i++) {
            if(printed == 6) {
                printed = 0;
                printf("\n");
            }
            printf(" %s\t", commands[i].name);
            printed++;
        }

        if (printed)
            printf("\n");
    }
    return 0;
}

int
cli_pwd()
{
    char dir[1024], *s;
    s = getwd(dir);
    if (s == 0) {
        fprintf(stdout, "Error getting pwd: %s\n", dir);
        return 1;
    }
    fprintf(stdout, "%s\n", dir);
    return 0;
}

int
cli_execeiop(arg)
    char *arg;
{
    int ret;
    unsigned int argc, argvlen;
    unsigned char argv[PKO_MAX_PATH];
    state = 1;
    argc = fix_cmd_arg(argv, arg, &argvlen);
    pko_cmd_con(dst_ip, PKO_CMD_PORT);
    ret = pko_execiop_req(pko_cmd_fd, argv, argvlen, argc);
    if ( ret < 0) {
        perror(" execiop failed");
    }
    return 0;
}

int
cli_execee(arg)
    char *arg;
{
    int ret;
    unsigned int argc, argvlen;
    unsigned char argv[PKO_MAX_PATH];
    if ( state == 1 ) {
        cli_reset("");
        close(pko_srv_fd);
        change_prompt();
        while(1) {
            sleep(1);
            pko_srv_fd = pko_cmd_setup(dst_ip, PKO_SRV_PORT, 1);
            if (pko_srv_fd > 0) {
                break;
            }
        }
        change_prompt();
    }
    state = 1;
    argc = fix_cmd_arg(argv, arg, &argvlen);
    pko_cmd_con(dst_ip, PKO_CMD_PORT);
    ret = pko_execee_req(pko_cmd_fd, argv, argvlen, argc);
    if ( ret < 0) {
        perror(" execee failed");
    }
    return 0;
}

int
cli_dumpregs(arg)
    char *arg;
{
    int i, ret;
    unsigned int regs = DUMP_REG_MAX;
    char file[PKO_MAX_PATH];
    char *ptr;
    trim(arg);
    if ((ptr = strchr(arg, ' ')) == NULL) {
        return -1;
    }
    strncpy(file, arg, strlen(arg)-strlen(ptr));
    file[strlen(arg)-strlen(ptr)] = '\0';
    if ((ptr = strchr(arg, ' ')) == NULL) {
        return -1;
    }
    for(i = 0; i < DUMP_REG_MAX; i++) {
        if(!strncmp(arg, DUMP_REG_SYM[i], ptr-arg) ) {
            regs = i;
            break;
        }
    }
    ret = dumpregs(file, regs);
    if ( ret < 0) {
        perror(" dumpregs failed");
    }
    return 0;
}
int
dumpregs(char *file, unsigned int regs) {
    char argv[PKO_MAX_PATH];
    arg_prepend_host(argv, file);
    pko_cmd_con(dst_ip, PKO_CMD_PORT);
    return pko_dumpregs_req(pko_cmd_fd, argv, regs);
}

int
cli_dumpmem(arg)
    char *arg;
{
    unsigned int size = 0;
    unsigned int offset = 0;
    int ret;
    char file[PKO_MAX_PATH];
    char *ptr;
    trim(arg);
    if ((ptr = strchr(arg, ' ')) == NULL) {
        return -1;
    }
    strncpy(file, arg, strlen(arg)-strlen(ptr));
    file[strlen(arg)-strlen(ptr)] = '\0';
    offset = (unsigned int)strtol(ptr, (char **)NULL, 0);
    ptr = trim(ptr);
    if ((ptr = strchr(ptr, ' ')) == NULL) {
        return -1;
    }
    size = (unsigned int)strtol(ptr, (char **)NULL, 0);
    ret = dumpmem(file, offset, size);
    if ( ret < 0) {
        perror(" dumpmem failed");
    }
    return 0;
}

int
dumpmem(char *file, unsigned int offset, unsigned int size) {
    char argv[PKO_MAX_PATH];
    pko_cmd_con(dst_ip, PKO_CMD_PORT);
    arg_prepend_host(argv, file);
    return pko_dumpmemory_req(pko_cmd_fd, argv, offset, size);
}

int
cli_exception(arg)
    char *arg;
{
    trim(arg);
    pko_cmd_con(dst_ip, PKO_CMD_PORT);
    if (strcmp(arg, "console") == 0) {
        DUMPSCREEN = 0;
        pko_dump2pc_req(pko_cmd_fd);
    } else if (strcmp(arg, "screen") == 0) {
        DUMPSCREEN = 1;
        pko_dump2screen_req(pko_cmd_fd);
    } else {
        printf("Unknown arg to dump: \"%s\"\n", arg);
    }
    return 0;
}

int
cli_vustart(char *arg) {
    int vpu = strtol(arg, (char **)NULL, 10);
    pko_cmd_con(dst_ip, PKO_CMD_PORT);
    if ( vpu != 0 && vpu != 1 ) {
        return -1;
    }
    pko_start_vu(pko_cmd_fd, vpu);
    return 0;
}

int
cli_vustop(char *arg) {
    int vpu = strtol(arg, (char **)NULL, 10);
    pko_cmd_con(dst_ip, PKO_CMD_PORT);
    if ( vpu != 0 && vpu != 1 ) {
        return -1;
    }
    pko_stop_vu(pko_cmd_fd, vpu);
    return 0;
}

/* int */
/* check_integer(char *arg) { */
/*     long long_var; */
/*     char *end_ptr; */
/*     long_var = strtol(arg, &end_ptr, 0); */

/*     if (ERANGE == errno) { */
/*     } else if (long_var > INT_MAX) { */
/*     } else if (long_var < INT_MIN) { */
/*     } else if (end_ptr == arg) { */
/*     } else { */
/*         return 1; */
/*     } */
/*     return 0; */
/* } */

int
cli_poweroff() {
    pko_cmd_con(dst_ip, PKO_CMD_PORT);
    pko_poweroff_req(pko_cmd_fd);
    return 0;
}

int
cli_reset() {
    state = 0;
    pko_cmd_con(dst_ip, PKO_CMD_PORT);
    pko_reset_req(pko_cmd_fd);
    return 0;
}

int
cli_status() {
    printf(" TCP srv fd = %d\n", pko_srv_fd);
    printf(" UDP log fd = %d\n", pko_log_fd);
    printf(" PKSH cmd fd = %d\n", pksh_srv_fd);
    if ( log_to_file )
        printf(" Logging to file\n");
    else
        printf(" Logging to stdout\n");
    if ( VERBOSE )
        printf(" Verbose mode is on\n");
    else
        printf(" Verbose mode is off\n");

    if(pko_debug()) {
        printf(" Debug is on\n");
    } else {
        printf(" Debug is off\n");
    }

    if ( DUMPSCREEN ) {
        printf(" Exception dumps to screen\n");
    } else {
        printf(" Exception dumps to console\n");
    }
    return 0;
}

int
cli_quit() {
    doloop = 0;
    return 0;
}

int
cli_reconnect() {
    close(pko_srv_fd);
    pko_srv_fd = pko_cmd_setup(dst_ip, PKO_SRV_PORT, 60);
    return 0;
}

int
cli_log(arg)
    char *arg;
{
    trim(arg);
    if ( (strcmp(arg, "stdout"))==0 || !*arg)
    {
        if ( log_to_file )
        {
            if (VERBOSE)
                printf(" Closing log file fd\n");
            close(log_f_fd);
        }
        if (VERBOSE)
            printf(" Logging to stdout\n");
        log_to_file = 0;
    } else {
        if (VERBOSE)
            printf(" Open file %s for logging\n", arg);
        log_to_file = 1;
        log_f_fd = open(arg, LOG_F_CREAT, LOG_F_MODE);
    }
    return 0;
}

int
cli_verbose() {
    if (VERBOSE) {
        printf(" Verbose off\n");
        VERBOSE = 0;
    } else {
        VERBOSE = 1;
        printf(" Verbose on\n");
    }
    return 0;
}

int
cli_debug() {
    if (pko_debug()) {
        pko_set_debug(0);
        printf(" Debug off\n");
    } else {
        pko_set_debug(1);
        printf(" Debug on\n");
    }
    return 0;
}

int
cli_setroot(arg)
    char *arg;
{
    pko_set_root(arg);
    return(0);
}

/*
 * Readline init.
 */
int
initialize_readline(void)
{
    rl_bind_key_in_map(META ('p'), cli_pwd, emacs_standard_keymap);
    rl_bind_key_in_map(META ('q'), cli_quit, emacs_standard_keymap);
    rl_bind_key_in_map(META ('r'), cli_reset, emacs_standard_keymap);
    rl_bind_key_in_map(META ('s'), cli_status, emacs_standard_keymap);
    rl_bind_key_in_map(META ('v'), cli_verbose, emacs_standard_keymap);
    rl_readline_name = "pksh";
    rl_attempted_completion_function = command_completion;
    rl_ignore_some_completions_function = filename_completion_ignore;
    if (strcmp(pksh_history, "") != 0) {
        if (read_history(pksh_history) != 0) {
            perror(pksh_history);
        }
    }
    rl_callback_handler_install(get_prompt(), cli_handler);
    return 0;
}

char *
command_generator(text, state)
    char *text;
    int state;
{
    static int list_index, len;
    char *name;

    if (!state)
    {
        list_index = 0;
        len = strlen(text);
    }

    while ((name = commands[list_index].name) )
    {
        list_index++;
        if (strncmp(name, text, len) == 0)
            return(dupstr(name));
    }
    return((char *)NULL); 
}

void
read_config(void)
{
    static char key[MAXPATHLEN];
    static char value[MAXPATHLEN];
    static char line[MAXPATHLEN];
    char *ptr = value;
    FILE *fd;
    if(get_home(line)) {
        read_pair(line, key, value);
    } else {
        strcpy(value, "./");
    }

    strcat(value, "/.pkshrc");
    if ( (fd = fopen(value, "rb")) == NULL ) {
        perror(value);
        return;
    }

    while((read_line(fd, line)) != -1) {
        if (line[0] == '#') {
            continue;
        }
        read_pair(line, key, value);
        trim(key);
        ptr = stripwhite(value);
        if (strcmp(key, "ip") == 0) {
            strcpy(dst_ip, ptr);
        } else if (strcmp(key, "log") == 0) {
            if (strcmp(ptr, "") == 0) {
                cli_log(stdout);
            } else {
                cli_log(ptr);
            }
        } else if (strcmp(key, "exception") == 0) {
            if (strcmp(ptr, "screen") == 0) {
                DUMPSCREEN = 1;
            } else {
                DUMPSCREEN = 0;
            }
        } else if (strcmp(key, "verbose") == 0) {
            if (strcmp(ptr, "yes") == 0) {
                printf(" Verbose mode on\n");
                VERBOSE = 1;
            }
        } else if (strcmp(key, "debug") == 0) {
            if (strcmp(ptr, "yes") == 0) {
                pko_set_debug(1);
            }
        } else if (strcmp(key, "histfile") == 0) {
            if (strcmp(ptr, "") != 0) {
                strcpy(pksh_history, ptr);
            }
        } else if (strcmp(key, "bind") == 0) {
            if (strcmp(ptr, "") != 0) {
                strcpy(src_ip, ptr);
            }
        } else if (strcmp(key, "path") == 0) {
            if (strcmp(ptr, "") != 0) {
                pko_set_path(path_split(ptr));
            }
        } else if (strcmp(key, "suffix") == 0) {
            if (strcmp(ptr, "") != 0) {
                common_set_suffix(path_split(ptr));
            }
        } else if (strcmp(key, "setroot") == 0) {
            if (strcmp(ptr, "") != 0) {
                pko_set_root(ptr);
            }
        } else if (strcmp(key, "home") == 0) {
            if (strcmp(ptr, "") != 0) {
                if (chdir(ptr) == -1) {
                    perror(ptr);
                }
            }
        }
    }
    return;
}
