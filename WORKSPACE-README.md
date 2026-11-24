# InkPad Notebook

This guide covers release 0.3.0.

Target: PB743K3.

Firmware reference: 6.10.2854.

## Install

Build `InkPad-Notebook.app`.

Copy it to `applications`.

Create `textEditor`.

Copy text files there.

Copy one keymap file:

`system/config/textEditor/map.keys`

Pair your keyboard first.

Launch the app.

Keep the older app.

The apps share documents.

Rollback means deleting this app.

Documents remain untouched.

## First launch

The app opens cards.

Each card shows metadata.

The first card creates documents.

Tap a folder card.

Tap a file card.

Choose **Read** for preview.

Choose **Edit** for typing.

## Touch editing

Tap text to position.

Drag across text.

Release to keep selection.

Long-press selects a word.

The selection menu opens.

It offers copy.

It offers cut.

It offers paste.

It offers bold.

It offers italic.

It offers headings.

Touch selection uses InkView.

It is not Android selection.

The reader must expose pointers.

## Closing menus

Open File, Edit, or View.

Choose **Close menu**.

Press Escape when available.

Touch outside may vary.

The explicit item remains.

## Page appearance

Open **View**.

Choose **White page**.

Choose **Ruled page**.

Choose **Grid page**.

Choose **Dark page**.

Choose a background preset.

Choose a font preset.

The app stores preferences.

Preferences are per reader.

InkPad Color shows colour.

Carta readers may dither.

## Formatting

Formatting writes Markdown.

Heading one writes `# `.

Heading two writes `## `.

Heading three writes `### `.

Bold writes `**text**`.

Italic writes `*text*`.

The Read action renders HTML.

TXT formatting creates Markdown.

The TXT source remains.

## Export

Open **File**.

Choose TXT export.

Choose Markdown export.

Choose HTML export.

Choose JSON export.

Files go here:

`textEditor/Exports/`

Exports never replace names.

Number suffixes are added.

JSON stores document content.

It stores source path.

It stores format metadata.

## Bluetooth sending

Choose **File > Bluetooth send**.

The app saves first.

It creates four exports.

It opens device selection.

Choose a paired device.

The app calls `BtSendFiles`.

The SDK declares this API.

Older firmware may omit it.

The fallback asks for MAC.

Use USB if rejected.

Keyboard pairing proves HID.

It does not prove OBEX.

The reader test remains required.

## Other sharing routes

USB remains the baseline.

Copy files from Exports.

PocketBook Cloud can upload.

Dropbox can synchronize.

Those routes use settings.

The app does not login.

Send-to-PocketBook receives email.

It does not send email.

No mail client exists.

## Keyboard

Install the French layout:

`kbLayouts/fr.keys`

It covers French accents.

It covers dead circumflex.

It covers dead diaeresis.

It covers AltGr euro.

Useful shortcuts:

| Shortcut | Action |
| --- | --- |
| Ctrl+N | New document |
| Ctrl+O | Documents |
| Ctrl+S | Save |
| Ctrl+Z | Undo |
| Ctrl+Y | Redo |
| Ctrl+A | Select all |
| Ctrl+C | Copy |
| Ctrl+X | Cut |
| Ctrl+V | Paste |
| Ctrl+B | Bold |
| Ctrl+I | Italic |
| Ctrl+F | Find |
| Escape | Documents |
| F1 | Help |

## Saving and recovery

Autosave follows pauses.

Long writing saves periodically.

Writes use temporary files.

Renames are atomic.

External changes block saves.

Use **Save copy** then.

The limit is 512 KiB.

Clipboard stays within app.

## Device matrix

Read `INKPAD.md` first.

PB743K3 uses extended keys.

PB743G uses extended keys.

Older models may need root.

InkPad Lite lacks Bluetooth.

The original InkPad lacks Bluetooth.

Do not mix SDK targets.

## Validation

Host tests pass.

They use InkView stubs.

They test state transitions.

They cannot prove firmware behavior.

Reader tests must cover:

1. Launch.
2. Pairing.
3. Typing.
4. Touch selection.
5. Menu closing.
6. Formatting.
7. Colour modes.
8. Export files.
9. Bluetooth sending.
10. Reopening documents.

Record model and firmware.

Keep failure messages.

## License

GPL-3.0.

The upstream license remains.
