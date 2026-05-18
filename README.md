# nRF Connect SDK 3.3.0 蓝牙 OTA 示例

本分支用于存放基于 nRF Connect SDK 3.3.0 的 `lbs_OTA` 示例工程、OTA 固件文件和客户使用说明。

## 目录说明

- `lbs_OTA/`：完整示例工程源码。当前上传的工程版本为 `1.0.0+0`。
- `ota_file/`：OTA 新固件包。该文件夹中存放的是已经改好的新固件 `NEW_application.zip`，固件版本号为 `1.1.0+0`。
- `ota_video/`：DFU 录屏文件。该文件夹中的视频是在 iOS 环境下，分别使用 `nRF Connect` 和 `nRF Device Manager` APP 进行 DFU 的录屏。

## 当前工程说明

`lbs_OTA/` 是基于 Nordic LBS 示例修改的蓝牙 OTA 工程，已加入 MCUboot 和 MCUmgr Bluetooth SMP OTA 支持。

当前上传的工程源码版本为 `1.0.0+0`，对应 `lbs_OTA/VERSION`：

```text
VERSION_MAJOR = 1
VERSION_MINOR = 0
PATCHLEVEL = 0
VERSION_TWEAK = 0
EXTRAVERSION =
```

工程中 OTA 和版本相关的主要修改包括：

- `lbs_OTA/prj.conf` 中启用了 `CONFIG_BOOTLOADER_MCUBOOT`、`CONFIG_NCS_SAMPLE_MCUMGR_BT_OTA_DFU`、`CONFIG_NCS_SAMPLE_MCUMGR_BT_OTA_DFU_SPEEDUP`。
- `lbs_OTA/sysbuild.conf` 中启用了 `SB_CONFIG_BOOTLOADER_MCUBOOT`。
- `lbs_OTA/boards/nrf54l15dk_nrf54l15_cpuapp.overlay` 和 `lbs_OTA/sysbuild/mcuboot/app.overlay` 中调整了 nRF54L15DK 的 MCUboot 分区，避免 FPROTECT 分区过大导致编译失败。
- `lbs_OTA/src/main.c` 中加入了 `#include <zephyr/app_version.h>`。
- `lbs_OTA/src/main.c` 中使用 `APP_VERSION_EXTENDED_STRING` 打印当前固件版本。
- 构建时会由 `VERSION` 文件自动生成 `CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION`，该值会写入 MCUboot 签名镜像和 OTA 包元数据。

## 固件版本机制

固件版本由工程根目录下的 `VERSION` 文件控制。构建时 Zephyr 会根据该文件生成版本宏，例如：

- `APP_VERSION_MAJOR`
- `APP_VERSION_MINOR`
- `APP_PATCHLEVEL`
- `APP_TWEAK`
- `APP_VERSION_STRING`
- `APP_VERSION_EXTENDED_STRING`

其中工程运行时打印使用的是 `APP_VERSION_EXTENDED_STRING`。

MCUboot OTA 签名版本由构建系统生成的 `CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION` 控制，它同样来自 `VERSION` 文件。

## 如何修改固件版本号

如果客户需要修改固件版本号，只需要修改 `lbs_OTA/VERSION` 文件：

- `VERSION_MAJOR`：主版本号，通常用于不兼容或较大的功能升级。
- `VERSION_MINOR`：次版本号，通常用于新增功能。
- `PATCHLEVEL`：补丁版本号，通常用于问题修复。
- `VERSION_TWEAK`：内部构建号或小版本修订号。

例如，如果要把工程固件从 `1.0.0+0` 升级到 `1.1.0+0`，可以修改为：

```text
VERSION_MAJOR = 1
VERSION_MINOR = 1
PATCHLEVEL = 0
VERSION_TWEAK = 0
EXTRAVERSION =
```

修改完成后，需要重新执行 pristine build。重新生成的 OTA zip 文件中会自动带上新的签名固件版本。

## OTA 固件文件

请使用 `ota_file/` 文件夹中的文件进行蓝牙 DFU 测试。

当前 OTA 新固件包：

```text
ota_file/NEW_application.zip
```

该 OTA 包中的固件版本：

```text
1.1.0+0
```

也就是说，本仓库中上传的工程源码版本是 `1.0.0+0`，`ota_file/NEW_application.zip` 是用于升级测试的新固件，版本是 `1.1.0+0`。

## iOS DFU 测试视频

`ota_video/` 文件夹用于存放 iOS 环境下的 DFU 录屏：

- `NRF_CONNECT.mp4`：使用 `nRF Connect` APP 进行 DFU 的录屏；
- `Device_manager.mp4`：使用 `nRF Device Manager` APP 进行 DFU 的录屏。

这些视频用于向客户展示在 iOS 环境下进行蓝牙 OTA 升级的完整流程。
