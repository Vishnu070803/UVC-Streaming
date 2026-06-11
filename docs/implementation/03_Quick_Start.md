# Complete Execution & Testing Guide

This guide is the definitive manual for building, configuring, and testing every component of the UVC H.264 streaming system. It covers what to run on the board, what to run on the host PC, and the parameters required.

## 1. Board Side: Build All Applications

Log into the i.MX8MP board and compile the three application variants.
```bash
cd /path/to/USB_Streaming/uvc-gadget/applications

# 1. Production IMU Application
cd sei_injector
make
cd ..

# 2. Mock/Simulated Testing Application
cd sei_mock_injector
make
cd ..

# 3. Base Video Application (No Metadata)
cd smart_video_only
make
```

## 2. Board Side: Configure USB ConfigFS
Before any application can run, the board must present itself to the Host PC as a UVC webcam. 

Navigate to the demo directory and run the initialization script:
```bash
cd /path/to/USB_Streaming/uvc-gadget-demo
./usb_uvc_all.sh
```
*Note: This script defines the `framebased` payload descriptors. The host PC will now recognize a new USB webcam if plugged in.*

## 3. Board Side: Start a Video Application

You must pick exactly **one** of the compiled applications to run. Each application takes the exact same command-line parameters.

### Execution Parameters
All three applications support the following flags:
* `-u <device>`: UVC gadget device (default: `/dev/video6`)
* `-c <device>`: Camera capture device (default: `/dev/video4`)
* `-s <speed>`: USB speed (0 = Full, 1 = High, 2 = Super/SS). *Default: 2*
* `-b`: Force bulk transfer mode instead of isochronous.
* `-e <type>`: Encoder type (1 = legacy `vpuenc_*`, 2 = modern `v4l2*enc`). *Default: 2*
* `-n <nbufs>`: Number of UVC buffers to allocate. *Default: 2*

### Choose Your Variant & Run
**Option A: Full Production IMU Injection**
Use this when you need physical hardware sensors embedded.
```bash
cd uvc-gadget/applications/sei_injector
./uvc_gadget_sei -u /dev/video6 -c /dev/video4 -s 1 -e 2
```

**Option B: Mock "AI" Testing**
Use this to test the host PC extractor logic without requiring physical IMU movement.
```bash
cd uvc-gadget/applications/sei_mock_injector
./uvc_gadget_sei -u /dev/video6 -c /dev/video4 -s 1 -e 2
```

**Option C: Baseline Video Only**
Use this to debug standard video issues without metadata complications.
```bash
cd uvc-gadget/applications/smart_video_only
./uvc_gadget_smart -u /dev/video6 -c /dev/video4 -s 1 -e 2
```

> The application will print "Waiting for USB host..." and pause. It does not start the physical camera sensor until the Host PC requests the stream to save power.

---

## 4. Host PC Side: Verify Connection
Plug the USB cable from the board into the Host PC (Linux).
Open a terminal on the Host PC and verify the camera is recognized:
```bash
lsusb -t
v4l2-ctl --list-devices
```
Identify the `/dev/videoX` node assigned to the UVC gadget (e.g., `/dev/video0`).

Check that the Host PC recognizes the H.264 formats:
```bash
v4l2-ctl -d /dev/video0 --list-formats-ext
```

## 5. Host PC Side: Receive & Extract Data
If you ran Option A or Option B on the board, you need the custom extractor to view the metadata. If you ran Option C, you can use a standard app like VLC or OBS.

### Build the Extractor
On the Host PC, compile the extractor tool:
```bash
cd /path/to/USB_Streaming/uvc-gadget/tools/sei_extractor
make
```

### Run the Extractor
The extractor accepts the following positional arguments (all optional):
* `/dev/videoX` — the V4L2 node for the UVC camera (default: `/dev/video0`)
* `h265` or `hevc` — switch to HEVC mode (default: `h264`)
* `-v` or `--verbose` — print full structured per-frame blocks instead of the compact one-liner

```bash
# Default: H.264, /dev/video0, compact output
./uvc_sei_extractor

# Explicit device node
./uvc_sei_extractor /dev/video2

# HEVC stream with verbose output
./uvc_sei_extractor /dev/video2 hevc --verbose
```

### What Happens Next:
1. The extractor opens the V4L2 device and builds the GStreamer pipeline: `v4l2src → h264parse → avdec_h264 → autovideosink`.
2. A pad probe is attached to the `sink` pad of the parser element.
3. Every incoming buffer is scanned for the SEI NAL start code (`0x00 0x00 0x00 0x01 0x06 0x05` for H.264).
4. The payload is extracted, Emulation Prevention Bytes (`0x03`) are stripped, and the raw bytes are cast back into the `SeiMetadata` struct (validated by checking `magic == 0x494D5538` i.e. "IMU8").
5. IMU data (accel, gyro, mag), frame ID, PTS, and IMU timestamp delta are printed to the terminal while the clean video plays in the window.

