#ifndef UVC_SMART_GST_H
#define UVC_SMART_GST_H

#include "uvc_smart_types.h"

/* ---------------------------------------------------------------------------
 * GStreamer pipeline management
 *
 * Builds the right pipeline based on the negotiated UVC format (dev->fcc).
 * Camera is only opened on STREAMON and released on STREAMOFF.
 * ---------------------------------------------------------------------------
 */

/*
 * Build and start the GStreamer pipeline for the current format.
 * Returns 0 on success, -1 on failure.
 */
static int gst_pipeline_build(struct uvc_device *dev)
{
    char pipeline_str[512];
    GError *error = NULL;
    GstStateChangeReturn sret;
    unsigned int fps_n, fps_d;

    if (dev->pipeline) {
        printf("GST: Pipeline already exists, destroying first.\n");
        gst_element_set_state(dev->pipeline, GST_STATE_NULL);
        gst_object_unref(dev->pipeline);
        dev->pipeline = NULL;
        dev->appsink = NULL;
    }

    /* Convert UVC frame interval (100ns units) to fps fraction */
    /* Default to 30fps if interval is 0 */
    fps_n = 30;
    fps_d = 1;
    if (dev->probe.dwFrameInterval > 0) {
        /* interval in 100ns → fps = 10000000 / interval */
        unsigned int interval = dev->probe.dwFrameInterval;
        if (interval == 333333) { fps_n = 30; fps_d = 1; }
        else if (interval == 666666) { fps_n = 15; fps_d = 1; }
        else if (interval == 1000000) { fps_n = 10; fps_d = 1; }
        else if (interval == 10000000) { fps_n = 1; fps_d = 1; }
        else { fps_n = 10000000; fps_d = interval; }
    }

    printf("GST: Building pipeline for %s %ux%u @ %u/%u fps\n",
           fcc_to_name(dev->fcc), dev->width, dev->height, fps_n, fps_d);

    switch (dev->fcc) {
    case V4L2_PIX_FMT_H264:
        snprintf(pipeline_str, sizeof(pipeline_str),
            "v4l2src device=%s ! "
            "video/x-raw,format=NV12,width=%u,height=%u,framerate=%u/%u ! "
            "%s ! h264parse config-interval=1 ! "
            "appsink name=sink emit-signals=false sync=false max-buffers=2 drop=true",
            dev->camera_dev, dev->width, dev->height, fps_n, fps_d,
            dev->encoder_type == 1 ? "vpuenc_h264" : "v4l2h264enc");
        break;

    case V4L2_PIX_FMT_HEVC:
        snprintf(pipeline_str, sizeof(pipeline_str),
            "v4l2src device=%s ! "
            "video/x-raw,format=NV12,width=%u,height=%u,framerate=%u/%u ! "
            "%s ! h265parse config-interval=1 ! "
            "appsink name=sink emit-signals=false sync=false max-buffers=2 drop=true",
            dev->camera_dev, dev->width, dev->height, fps_n, fps_d,
            dev->encoder_type == 1 ? "vpuenc_hevc" : "v4l2h265enc");
        break;

    case V4L2_PIX_FMT_YUYV:
        snprintf(pipeline_str, sizeof(pipeline_str),
            "v4l2src device=%s ! "
            "video/x-raw,format=YUY2,width=%u,height=%u,framerate=%u/%u ! "
            "appsink name=sink emit-signals=false sync=false max-buffers=2 drop=true",
            dev->camera_dev, dev->width, dev->height, fps_n, fps_d);
        break;

    case V4L2_PIX_FMT_MJPEG:
        snprintf(pipeline_str, sizeof(pipeline_str),
            "v4l2src device=%s ! "
            "video/x-raw,format=NV12,width=%u,height=%u,framerate=%u/%u ! "
            "jpegenc ! image/jpeg ! "
            "appsink name=sink emit-signals=false sync=false max-buffers=2 drop=true",
            dev->camera_dev, dev->width, dev->height, fps_n, fps_d);
        break;

    default:
        printf("GST: Unsupported format 0x%08x\n", dev->fcc);
        return -1;
    }

    printf("GST: Pipeline: %s\n", pipeline_str);

    dev->pipeline = gst_parse_launch(pipeline_str, &error);
    if (!dev->pipeline) {
        printf("GST: Failed to create pipeline: %s\n",
               error ? error->message : "unknown error");
        if (error) g_error_free(error);
        return -1;
    }
    if (error) {
        printf("GST: Pipeline warning: %s\n", error->message);
        g_error_free(error);
    }

    dev->appsink = gst_bin_get_by_name(GST_BIN(dev->pipeline), "sink");
    if (!dev->appsink) {
        printf("GST: Failed to find appsink element\n");
        gst_object_unref(dev->pipeline);
        dev->pipeline = NULL;
        return -1;
    }

    /* Start the pipeline */
    sret = gst_element_set_state(dev->pipeline, GST_STATE_PLAYING);
    if (sret == GST_STATE_CHANGE_FAILURE) {
        printf("GST: Failed to set pipeline to PLAYING\n");
        gst_object_unref(dev->appsink);
        gst_object_unref(dev->pipeline);
        dev->appsink = NULL;
        dev->pipeline = NULL;
        return -1;
    }

    printf("GST: Pipeline PLAYING — camera %s acquired.\n", dev->camera_dev);
    return 0;
}

/*
 * Stop and destroy the GStreamer pipeline, releasing the camera.
 */
static void gst_pipeline_destroy(struct uvc_device *dev)
{
    if (!dev->pipeline)
        return;

    printf("GST: Destroying pipeline — releasing camera %s.\n", dev->camera_dev);

    gst_element_set_state(dev->pipeline, GST_STATE_NULL);

    if (dev->appsink) {
        gst_object_unref(dev->appsink);
        dev->appsink = NULL;
    }

    gst_object_unref(dev->pipeline);
    dev->pipeline = NULL;

    printf("GST: Pipeline destroyed, camera released.\n");
}

/*
 * Check GStreamer bus for error/warning/eos messages (non-blocking).
 */
static void gst_check_bus(struct uvc_device *dev)
{
    GstBus *bus;
    GstMessage *msg;

    if (!dev->pipeline)
        return;

    bus = gst_element_get_bus(dev->pipeline);
    while ((msg = gst_bus_pop(bus)) != NULL) {
        switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError *err = NULL;
            gchar *debug = NULL;
            gst_message_parse_error(msg, &err, &debug);
            printf("GST ERROR: %s\n", err->message);
            if (debug) printf("GST DEBUG: %s\n", debug);
            g_error_free(err);
            g_free(debug);
            /* Pipeline error — tear it down */
            gst_pipeline_destroy(dev);
            break;
        }
        case GST_MESSAGE_WARNING: {
            GError *err = NULL;
            gchar *debug = NULL;
            gst_message_parse_warning(msg, &err, &debug);
            printf("GST WARNING: %s\n", err->message);
            g_error_free(err);
            g_free(debug);
            break;
        }
        case GST_MESSAGE_EOS:
            printf("GST: End of stream.\n");
            gst_pipeline_destroy(dev);
            break;
        default:
            break;
        }
        gst_message_unref(msg);
    }
    gst_object_unref(bus);
}

#endif /* UVC_SMART_GST_H */
