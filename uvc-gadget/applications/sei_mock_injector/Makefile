CC = gcc
CFLAGS = -O2 -Wall $(shell pkg-config --cflags gstreamer-1.0 gstreamer-app-1.0) -I../../include
LDFLAGS = $(shell pkg-config --libs gstreamer-1.0 gstreamer-app-1.0)

all: uvc_gadget_sei

uvc_gadget_sei: uvc_gadget_sei.c uvc_sei_types.h uvc_sei_uvc.h uvc_sei_gst.h uvc_sei_events.h uvc_sei_injection.h
	$(CC) $(CFLAGS) -o uvc_gadget_sei uvc_gadget_sei.c $(LDFLAGS)

clean:
	rm -f uvc_gadget_sei
