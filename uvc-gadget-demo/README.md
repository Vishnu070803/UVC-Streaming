# UVC Gadget Demo (ConfigFS Board Configuration)

This directory contains the essential scripts used to configure the Linux USB Gadget framework (`configfs`) on the board before the UVC C application can run.

## Purpose
When you plug the NXP board into a Host PC, the Host PC queries the board for its USB Descriptors (to figure out what the device is and what video formats it supports). The Linux Kernel does not know this by default. 

The scripts in this directory mount `configfs`, define the UVC Gadget, and set up the `framebased` payload descriptors (with our custom H.264/HEVC GUIDs) so that the Host PC successfully recognizes the board as a UVC 1.1 compliant webcam capable of streaming H.264 video.

## Usage
Run the main setup script before launching the `sei_injector` application:
```bash
./usb_uvc_all.sh
```

Please refer to the `docs/knowledge_base/02_UVC_ConfigFS_Internals.md` file in the root of the repository for a deep dive into how ConfigFS, the `framebased` hack, and UVC descriptors work.
