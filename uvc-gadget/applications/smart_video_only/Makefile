CC = gcc
CFLAGS = -O2 -Wall $(shell pkg-config --cflags gstreamer-1.0 gstreamer-app-1.0) -I../../include
LDFLAGS = $(shell pkg-config --libs gstreamer-1.0 gstreamer-app-1.0)

all: uvc_gadget_smart

uvc_gadget_smart: uvc_gadget_smart.c uvc_smart_types.h uvc_smart_uvc.h uvc_smart_gst.h uvc_smart_events.h
	$(CC) $(CFLAGS) -o $@ uvc_gadget_smart.c $(LDFLAGS)

clean:
	rm -f uvc_gadget_smart
