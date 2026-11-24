# Build

## Which SDK

| Readers | Toolchain |
| --- | --- |
| InkPad 4, InkPad Color, PocketBook Color, Touch HD 3 | B288 SDK |
| InkPad Color 2, InkPad Color 3 | B300 SDK |
| InkPad 3, InkPad X, 2014 InkPad | Firmware SDK for that i.MX6 image |

Use Linux. Install CMake and the matching PocketBook SDK.

The CMake file asks for `PB_SDK_ROOT` and uses the `arm-obreey-linux-gnueabi` compilers shipped in the SDK.

## Configure

```sh
cmake -S . -B build \
  -DPB_SDK_ROOT=/path/to/SDK-B288
cmake --build build -j2
```

The executable is `build/bin/Typewriter.app`.

## Desktop check

Model matching does not need the SDK:

```sh
g++ -std=c++17 -I src/device -o /tmp/modeltest \
  tests/model_match_test.cpp \
  src/device/device_match.cpp \
  src/device/input_scan.cpp
/tmp/modeltest
```

## Install

Copy `Typewriter.app` to `applications` on the reader. Eject, then open it from the app menu.

This package is source only. No compiled binary is included. Do not install a binary you have not built against the firmware SDK.
