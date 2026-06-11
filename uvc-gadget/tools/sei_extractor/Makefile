CC = gcc
CFLAGS = -O2 -Wall $(shell pkg-config --cflags gstreamer-1.0 gstreamer-app-1.0) -I../../include
LDFLAGS = $(shell pkg-config --libs gstreamer-1.0 gstreamer-app-1.0)

all: uvc_sei_extractor

uvc_sei_extractor: uvc_sei_extractor.c ../../include/sei_metadata.h
	$(CC) $(CFLAGS) -o uvc_sei_extractor uvc_sei_extractor.c $(LDFLAGS) -lm

clean:
	rm -f uvc_sei_extractor
