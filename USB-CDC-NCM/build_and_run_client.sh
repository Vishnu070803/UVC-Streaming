#!/bin/bash

echo "Checking and installing required dependencies for Host PC..."
sudo apt-get update
sudo apt-get install -y gcc pkg-config libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly

echo "Compiling SEI Client App..."
gcc sei_client.c -o sei_client $(pkg-config --cflags --libs gstreamer-1.0)

if [ $? -eq 0 ]; then
    echo "Build successful. Listening for stream..."
    ./sei_client
else
    echo "Build failed. Check your GStreamer includes."
fi
