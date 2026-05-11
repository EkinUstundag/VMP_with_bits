CC = g++
TARGET = main
BIT_OP = bit_operations.cpp
ALGO = algorithm.cpp
INPUT ?= VMP_A100.vmp
OUTPUT ?= output.txt

all:
	$(CC) $(BIT_OP) -o $(TARGET) -std=c++17 -lstdc++fs
run:
	./$(TARGET) $(INPUT)