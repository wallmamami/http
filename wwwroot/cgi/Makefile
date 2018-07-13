ROOT=$(shell pwd)
INCLUDE=-I$(ROOT)/../../lib/include
LIB=-L$(ROOT)/../../lib/lib
LIB_NAME=-lmysqlclient

.PHONY:all

all:Search insertData selectData

Search:Search.c
		gcc -o $@ $^
insertData:insertData.c
		gcc -o $@ $^ $(INCLUDE) $(LIB) $(LIB_NAME)
selectData:selectData.c
		gcc -o $@ $^ $(INCLUDE) $(LIB) $(LIB_NAME)

.PHONY:clean

clean:
	rm -f Search insertData selectData
