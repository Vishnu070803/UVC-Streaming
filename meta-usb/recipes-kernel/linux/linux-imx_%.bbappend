FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

# Phase 1: Stabilization Fixes
SRC_URI += " \
    file://0001-usb-dwc3-gadget-Disable-scatter-gather-for-i.MX8MP-U.patch \
    file://0002-usb-gadget-uvc-Add-framebased-format-support.patch \
"

# Note: The user mentioned they will add the 'maximum-speed = "high-speed"' 
# change to their custom DTS manually.
