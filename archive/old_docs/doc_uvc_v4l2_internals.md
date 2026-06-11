# UVC & V4L2 Internal Workings

## ConfigFS & USB Descriptors
Before the C application even starts, the `usb_uvc_all.sh` script runs. This script uses `configfs` to build the **USB Descriptor Tree**.
When you plug the NXP board into a Host PC, the Host PC asks: "What are you?"
The board sends back these descriptors. 

### The `framebased` Hack
Standard webcams only advertise Uncompressed (YUYV) and MJPEG formats. To stream H.264/HEVC, we use the `framebased` payload specification. 
- In the script, we wrote custom GUIDs (Globally Unique Identifiers) for H.264 (`H264\x00...`) and HEVC (`HEVC\x00...`).
- We are using **UVC 1.1** compliance because the mainline Linux gadget driver does not support creating **Encoding Units (EU)**, which are required for full UVC 1.5 compliance.
- Because we lack an Encoding Unit, the Host PC cannot send dynamic bitrate adjustment commands. Bitrate is statically controlled on the board side by GStreamer.

## V4L2 IOCTL Deep Dive
Our C application interacts with the Linux kernel using specific IOCTL (Input/Output Control) system calls on `/dev/video6`.

### 1. `VIDIOC_SUBSCRIBE_EVENT`
During initialization, the application subscribes to UVC control events. This tells the kernel: "Wake me up if the Host PC sends a USB control packet."

### 2. `VIDIOC_REQBUFS` & `VIDIOC_QUERYBUF`
When the stream starts, the app asks the kernel to allocate memory buffers in kernel space (`REQBUFS`). It then asks for the physical memory addresses of those buffers (`QUERYBUF`) and uses `mmap()` to map them into the userspace C application. 
*Why?* This is called **Zero-Copy streaming**. Instead of copying megabytes of video data from userspace to kernel space (which wastes CPU), the C application writes the video directly into the kernel's RAM.

### 3. `VIDIOC_QBUF` & `VIDIOC_DQBUF`
This is the core video loop.
- **`QBUF` (Queue Buffer)**: The app takes a buffer filled with H.264 video data and gives it back to the kernel. The kernel instantly sends it over USB.
- **`DQBUF` (Dequeue Buffer)**: The app waits for the kernel to finish sending the USB packet. Once sent, the kernel gives the empty buffer back to the app to be refilled with the next video frame.

## The AppSink Connection
How does the video get from GStreamer into the V4L2 buffers?
We use a GStreamer element called `appsink`. 
1. `appsink` emits a signal when a new video frame is encoded by the hardware.
2. Our C app pulls the raw memory pointer (`GstSample`) out of GStreamer.
3. The app `memcpy`s the GStreamer bytes directly into the `mmap`'d V4L2 buffer.
4. The app calls `VIDIOC_QBUF` to send it over USB.
