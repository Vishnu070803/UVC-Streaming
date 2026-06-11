# Development Guide

This guide details the structure of the C code and how to extend it, particularly focusing on the SEI metadata structures.

## Codebase Structure
The core application lives in `uvc-gadget/applications/sei_injector/uvc_gadget_sei.c`. It interfaces closely with:
- `include/sei_metadata.h`: Defines the core metadata structures.
- `include/icm20948_ioctl.h`: Defines the IOCTL interface to the IMU driver.
- The GStreamer pad probe (usually defined in a header like `uvc_sei_injection.h`), which handles the SEI assembly and IMU reading.

## Modifying Metadata Fields
If you need to add new sensor data (e.g., Temperature, GPS) to the SEI stream, you must modify the metadata structure.

### 1. Update `sei_metadata.h`
Modify the `SeiMetadata` struct to include your new fields. 
```c
typedef struct {
    uint64_t frame_pts_ns;
    uint64_t imu_timestamp_ns;
    // ... existing fields ...
    float temperature_c; // Example new field
} SeiMetadata;
```
> [!WARNING]
> Both the Injector (on the board) and the Extractor (on the PC) MUST use the exact same `sei_metadata.h` definition. If they fall out of sync, the Extractor will read garbage data.

### 2. Update the Injector Logic
In the injection probe, populate the new field before the structure is encoded into the SEI byte array.
```c
meta.temperature_c = read_temperature();
```

### 3. Update the Extractor Logic
In `uvc-gadget/tools/sei_extractor/uvc_sei_extractor.c`, update the `printf` or CSV logging statements to output your new field after the struct has been extracted from the payload.

## Building the Code
The project uses standard `make`. 
- **Injector**: `cd uvc-gadget/applications/sei_injector && make`
- **Extractor**: `cd uvc-gadget/tools/sei_extractor && make`
