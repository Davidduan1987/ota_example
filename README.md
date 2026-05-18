# nRF Connect SDK 3.3.0 Bluetooth OTA Example

This branch contains the OTA deliverables and customer instructions for the `lbs_OTA` example based on nRF Connect SDK 3.3.0.

## Contents

- `ota_file/`: OTA firmware package. The package in this folder is built from the current project and its firmware version is `1.1.0`.
- `ota_video/`: DFU screen recordings. The videos in this folder are recorded on iOS using the `nRF Connect` and `nRF Device Manager` apps.

## Firmware Version

The firmware version is controlled by the `VERSION` file in the application project:

```text
VERSION_MAJOR = 1
VERSION_MINOR = 1
PATCHLEVEL = 0
VERSION_TWEAK = 0
EXTRAVERSION =
```

This produces firmware version `1.1.0+0`.

The same version is used by:

- the runtime firmware log printed by the application;
- the MCUboot signed image version;
- the OTA DFU package metadata.

## How To Modify The Firmware Version

To release a new OTA firmware version, edit the project `VERSION` file:

- Change `VERSION_MAJOR` for a major release.
- Change `VERSION_MINOR` for a minor feature release.
- Change `PATCHLEVEL` for a bug fix release.
- Change `VERSION_TWEAK` for build or internal revision updates.

For example, to upgrade from `1.1.0+0` to `1.2.0+0`:

```text
VERSION_MAJOR = 1
VERSION_MINOR = 2
PATCHLEVEL = 0
VERSION_TWEAK = 0
EXTRAVERSION =
```

After changing the version, rebuild the project with a pristine build. The generated `dfu_application.zip` will contain the new signed firmware version.

## OTA File

Use the file in `ota_file/` for Bluetooth DFU testing.

Current OTA package:

```text
ota_file/dfu_application_1.1.0.zip
```

Firmware version:

```text
1.1.0+0
```

## iOS DFU Test Videos

The `ota_video/` folder is reserved for iOS DFU recordings:

- `nRF Connect` app DFU recording
- `nRF Device Manager` app DFU recording

These videos demonstrate the Bluetooth OTA update procedure in an iOS environment.
