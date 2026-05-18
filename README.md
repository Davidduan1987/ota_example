# nRF Connect SDK 3.3.0 蓝牙 OTA 示例

本分支用于存放基于 nRF Connect SDK 3.3.0 的 `lbs_OTA` 示例工程 OTA 文件和客户使用说明。

## 目录说明

- `ota_file/`：OTA 固件包。该文件夹中存放的是当前工程的新固件，固件版本号为 `1.1.0`。
- `ota_video/`：DFU 录屏文件。该文件夹中的视频是在 iOS 环境下，分别使用 `nRF Connect` 和 `nRF Device Manager` APP 进行 DFU 的录屏。

## 固件版本

固件版本由应用工程根目录下的 `VERSION` 文件控制：

```text
VERSION_MAJOR = 1
VERSION_MINOR = 1
PATCHLEVEL = 0
VERSION_TWEAK = 0
EXTRAVERSION =
```

以上配置生成的固件版本为 `1.1.0+0`。

该版本会同时用于：

- 应用启动时打印的运行时固件版本；
- MCUboot 签名镜像中的固件版本；
- OTA DFU 包中的版本信息。

## 如何修改固件版本号

如果客户需要发布新的 OTA 固件版本，只需要修改工程根目录下的 `VERSION` 文件：

- `VERSION_MAJOR`：主版本号，通常用于不兼容或较大的功能升级。
- `VERSION_MINOR`：次版本号，通常用于新增功能。
- `PATCHLEVEL`：补丁版本号，通常用于问题修复。
- `VERSION_TWEAK`：内部构建号或小版本修订号。

例如，如果要把固件从 `1.1.0+0` 升级到 `1.2.0+0`，可以修改为：

```text
VERSION_MAJOR = 1
VERSION_MINOR = 2
PATCHLEVEL = 0
VERSION_TWEAK = 0
EXTRAVERSION =
```

修改完成后，需要重新执行 pristine build。重新生成的 `dfu_application.zip` 中会自动带上新的签名固件版本。

## OTA 固件文件

请使用 `ota_file/` 文件夹中的文件进行蓝牙 DFU 测试。

当前 OTA 固件包：

```text
ota_file/dfu_application_1.1.0.zip
```

当前固件版本：

```text
1.1.0+0
```

## iOS DFU 测试视频

`ota_video/` 文件夹用于存放 iOS 环境下的 DFU 录屏：

- 使用 `nRF Connect` APP 进行 DFU 的录屏；
- 使用 `nRF Device Manager` APP 进行 DFU 的录屏。

这些视频用于向客户展示在 iOS 环境下进行蓝牙 OTA 升级的完整流程。
