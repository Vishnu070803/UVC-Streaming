#ifndef UVC_SEI_INJECTION_H
#define UVC_SEI_INJECTION_H

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <gst/gst.h>

/* The AI Metadata structure */
typedef struct {
    float x;
    float y;
    float width;
    float height;
    uint32_t frame_id;
    uint32_t object_class; /* e.g. 0=person, 1=car */
    float confidence;
} AIData;

typedef struct {
    AIData current_data;
    pthread_mutex_t mutex;
    int is_running;
} AppContext;

extern AppContext app_ctx;
AppContext app_ctx = { .mutex = PTHREAD_MUTEX_INITIALIZER, .is_running = 0 };

/* Thread function to generate dummy AI tracking data */
static void* ai_thread_func(void* arg) {
    float pos_x = 0.0f;
    float pos_y = 0.0f;
    float velocity_x = 2.0f;
    float velocity_y = 1.5f;
    uint32_t frame_counter = 0;

    app_ctx.is_running = 1;

    while (app_ctx.is_running) {
        pthread_mutex_lock(&app_ctx.mutex);
        
        /* Simulate an object bouncing around the 1920x1080 screen */
        pos_x += velocity_x;
        pos_y += velocity_y;
        
        if (pos_x > 1800.0f || pos_x < 0.0f) velocity_x = -velocity_x;
        if (pos_y > 1000.0f || pos_y < 0.0f) velocity_y = -velocity_y;

        app_ctx.current_data.x = pos_x;
        app_ctx.current_data.y = pos_y;
        app_ctx.current_data.width = 120.0f;
        app_ctx.current_data.height = 250.0f;
        app_ctx.current_data.frame_id = frame_counter++;
        app_ctx.current_data.object_class = 0; /* Person */
        app_ctx.current_data.confidence = 0.95f + (pos_x / 36000.0f); /* Just varying slightly */

        pthread_mutex_unlock(&app_ctx.mutex);
        
        /* 30 fps generation */
        usleep(33333);
    }
    return NULL;
}

/* Emulation Prevention (EPB) to avoid false start codes (0x00 0x00 0x00 / 0x01 / 0x02 / 0x03) */
static int apply_emulation_prevention(const uint8_t *src, int src_len, uint8_t *dst) {
    int dst_len = 0, zero_count = 0;
    for (int i = 0; i < src_len; i++) {
        if (zero_count == 2 && src[i] <= 0x03) {
            dst[dst_len++] = 0x03;
            zero_count = 0;
        }
        if (src[i] == 0x00) {
            zero_count++;
        } else {
            zero_count = 0;
        }
        dst[dst_len++] = src[i];
    }
    return dst_len;
}

/* GStreamer probe to inject SEI NAL unit into encoded buffers */
static GstPadProbeReturn sei_injection_probe(GstPad *pad, GstPadProbeInfo *info, gpointer user_data) {
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (!buffer) return GST_PAD_PROBE_OK;

    int is_hevc = GPOINTER_TO_INT(user_data);

    /* Fetch latest AI data safely */
    pthread_mutex_lock(&app_ctx.mutex);
    AIData local_ai = app_ctx.current_data;
    pthread_mutex_unlock(&app_ctx.mutex);

    uint8_t raw_payload[sizeof(AIData)];
    memcpy(raw_payload, &local_ai, sizeof(AIData));

    /* Apply emulation prevention */
    uint8_t epb_payload[sizeof(AIData) * 2]; 
    int epb_len = apply_emulation_prevention(raw_payload, sizeof(AIData), epb_payload);

    /* Construct NAL Unit Header */
    uint8_t sei_header[10];
    int header_len = 0;
    
    if (is_hevc) {
        /* HEVC SEI NAL Unit: Start Code (4 bytes) + NAL Header (2 bytes) + SEI Type (5 = User Data Unregistered) */
        /* NAL unit type for HEVC SEI is 39. (39 << 1) = 0x4E */
        sei_header[0] = 0x00;
        sei_header[1] = 0x00;
        sei_header[2] = 0x00;
        sei_header[3] = 0x01;
        sei_header[4] = 0x4E;
        sei_header[5] = 0x01;
        sei_header[6] = 0x05; /* Payload type: 5 */
        header_len = 7;
    } else {
        /* H264 SEI NAL Unit: Start Code (4 bytes) + NAL Header (1 byte) + SEI Type (5 = User Data Unregistered) */
        /* NAL unit type for H264 SEI is 6. */
        sei_header[0] = 0x00;
        sei_header[1] = 0x00;
        sei_header[2] = 0x00;
        sei_header[3] = 0x01;
        sei_header[4] = 0x06;
        sei_header[5] = 0x05; /* Payload type: 5 */
        header_len = 6;
    }

    int total_size = header_len + 1 + epb_len + 1; /* Header + Size byte + Payload + RBSP trailing bit */
    uint8_t *sei_packet = g_malloc(total_size);
    
    int offset = 0;
    memcpy(sei_packet + offset, sei_header, header_len); 
    offset += header_len;
    
    /* Write Payload Size (assuming size < 255 for simplicity) */
    sei_packet[offset++] = epb_len;
    
    /* Write Payload */
    memcpy(sei_packet + offset, epb_payload, epb_len); 
    offset += epb_len;
    
    /* RBSP trailing bit */
    sei_packet[offset] = 0x80; 

    /* Zero-copy prepend the SEI NAL unit to the video buffer */
    GstBuffer *sei_buf = gst_buffer_new_wrapped(sei_packet, total_size);
    GstBuffer *new_buffer = gst_buffer_append(sei_buf, gst_buffer_ref(buffer));
    
    /* Replace buffer in the probe */
    GST_PAD_PROBE_INFO_DATA(info) = new_buffer;
    gst_buffer_unref(buffer);

    return GST_PAD_PROBE_OK;
}

#endif /* UVC_SEI_INJECTION_H */
