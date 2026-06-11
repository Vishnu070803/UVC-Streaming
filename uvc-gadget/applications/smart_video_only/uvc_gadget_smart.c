/*
 * UVC Gadget Smart Application
 *
 * Embeds GStreamer to dynamically build capture/encode pipelines
 * based on USB host format negotiation (H264, H265, YUYV, MJPEG).
 *
 * Camera is only acquired on STREAMON and released on STREAMOFF.
 *
 * Phase 1: Multi-format streaming
 * Phase 2: SEI metadata injection (future)
 *
 * Copyright (C) 2026
 * Based on uvc-gadget by Laurent Pinchart (Ideas on board SPRL)
 */

#include "uvc_smart_types.h"
#include "uvc_smart_uvc.h"
#include "uvc_smart_gst.h"
#include "uvc_smart_events.h"

/* Global flag for clean shutdown */
static volatile int g_running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

static void usage(const char *argv0)
{
    fprintf(stderr, "Usage: %s [options]\n", argv0);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -u device   UVC gadget device (e.g. /dev/video6)\n");
    fprintf(stderr, "  -c device   Camera capture device (default: /dev/video4)\n");
    fprintf(stderr, "  -s speed    USB speed: 0=FS, 1=HS, 2=SS (default: 2)\n");
    fprintf(stderr, "  -b          Use bulk transfer mode\n");
    fprintf(stderr, "  -e type     Encoder type: 1 = vpuenc, 2 = v4l2enc (default: 2)\n");
    fprintf(stderr, "  -n nbufs    Number of UVC buffers (2-32, default: 2)\n");
    fprintf(stderr, "  -h          Show this help\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Format is automatically selected by the USB host.\n");
    fprintf(stderr, "Supported: YUYV, MJPEG, H.264, H.265/HEVC\n");
}

int main(int argc, char *argv[])
{
    struct uvc_device *udev = NULL;
    struct timeval tv;
    char *uvc_devname = "/dev/video6";
    char *camera_dev = "/dev/video4";
    int ret, opt;
    int bulk_mode = 0;
    int nbufs = 2;
    enum usb_device_speed speed = USB_SPEED_SUPER;

    int encoder_type = 2; /* Default to v4l2enc */

    /* Parse command line */
    while ((opt = getopt(argc, argv, "u:c:s:be:n:h")) != -1) {
        switch (opt) {
        case 'u':
            uvc_devname = optarg;
            break;
        case 'c':
            camera_dev = optarg;
            break;
        case 's':
            if (atoi(optarg) < 0 || atoi(optarg) > 2) {
                usage(argv[0]);
                return 1;
            }
            speed = atoi(optarg);
            break;
        case 'b':
            bulk_mode = 1;
            break;
        case 'e':
            encoder_type = atoi(optarg);
            if (encoder_type != 1 && encoder_type != 2) {
                fprintf(stderr, "Error: encoder_type must be 1 or 2\n");
                return 1;
            }
            break;
        case 'n':
            nbufs = atoi(optarg);
            if (nbufs < 2 || nbufs > 32) {
                fprintf(stderr, "Error: nbufs must be 2-32\n");
                return 1;
            }
            break;
        case 'h':
            usage(argv[0]);
            return 0;
        default:
            usage(argv[0]);
            return 1;
        }
    }

    /* Initialize GStreamer */
    gst_init(&argc, &argv);

    /* Install signal handlers for clean shutdown */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Open the UVC gadget device */
    ret = uvc_open(&udev, uvc_devname);
    if (udev == NULL || ret < 0) {
        fprintf(stderr, "Failed to open UVC device %s\n", uvc_devname);
        return 1;
    }

    udev->uvc_devname = uvc_devname;
    udev->camera_dev = camera_dev;

    /* Set default format — host will override via PROBE/COMMIT */
    udev->width = 1920;
    udev->height = 1080;
    udev->fcc = V4L2_PIX_FMT_H264;
    udev->imgsize = 2 * 1024 * 1024;   /* 2MB default for compressed */
    udev->encoder_type = encoder_type;
    udev->bulk = bulk_mode;
    udev->nbufs = nbufs;
    udev->speed = speed;

    /* Set max packet size based on USB speed */
    switch (speed) {
    case USB_SPEED_FULL:
        udev->maxpkt = bulk_mode ? 64 : 1023;
        break;
    case USB_SPEED_HIGH:
        udev->maxpkt = bulk_mode ? 512 : 1024;
        break;
    case USB_SPEED_SUPER:
    default:
        udev->maxpkt = 1024;
        break;
    }

    /* Subscribe to UVC events */
    uvc_events_init(udev);

    printf("\n");
    printf("========================================\n");
    printf("  UVC Gadget Smart — Phase 1\n");
    printf("========================================\n");
    printf("  UVC device : %s\n", uvc_devname);
    printf("  Camera     : %s\n", camera_dev);
    printf("  USB speed  : %s\n",
           speed == USB_SPEED_SUPER ? "SuperSpeed" :
           speed == USB_SPEED_HIGH  ? "HighSpeed" : "FullSpeed");
    printf("  Encoder    : %s\n", encoder_type == 1 ? "vpuenc_*" : "v4l2*enc");
    printf("  Formats    : YUYV, MJPEG, H264, HEVC\n");
    printf("========================================\n");
    printf("  Waiting for USB host...\n\n");

    /* ---------------------------------------------------------------
     * Main event loop
     * --------------------------------------------------------------- */
    while (g_running) {
        fd_set fdsu, efds, dfds;

        FD_ZERO(&fdsu);
        FD_SET(udev->uvc_fd, &fdsu);
        efds = fdsu;    /* exception (UVC events) */
        dfds = fdsu;    /* writable (UVC buffer ready) */

        tv.tv_sec = 0;
        tv.tv_usec = 100000;   /* 100ms timeout for bus checks */

        ret = select(udev->uvc_fd + 1, NULL, &dfds, &efds, &tv);

        if (ret == -1) {
            if (errno == EINTR)
                continue;
            printf("select error: %s (%d)\n", strerror(errno), errno);
            break;
        }

        /* Timeout — check GStreamer bus for errors */
        if (ret == 0) {
            gst_check_bus(udev);
            continue;
        }

        /* UVC event (SETUP, DATA, STREAMON, STREAMOFF) */
        if (FD_ISSET(udev->uvc_fd, &efds))
            uvc_events_process(udev);

        /* UVC buffer ready — pull from GStreamer and push to USB */
        if (FD_ISSET(udev->uvc_fd, &dfds))
            uvc_video_process(udev);

        /* Check GStreamer bus */
        gst_check_bus(udev);

        /* Handle USB disconnect */
        if (udev->uvc_shutdown_requested) {
            printf("USB shutdown requested — cleaning up.\n");
            uvc_handle_streamoff_event(udev);
            udev->uvc_shutdown_requested = 0;
        }
    }

    /* Cleanup */
    printf("\nShutting down...\n");

    if (udev->is_streaming)
        uvc_handle_streamoff_event(udev);

    uvc_close(udev);
    gst_deinit();

    printf("Done.\n");
    return 0;
}
