OBJ:=mqtt

FLAG=-g -lpaho-mqtt3c -lrt 
SQLITE=-lsqlite3

LIBS+= -L./paholib
LIBS+= -L./openssllib
LIBS+= -L./sqlitelib

INC+=-I./include

cc:=arm-linux-gnueabihf-gcc

OBJS+=log.c
OBJS+=main.c
OBJS+=framebuff.c
OBJS+=font_8x16.c
OBJS+=font_16x16.c
OBJS+=mailbox.c
OBJS+=mqtt.c
OBJS+=storage.c

$(OBJ):$(OBJS)
	$(cc) $^ -o $@ -lpthread $(FLAG) $(SQLITE) $(LIBS) $(INC) -lssl -lcrypto -g
	cp $(OBJ) ~/nfs/rootfs/mqtt_test/
.PHONY:

clean:
	rm $(OBJ)

disclean:
	rm $(OBJ)
	rm ~/nfs/rootfs/mqtt_test/$(OBJ)
               
            