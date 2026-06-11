# System Architecture & Hardware Flow

## Overview
This document details the high-level architecture of our Custom Multi-Format UVC Gadget running on an NXP i.MX8MP processor. The system acts as a bridge between a physical camera sensor (`/dev/video4`) and a USB Host PC, presenting itself as a standard UVC (USB Video Class) webcam.

## Hardware & Driver Topology

### 1. The Physical Camera (`/dev/video4`)
This is the V4L2 capture node representing the actual camera sensor (or ISP) connected to the NXP board. 
- **Role**: Captures raw light and converts it into raw pixel formats (like YUYV or NV12).
- **Control**: Our application controls this via GStreamer's `v4l2src` element, which uses V4L2 IOCTLs (`VIDIOC_S_FMT`, `VIDIOC_STREAMON`) to configure the hardware sensor to the exact resolution and framerate requested by the USB Host.

### 2. The Hardware Encoders (VPU)
The i.MX8MP has dedicated Video Processing Units (VPUs) for hardware-accelerated video encoding.
- **Legacy Encoders**: `vpuenc_h264` and `vpuenc_hevc`. These interface directly with the NXP VPU API.
- **V4L2 Encoders**: `v4l2h264enc` and `v4l2h265enc`. These are standard V4L2 wrappers around the NXP VPU. We default to these because they exhibited fewer visual artifacts (like green lines) than the legacy encoders.

### 3. The UVC Gadget Driver (`/dev/video6`)
This is a virtual V4L2 output node created by the Linux kernel's USB Gadget subsystem (`g_webcam` / `uvc_configfs`).
- **Role**: It acts as the "transmitter". Whatever data you push into `/dev/video6` is instantly packaged into USB packets and sent across the USB cable to the Host PC.
- **Connection**: Our C application (`uvc_gadget_sei.c`) acts as the glue. It pulls encoded frames from GStreamer and pushes them into `/dev/video6` using memory-mapped (`mmap`) buffers and the `VIDIOC_QBUF` IOCTL.

## The Event-Driven State Machine

Our application is "Smart" because it does not blindly stream video 24/7. It listens to the USB bus for commands from the Host PC.

1. **Idle State**: The application listens to `/dev/video6` using the `select()` system call. The physical camera is OFF.
2. **UVC_EVENT_SETUP**: The Host PC connects to the camera and queries its capabilities (Probe). The board responds with min/max bitrates and frame sizes.
3. **UVC_EVENT_DATA (Commit)**: The Host PC selects a specific format (e.g., H.264, 1080p, 60fps). 
4. **UVC_EVENT_STREAMON**: The Host PC opens the video application (e.g., VLC or Zoom). 
   - Our app intercepts this event.
   - It dynamically builds a GStreamer pipeline matching the requested format.
   - It starts the physical camera and the hardware encoder.
   - Video begins flowing from `/dev/video4` -> Encoder -> C App -> `/dev/video6` -> USB Cable.
5. **UVC_EVENT_STREAMOFF**: The Host PC closes the video app. Our app catches this, gracefully destroys the GStreamer pipeline, and turns off the physical camera to save power and USB bandwidth.
