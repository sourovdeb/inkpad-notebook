# Upstream and changes

Base project: Pocketbook-Texteditor.

Original author: JuanJakobo.

Upstream commit: `6e8935368ac14b230c69898391e54c83b0fa11aa`.

Upstream LICENSE contains GPL version 3.

This package preserves that license. The earlier MIT description was incorrect.

## Prototype changes

The app label follows the detected reader. InkPad names come from `GetDeviceModel()`.

InkPad 4, InkPad Color 2 and InkPad Color 3 use InkView extended key events. That path does not create a device node and does not need root.

Other Bluetooth readers still use a temporary node under `/tmp`. The app deletes that node on exit. It does not replace `/dev/input`.

Keyboard detection uses the USB or Bluetooth bus. Touch, gpio keys and Wacom nodes are ignored. Event numbers below 7 are no longer discarded.

Drafts save during typing. Unknown keys no longer open a dialog. Shift and AltGr follow press and release. The default English keymap is written on first launch.

Type size follows panel dpi. Color panels refresh fully on a slow cycle.

## Compatibility status

Upstream tested PocketBook Touch HD 3 with a Logitech K380.

InkPad profiles are adapted from public firmware notes. They have not been run on a reader.

PB633 remains unverified. Root and a firmware-matched jailbreak are still required on the raw-input models.
