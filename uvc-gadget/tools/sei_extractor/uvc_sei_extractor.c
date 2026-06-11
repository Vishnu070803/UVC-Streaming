#include <gst/gst.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include "sei_metadata.h"

/* Reverse Emulation Prevention (EPB) */
static int remove_emulation_prevention(const uint8_t *src, int src_len, uint8_t *dst) {
    int dst_len = 0, zero_count = 0;
    for (int i = 0; i < src_len; i++) {
        if (zero_count == 2 && src[i] == 0x03) {
            zero_count = 0;
            continue;
        }
        if (src[i] == 0x00) zero_count++;
        else                zero_count = 0;
        dst[dst_len++] = src[i];
    }
    return dst_len;
}

static int g_verbose = 0;

/* GStreamer probe to extract SEI NAL unit */
static GstPadProbeReturn sei_extraction_probe(GstPad *pad, GstPadProbeInfo *info,
                                               gpointer user_data) {
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (!buffer) return GST_PAD_PROBE_OK;

    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        uint8_t *data = map.data;
        gsize size = map.size;

        for (gsize i = 0; i < size - 6; i++) {
            int is_h264_sei = (data[i] == 0x00 && data[i+1] == 0x00 &&
                               data[i+2] == 0x00 && data[i+3] == 0x01 &&
                               data[i+4] == 0x06 && data[i+5] == 0x05);

            int is_hevc_sei = (i < size - 7) &&
                              (data[i] == 0x00 && data[i+1] == 0x00 &&
                               data[i+2] == 0x00 && data[i+3] == 0x01 &&
                               data[i+4] == 0x4E && data[i+5] == 0x01 &&
                               data[i+6] == 0x05);

            if (is_h264_sei || is_hevc_sei) {
                int payload_offset = is_hevc_sei ? (i + 7) : (i + 6);

                /* Decode multi-byte payload size */
                int payload_size = 0;
                while (payload_offset < (int)size && data[payload_offset] == 0xFF) {
                    payload_size += 255;
                    payload_offset++;
                }
                if (payload_offset < (int)size) {
                    payload_size += data[payload_offset];
                    payload_offset++;
                }

                if (payload_offset + payload_size > (int)size)
                    break;

                /* Remove EPB */
                uint8_t epb_buf[512];
                memcpy(epb_buf, &data[payload_offset], payload_size);

                uint8_t raw[sizeof(SeiMetadata)];
                int decoded_len = remove_emulation_prevention(epb_buf, payload_size, raw);

                if (decoded_len >= (int)sizeof(SeiMetadata)) {
                    SeiMetadata meta;
                    memcpy(&meta, raw, sizeof(SeiMetadata));

                    /* Validate magic */
                    if (meta.magic != SEI_METADATA_MAGIC)
                        break;

                    if (g_verbose) {
                        g_print("\n── SEI Frame #%u ────────────────────────────\n", meta.frame_id);
                        g_print("  PTS       : %lu ns (camera)\n", (unsigned long)meta.frame_pts_ns);
                        g_print("  IMU time  : %lu ns (monotonic)\n", (unsigned long)meta.imu_timestamp_ns);
                        g_print("  Delta     : %ld ns\n",
                                (long)(meta.imu_timestamp_ns - meta.frame_pts_ns));
                        g_print("  Accel     : X:%d mg  Y:%d mg  Z:%d mg\n",
                                meta.accel_x_mg, meta.accel_y_mg, meta.accel_z_mg);
                        g_print("  Gyro      : X:%d mdps  Y:%d mdps  Z:%d mdps\n",
                                meta.gyro_x_mdps, meta.gyro_y_mdps, meta.gyro_z_mdps);
                        g_print("  Mag       : X:%d uT  Y:%d uT  Z:%d uT\n",
                                meta.mag_x_ut, meta.mag_y_ut, meta.mag_z_ut);
                        g_print("  Device    : %.16s | Env:%d | %.4f°N, %.4f°E\n",
                                meta.device_serial, meta.environment_type,
                                meta.latitude_e7 / 1e7, meta.longitude_e7 / 1e7);
                        g_print("  Session   : %.16s\n", meta.session_id);
                        g_print("─────────────────────────────────────────────\n");
                    } else {
                        g_print("\rSEI #%u | A(%d,%d,%d)mg G(%d,%d,%d)mdps M(%d,%d,%d)uT | D:%ldns   ",
                                meta.frame_id,
                                meta.accel_x_mg, meta.accel_y_mg, meta.accel_z_mg,
                                meta.gyro_x_mdps, meta.gyro_y_mdps, meta.gyro_z_mdps,
                                meta.mag_x_ut, meta.mag_y_ut, meta.mag_z_ut,
                                (long)(meta.imu_timestamp_ns - meta.frame_pts_ns));
                        fflush(stdout);
                    }
                }
                break;
            }
        }
        gst_buffer_unmap(buffer, &map);
    }
    return GST_PAD_PROBE_OK;
}

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);

    const char *codec = "h264";
    const char *device = "/dev/video0";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "hevc") == 0 || strcmp(argv[i], "h265") == 0)
            codec = "h265";
        else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0)
            g_verbose = 1;
        else if (strncmp(argv[i], "/dev/", 5) == 0)
            device = argv[i];
    }

    g_print("SEI Metadata Extractor (v%d)\n", SEI_METADATA_VERSION);
    g_print("  Codec  : %s\n", codec);
    g_print("  Device : %s\n", device);
    g_print("  Verbose: %s\n", g_verbose ? "yes" : "no (use -v for detail)");
    g_print("  Payload: %zu bytes (SeiMetadata)\n\n", sizeof(SeiMetadata));

    char pipeline_str[512];
    snprintf(pipeline_str, sizeof(pipeline_str),
        "v4l2src device=%s ! "
        "video/x-%s,width=1920,height=1080 ! "
        "%sparse name=client_parser ! "
        "avdec_%s ! videoconvert ! autovideosink sync=false",
        device, codec, codec, codec);

    g_print("Pipeline: %s\n\n", pipeline_str);

    GError *error = NULL;
    GstElement *pipeline = gst_parse_launch(pipeline_str, &error);
    if (error) {
        g_printerr("Pipeline error: %s\n", error->message);
        return -1;
    }

    GstElement *parser = gst_bin_get_by_name(GST_BIN(pipeline), "client_parser");
    if (parser) {
        GstPad *sink_pad = gst_element_get_static_pad(parser, "sink");
        gst_pad_add_probe(sink_pad, GST_PAD_PROBE_TYPE_BUFFER,
                          sei_extraction_probe, NULL, NULL);
        gst_object_unref(sink_pad);
        gst_object_unref(parser);
        g_print("Extraction probe attached to parser sink pad.\n");
    } else {
        g_printerr("Error: Could not find parser element!\n");
    }

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    GstBus *bus = gst_element_get_bus(pipeline);
    gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                               GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    g_print("\nExtractor exiting.\n");
    return 0;
}
