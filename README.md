# InkPad Notebook

InkPad Notebook is a PocketBook writing app.

It targets InkView readers.

The main target is PB743K3.

That model is InkPad Color 3.

The app also documents PB743G.

PB743G is InkPad 4.

Check your model first.

## What it provides

- Bluetooth keyboard typing.
- Document cards on launch.
- Touch cursor placement.
- Touch drag selection.
- Long-press word selection.
- Copy, cut, and paste.
- Bold and italic markers.
- Heading levels one to three.
- TXT and Markdown files.
- HTML reading preview.
- TXT, Markdown, HTML exports.
- JSON data export.
- Bluetooth export bundle sending.
- White page mode.
- Ruled page mode.
- Grid page mode.
- Dark page mode.
- Background colour presets.
- Font colour presets.
- Portrait and landscape layouts.
- Autosave and recovery checks.

## Current status

The user validated typing.

The user validated connectivity.

Host tests pass.

ARM B300 build passes.

Reader tests remain required.

Build with the SDK.

The PB743K3 binary is packaged.

It remains hardware-unverified.

## Repository map

| Path | Purpose |
| --- | --- |
| `src/workspace.cpp` | InkView interface |
| `src/core/workspace_core.*` | Text and export logic |
| `src/device/*` | Models and keymaps |
| `kbLayouts/fr.keys` | French layout |
| `tests/*` | Host tests |
| `BUILD_PB743K3.md` | PB743K3 build |
| `INKPAD.md` | Device matrix |
| `WORKSPACE-README.md` | Full user guide |
| `RELEASE-VALIDATION.md` | Evidence and limits |
| `docs/DEVICE-TEST.md` | Reader test sheet |

## Reader install

Build `InkPad-Notebook.app`.

Copy it to `applications`.

Create `textEditor`.

Copy text files there.

Copy a keymap file.

Use this path:

`system/config/textEditor/map.keys`

Pair the keyboard first.

Launch InkPad Notebook.

Keep your existing app.

This app uses `textEditor`.

It does not alter files.

It does not need root.

That applies to PB743K3.

Other models differ.

Read `INKPAD.md` first.

## Build on Linux

Install CMake first.

Download PocketBook B300 SDK.

Extract `SDK-B300-6.8`.

Set `PB_SDK_ROOT`.

Run the path script.

```sh
sh tests/run_host_tests.sh

export PB_SDK_ROOT=/path/SDK-B300-6.8

"$PB_SDK_ROOT/bin/update_path.sh"

cmake -S . -B build-b300 \
  -DPB_SDK_ROOT="$PB_SDK_ROOT" \
  -DPB_TARGET=B300 \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build-b300 \
  --target InkPad-Notebook.app \
  --parallel 2
```

The output is:

`build-b300/bin/InkPad-Notebook.app`

The package is:

`dist/InkPad-Notebook-0.3.0-PB743K3-install.zip`

## Build on Windows

Use WSL2.

Install CMake in WSL.

Keep the SDK in WSL.

Run the Linux commands.

Copy the app through Explorer.

Do not use B288.

PB743K3 needs B300.

## Menus and touch

Tap a toolbar item.

Choose **Close menu**.

Press Escape to close.

Drag across text.

Release to keep selection.

Long-press a word.

The selection menu opens.

Choose copy or cut.

Choose a format action.

The menu uses native InkView.

Touch events use InkView.

The screen is not resizable.

Orientation changes layout.

Text size has controls.

## Page appearance

Open **View**.

Choose a page mode.

Choose a background colour.

Choose a font colour.

Settings persist locally.

InkPad Color renders colour.

Grayscale readers dither colour.

## Export and sharing

Open **File**.

Choose an export format.

Files go to:

`textEditor/Exports/`

The Bluetooth action creates:

- TXT
- Markdown
- HTML
- JSON

It then opens devices.

Choose a paired device.

The app calls `BtSendFiles`.

Older firmware may lack it.

If absent, use USB.

Cloud and Dropbox remain external.

Send-to-PocketBook receives email.

It is not outbound email.

## File formats

| Format | Use |
| --- | --- |
| TXT | Plain exchange |
| Markdown | Editable formatting |
| HTML | Reading preview |
| JSON | Data export |

Markdown supports headings.

It supports bold markers.

It supports italic markers.

Complex Markdown remains source.

DOCX and PDF need conversion.

The legacy target is optional.

Set `BUILD_LEGACY_TYPEWRITER=ON`.

## Limits

The document limit is 512 KiB.

Clipboard stays inside app.

No mail client exists.

No WYSIWYG claim exists.

Bluetooth transfer needs hardware proof.

Native menus need hardware proof.

Native preview needs hardware proof.

## Device acceptance

1. Confirm the model.
2. Pair the keyboard.
3. Launch the app.
4. Open an existing file.
5. Type French text.
6. Select text by touch.
7. Copy and paste.
8. Apply a format.
9. Change page mode.
10. Export every format.
11. Send by Bluetooth.
12. Reopen the files.

Report failures with:

- Model number.
- Firmware number.
- Menu name.
- Exact action.
- Resulting message.

## Sources

- [PocketBook manual](https://support.pocketbook-int.com/fw/743K3/u/6.10.2854/manual/User_Manual_InkPad_Color_3_EN.pdf)
- [PocketBook device page](https://pocketbook.ch/en-ch/catalog/color-e-readers/pocketbook-inkpad-color-3-ch)
- [PocketBook SDK](https://github.com/pocketbook/SDK_6.3.0/releases/tag/6.8)
- [InkView simulator header](https://github.com/esix/pocketbook-simulator/blob/master/include/inkview.h)
- [Upstream text editor](https://github.com/JuanJakobo/Pocketbook-Texteditor)

## License

GPL-3.0.

See [LICENSE](LICENSE).
