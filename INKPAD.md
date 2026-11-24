# InkPad Notebook compatibility

PocketBook Color (PB633) is a 6 inch, 1072×1448 panel. An InkPad is not a larger PB633.

The typewriter now picks a profile from `GetDeviceModel()`.

| Reader | Model | Panel | Keyboard path | Root |
| --- | --- | --- | --- | --- |
| InkPad 4 | PB743G | 1404×1872, Carta | InkView extended keys | No |
| InkPad Color 3 | PB743K3 | 1404×1872, Kaleido | InkView extended keys | No |
| InkPad Color 2 | PB743C | 1404×1872, Kaleido | InkView extended keys | No |
| InkPad Color | PB741 | 1404×1872, Kaleido | Temporary `/tmp` node | Yes |
| InkPad 3 Pro | PB740-2 / PB740-3 | 1404×1872, Carta, stylus | Temporary `/tmp` node | Yes |
| InkPad 3 | PB740 | 1404×1872, Carta, stylus | Temporary `/tmp` node | Yes |
| InkPad X | PB1040 | 1404×1872, 227 dpi, stylus | Temporary `/tmp` node | Yes |
| InkPad Lite | PB970 | 825×1200 | No Bluetooth radio | — |
| InkPad (2014) | PB840 | 1200×1600 | No Bluetooth radio | — |
| PocketBook Color | PB633 | 1072×1448, Kaleido | Temporary `/tmp` node | Yes |

InkPad Eo runs Android, not InkView. This source does not target it.

## Why InkPad 4 was failing

Firmware 6 reports a Bluetooth keyboard as `EVT_KEYPRESS_EXT` (event type 40), `EVT_KEYRELEASE_EXT` (41) and `EVT_KEYREPEAT_EXT` (42). The key code is the Linux code in the first argument. Those same numbers were touch events on older firmware, so the app only treats them as keys on the InkPad 4 / Color 2 / Color 3 profile, and only when the second argument is 0.

The old scan kept any input node numbered above 6. On an InkPad that drops a keyboard on `event3`, and it can offer the Wacom stylus as a keyboard when that node is `event7`. The scan now keeps Bluetooth (`Bus=0005`) and USB HID (`Bus=0003`) devices, and it rejects gpio keys, the touchscreen and the digitizer.

Typing no longer blocks the InkView loop. Page keys (including the second pair on some bezels) and the menu key keep working. Escape still saves and closes input.

## Page size

Type is 14 pt, from the panel dpi, so an InkPad 4 line is not a blown-up 6 inch line. Kaleido panels (Color, Color 2, Color 3, PB633) run a full refresh every 96 characters and on each new page. Carta panels stay on partial updates.

The first launch writes an English keymap to `/mnt/ext1/system/config/textEditor/map.keys` if that file is missing. Replace it with [kbLayouts/de.keys](kbLayouts/de.keys) for German or [kbLayouts/fr.keys](kbLayouts/fr.keys) for French AZERTY.

French accented letters that sit on their own key (é, è, à, ç, ù) are stored as UTF-8. The circumflex and diaeresis keys are dead keys: press the accent, then the vowel, and the app writes â, ê, î, ô, û or ë, ï, ü. AltGr on E writes €.

## Files on the reader

Drag a `.txt` file onto the reader over USB and put it in `/mnt/ext1/textEditor`. Open that file from the list inside the app. The app does not replace `/dev/input`. On raw-input models it creates a temporary node under `/tmp` and removes that node when the handler is destroyed.

`main` owns the event handler. Only exit deletes it, and the pointer is cleared before that delete. Hide does not delete: the process is still running. The menu callback holds a plain pointer, not a second owner, and the destructor clears that pointer.

## Build

InkPad 4 and PB633 use the B288 SDK. The CMake file still expects that toolchain.

InkPad Color 2, Color 3 and other B300 readers need the B300 SDK when you link. Do not copy a B288 binary onto a B300 device and assume it will load.

InkPad 3 and InkPad X are i.MX6 devices. Build them with the SDK that matches their firmware, not the B288 tree.

Check the model in Settings, then About, before jailbreaking anything. Root methods differ by firmware. Do not run a jailbreak you have not matched to that version.

The target reader is PB743K3. Its reported firmware is U743K3.6.10.2854.

The release target is B300. Device testing still remains required.
