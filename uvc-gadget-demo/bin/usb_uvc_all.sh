#!/bin/sh

# Teardown existing gadget if it exists to prevent "File exists" errors
if [ -d /sys/kernel/config/usb_gadget/g1 ]; then
    echo "Tearing down existing USB gadget..."
    echo "" > /sys/kernel/config/usb_gadget/g1/UDC 2>/dev/null || true
    find /sys/kernel/config/usb_gadget/g1 -type l -exec rm {} + 2>/dev/null || true
    find /sys/kernel/config/usb_gadget/g1 -type d -depth -exec rmdir {} + 2>/dev/null || true
fi

mkdir -p /sys/kernel/config/usb_gadget/g1
cd /sys/kernel/config/usb_gadget/g1
echo "0x1d6b" > idVendor
echo "0x0104" > idProduct
echo "0x0200" > bcdUSB
echo "0xEF" > bDeviceClass
echo "0x2" > bDeviceSubClass
echo "0x1" > bDeviceProtocol
mkdir -p strings/0x409
echo "0123456789" > strings/0x409/serialnumber
echo "NXP" > strings/0x409/manufacturer
echo "i.MX8MP Multi-Format UVC" > strings/0x409/product

mkdir -p functions/uvc.usb0

# Helper function to create resolutions
create_resolutions() {
    FORMAT_DIR=$1
    BPP=$2

    # Array of resolutions "width height name"
    RESO_LIST="
    256 144 144p
    426 240 240p
    640 360 360p
    854 480 480p
    1280 720 720p
    1920 1080 1080p
    "

    echo "$RESO_LIST" | while read w h name; do
        if [ -z "$w" ]; then continue; fi
        RES_DIR="$FORMAT_DIR/$name"
        mkdir -p "$RES_DIR"
        echo $w > "$RES_DIR/wWidth"
        echo $h > "$RES_DIR/wHeight"
        
        # Calculate sizes and bitrates
        if [ "$BPP" = "0" ]; then
            # Framebased (compressed)
            echo 0 > "$RES_DIR/dwBytesPerLine"
            echo 31104000 > "$RES_DIR/dwMinBitRate"
            echo 31104000 > "$RES_DIR/dwMaxBitRate"
        else
            # Uncompressed/MJPEG
            SIZE=$(( w * h * BPP ))
            echo $SIZE > "$RES_DIR/dwMaxVideoFrameBufferSize"
        fi

        # Write frame intervals (60, 55, 50, 45, 40, 35, 30, 28, 25, 24, 20, 15, 10, 5)
        cat <<EOF > "$RES_DIR/dwFrameInterval"
166666
181818
200000
222222
250000
285714
333333
357142
400000
416666
500000
666666
1000000
2000000
EOF
    done
}

# ================= YUYV (Uncompressed, 2 BPP) =================
mkdir -p functions/uvc.usb0/streaming/uncompressed/u
create_resolutions "functions/uvc.usb0/streaming/uncompressed/u" "2"

# ================= MJPEG (1 BPP approx) =================
mkdir -p functions/uvc.usb0/streaming/mjpeg/m
create_resolutions "functions/uvc.usb0/streaming/mjpeg/m" "1"

# ================= H.264 (Framebased) =================
mkdir -p functions/uvc.usb0/streaming/framebased/h264
printf 'H264\x00\x00\x10\x00\x80\x00\x00\xaa\x00\x38\x9b\x71' > functions/uvc.usb0/streaming/framebased/h264/guidFormat
create_resolutions "functions/uvc.usb0/streaming/framebased/h264" "0"

# ================= H.265 / HEVC (Framebased) =================
mkdir -p functions/uvc.usb0/streaming/framebased/hevc
printf 'HEVC\x00\x00\x10\x00\x80\x00\x00\xaa\x00\x38\x9b\x71' > functions/uvc.usb0/streaming/framebased/hevc/guidFormat
create_resolutions "functions/uvc.usb0/streaming/framebased/hevc" "0"

# ================= Link Everything =================
mkdir -p functions/uvc.usb0/streaming/header/h
cd functions/uvc.usb0/streaming/header/h
ln -s ../../uncompressed/u
ln -s ../../mjpeg/m
ln -s ../../framebased/h264
ln -s ../../framebased/hevc
cd ../../class/fs
ln -s ../../header/h
cd ../../class/hs
ln -s ../../header/h
cd ../../class/ss
ln -s ../../header/h

cd ../../../control
mkdir -p header/h
ln -s header/h class/fs
ln -s header/h class/ss

cd /sys/kernel/config/usb_gadget/g1
echo 3072 > functions/uvc.usb0/streaming_maxpacket
echo 0 > functions/uvc.usb0/streaming_maxburst
echo 1 > functions/uvc.usb0/streaming_interval

mkdir -p configs/c.1
mkdir -p configs/c.1/strings/0x409
echo "VIDEO UVC" > configs/c.1/strings/0x409/configuration

ln -s functions/uvc.usb0 configs/c.1

echo "38100000.usb" > UDC
echo "UVC Multi-Format (All Resolutions) enabled!"
