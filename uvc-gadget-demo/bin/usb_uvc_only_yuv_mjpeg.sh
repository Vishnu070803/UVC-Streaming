#!/bin/sh

mkdir /sys/kernel/config/usb_gadget/g1
cd /sys/kernel/config/usb_gadget/g1
echo "0x1d6b" > idVendor
echo "0x0104" > idProduct
echo "0x0200" > bcdUSB
echo "0xEF" > bDeviceClass
echo "0x2" > bDeviceSubClass
echo "0x1" > bDeviceProtocol
mkdir strings/0x409
echo "0123456789" > strings/0x409/serialnumber
echo "Foo Inc." > strings/0x409/manufacturer
echo "Bar Gadget" > strings/0x409/product

mkdir functions/uvc.usb0

mkdir -p functions/uvc.usb0/streaming/mjpeg/m/480p
echo 640 > functions/uvc.usb0/streaming/mjpeg/m/480p/wWidth
echo 480 > functions/uvc.usb0/streaming/mjpeg/m/480p/wHeight
echo 460800 > functions/uvc.usb0/streaming/mjpeg/m/480p/dwMaxVideoFrameBufferSize
cat <<EOF > functions/uvc.usb0/streaming/mjpeg/m/480p/dwFrameInterval
333333
666666
10000000
EOF

mkdir -p functions/uvc.usb0/streaming/mjpeg/m/720p
echo 1280 > functions/uvc.usb0/streaming/mjpeg/m/720p/wWidth
echo 720 > functions/uvc.usb0/streaming/mjpeg/m/720p/wHeight
echo 1382400 > functions/uvc.usb0/streaming/mjpeg/m/720p/dwMaxVideoFrameBufferSize
cat <<EOF > functions/uvc.usb0/streaming/mjpeg/m/720p/dwFrameInterval
333333
666666
10000000
EOF

mkdir -p functions/uvc.usb0/streaming/mjpeg/m/1080p
echo 1920 > functions/uvc.usb0/streaming/mjpeg/m/1080p/wWidth
echo 1080 > functions/uvc.usb0/streaming/mjpeg/m/1080p/wHeight
echo 3110400 > functions/uvc.usb0/streaming/mjpeg/m/1080p/dwMaxVideoFrameBufferSize
cat <<EOF > functions/uvc.usb0/streaming/mjpeg/m/1080p/dwFrameInterval
333333
666666
10000000
EOF

mkdir -p functions/uvc.usb0/streaming/uncompressed/u/480p
echo 640 > functions/uvc.usb0/streaming/uncompressed/u/480p/wWidth
echo 480 > functions/uvc.usb0/streaming/uncompressed/u/480p/wHeight
echo 614400 > functions/uvc.usb0/streaming/uncompressed/u/480p/dwMaxVideoFrameBufferSize
cat <<EOF > functions/uvc.usb0/streaming/uncompressed/u/480p/dwFrameInterval
333333
666666
1000000
EOF

mkdir -p functions/uvc.usb0/streaming/uncompressed/u/720p
echo 1280 > functions/uvc.usb0/streaming/uncompressed/u/720p/wWidth
echo 720 > functions/uvc.usb0/streaming/uncompressed/u/720p/wHeight
echo 1843200 > functions/uvc.usb0/streaming/uncompressed/u/720p/dwMaxVideoFrameBufferSize
cat <<EOF > functions/uvc.usb0/streaming/uncompressed/u/720p/dwFrameInterval
333333
666666
1000000
EOF

mkdir -p functions/uvc.usb0/streaming/uncompressed/u/1080p
echo 1920 > functions/uvc.usb0/streaming/uncompressed/u/1080p/wWidth
echo 1080 > functions/uvc.usb0/streaming/uncompressed/u/1080p/wHeight
echo 4147200 > functions/uvc.usb0/streaming/uncompressed/u/1080p/dwMaxVideoFrameBufferSize
cat <<EOF > functions/uvc.usb0/streaming/uncompressed/u/1080p/dwFrameInterval
333333
666666
1000000
EOF

mkdir -p functions/uvc.usb0/streaming/framebased/h/720p
echo 1280 > functions/uvc.usb0/streaming/framebased/h/720p/wWidth
echo 720 > functions/uvc.usb0/streaming/framebased/h/720p/wHeight
echo 0 > functions/uvc.usb0/streaming/framebased/h/720p/dwBytesPerLine
echo 29491200 > functions/uvc.usb0/streaming/framebased/h/720p/dwMinBitRate
echo 29491200 > functions/uvc.usb0/streaming/framebased/h/720p/dwMaxBitRate
cat <<EOF > functions/uvc.usb0/streaming/framebased/h/720p/dwFrameInterval
333333
666666
1000000
EOF

mkdir -p functions/uvc.usb0/streaming/framebased/h/1080p
echo 1920 > functions/uvc.usb0/streaming/framebased/h/1080p/wWidth
echo 1080 > functions/uvc.usb0/streaming/framebased/h/1080p/wHeight
echo 0 > functions/uvc.usb0/streaming/framebased/h/1080p/dwBytesPerLine
echo 31104000 > functions/uvc.usb0/streaming/framebased/h/1080p/dwMinBitRate
echo 31104000 > functions/uvc.usb0/streaming/framebased/h/1080p/dwMaxBitRate
cat <<EOF > functions/uvc.usb0/streaming/framebased/h/1080p/dwFrameInterval
333333
666666
1000000
EOF

# Write H264 GUID (16 raw bytes)
printf 'H264\x00\x00\x10\x00\x80\x00\x00\xaa\x00\x38\x9b\x71' > functions/uvc.usb0/streaming/framebased/h/guidFormat

mkdir functions/uvc.usb0/streaming/header/h
cd functions/uvc.usb0/streaming/header/h
ln -s ../../uncompressed/u
ln -s ../../mjpeg/m
ln -s ../../framebased/h
cd ../../class/fs
ln -s ../../header/h
cd ../../class/hs
ln -s ../../header/h
cd ../../class/ss
ln -s ../../header/h

cd ../../../control
mkdir header/h
ln -s header/h class/fs
#ln -s header/h class/hs
ln -s header/h class/ss

cd /sys/kernel/config/usb_gadget/g1
echo 3072 > functions/uvc.usb0/streaming_maxpacket
echo 0 > functions/uvc.usb0/streaming_maxburst
echo 1 > functions/uvc.usb0/streaming_interval

mkdir configs/c.1
mkdir configs/c.1/strings/0x409
echo "VIDEO UVC" > configs/c.1/strings/0x409/configuration

ln -s functions/uvc.usb0 configs/c.1

#echo "ci_hdrc.0" > UDC
echo "38100000.usb" > UDC
echo "UVC enabled"

