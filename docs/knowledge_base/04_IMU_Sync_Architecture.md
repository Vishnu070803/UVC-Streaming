# IMU 250 Hz Frequency Analysis & Architectures

This document explores the architectural solution to the core conflict: providing a **>= 250 Hz IMU sample rate** alongside a strict **Maximum sync tolerance <= 1 ms** to the 30 FPS video frame.

## The Core Conflict
The video pipeline operates at **30 FPS (33.3 ms per frame)**.
An IMU operating at 250 Hz generates a sample every **4.0 ms**.

Because SEI metadata must be attached to a video frame, you cannot transmit 250 SEI NAL units per second. You must "batch" the IMU data (approx. 8 to 9 samples per frame). However, standard batching introduces synchronization gaps between the asynchronous camera shutter and the rigid 4ms IMU polling loop.

> [!WARNING]
> **Hardware Magnetometer Limitation**
> The ICM-20948 chip contains an AK09916 magnetometer. According to its datasheet, its absolute maximum continuous Output Data Rate (ODR) is **100 Hz** (Continuous Measurement Mode 4). The magnetometer physically cannot produce 250 unique samples per second. Any 250 Hz model will result in the magnetometer values being duplicated across 2 or 3 consecutive samples (while Accel and Gyro will successfully provide unique 250 Hz data).

---

## Architectural Models & Trade-offs

### 1. 30 Hz Frame-Locked Inline
The IMU is sampled synchronously (via IOCTL) inside a zero-latency GStreamer pad probe the exact microsecond the 30 FPS frame hits user-space.
*   **Pros:** Mathematically guarantees sub-1ms sync tolerance. Extremely low CPU overhead.
*   **Cons:** Fails the 250 Hz frequency requirement (provides exactly 30 Hz).

### 2. Standard Background Batching: 250 Hz Polling
A dedicated `pthread` polls the IMU every 4 ms and saves the data to a software array. When the 30 FPS frame arrives, the accumulated array (~8 samples) is embedded into the SEI payload.
*   **Pros:** Meets the 250 Hz requirement.
*   **Cons:** **Fails the <= 1 ms sync tolerance requirement.** Because the camera shutter fires asynchronously to the 4ms polling loop, the closest IMU sample to the frame could be up to **2.0 ms** away (half the polling interval).

### 3. Hardware FIFO Batching
The ICM-20948 internal hardware FIFO is configured to automatically sample at exactly 250 Hz. When the 30 FPS frame arrives, the software issues a bulk IOCTL to drain the hardware FIFO.
*   **Pros:** Lowest CPU polling overhead (interrupt driven). Perfect 250 Hz timing without thread jitter.
*   **Cons:** **Fails the <= 1 ms sync tolerance requirement** for the exact same reason as Model 2. The FIFO clock and Camera clock are independent, yielding up to a 2.0 ms alignment gap.

### 4. The Hybrid Array Model (The Ultimate Solution)
To satisfy **both** the 250 Hz requirement and the <= 1 ms sync requirement, a Hybrid approach must be used that combines Model 1 and Model 2.
1.  **Background Polling:** A thread (or FIFO) polls the IMU every 4 ms to build a high-frequency history array.
2.  **Inline Frame Sync:** When the 30 FPS frame interrupt fires, the GStreamer pad probe instantly triggers a synchronous IMU IOCTL read.
3.  **SEI Array Payload:** The SEI payload structure is expanded into an array. `Sample[0]` is the inline frame-synced read. `Sample[1..8]` are the background polled samples spanning the time since the previous frame.

*   **Pros:** Satisfies **all** requirements simultaneously. `Sample[0]` guarantees < 1 ms sync tolerance to the frame, while the array provides the high-frequency 250 Hz curvature.
*   **Cons:** Most complex software architecture. Increases SEI payload size significantly (~369 bytes per frame). The PC client extraction script must iterate through dynamic struct arrays.

---

## Alternative Solutions (Bypassing SEI)

### Option A: Sidecar Files (MP4 + CSV + JSON)
If the goal is purely **recording** (or analyzing post-capture) rather than live USB streaming, Option A completely bypasses the SEI complexities.
1.  **`video.mp4`**: The GStreamer pipeline records the 30 FPS video. No SEI injection is performed.
2.  **`imu.csv`**: A dedicated C program loops at 250 Hz, reading the IMU and writing directly to a CSV file. Every row contains the exact `CLOCK_MONOTONIC` timestamp.
3.  **`meta.json`**: Records the exact `CLOCK_MONOTONIC` base time when the MP4 recording started.

**How Synchronization is Proven (The Math):**
Because the MP4 video timestamps start at `0`, for any frame at time $T_{relative}$ in the MP4, its true hardware time is:
$$T_{absolute} = T_{base\_time} + T_{relative}$$
You simply look up $T_{absolute}$ in the `imu.csv` to find the exact IMU samples captured at that microsecond. This trivially supports 250 Hz without any SEI limitations.
