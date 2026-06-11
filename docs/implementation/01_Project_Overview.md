# Project Overview

## Purpose
This project provides a complete, low-latency, hardware-accelerated video streaming pipeline for the NXP i.MX8MP processor. It functions as a **Custom Multi-Format UVC Gadget**, bridging a physical camera sensor (e.g., `/dev/video4`) to a USB Host PC, presenting itself as a standard UVC (USB Video Class) webcam.

Crucially, it solves the problem of transmitting synchronized high-frequency (250Hz) IMU sensor data alongside the 30 FPS video stream by injecting the sensor data directly into the H.264 video stream as **SEI (Supplemental Enhancement Information)** metadata.

## Key Features
* **Native H.264/HEVC UVC Streaming:** Uses the `framebased` payload hack to achieve UVC 1.1 compliance for compressed formats.
* **Hardware Acceleration (VPU):** Leverages the i.MX8MP's native V4L2 hardware encoders (`v4l2h264enc`) for zero-copy (`DMABUF`) encoding.
* **Sub-Millisecond Synchronization:** The IMU is sampled via IOCTL inside a zero-latency GStreamer pad probe exactly when the frame hits user-space, guaranteeing < 1ms sync tolerance.
* **Hybrid SEI Array Payload:** Satisfies both the 250Hz IMU sampling requirement and the tight video sync requirement by embedding an array of IMU samples into a single SEI NAL unit per frame.
* **Emulation Prevention:** Ensures binary metadata cannot accidentally trigger H.264 start codes (0x000001) by implementing Emulation Prevention Byte (0x03) insertion and removal.

## Directory Structure
* **`uvc-gadget/`**: Contains the core C applications.
  * `applications/sei_injector/`: Primary application reading real IMU data via synchronous IOCTL and injecting it into SEI.
  * `applications/sei_mock_injector/`: Testing variant using a background thread to generate dummy metadata.
  * `applications/smart_video_only/`: Base variant that handles pure video streaming and the UVC state machine without any SEI metadata.
  * `tools/sei_extractor/`: The client-side application that runs on the Host PC to extract the metadata.
* **`uvc-gadget-demo/`**: Contains scripts to configure the board's USB interface via ConfigFS before the C application runs.
* **`docs/`**: This documentation hub.
