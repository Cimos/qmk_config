# qmk_config

QMK keyboard definitions for the MadMan boards.

| Board | Folder |
|---|---|
| Corne (RP2040, OLED, per-key RGB) | `madman/corne` |
| Mad_RP2040 test boards | `madman/mad_rp2040/{two_key,anything,anything_split}` |

## Build

GitHub Actions builds every board on each push. The UF2 files are in the run's artifacts.
QMK is pinned in `.github/workflows/build.yml` (`QMK_REF`); bump it after reading the QMK changelog.

To build locally, copy `madman/` into `qmk_firmware/keyboards/` and run:

```
qmk compile -e SKIP_VERSION=yes -kb madman/corne -km via
```

`SKIP_VERSION=yes` matters: VIA wipes the stored keymap whenever the firmware build date
changes. Pinning it keeps your keymap across updates.

## Update the Corne

Each half is flashed through its own USB port. On the half plugged into the PC:

- From Windows: `tools/update.ps1 -Uf2 <file.uf2> -Both`. For each half it backs up the
  VIA keymap, macros and RGB, reboots into the bootloader, copies the UF2, then restores and
  checks everything. With `-Both` it then waits for you to move the cable and does the other
  half. A half already in boot mode is flashed and restored from the latest backup.
  Needs Python with `hidapi` in `tools/venv`.
- By hand: hold the top-outer key (Tab on the left, Backspace on the right) while plugging in,
  or press the BOOT button, then copy the UF2 onto the `RPI-RP2` drive.

Holding the top-outer key while plugging in also clears the stored settings, so restore from
a backup afterwards (`tools/via.py restore <backup.json>`).

## Screens

The half plugged into USB shows a PCB auto-router: nets route themselves between pads and
vias with 45-degree corners, faster as you type, and the board is wiped when it is full.
WPM, current layer and Caps Lock sit on the right. The other half shows the spaceship,
which flies faster the faster you type. `lib/pcbroute.c` has no QMK dependencies, so it can
be built and previewed on a PC.

## Layer colours

On any layer above BASE the keys light in the layer's colour (LOWR blue, RAIS green,
ADJ red) and keys with nothing on that layer go dark.
