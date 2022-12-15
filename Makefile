CC = gcc
C_OBJ = src/impl/atomos_drm.c
MAIN = src/main.c
TARGET = atomui
FLAGS = -O3

all:
	$(CC) $(MAIN) $(FLAGS) -o $(TARGET) $(C_OBJ)


clean:
	rm $(TARGET)