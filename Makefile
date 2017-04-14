CC = g++

BASE_FLAGS = -dynamiclib -fPIC

# INCLUDE BASE DIRECTORY AND BOOST DIRECTORY FOR HEADERS
# LDFLAGS = -I /usr/local/opt/boost/include -I /usr/local/opt/openssl/include

# INCLUDE BASE DIRECTORY AND BOOST DIRECTORY FOR LIB FILES
# LLIBFLAGS = -L /usr/local/opt/boost/lib -L /usr/local/opt/openssl/lib

# SPECIFIY LINK OPTIONS
# LINKFLAGS =  -lssl -lcrypto


# FINAL FLAGS -- TO BE USED THROUGHOUT
FLAGS = $(BASE_FLAGS) $(LLIBFLAGS) $(LDFLAGS) $(LINKFLAGS)

# NOTE FOR BOOST -- YOU ONLY NEED TO INCLUDE THE PATH BECAUSE IT ONLY INSTALLS HEADER FILES
main: sqltorrent.cpp
	$(CC) $(FLAGS) libboost_system.dylib libboost_thread.dylib libsqlite3.dylib libtorrent-rasterbar.9.dylib -o sqltorrent.dylib sqltorrent.cpp
