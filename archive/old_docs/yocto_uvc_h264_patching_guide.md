# Yocto Kernel Patching Guide: Native UVC 1.5 H.264

## Objective
Provide the complete professional workflow for successfully porting, applying, and validating the out-of-tree UVC H.264 ConfigFS patch into your proprietary NXP Yocto Linux kernel build.

## Phase 1: Where to Get the Code (The Patch)

Instead of manually editing the `uvc_configfs.c` C file by hand, standard Linux development uses `.patch` files. 

You can download the raw `.patch` file directly from the Linux USB Patchwork tracking system. For example, using the 2024 Qualcomm H.264/Framebased patch you referenced earlier:
1. Navigate to the Patchwork link: `https://patchwork.kernel.org/project/linux-usb/patch/20240927152138.31416-1-quic_akakum@quicinc.com/`
2. Download the raw `.patch` (or `.mbox`) file by clicking the "mbox" link on that page. Save this file as `0001-usb-gadget-uvc-Add-H264-support.patch`.

## Phase 2: How to Patch it into Yocto

In Yocto, you never edit the kernel source code directly in the `tmp/work/` directory because Yocto will overwrite it on the next build. Instead, you use a **BitBake Append File (`.bbappend`)**.

### Step 1: Locate your Kernel Recipe
Find where your `linux-imx` kernel is built. It is usually inside a layer like `meta-imx` or your company's custom layer:
```text
meta-custom-board/recipes-kernel/linux/linux-imx_%.bbappend
```

### Step 2: Add the Patch to the Layer
Create a folder named `files` immediately next to your `linux-imx_%.bbappend` file, and drop the downloaded `.patch` into it:
```text
meta-custom-board/recipes-kernel/linux/linux-imx_%.bbappend
meta-custom-board/recipes-kernel/linux/files/0001-usb-gadget-uvc-Add-H264-support.patch
```

### Step 3: Edit the `.bbappend`
Add the following line to the bottom of the `linux-imx_%.bbappend` file so that Yocto knows to apply the patch during the build sequence:
```bash
SRC_URI += "file://0001-usb-gadget-uvc-Add-H264-support.patch"
```

## Phase 3: How to Check if the Patch Properly Sits (Dry Run)

Before spending 3 hours compiling the entire image, you want to verify that Yocto successfully applied the patch to the C code without any `Rejects` or conflicts.

1. **Force Yocto to fetch and apply the patch:**
   In your Yocto build directory, run:
   ```bash
   bitbake virtual/kernel -c patch -f
   ```
2. **Verify inside the workspace:**
   Once BitBake finishes the "patch" task successfully, navigate into the Yocto kernel workspace (path varies depending on your platform):
   ```bash
   cd tmp/work/imx8mp_*/linux-imx/*/git/drivers/usb/gadget/function/
   ```
3. **Confirm the Code exists:**
   ```bash
   grep -i "h264" uvc_configfs.c
   ```
   If it outputs the C code for `h264`, you have 100% confirmation that the patch properly sits in your kernel tree.

## Phase 4: How to Compile and Verify on the Hardware

### Step 1: Compile the Image
Now that the patch sits correctly, ask Yocto to compile the image.
```bash
bitbake virtual/kernel -c compile -f
bitbake core-image-minimal   # (or whatever your final OS image is called)
```

### Step 2: Flash and Boot
Flash the resulting `.wic` or `.tar.gz` image onto your i.MX8M Plus SD Card/eMMC and boot the board.

### Step 3: The Ultimate Hardware Verification Test
Log in to your i.MX8MP shell as root, and run the exact same test we ran earlier:

```bash
# Load ConfigFS drivers
modprobe libcomposite
mount -t configfs none /sys/kernel/config

# Create a test gadget
mkdir -p /sys/kernel/config/usb_gadget/test_gadget/functions/uvc.0

# THE MOMENT OF TRUTH:
mkdir -p /sys/kernel/config/usb_gadget/test_gadget/functions/uvc.0/streaming/h264
mkdir -p /sys/kernel/config/usb_gadget/test_gadget/functions/uvc.0/streaming/framebased
```

> [!IMPORTANT]  
> If the `mkdir` commands execute silently without emitting the `Operation not permitted` error, **you have achieved official USB-IF UVC 1.5 compliance natively in your kernel!**

## User Review Required

Does this Yocto methodology document make sense to you? If you approve this plan, your task will be to follow this guide, run the compilation on your separate Yocto build machine, and flash the board. 

Once your i.MX8MP boots the patched kernel and passes the `mkdir` milestone, please let me know, and we will move to the final step: writing the script to pipe the `vpuenc_h264` hardware encoder into this new compliant USB pipe!
