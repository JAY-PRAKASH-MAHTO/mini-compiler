CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c11

COMMON_SOURCES = main.c diagnostics.c lexerf.c parserf.c semanticf.c codegeneratorf.c
WINDOWS_SOURCES = $(COMMON_SOURCES) editor_win.c

ifeq ($(OS),Windows_NT)
TARGET = build/bhasacore.exe
SOURCES = $(WINDOWS_SOURCES)
LDLIBS = -luser32 -lgdi32 -lcomdlg32
else
TARGET = build/bhasacore
SOURCES = $(COMMON_SOURCES)
LDLIBS =
endif

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET) $(LDLIBS)

clean:
	rm -rf build
