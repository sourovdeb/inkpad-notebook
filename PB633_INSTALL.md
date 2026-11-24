# Install

## Check the reader

Open Settings, then About. Write down the model and the firmware.

Use [INKPAD.md](INKPAD.md) for the keyboard path. InkPad 4, InkPad Color 2 and InkPad Color 3 do not need root. The other Bluetooth models do.

InkPad Lite and the 2014 InkPad have no Bluetooth radio. Stop there.

## Root, only when the table says so

Raw-input models read Linux keyboard events through a node created under `/tmp`.

That needs `/mnt/secure/su` from a jailbreak matched to the firmware. Do not reuse a jailbreak from another model or another firmware version.

The app removes its temporary node on exit. It does not replace `/dev/input`.

## Pair the keyboard

Enable Bluetooth in Settings.

Open PBTerm or an SSH shell. Run `bluetoothctl`. Scan, pair, trust and connect the keyboard.

The settings screen on many firmwares will not finish the pairing by itself.

## Copy the app

Build `Typewriter.app` with the SDK named in [BUILD_PB633.md](BUILD_PB633.md).

Connect the reader, open its storage, and copy the app into `applications`. Eject. Launch Typewriter.

On InkPad 4 and the later color InkPads, choose a file and type. Escape saves and closes input.

On raw-input models, choose the keyboard first. If the list is empty, the reader did not see a Bluetooth or USB keyboard. The stylus will not appear in that list.

## Drafts

Files are plain text under `textEditor`.

The app saves every 24 characters, on Enter, on Backspace and on Escape.

An English keymap is created at `system/config/textEditor/map.keys` if you do not provide one.

## If pairing fails

Do not repeat a jailbreak. Check the model, the firmware, Bluetooth, and, on raw-input models, that root exists. On InkPad 4, root is not the thing to chase.
