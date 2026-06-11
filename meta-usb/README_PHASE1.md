# meta-usb: Phase 1 (Stabilization) Integration Guide

This phase implements the community-recommended fixes to stabilize the i.MX8MP USB controller before adding native H.264 features.

## Files to Copy
Copy the contents of this folder into your local `meta-usb` layer.

## Manual Actions Required
Since you are using a **custom Device Tree**, you must manually add the high-speed constraint to your DTS file:

1. Locate your custom `.dts` or `.dtsi` file.
2. Find the `&usb_dwc3_0` node.
3. Add the following property:
   ```dts
   &usb_dwc3_0 {
       maximum-speed = "high-speed";
       ...
   }
   ```

## Image Installation
To include `v4l2loopback` for video testing, add this to your `local.conf` or image recipe:
```bitbake
IMAGE_INSTALL:append = " v4l2loopback"
```

## Build Commands
```bash
bitbake virtual/kernel -c patch -f
# Verify 0001 patch applied to drivers/usb/dwc3/gadget.c
bitbake virtual/kernel
```

## Verification
1. Boot the board.
2. Run `lsusb -t` — look for `480M` on the gadget port.
3. Run `modprobe v4l2loopback video_nr=10` to ensure the module works.
