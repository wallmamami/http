ROOT=$(shell pwd)
INCLUDE=-I$(ROOT)/../../lib/include
LIB=-L$(ROOT)/../../lib/lib
LIB_NAME=-lmysqlclient

.PHONY:all

all:insertData selectData

insertData:insertData.c
		gcc -o $@ $^ $(INCLUDE) $(LIB) $(LIB_NAME)
selectData:selectData.c
		gcc -o $@ $^ $(INCLUDE) $(LIB) $(LIB_NAME)

.PHONY:clean

clean:
	rm -f insertData selectData
