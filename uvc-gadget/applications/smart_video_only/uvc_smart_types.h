#ifndef UVC_SMART_TYPES_H
#define UVC_SMART_TYPES_H

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/usb/ch9.h>
#include <linux/usb/video.h>
#include <linux/videodev2.h>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include "uvc.h"

/* Macros */
#define CLEAR(x)    memset(&(x), 0, sizeof(x))
#define max(a, b)   (((a) > (b)) ? (a) : (b))
#define MIN(a, b)   (((a) < (b)) ? (a) : (b))
#define ARRAY_SIZE(a) ((sizeof(a) / sizeof(a[0])))
#define pixfmtstr(x) (x) & 0xff, ((x) >> 8) & 0xff, ((x) >> 16) & 0xff, ((x) >> 24) & 0xff

#define clamp(val, min, max) ({         \
    typeof(val) __val = (val);          \
    typeof(min) __min = (min);          \
    typeof(max) __max = (max);          \
    __val = __val < __min ? __min : __val; \
    __val > __max ? __max : __val;      \
})

/* Processing Unit brightness defaults */
#define PU_BRIGHTNESS_MIN_VAL       0
#define PU_BRIGHTNESS_MAX_VAL       255
#define PU_BRIGHTNESS_STEP_SIZE     1
#define PU_BRIGHTNESS_DEFAULT_VAL   127

/* Buffer representing one video frame */
struct buffer {
    struct v4l2_buffer buf;
    void *start;
    size_t length;
};

/* IO methods */
enum io_method {
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR,
};

/* ---------------------------------------------------------------------------
 * Format / Frame tables
 * Order MUST match the symlink order in usb_uvc_all.sh:
 *   1 = uncompressed (YUYV)
 *   2 = mjpeg
 *   3 = framebased h264
 *   4 = framebased hevc
 * ---------------------------------------------------------------------------
 */
struct uvc_frame_info {
    unsigned int width;
    unsigned int height;
    unsigned int intervals[16];
};

struct uvc_format_info {
    unsigned int fcc;
    const struct uvc_frame_info *frames;
};

/* All intervals requested by user */
#define INTERVALS { 166666, 181818, 200000, 222222, 250000, 285714, 333333, 357142, 400000, 416666, 500000, 666666, 1000000, 2000000, 0 }

static const struct uvc_frame_info uvc_frames_yuyv[] = {
    {  256,  144, INTERVALS },
    {  426,  240, INTERVALS },
    {  640,  360, INTERVALS },
    {  854,  480, INTERVALS },
    { 1280,  720, INTERVALS },
    { 1920, 1080, INTERVALS },
    { 0, 0, { 0 } },
};

static const struct uvc_frame_info uvc_frames_mjpeg[] = {
    {  256,  144, INTERVALS },
    {  426,  240, INTERVALS },
    {  640,  360, INTERVALS },
    {  854,  480, INTERVALS },
    { 1280,  720, INTERVALS },
    { 1920, 1080, INTERVALS },
    { 0, 0, { 0 } },
};

static const struct uvc_frame_info uvc_frames_h264[] = {
    {  256,  144, INTERVALS },
    {  426,  240, INTERVALS },
    {  640,  360, INTERVALS },
    {  854,  480, INTERVALS },
    { 1280,  720, INTERVALS },
    { 1920, 1080, INTERVALS },
    { 0, 0, { 0 } },
};

static const struct uvc_frame_info uvc_frames_hevc[] = {
    {  256,  144, INTERVALS },
    {  426,  240, INTERVALS },
    {  640,  360, INTERVALS },
    {  854,  480, INTERVALS },
    { 1280,  720, INTERVALS },
    { 1920, 1080, INTERVALS },
    { 0, 0, { 0 } },
};

static const struct uvc_format_info uvc_formats[] = {
    { V4L2_PIX_FMT_YUYV,  uvc_frames_yuyv  },   /* index 1 */
    { V4L2_PIX_FMT_MJPEG, uvc_frames_mjpeg },   /* index 2 */
    { V4L2_PIX_FMT_H264,  uvc_frames_h264  },   /* index 3 */
    { V4L2_PIX_FMT_HEVC,  uvc_frames_hevc  },   /* index 4 */
};

/* ---------------------------------------------------------------------------
 * UVC device structure (with GStreamer fields)
 * ---------------------------------------------------------------------------
 */
struct uvc_device {
    /* UVC device */
    int uvc_fd;
    int is_streaming;
    char *uvc_devname;

    /* UVC control request */
    struct uvc_streaming_control probe;
    struct uvc_streaming_control commit;
    int control;
    struct uvc_request_data request_error_code;
    unsigned int brightness_val;

    /* UVC buffers (MMAP) */
    struct buffer *mem;
    unsigned int nbufs;
    unsigned int fcc;
    unsigned int width;
    unsigned int height;
    unsigned int imgsize;

    /* USB speed */
    int bulk;
    int mult;
    int burst;
    int maxpkt;
    enum usb_device_speed speed;

    /* UVC flags */
    int first_buffer_queued;
    int uvc_shutdown_requested;
    unsigned long long int qbuf_count;
    unsigned long int dqbuf_count;

    /* GStreamer pipeline */
    GstElement *pipeline;
    GstElement *appsink;

    /* Camera device path */
    char *camera_dev;

    /* Encoder type: 1 = vpuenc_*, 2 = v4l2*enc */
    int encoder_type;
};

/* Forward declarations */
static int uvc_video_stream(struct uvc_device *dev, int enable);

/* Helper: format name for logging */
static const char *fcc_to_name(unsigned int fcc)
{
    switch (fcc) {
    case V4L2_PIX_FMT_YUYV:  return "YUYV";
    case V4L2_PIX_FMT_MJPEG: return "MJPEG";
    case V4L2_PIX_FMT_H264:  return "H264";
    case V4L2_PIX_FMT_HEVC:  return "HEVC";
    default:                  return "UNKNOWN";
    }
}

#endif /* UVC_SMART_TYPES_H */
