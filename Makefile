#CC=gcc -O2 -Wall -g 
CC=gcc 
CFLAGS= -Wall -O2 -march=i686

ARCH=/ice/mod/cluster
APP=cluster_send cluster_recv safe_cluster sync_conf.sh
BIN_ARCH=/ice/bin
BIN_APP=send_arp

INCLUDE=-Iinclude
LIB= ../lib/libifcommon.a

###################################################

OBJ = 
OBJ1 = cluster_utils.o

SAFE_CLUSTER_OBJ:= safe_cluster.o $(OBJ) $(OBJ1)
CLUSTER_SEND_OBJ:= cluster_send.o $(OBJ) $(OBJ1)
CLUSTER_RECV_OBJ:= cluster_recv.o $(OBJ) $(OBJ1)

all:  safe_cluster cluster_send cluster_recv send_arp Strip  

.c.o:
	$(CC) $(INCLUDE) $(CFLAGS) -c -o $*.o $< 

safe_cluster: local.h $(SAFE_CLUSTER_OBJ)
	$(CC) $(INCLUDE) $(CFLAGS) -o $@ $(SAFE_CLUSTER_OBJ) $(LIB)

cluster_send: local.h $(CLUSTER_SEND_OBJ)
	$(CC) $(INCLUDE) $(CFLAGS) -o $@ $(CLUSTER_SEND_OBJ) $(LIB)

cluster_recv: local.h $(CLUSTER_RECV_OBJ)
	$(CC) $(INCLUDE) $(CFLAGS) -o $@ $(CLUSTER_RECV_OBJ) $(LIB)

send_arp: send_arp.o
	$(CC) $(INCLUDE) $(CFLAGS) -o $@ send_arp.o

clean:
	rm -fr *.o
	rm -fr cluster_send cluster_recv safe_cluster send_arp

Strip:
	strip cluster_send cluster_recv

install:
	test -d $(ARCH) || mkdir -p $(ARCH)
	@cp -vfr $(APP) $(ARCH)
	@cp -vfr $(APP) /dist
	@cp -va cluster_stop $(ARCH)
	test -d $(BIN_ARCH) || mkdir -p $(BIN_ARCH)
	@cp -vfr $(BIN_APP) $(BIN_ARCH)


test:	a.c 
	$(CC) $(CFLAGS) -o $@ a.c 
	cp test /dist
