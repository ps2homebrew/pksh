.SUFFIXES : .c .o
CC = /usr/bin/gcc
CFLAGS = -g
LIBS = -lreadline -lcurses -lncurses
INC = -L/usr/local/lib -I/usr/local/include -I/usr/include
NPLIBS = -lusb -lreadline -lc -lcurses -lncurses
RM = rm -rf
TAR = tar -cf
ZIP = gzip -9
FILES = pksh/Makefile pksh/README.npsh pksh/README.pksh  pksh/execee.c pksh/execiop.c\
	pksh/pksh.c pksh/pksh.h pksh/linkproto_stub.c pksh/linkproto_core.c\
	pksh/reset.c pksh/naplink.c pksh/pl2301.h\
	pksh/pl2301.c pksh/npsh.h\
	pksh/npsh.c pksh/common.c pksh/common.h pksh/packet.c pksh/packet.h\
	pksh/naplink.h pksh/SAMPLE.npshrc\
    pksh/SAMPLE.pkshrc pksh/LICENSE pksh/rl_common.c pksh/rl_common.h

PKSH_OBJ = rl_common.o common.o linkproto_stub.o linkproto_core.o pksh.o
NPSH_OBJ = rl_common.o common.o packet.o pl2301.o npsh.o naplink.o

# protective gear on
CFLAGS += -W -Wall -Wpointer-arith -Wcast-align
CFLAGS += -Wbad-function-cast -Wsign-compare
CFLAGS += -Waggregate-return -Wmissing-noreturn -Wnested-externs
CFLAGS += -Wchar-subscripts -Wformat-security

.o:
	$(CC) $(CFLAGS) -o $@ -c $< $(INC)

all: pksh npsh execee execiop reset

pksh: $(PKSH_OBJ)
	$(CC) $(PKSH_OBJ) -o $@ $(LIBS) $(INC)

execee: common.o linkproto_stub.o execee.o
	$(CC) common.o linkproto_stub.o execee.o -o $@

execiop: common.o linkproto_stub.o execiop.o
	$(CC) common.o linkproto_stub.o execiop.o -o $@

reset: common.o linkproto_stub.o reset.o 
	$(CC) common.o linkproto_stub.o reset.o -o $@

naplink.o:
	$(CC) -c naplink.c $(INC)
packet.o:
	$(CC) -c packet.c $(INC)
pl2301.o:
	$(CC) -c pl2301.c $(INC)
npsh.o:
	$(CC) -c npsh.c $(INC)
npsh: $(NPSH_OBJ)
	$(CC) $(NPSH_OBJ) -o $@ $(INC) $(NPLIBS)

zip:
	@cd ..;\
	$(TAR) loadclients.tar ${FILES};\
    $(ZIP) loadclients.tar
	@echo "../loadclients.tar.gz"

clean:
	$(RM) *.o *.so

docs:
	doxygen doxy.conf
