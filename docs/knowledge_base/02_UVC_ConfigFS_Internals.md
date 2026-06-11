# UVC ConfigFS Internals & Data Flow

## ConfigFS & USB Descriptors
Before the C application even starts, the `usb_uvc_all.sh` script (located in `uvc-gadget-demo/`) runs. This script uses `configfs` to build the **USB Descriptor Tree**.
When you plug the NXP board into a Host PC, the Host PC asks: "What are you?"
The board sends back these exact descriptors. 

### The `framebased` Hack
Standard webcams only advertise Uncompressed (YUYV) and MJPEG formats. To stream H.264/HEVC, we use the `framebased` payload specification. 
- In the script, we wrote custom GUIDs (Globally Unique Identifiers) for H.264 (`H264\x00...`) and HEVC (`HEVC\x00...`).
- We are using **UVC 1.1** compliance because the mainline Linux gadget driver does not support creating **Encoding Units (EU)**, which are required for full UVC 1.5 compliance.
- Because we lack an Encoding Unit, the Host PC cannot send dynamic bitrate adjustment commands. Bitrate is statically controlled on the board side by GStreamer.

## System Architecture & Hardware Flow

### 1. The Physical Camera (`/dev/video4`)
This is the V4L2 capture node representing the actual camera sensor connected to the NXP board. 
- **Role**: Captures raw light and converts it into raw pixel formats (like YUYV or NV12).
- **Control**: Our application controls this via GStreamer's `v4l2src` element, which uses V4L2 IOCTLs to configure the hardware sensor to the exact resolution and framerate requested by the USB Host.

### 2. The Hardware Encoders (VPU)
The i.MX8MP has dedicated Video Processing Units (VPUs) for hardware-accelerated video encoding.
- We use the V4L2 Encoders (`v4l2h264enc`) because they act as standard V4L2 wrappers around the NXP VPU, avoiding the visual artifacts found in the legacy `vpuenc_h264` encoders.

### 3. The UVC Gadget Driver (`/dev/video6`)
This is a virtual V4L2 output node created by the Linux kernel's USB Gadget subsystem (`g_webcam` / `uvc_configfs`).
- **Role**: It acts as the "transmitter". Whatever data you push into `/dev/video6` is instantly packaged into USB packets and sent across the USB cable to the Host PC.

## The Event-Driven State Machine
Our C application is "Smart" because it does not blindly stream video 24/7. It listens to the USB bus for commands from the Host PC.

1. **Idle State**: The application listens to `/dev/video6` using the `select()` system call. The physical camera is OFF.
2. **UVC_EVENT_SETUP**: The Host PC connects to the camera and queries its capabilities (Probe). The board responds with min/max bitrates and frame sizes.
3. **UVC_EVENT_DATA (Commit)**: The Host PC selects a specific format (e.g., H.264, 1080p, 30fps). 
4. **UVC_EVENT_STREAMON**: The Host PC opens the video application. 
   - Our app intercepts this event.
   - It dynamically builds a GStreamer pipeline matching the requested format.
   - It starts the physical camera and the hardware encoder.
   - Video begins flowing from `/dev/video4` -> Encoder -> C App -> `/dev/video6` -> USB Cable.
5. **UVC_EVENT_STREAMOFF**: The Host PC closes the video app. Our app catches this, gracefully destroys the GStreamer pipeline, and turns off the physical camera to save power and USB bandwidth.

## The AppSink Connection
How does the video get from GStreamer into the V4L2 buffers? We use a GStreamer element called `appsink`. 
1. `appsink` emits a signal when a new video frame is encoded by the hardware.
2. Our C app pulls the raw memory pointer (`GstSample`) out of GStreamer.
3. The app `memcpy`s the GStreamer bytes directly into the `mmap`'d V4L2 buffer.
4. The app calls `VIDIOC_QBUF` to send it over USB.
