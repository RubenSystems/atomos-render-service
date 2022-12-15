CC = gcc
C_OBJ = src/impl/atomui_drm.c src/impl/atomui_graphics.c
MAIN = src/main.c 
TARGET = atomui
FLAGS = -Wno-int-to-pointer-cast

all:
	$(CC) $(MAIN) $(FLAGS) -o $(TARGET) $(C_OBJ)


clean:
	rm $(TARGET)