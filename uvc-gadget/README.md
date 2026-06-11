# UVC Gadget Application

This directory contains the primary production C applications for the USB Video Class (UVC) Gadget.

## Directory Structure
* **`applications/sei_injector/` (Primary / Real IMU):** The core application that runs on the board. It interfaces with the real physical IMU (ICM-20948) and injects synchronous 250Hz IMU data natively into the H.264 stream.
* **`applications/sei_mock_injector/` (Testing Variant):** A variant that generates mock/simulated "AI" metadata via a background thread instead of reading physical sensors. Useful for testing host-side extraction pipelines without requiring physical hardware.
* **`applications/smart_video_only/` (Base Variant):** A pure video-streaming UVC application. It handles the dynamic GStreamer pipeline and UVC state machine (power saving) but **does not** inject any SEI metadata.
* **`tools/sei_extractor/`**: The client-side application meant to be run on the Host PC (Linux). It intercepts the incoming H.264 stream, extracts the custom SEI IMU data, strips out the Emulation Prevention Bytes, and logs the IMU values.
* **`include/`**: Contains shared headers, crucially `sei_metadata.h` (which defines the shared SEI payload structure) and `icm20948_ioctl.h` (the kernel interface for the IMU driver).

Please refer to the `docs/implementation/` folder in the root of the repository for detailed guides on how to configure, run, and modify this code.
