#ifndef UVC_SMART_UVC_H
#define UVC_SMART_UVC_H

#include "uvc_smart_types.h"

/* ---------------------------------------------------------------------------
 * UVC device open / close
 * ---------------------------------------------------------------------------
 */
static int uvc_open(struct uvc_device **uvc, char *devname)
{
    struct uvc_device *dev;
    struct v4l2_capability cap;
    int fd, ret = -EINVAL;

    fd = open(devname, O_RDWR | O_NONBLOCK);
    if (fd == -1) {
        printf("UVC: device open failed: %s (%d).\n", strerror(errno), errno);
        return ret;
    }

    ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
    if (ret < 0) {
        printf("UVC: unable to query device: %s (%d)\n", strerror(errno), errno);
        goto err;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
        printf("UVC: %s is no video output device\n", devname);
        goto err;
    }

    dev = calloc(1, sizeof *dev);
    if (dev == NULL) {
        ret = -ENOMEM;
        goto err;
    }

    printf("UVC device is %s on bus %s\n", cap.card, cap.bus_info);
    printf("UVC open succeeded, fd = %d\n", fd);

    dev->uvc_fd = fd;
    *uvc = dev;
    return 0;

err:
    close(fd);
    return ret;
}

static void uvc_close(struct uvc_device *dev)
{
    close(dev->uvc_fd);
    free(dev);
}

/* ---------------------------------------------------------------------------
 * UVC streaming control
 * ---------------------------------------------------------------------------
 */
static int uvc_video_stream(struct uvc_device *dev, int enable)
{
    int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    int ret;

    if (!enable) {
        ret = ioctl(dev->uvc_fd, VIDIOC_STREAMOFF, &type);
        if (ret < 0) {
            printf("UVC: VIDIOC_STREAMOFF failed: %s (%d).\n", strerror(errno), errno);
            return ret;
        }
        printf("UVC: Stopping video stream.\n");
        return 0;
    }

    ret = ioctl(dev->uvc_fd, VIDIOC_STREAMON, &type);
    if (ret < 0) {
        printf("UVC: Unable to start streaming %s (%d).\n", strerror(errno), errno);
        return ret;
    }

    printf("UVC: Starting video stream.\n");
    dev->uvc_shutdown_requested = 0;
    return 0;
}

/* ---------------------------------------------------------------------------
 * UVC buffer management (MMAP only — simpler and sufficient)
 * ---------------------------------------------------------------------------
 */
static int uvc_uninit_device(struct uvc_device *dev)
{
    unsigned int i;

    if (!dev->mem)
        return 0;

    for (i = 0; i < dev->nbufs; ++i) {
        if (dev->mem[i].start && dev->mem[i].start != MAP_FAILED)
            munmap(dev->mem[i].start, dev->mem[i].length);
    }
    free(dev->mem);
    dev->mem = NULL;
    return 0;
}

static int uvc_video_reqbufs(struct uvc_device *dev, int nbufs)
{
    struct v4l2_requestbuffers rb;
    unsigned int i;
    int ret;

    CLEAR(rb);
    rb.count = nbufs;
    rb.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    rb.memory = V4L2_MEMORY_MMAP;

    ret = ioctl(dev->uvc_fd, VIDIOC_REQBUFS, &rb);
    if (ret < 0) {
        printf("UVC: VIDIOC_REQBUFS error %s (%d).\n", strerror(errno), errno);
        return ret;
    }

    if (!rb.count)
        return 0;

    if (rb.count < 2) {
        printf("UVC: Insufficient buffer memory.\n");
        return -EINVAL;
    }

    dev->mem = calloc(rb.count, sizeof dev->mem[0]);
    if (!dev->mem) {
        printf("UVC: Out of memory\n");
        return -ENOMEM;
    }

    for (i = 0; i < rb.count; ++i) {
        memset(&dev->mem[i].buf, 0, sizeof(dev->mem[i].buf));
        dev->mem[i].buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        dev->mem[i].buf.memory = V4L2_MEMORY_MMAP;
        dev->mem[i].buf.index = i;

        ret = ioctl(dev->uvc_fd, VIDIOC_QUERYBUF, &(dev->mem[i].buf));
        if (ret < 0) {
            printf("UVC: VIDIOC_QUERYBUF failed for buf %d: %s (%d).\n",
                   i, strerror(errno), errno);
            free(dev->mem);
            dev->mem = NULL;
            return -EINVAL;
        }

        dev->mem[i].start = mmap(NULL, dev->mem[i].buf.length,
                                 PROT_READ | PROT_WRITE, MAP_SHARED,
                                 dev->uvc_fd, dev->mem[i].buf.m.offset);

        if (MAP_FAILED == dev->mem[i].start) {
            printf("UVC: Unable to map buffer %u: %s (%d).\n",
                   i, strerror(errno), errno);
            free(dev->mem);
            dev->mem = NULL;
            return -EINVAL;
        }

        dev->mem[i].length = dev->mem[i].buf.length;
        printf("UVC: Buffer %u mapped at %p, length %zu.\n",
               i, dev->mem[i].start, dev->mem[i].length);
    }

    dev->nbufs = rb.count;
    printf("UVC: %u buffers allocated.\n", rb.count);
    return 0;
}

static int uvc_video_qbuf(struct uvc_device *dev)
{
    unsigned int i;
    int ret;

    for (i = 0; i < dev->nbufs; ++i) {
        memset(&dev->mem[i].buf, 0, sizeof(dev->mem[i].buf));
        dev->mem[i].buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        dev->mem[i].buf.memory = V4L2_MEMORY_MMAP;
        dev->mem[i].buf.index = i;

        /* Queue with zero bytesused — first real frames come from GStreamer */
        dev->mem[i].buf.bytesused = 0;

        ret = ioctl(dev->uvc_fd, VIDIOC_QBUF, &(dev->mem[i].buf));
        if (ret < 0) {
            printf("UVC: VIDIOC_QBUF failed: %s (%d).\n", strerror(errno), errno);
            return ret;
        }
        dev->qbuf_count++;
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * UVC video process — pulls a frame from GStreamer appsink, copies to UVC buf
 * ---------------------------------------------------------------------------
 */
static int uvc_video_process(struct uvc_device *dev)
{
    struct v4l2_buffer ubuf;
    int ret;

    if (!dev->is_streaming)
        return 0;

    /* Dequeue a spent buffer from UVC */
    CLEAR(ubuf);
    ubuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ubuf.memory = V4L2_MEMORY_MMAP;

    ret = ioctl(dev->uvc_fd, VIDIOC_DQBUF, &ubuf);
    if (ret < 0)
        return ret;

    dev->dqbuf_count++;

    /* Pull a frame from GStreamer appsink */
    if (dev->appsink) {
        GstSample *sample = gst_app_sink_try_pull_sample(
            GST_APP_SINK(dev->appsink), 100 * GST_MSECOND);

        if (sample) {
            GstBuffer *gstbuf = gst_sample_get_buffer(sample);
            GstMapInfo map;

            if (gst_buffer_map(gstbuf, &map, GST_MAP_READ)) {
                size_t copy_size = MIN(map.size, dev->mem[ubuf.index].length);
                memcpy(dev->mem[ubuf.index].start, map.data, copy_size);
                ubuf.bytesused = copy_size;
                gst_buffer_unmap(gstbuf, &map);
            }
            gst_sample_unref(sample);
        } else {
            /* No frame ready — send minimal data */
            ubuf.bytesused = 0;
        }
    } else {
        ubuf.bytesused = 0;
    }

    /* Re-queue the buffer */
    ret = ioctl(dev->uvc_fd, VIDIOC_QBUF, &ubuf);
    if (ret < 0) {
        if (errno == ENODEV) {
            dev->uvc_shutdown_requested = 1;
            printf("UVC: Possible USB shutdown during VIDIOC_QBUF\n");
            return 0;
        }
        return ret;
    }

    dev->qbuf_count++;

    if (!dev->first_buffer_queued) {
        uvc_video_stream(dev, 1);
        dev->first_buffer_queued = 1;
    }

    return 0;
}

#endif /* UVC_SMART_UVC_H */
