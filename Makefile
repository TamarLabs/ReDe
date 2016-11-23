#set environment variable RM_INCLUDE_DIR to the location of redismodule.h
ifndef RM_INCLUDE_DIR
	RM_INCLUDE_DIR=./
endif

ifndef RMUTIL_LIBDIR
	RMUTIL_LIBDIR=rmutil
endif

all: module.so

module.so:
	$(MAKE) -C ./src
	cp ./src/module.so .

clean: FORCE
	rm -rf *.xo *.so *.o
	rm -rf ./src/*.xo ./src/*.so ./src/*.o
	rm -rf ./$(RMUTIL_LIBDIR)/*.so ./$(RMUTIL_LIBDIR)/*.o ./$(RMUTIL_LIBDIR)/*.a

FORCE:
