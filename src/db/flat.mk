DB_O=\
db/database.o\
db/alloc.o\
db/rc.o\
db/thumbnails.o
DB_H=\
#db/db.h\
db/alloc.h\
db/thumbnails.h\
db\database.h\
db/stringpool.h
DB_CFLAGS=$(shell pkg-config --cflags sqlite3) $(shell pkg-config --cflags zlib)
DB_LDFLAGS=$(shell pkg-config --libs sqlite3) $(shell pkg-config --libs zlib)
