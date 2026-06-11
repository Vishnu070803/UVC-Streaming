# SEI Metadata Injection & Extraction

## The Problem with UVC Metadata
Standard UVC 1.5 allows metadata (like bounding boxes) to be appended to the UVC Payload Header. However:
1. The Linux Kernel Gadget driver (`uvc_video.c`) hardcodes the Payload Header and provides no V4L2 userspace API to inject custom data.
2. Standard Host PC applications (VLC, OBS, Zoom) do not know how to read the `/dev/video1` metadata node to retrieve it.

## The Solution: SEI
**SEI (Supplemental Enhancement Information)** is a standard feature of the H.264 and HEVC video codecs. Instead of putting the metadata in the USB wrapper, we put it **inside the video stream itself**. 

### NAL Units and Access Units
An H.264 video stream is made of NAL (Network Abstraction Layer) units. A single "video frame" (called an Access Unit) is actually a sequence of NAL units stuck together in one contiguous memory buffer.

To inject metadata, our GStreamer `sei_injection_probe` creates a brand new SEI NAL Unit and physically attaches it to the front of the Video Slice NAL Unit. 
`[SEI NAL] + [Video NAL] = 1 Video Frame`

Because they are in the same memory buffer, they are transmitted across the USB cable at the exact same millisecond. They can never lose synchronization.

### Emulation Prevention Bytes (EPB)
In H.264, the byte sequence `0x00 0x00 0x00 0x01` is reserved as the "Start Code" that separates NAL units. 
If our AI Bounding Box coordinates happen to mathematically evaluate to `0x00 0x00 0x01` in memory, the H.264 decoder on the Host PC will crash, thinking a new video frame has started in the middle of our metadata!

To prevent this, our C code runs an **Emulation Prevention** algorithm. Before injecting the metadata, it scans the bytes. If it sees `0x00 0x00 0x00`, it artificially inserts a `0x03` byte to break the pattern (`0x00 0x00 0x03 0x00`). The Host PC extractor reverses this process to get the original floats back.

## Host PC Extraction (`uvc_sei_extractor.c`)
Because standard applications (like VLC) see the NAL Unit Type (6 for H.264 SEI, 39 for HEVC SEI) and recognize the payload type as "User Data Unregistered", they gracefully ignore it and decode the video perfectly.

Our custom Host PC tool (`uvc_sei_extractor`) uses GStreamer to intercept the stream right before it reaches the video decoder.
1. It uses a pad probe on the `sink` pad of `h264parse`.
2. It scans the raw bytes for our custom SEI start code (`0x00 0x00 0x00 0x01 0x06 0x05`).
3. It extracts the payload length.
4. It strips out the `0x03` Emulation Prevention Bytes.
5. It casts the raw memory back into the `AIData` C-struct.
6. It prints the X, Y, Width, and Height to the terminal while passing the video frame down the pipeline to be rendered on the screen.
