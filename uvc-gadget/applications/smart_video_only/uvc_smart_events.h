#ifndef UVC_SMART_EVENTS_H
#define UVC_SMART_EVENTS_H

#include "uvc_smart_types.h"
#include "uvc_smart_uvc.h"
#include "uvc_smart_gst.h"

/* ---------------------------------------------------------------------------
 * UVC Request processing
 * ---------------------------------------------------------------------------
 */

static void
uvc_fill_streaming_control(struct uvc_device *dev,
                            struct uvc_streaming_control *ctrl,
                            int iframe, int iformat)
{
    const struct uvc_format_info *format;
    const struct uvc_frame_info *frame;
    unsigned int nframes;

    if (iformat < 0)
        iformat = ARRAY_SIZE(uvc_formats) + iformat;
    if (iformat < 0 || iformat >= (int)ARRAY_SIZE(uvc_formats))
        return;
    format = &uvc_formats[iformat];

    nframes = 0;
    while (format->frames[nframes].width != 0)
        ++nframes;

    if (iframe < 0)
        iframe = nframes + iframe;
    if (iframe < 0 || iframe >= (int)nframes)
        return;
    frame = &format->frames[iframe];

    memset(ctrl, 0, sizeof *ctrl);

    ctrl->bmHint = 1;
    ctrl->bFormatIndex = iformat + 1;
    ctrl->bFrameIndex = iframe + 1;
    ctrl->dwFrameInterval = frame->intervals[0];

    switch (format->fcc) {
    case V4L2_PIX_FMT_YUYV:
        ctrl->dwMaxVideoFrameSize = frame->width * frame->height * 2;
        break;
    case V4L2_PIX_FMT_MJPEG:
    case V4L2_PIX_FMT_H264:
    case V4L2_PIX_FMT_HEVC:
        ctrl->dwMaxVideoFrameSize = dev->imgsize;
        break;
    }

    if (!dev->bulk)
        ctrl->dwMaxPayloadTransferSize = dev->maxpkt * (dev->mult + 1) * (dev->burst + 1);
    else
        ctrl->dwMaxPayloadTransferSize = ctrl->dwMaxVideoFrameSize;

    ctrl->bmFramingInfo = 3;
    ctrl->bPreferedVersion = 1;
    ctrl->bMaxVersion = 1;
}

/* Handle STREAMON — build GStreamer pipeline and start UVC streaming */
static int uvc_handle_streamon_event(struct uvc_device *dev)
{
    int ret;

    printf("\n=== STREAMON: Host requested %s %ux%u ===\n",
           fcc_to_name(dev->fcc), dev->width, dev->height);

    /* 1. Build GStreamer pipeline for the negotiated format */
    ret = gst_pipeline_build(dev);
    if (ret < 0) {
        printf("Failed to build GStreamer pipeline!\n");
        return ret;
    }

    /* 2. Allocate UVC buffers */
    ret = uvc_video_reqbufs(dev, dev->nbufs);
    if (ret < 0) {
        gst_pipeline_destroy(dev);
        return ret;
    }

    /* 3. Queue initial buffers */
    ret = uvc_video_qbuf(dev);
    if (ret < 0) {
        gst_pipeline_destroy(dev);
        return ret;
    }

    /* 4. Start UVC streaming */
    uvc_video_stream(dev, 1);
    dev->first_buffer_queued = 1;
    dev->is_streaming = 1;

    printf("=== Streaming started ===\n\n");
    return 0;
}

/* Handle STREAMOFF — destroy GStreamer pipeline and stop UVC */
static void uvc_handle_streamoff_event(struct uvc_device *dev)
{
    printf("\n=== STREAMOFF ===\n");

    /* Stop UVC streaming */
    if (dev->is_streaming) {
        uvc_video_stream(dev, 0);
        uvc_uninit_device(dev);
        uvc_video_reqbufs(dev, 0);
        dev->is_streaming = 0;
        dev->first_buffer_queued = 0;
        dev->qbuf_count = 0;
        dev->dqbuf_count = 0;
    }

    /* Destroy GStreamer pipeline — releases camera */
    gst_pipeline_destroy(dev);

    printf("=== Stream stopped, camera released ===\n\n");
}

/* ---------------------------------------------------------------------------
 * UVC control event handlers (from original, simplified)
 * ---------------------------------------------------------------------------
 */

static void
uvc_events_process_standard(struct uvc_device *dev,
                             struct usb_ctrlrequest *ctrl,
                             struct uvc_request_data *resp)
{
    (void)dev; (void)ctrl; (void)resp;
}

static void
uvc_events_process_control(struct uvc_device *dev, uint8_t req,
                            uint8_t cs, uint8_t entity_id,
                            uint8_t len, struct uvc_request_data *resp)
{
    (void)len;

    switch (entity_id) {
    case 2: /* Processing Unit */
        switch (cs) {
        case UVC_PU_BRIGHTNESS_CONTROL:
            switch (req) {
            case UVC_SET_CUR:
                resp->data[0] = 0x0;
                resp->length = len;
                dev->request_error_code.data[0] = 0x00;
                dev->request_error_code.length = 1;
                break;
            case UVC_GET_MIN:
                resp->data[0] = PU_BRIGHTNESS_MIN_VAL;
                resp->length = 2;
                dev->request_error_code.data[0] = 0x00;
                dev->request_error_code.length = 1;
                break;
            case UVC_GET_MAX:
                resp->data[0] = PU_BRIGHTNESS_MAX_VAL;
                resp->length = 2;
                dev->request_error_code.data[0] = 0x00;
                dev->request_error_code.length = 1;
                break;
            case UVC_GET_CUR:
                resp->length = 2;
                memcpy(&resp->data[0], &dev->brightness_val, resp->length);
                dev->request_error_code.data[0] = 0x00;
                dev->request_error_code.length = 1;
                break;
            case UVC_GET_INFO:
                resp->data[0] = 0x03;
                resp->length = 1;
                dev->request_error_code.data[0] = 0x00;
                dev->request_error_code.length = 1;
                break;
            case UVC_GET_DEF:
                resp->data[0] = PU_BRIGHTNESS_DEFAULT_VAL;
                resp->length = 2;
                dev->request_error_code.data[0] = 0x00;
                dev->request_error_code.length = 1;
                break;
            case UVC_GET_RES:
                resp->data[0] = PU_BRIGHTNESS_STEP_SIZE;
                resp->length = 2;
                dev->request_error_code.data[0] = 0x00;
                dev->request_error_code.length = 1;
                break;
            default:
                resp->length = -EL2HLT;
                dev->request_error_code.data[0] = 0x07;
                dev->request_error_code.length = 1;
                break;
            }
            break;

        default:
            resp->length = -EL2HLT;
            dev->request_error_code.data[0] = 0x06;
            dev->request_error_code.length = 1;
            break;
        }
        break;

    default:
        resp->length = -EL2HLT;
        dev->request_error_code.data[0] = 0x06;
        dev->request_error_code.length = 1;
        break;
    }

    printf("control request (req %02x cs %02x)\n", req, cs);
}

static void
uvc_events_process_streaming(struct uvc_device *dev, uint8_t req,
                              uint8_t cs, struct uvc_request_data *resp)
{
    struct uvc_streaming_control *ctrl;

    printf("streaming request (req %02x cs %02x)\n", req, cs);

    if (cs != UVC_VS_PROBE_CONTROL && cs != UVC_VS_COMMIT_CONTROL)
        return;

    ctrl = (struct uvc_streaming_control *)&resp->data;
    resp->length = sizeof *ctrl;

    switch (req) {
    case UVC_SET_CUR:
        dev->control = cs;
        resp->length = 34;
        break;
    case UVC_GET_CUR:
        if (cs == UVC_VS_PROBE_CONTROL)
            memcpy(ctrl, &dev->probe, sizeof *ctrl);
        else
            memcpy(ctrl, &dev->commit, sizeof *ctrl);
        break;
    case UVC_GET_MIN:
    case UVC_GET_MAX:
    case UVC_GET_DEF:
        uvc_fill_streaming_control(dev, ctrl,
            req == UVC_GET_MAX ? -1 : 0,
            req == UVC_GET_MAX ? -1 : 0);
        break;
    case UVC_GET_RES:
        CLEAR(ctrl);
        break;
    case UVC_GET_LEN:
        resp->data[0] = 0x00;
        resp->data[1] = 0x22;
        resp->length = 2;
        break;
    case UVC_GET_INFO:
        resp->data[0] = 0x03;
        resp->length = 1;
        break;
    }
}

static void
uvc_events_process_class(struct uvc_device *dev,
                          struct usb_ctrlrequest *ctrl,
                          struct uvc_request_data *resp)
{
    if ((ctrl->bRequestType & USB_RECIP_MASK) != USB_RECIP_INTERFACE)
        return;

    switch (ctrl->wIndex & 0xff) {
    case UVC_INTF_CONTROL:
        uvc_events_process_control(dev, ctrl->bRequest,
            ctrl->wValue >> 8, ctrl->wIndex >> 8,
            ctrl->wLength, resp);
        break;
    case UVC_INTF_STREAMING:
        uvc_events_process_streaming(dev, ctrl->bRequest,
            ctrl->wValue >> 8, resp);
        break;
    default:
        break;
    }
}

static void
uvc_events_process_setup(struct uvc_device *dev,
                          struct usb_ctrlrequest *ctrl,
                          struct uvc_request_data *resp)
{
    dev->control = 0;

    switch (ctrl->bRequestType & USB_TYPE_MASK) {
    case USB_TYPE_STANDARD:
        uvc_events_process_standard(dev, ctrl, resp);
        break;
    case USB_TYPE_CLASS:
        uvc_events_process_class(dev, ctrl, resp);
        break;
    default:
        break;
    }
}

static int
uvc_events_process_data(struct uvc_device *dev,
                         struct uvc_request_data *data)
{
    struct uvc_streaming_control *target;
    struct uvc_streaming_control *ctrl;
    const struct uvc_format_info *format;
    const struct uvc_frame_info *frame;
    const unsigned int *interval;
    unsigned int iformat, iframe, nframes;
    unsigned int *val = (unsigned int *)data->data;

    switch (dev->control) {
    case UVC_VS_PROBE_CONTROL:
        printf("setting probe control, length = %d\n", data->length);
        target = &dev->probe;
        break;
    case UVC_VS_COMMIT_CONTROL:
        printf("setting commit control, length = %d\n", data->length);
        target = &dev->commit;
        break;
    default:
        printf("setting unknown control, length = %d\n", data->length);
        if (*val > PU_BRIGHTNESS_MAX_VAL)
            return -EINVAL;
        dev->brightness_val = *val;
        return 0;
    }

    ctrl = (struct uvc_streaming_control *)&data->data;
    iformat = clamp((unsigned int)ctrl->bFormatIndex, 1U,
                    (unsigned int)ARRAY_SIZE(uvc_formats));
    format = &uvc_formats[iformat - 1];

    nframes = 0;
    while (format->frames[nframes].width != 0)
        ++nframes;

    iframe = clamp((unsigned int)ctrl->bFrameIndex, 1U, nframes);
    frame = &format->frames[iframe - 1];
    interval = frame->intervals;

    while (interval[0] < ctrl->dwFrameInterval && interval[1])
        ++interval;

    target->bFormatIndex = iformat;
    target->bFrameIndex = iframe;

    switch (format->fcc) {
    case V4L2_PIX_FMT_YUYV:
        target->dwMaxVideoFrameSize = frame->width * frame->height * 2;
        break;
    case V4L2_PIX_FMT_MJPEG:
    case V4L2_PIX_FMT_H264:
    case V4L2_PIX_FMT_HEVC:
        target->dwMaxVideoFrameSize = dev->imgsize;
        break;
    }
    target->dwFrameInterval = *interval;

    if (dev->control == UVC_VS_COMMIT_CONTROL) {
        dev->fcc = format->fcc;
        dev->width = frame->width;
        dev->height = frame->height;
        printf(">>> Host committed: %s %ux%u\n",
               fcc_to_name(dev->fcc), dev->width, dev->height);
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 * Main UVC event dispatcher
 * ---------------------------------------------------------------------------
 */
static void uvc_events_process(struct uvc_device *dev)
{
    struct v4l2_event v4l2_event;
    struct uvc_event *uvc_event = (void *)&v4l2_event.u.data;
    struct uvc_request_data resp;
    int ret;

    ret = ioctl(dev->uvc_fd, VIDIOC_DQEVENT, &v4l2_event);
    if (ret < 0) {
        printf("VIDIOC_DQEVENT failed: %s (%d)\n", strerror(errno), errno);
        return;
    }

    memset(&resp, 0, sizeof resp);
    resp.length = -EL2HLT;

    switch (v4l2_event.type) {
    case UVC_EVENT_CONNECT:
        printf("UVC: Host connected.\n");
        return;

    case UVC_EVENT_DISCONNECT:
        dev->uvc_shutdown_requested = 1;
        printf("UVC: Host disconnected.\n");
        return;

    case UVC_EVENT_SETUP:
        uvc_events_process_setup(dev, &uvc_event->req, &resp);
        break;

    case UVC_EVENT_DATA:
        ret = uvc_events_process_data(dev, &uvc_event->data);
        if (ret < 0)
            break;
        return;

    case UVC_EVENT_STREAMON:
        if (!dev->bulk)
            uvc_handle_streamon_event(dev);
        return;

    case UVC_EVENT_STREAMOFF:
        uvc_handle_streamoff_event(dev);
        return;
    }

    ret = ioctl(dev->uvc_fd, UVCIOC_SEND_RESPONSE, &resp);
    if (ret < 0) {
        printf("UVCIOC_SEND_RESPONSE failed: %s (%d)\n", strerror(errno), errno);
    }
}

static void uvc_events_init(struct uvc_device *dev)
{
    struct v4l2_event_subscription sub;
    unsigned int payload_size;

    switch (dev->fcc) {
    case V4L2_PIX_FMT_YUYV:
        payload_size = dev->width * dev->height * 2;
        break;
    case V4L2_PIX_FMT_MJPEG:
    case V4L2_PIX_FMT_H264:
    case V4L2_PIX_FMT_HEVC:
        payload_size = dev->imgsize;
        break;
    default:
        payload_size = dev->imgsize;
        break;
    }

    uvc_fill_streaming_control(dev, &dev->probe, 0, 0);
    uvc_fill_streaming_control(dev, &dev->commit, 0, 0);

    if (dev->bulk)
        dev->probe.dwMaxPayloadTransferSize =
            dev->commit.dwMaxPayloadTransferSize = payload_size;

    memset(&sub, 0, sizeof sub);
    sub.type = UVC_EVENT_SETUP;
    ioctl(dev->uvc_fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_DATA;
    ioctl(dev->uvc_fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_STREAMON;
    ioctl(dev->uvc_fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_STREAMOFF;
    ioctl(dev->uvc_fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
}

#endif /* UVC_SMART_EVENTS_H */
