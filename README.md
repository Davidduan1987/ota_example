# nRF Connect SDK 3.3.1 蓝牙 OTA 示例

本 tag 用于存放基于 nRF Connect SDK 3.3.1 的 `lbs_OTA` 示例工程、OTA 固件文件和客户使用说明。

> 注意：当前 NCS3.3.1 版本只在 `nrf54lm20dk/nrf54lm20b/cpuapp`，即 nRF54LM20B DK 上完成过构建和运行验证。

## 目录说明

- `lbs_OTA/`：完整示例工程源码。当前上传的工程版本为 `1.0.0+0`。
- `ota_file/`：OTA 新固件包。该文件夹中存放的是已经改好的新固件 `NEW_application.zip`，固件版本号为 `1.1.0+0`。
- `ota_video/`：DFU 录屏文件。该文件夹中的视频是在 iOS 环境下，分别使用 `nRF Connect` 和 `nRF Device Manager` APP 进行 DFU 的录屏。

## 当前工程说明

`lbs_OTA/` 是基于 Nordic LBS 示例修改的蓝牙 OTA 工程，已加入 MCUboot 和 MCUmgr Bluetooth SMP OTA 支持。
NCS3.3.1 版本新增了 nRF54LM20B DK 的 board overlay，并针对 Bluetooth SMP OTA 吞吐率做了配置优化。

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
- `lbs_OTA/boards/nrf54lm20dk_nrf54lm20b_cpuapp.overlay` 中将 nRF54LM20B 上默认分配给 RISC-V FLPR 的 SRAM/RRAM 空间交还给 ARM cpuapp 使用。
- `lbs_OTA/src/main.c` 中加入了 `#include <zephyr/app_version.h>`。
- `lbs_OTA/src/main.c` 中使用 `APP_VERSION_EXTENDED_STRING` 打印当前固件版本。
- `lbs_OTA/src/main.c` 中在 BLE 连接建立后主动请求 LE Data Length Update 和 2M PHY Update，用于提高 OTA 链路吞吐率。
- 构建时会由 `VERSION` 文件自动生成 `CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION`，该值会写入 MCUboot 签名镜像和 OTA 包元数据。

## OTA 传输速度优化

NCS3.3.1 版本参考 Nordic SMP Server Bluetooth overlay 的高吞吐配置，在 `lbs_OTA/prj.conf` 中特别加入或显式设置了下面这些宏：

- `CONFIG_MCUMGR_TRANSPORT_BT_CONN_PARAM_CONTROL=y`：MCUmgr 检测到 SMP 传输时自动调整 BLE connection parameters。
- `CONFIG_MCUMGR_TRANSPORT_BT_REASSEMBLY=y`：开启 MCUmgr Bluetooth reassembly，允许更大的 SMP 包分片重组。
- `CONFIG_MCUMGR_TRANSPORT_NETBUF_SIZE=2475`：把 MCUmgr netbuf 调大到可容纳 5 个最大 MTU 写命令组合的 SMP 包。
- `CONFIG_BT_L2CAP_TX_MTU=498`：提高 L2CAP TX MTU，减少 OTA 数据分片开销。
- `CONFIG_BT_BUF_ACL_RX_SIZE=502` 和 `CONFIG_BT_BUF_ACL_TX_SIZE=502`：匹配 498-byte MTU 的 ACL buffer 大小。
- `CONFIG_BT_CTLR_DATA_LENGTH_MAX=251`：启用 BLE controller 最大 Data Length。
- `CONFIG_BT_PHY_UPDATE=y`、`CONFIG_BT_CTLR_PHY_2M=y`、`CONFIG_BT_USER_PHY_UPDATE=y`：允许应用主动请求 2M PHY。
- `CONFIG_BT_DATA_LEN_UPDATE=y`、`CONFIG_BT_USER_DATA_LEN_UPDATE=y`：允许应用主动请求最大 Data Length。
- `CONFIG_BT_CTLR_LE_PING=n`：关闭 OTA 场景中不需要的 LE Ping controller 支持，减少资源占用。
- `CONFIG_MCUMGR_TRANSPORT_WORKQUEUE_STACK_SIZE=4608`：给 MCUmgr transport workqueue 留出足够栈空间。
- `CONFIG_NCS_SAMPLE_MCUMGR_BT_OTA_DFU_VALIDATION=n`：关闭 NCS sample 对默认 247-byte MTU speedup 配置的 warning 校验；本工程使用的是 Nordic SMP Server overlay 中更大的 498-byte MTU 配置。

实际吞吐率仍取决于手机或 PC central 是否接受 2M PHY、Data Length、连接间隔等协商结果。

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
