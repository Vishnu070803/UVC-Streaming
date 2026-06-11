# Environment Setup (Yocto Kernel Patching)

Before the UVC H.264 streaming application can run natively, the Linux kernel must be patched to stabilize the USB controller and support out-of-tree UVC H.264 ConfigFS capabilities.

## Hardware & Software Requirements

* **Boards Tested:** NXP i.MX8M Plus EVK and CompuLab UCM-iMX8M-Plus.
* **Kernel Version:** Tested on the latest NXP IMX Release Notes for LF6.12.y series, but ideally works on all modern i.MX BSP kernel versions.

## 1. Obtain the Patches

This project requires specific patches located in the `meta-usb` layer to stabilize the USB and enable H.264 payload support:

1. `0001-usb-dwc3-gadget-Disable-scatter-gather-for-i.MX8MP-U.patch` (Disables Scatter-Gather to prevent hardware buffer starvation on the i.MX8MP).
2. `0002-usb-gadget-uvc-Add-framebased-format-support.patch` (Adds the UVC framebased format support required for H.264/HEVC injection).

## 2. Apply the Patches via Yocto

In Yocto, you must use a BitBake Append File (`.bbappend`) instead of editing the kernel source code directly.

1. Locate your kernel recipe (e.g., `meta-custom-board/recipes-kernel/linux/linux-imx_%.bbappend`).
2. Create a folder named `linux-imx` (or `files`) next to your `.bbappend` and place both patches inside it.
3. Edit the `.bbappend` file to include the patches:

   ```bash
   FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

   SRC_URI += " \
       file://0001-usb-dwc3-gadget-Disable-scatter-gather-for-i.MX8MP-U.patch \
       file://0002-usb-gadget-uvc-Add-framebased-format-support.patch \
   "
   ```

## 3. Manual Device Tree (DTS) Constraint

Since you are using a custom Device Tree, you must manually add the high-speed constraint to your DTS file to ensure stable connection.

1. Locate your custom `.dts` or `.dtsi` file.
2. Find the `&usb_dwc3_0` node.
3. Add the `maximum-speed` property:

   ```dts
   &usb_dwc3_0 {
       maximum-speed = "high-speed";
       ...
   }
   ```

## 4. Include v4l2loopback

To include `v4l2loopback` for video testing, add this to your `local.conf` or your custom image recipe:

```bitbake
IMAGE_INSTALL:append = " v4l2loopback"
```

## 5. Compile and Validate on Hardware

Compile the image:

```bash
bitbake virtual/kernel -c patch -f
bitbake virtual/kernel
bitbake core-image-minimal
```

Flash the resulting image to the board and boot.

1. Run `lsusb -t` — look for `480M` on the gadget port to confirm High-Speed mode.
2. Run `modprobe v4l2loopback video_nr=10` to ensure the module works.
3. Mount ConfigFS and create the endpoints to verify the `framebased` patch took effect:

   ```bash
   modprobe libcomposite
   mount -t configfs none /sys/kernel/config
   mkdir -p /sys/kernel/config/usb_gadget/test_gadget/functions/uvc.0/streaming/framebased
   ```

> [!IMPORTANT]
> If the `mkdir` command executes silently without emitting the `Operation not permitted` error, you have successfully enabled the required USB and UVC capabilities!
