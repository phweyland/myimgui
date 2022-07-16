CORE_O=core/log.o \
       core/threads.o \
			 core/qvk_util.o
CORE_H=core/core.h \
       core/log.h \
       core/threads.h \
			 core/signal.h\
			 core/qvk_util.h
CORE_CFLAGS=
CORE_LDFLAGS=-pthread
