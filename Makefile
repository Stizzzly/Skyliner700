CC = gcc
CFLAGS = -O2 -Wall -Wextra -static -mwindows
LIBS = -ld3d9 -ld3dx9 -lgdi32 -luser32 -lkernel32 -lm

SRCS = main.c \
       core/window.c \
       render/d3d9/d3d9_device.c \
       render/d3d9/d3d9_matrix.c \
       render/d3d9/d3d9_mesh.c \
       render/renderer.h \
       model/plane.c

TARGET = SkyLiner700.exe

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LIBS)

clean:
	del $(TARGET) 2>nul || echo "No exe to clean"

run: $(TARGET)
	$(TARGET)

.PHONY: clean run
