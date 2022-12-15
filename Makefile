CC = gcc
C_OBJ = src/impl/atomui_drm.c
MAIN = src/main.c
TARGET = atomui
FLAGS = -O3 -Wno-int-to-pointer-cast

all:
	$(CC) $(MAIN) $(FLAGS) -o $(TARGET) $(C_OBJ)


clean:
	rm $(TARGET)