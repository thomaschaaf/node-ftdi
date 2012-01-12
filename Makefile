CC = g++
FLAGS = -lftdi -lusb

FTDI_CC = build/ftdi.o

LIB_DIR = lib
BUILD_DIR = build


build_dir:
	mkdir -p $(BUILD_DIR)

ftdi.o: build_dir
	$(CC) $(FLAGS) $(LIB_DIR)/ftdi.cpp -shared -o $(FTDI_CC)

find_all: ftdi.o
	$(CC) $(FLAGS) src/find_all.cc $(FTDI_CC) -I $(LIB_DIR) -o find_all

clean:
	@rm -rf find_all $(BUILD_DIR)