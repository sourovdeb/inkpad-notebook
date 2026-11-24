# InkPad Notebook build

Target: PocketBook InkPad Color 3  
Model: PB743K3  
Firmware: U743K3.6.10.2854  
SDK: PocketBook B300 6.8

The SDK supports firmware 6.8+.

## Build

Install CMake first.

Extract `SDK-B300-6.8.7z`.

Run the path updater:

```sh
SDK_ROOT=/path/to/SDK-B300-6.8
"$SDK_ROOT/bin/update_path.sh"
```

Build the application:

```sh
cmake -S . -B build-b300 \
  -DPB_SDK_ROOT="$SDK_ROOT" \
  -DPB_TARGET=B300 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-b300 --parallel 2
```

The result appears here:

`build-b300/bin/InkPad-Notebook.app`

## Host tests

Run the SDK-free tests:

```sh
tests/run_host_tests.sh
```

## Install

Connect through USB.

Copy the application here:

`/applications/InkPad-Notebook.app`

Copy the French layout:

`/system/config/textEditor/map.keys`

Create this folder:

`/textEditor`

Add text files there.

Disconnect the reader safely.

Pair the keyboard first.

Launch `InkPad Notebook` from Apps.

No jailbreak is required.

## Device test

Type this line:

`Bonjour été € — Noël à côté`

Then test these keys:

- Shift
- Caps Lock
- AltGr
- Backspace
- Enter
- Escape

Escape saves the file.

The diagnostic log appears here:

`/system/config/textEditor/logfile.txt`

The first 32 events appear.

They include `par2` values.

## Rollback

Delete this application:

`/applications/InkPad-Notebook.app`

Delete this configuration folder:

`/system/config/textEditor`

Text files remain untouched.
