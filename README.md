# 🚀 USB Streaming: The Zero to Hero Guide

Welcome to the **i.MX8MP UVC Gadget & SEI Injector** project! 

If you are new here, staring at kernel patches, DMABUFs, and H.264 NAL units can feel incredibly overwhelming. **Don't panic.** This repository is designed to take you from absolute beginner to an embedded multimedia expert.

We have structured the documentation into four levels. If you are a "baby" to this project, start at Level 1 and read sequentially. If you are a senior engineer looking for a quick reference, skip straight to Level 2 or 3.

---

## 👶 Level 1: The Concepts (For the Beginner)
*Start here if you have zero idea how this project works.*
* 📖 [**docs/00_Zero_To_Hero_Concepts.md**](docs/00_Zero_To_Hero_Concepts.md)
  * **Read this to:** Understand the high-level goal using the "Sticky Note" analogy without getting bogged down in C code. It explains cameras, USB pipes, and SEI.

## 🛠️ Level 2: The Sandbox (For the Junior Developer)
*Start here if you want to flash the board, compile the code, and run it, but you don't need to write any C code yourself.*
* 📄 [**docs/implementation/02_Environment_Setup.md**](docs/implementation/02_Environment_Setup.md)
  * **Read this to:** Learn how to apply the Yocto kernel patches, configure the Device Tree for USB High-Speed, and flash the i.MX8MP board.
* 📄 [**build_requirements.txt**](build_requirements.txt)
  * **Read this to:** Get the exact `apt-get` commands for compiling the Extractor on your Host PC, and the `bitbake` recipes for compiling on the board.
* 📄 [**docs/implementation/03_Quick_Start.md**](docs/implementation/03_Quick_Start.md)
  * **Read this to:** Get the exact terminal commands to run the UVC gadget on the board, and how to test the Extractor on the Host PC.

## 🧠 Level 3: The Architecture (For the Senior Engineer)
*Start here if you are tasked with modifying the C codebase or fixing bugs.*
* 📄 [**docs/implementation/01_Project_Overview.md**](docs/implementation/01_Project_Overview.md)
  * **Read this to:** Understand the directory structure and the difference between the 3 variants (`sei_injector`, `sei_mock_injector`, `smart_video_only`).
* 📄 [**docs/implementation/04_Development_Guide.md**](docs/implementation/04_Development_Guide.md)
  * **Read this to:** Learn how to add new sensor data (like Temperature or GPS) to the `sei_metadata.h` C-struct and ensure the Extractor can read it.
* 📄 [**docs/implementation/05_Troubleshooting.md**](docs/implementation/05_Troubleshooting.md)
  * **Read this to:** See how past bugs (like 100% CPU usage or memory starvation) were solved using NV12 colorspaces and GStreamer deep copies.

## 🧙 Level 4: The Deep Magic (For the Expert)
*Start here if you want to understand the extreme low-level operating system mechanics that make zero-latency streaming possible.*
* 🎓 [**docs/knowledge_base/01_Linux_Kernel_Memory_V4L2.md**](docs/knowledge_base/01_Linux_Kernel_Memory_V4L2.md)
  * **Read this to:** Understand why the kernel doesn't dynamically allocate memory for video, and how `vb2` and `DMABUF` enable zero-copy hardware encoding.
* 🎓 [**docs/knowledge_base/02_UVC_ConfigFS_Internals.md**](docs/knowledge_base/02_UVC_ConfigFS_Internals.md)
  * **Read this to:** Understand the UVC 1.1 `framebased` hack and how the event-driven state machine (`UVC_EVENT_STREAMON`) saves power.
* 🎓 [**docs/knowledge_base/03_SEI_Metadata_Injection.md**](docs/knowledge_base/03_SEI_Metadata_Injection.md)
  * **Read this to:** Learn the binary structure of an H.264 stream and why Emulation Prevention Bytes (`0x03`) are critical to prevent crashes.
* 🎓 [**docs/knowledge_base/04_IMU_Sync_Architecture.md**](docs/knowledge_base/04_IMU_Sync_Architecture.md)
  * **Read this to:** Understand the "Hybrid Array Model" that solves the mathematical conflict between a 250Hz IMU requirement and a 30FPS video constraint.

---

## 📂 Repository Layout
```text
USB_Streaming/
├── uvc-gadget/                 # Core C Applications
│   ├── applications/           
│   │   ├── sei_injector/       # [Variant A] Production: Real IMU -> Video
│   │   ├── sei_mock_injector/  # [Variant B] Testing: Fake IMU -> Video
│   │   └── smart_video_only/   # [Variant C] Base: Pure Video, No IMU
│   ├── tools/
│   │   └── sei_extractor/      # PC Client to strip the IMU data out
│   └── include/                # Shared C Headers
├── uvc-gadget-demo/            # Shell scripts to configure USB on the board
├── meta-usb/                   # Yocto Layer containing Kernel Patches
├── docs/                       # All the documentation linked above
└── build_requirements.txt      # Debian/Ubuntu APT dependencies
```
