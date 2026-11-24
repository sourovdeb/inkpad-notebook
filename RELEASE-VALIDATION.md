# Release validation

Date: 2026-11-24.

Release: InkPad Notebook 0.3.0.

Target: PB743K3.

## Evidence completed

- Host compiler tests pass.
- Strict warning flags pass.
- Model tests pass.
- Keymap tests pass.
- UTF-8 tests pass.
- Save tests pass.
- Export tests pass.
- Touch fixture tests pass.
- Menu-close tests pass.
- Appearance tests pass.
- JSON tests pass.
- ARM B300 build passes.
- ELF target is ARM EABI5.
- GPL-3.0 remains.

The fixture uses stubs.

It is not firmware.

## New paths covered

- Pointer down.
- Pointer move.
- Pointer long press.
- Pointer up.
- Drag selection.
- Word selection.
- Context menu actions.
- Explicit menu close.
- Escape menu close.
- Ruled lines.
- Grid lines.
- Page colours.
- Font colours.
- JSON export.
- Four-file Bluetooth bundle.

## Not proven here

- InkView menu appearance.
- InkView touch timing.
- Screen keyboard callbacks.
- HTML preview return.
- Bluetooth device selection.
- Bluetooth file transfer.
- E-ink refresh quality.
- Colour rendering.
- Battery and crash behavior.

The SDK build used B300 6.8.

No reader ran this build.

Do not call it ready.

Call it a test candidate.

## Required reader test

1. Confirm the model.
2. Confirm the firmware.
3. Pair the keyboard.
4. Launch the app.
5. Open a document.
6. Drag-select text.
7. Long-press a word.
8. Copy and paste.
9. Close each menu.
10. Apply each format.
11. Check each page mode.
12. Check each colour.
13. Export four formats.
14. Send the bundle.
15. Reopen each file.

Record each result.

Keep error text.
