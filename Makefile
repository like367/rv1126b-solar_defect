CC = aarch64-linux-gnu-gcc
CFLAGS = -I/usr/include -I/usr/include/libdrm
LDFLAGS = -L/usr/lib -L/usr/lib/aarch64-linux-gnu
LIBS = -lrknnrt -lrga -ldrm -ldl -lm -lpthread -ljpeg
TARGET = solar_defect
SRCS = solar_defect.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(SRCS) -o $(TARGET) $(CFLAGS) $(LDFLAGS) $(LIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
