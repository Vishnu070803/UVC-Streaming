# Troubleshooting Guide

This guide details common issues encountered during the development of the UVC H.264 SEI pipeline and their solutions.

## Issue 1: Hardware Buffer Starvation (Pipeline Stalls)
**Symptoms:** The pipeline starts correctly but freezes after roughly 50 to 100 frames. The `sei_client` stops receiving data.
**Root Cause:** The `v4l2h264enc` hardware encoder has a very small, fixed pool of DMABUFs. If downstream elements (like a `tee` to a file sink or an RTSP queue) hold references to these buffers, the encoder runs out of memory and halts.
**Solution:** A "Deep Copy" refactor was implemented. In the pad probe, `gst_buffer_ref()` was replaced with `gst_buffer_copy_deep()`. Copying the small H.264 payload into user-space RAM instantly releases the hardware buffer back to the encoder's pool without impacting latency.

## Issue 2: Low Framerate / High CPU Usage during Encoding
**Symptoms:** Framerate drops from 30 FPS to ~15 FPS. CPU usage on the board spikes to nearly 100%.
**Root Cause:** The `videoconvert` element in GStreamer was configured to output `format=I420`. The i.MX8MP VPU natively prefers `NV12`. Forcing `I420` causes GStreamer to perform a software-based colorspace conversion on every single 1080p frame, destroying performance.
**Solution:** Ensure the GStreamer caps immediately before the encoder specify `format=NV12`.
```text
videoconvert ! video/x-raw,format=NV12 ! v4l2h264enc
```

## Issue 3: Synchronization Latency Drift
**Symptoms:** The extracted IMU timestamp and the video frame PTS show a latency gap that drifts or exceeds 2ms.
**Root Cause:** Early versions relied on relative PTS timestamps instead of absolute kernel capture time.
**Solution:** The inline pad probe now calculates the absolute kernel time using: `abs_kernel_time = pts + base_time`. This firmly locks the synchronization to the physical V4L2 kernel-to-userspace handoff, maintaining a stable ~1.75ms latency.

## Issue 4: "Operation not permitted" when configuring ConfigFS
**Symptoms:** Running `mkdir` for the `h264` or `framebased` payload directories in `/sys/kernel/config/` fails.
**Root Cause:** The kernel is missing the UVC framebased format patch. 
**Solution:** Follow the steps in `02_Environment_Setup.md` to properly apply the `0002-usb-gadget-uvc-Add-framebased-format-support.patch` via a Yocto `.bbappend`. Note: both patches are required — `0001` fixes the DWC3 scatter-gather bug and `0002` adds the `framebased` ConfigFS support.
