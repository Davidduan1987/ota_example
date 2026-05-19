# nRF54L15 LBS OTA 示例（NCS 3.2.x）

本分支用于保存 nRF54L15 DK 上的 Bluetooth LBS + MCUboot OTA 示例工程，目标分支为 `NCS3.2.X`。

## 工程内容

- `src/main.c`：LBS 外设示例，包含蓝牙广播、LED、按键和固件版本打印。
- `prj.conf`：应用镜像配置，启用 BLE、LBS、DK library 和 MCUmgr Bluetooth OTA。
- `sysbuild.conf`：启用 sysbuild 下的 MCUboot。
- `sysbuild/mcuboot/prj.conf`：MCUboot 子镜像配置域，只影响 bootloader，不影响应用镜像。
- `VERSION`：应用固件版本号。
- `新固件/dfu_application.zip`：已生成的新固件包，可直接用于手机端 OTA 升级测试。

## MCUboot 配置域说明

NCS 使用 sysbuild 时，应用和 MCUboot 是两个不同的配置域：

- 应用配置域：`prj.conf`
- MCUboot 配置域：`sysbuild/mcuboot/prj.conf`
- MCUboot overlay：`sysbuild/mcuboot/app.overlay`

不要把 MCUboot 专用 Kconfig 配置随意放到应用 `prj.conf` 中，也不要在 MCUboot 配置域中加入未确认用途的宏。MCUboot 的 slot、签名、swap、启动地址、镜像校验等配置会直接影响启动和 OTA 行为，修改前必须确认该配置的作用和影响。

## 与 NCS3.3.0/lbs_OTA 的 MCUboot 配置差异

参考工程：

https://github.com/Davidduan1987/ota_example/tree/NCS3.3.0/lbs_OTA

主要差异如下：

- `NCS3.3.0/lbs_OTA` 的 MCUboot 配置更偏向 NCS 3.3.0 默认示例配置，`sysbuild/mcuboot/prj.conf` 中包含日志裁剪、picolibc、LTO、boot banner 等配置。
- 当前 `NCS3.2.X` 分支面向 NCS 3.2.x 工程验证，MCUboot 配置保持在当前可运行状态，不额外添加未验证的启动、swap、签名或 slot 相关宏。
- `NCS3.3.0/lbs_OTA` 中曾通过 MCUboot overlay 处理 bootloader 代码分区；当前分支保留当前工程可运行配置，不建议在未验证前直接照搬 NCS 3.3.0 分支的 MCUboot 配置。
- 两个分支都使用 sysbuild 管理 MCUboot 子镜像，但 NCS 版本不同，MCUboot 默认 Kconfig、Partition Manager 行为和板级 DTS 默认值可能存在差异，不能简单逐行复制。

## 固件版本修改方法

固件版本在工程根目录 `VERSION` 文件中修改：

```text
VERSION_MAJOR = 1
VERSION_MINOR = 0
PATCHLEVEL = 0
VERSION_TWEAK = 0
EXTRAVERSION =
```

例如要升级到 `1.0.1`，修改为：

```text
VERSION_MAJOR = 1
VERSION_MINOR = 0
PATCHLEVEL = 1
VERSION_TWEAK = 0
EXTRAVERSION =
```

修改版本后建议删除旧 `build` 目录并重新构建，确保 `dfu_application.zip` 中的镜像版本是新版本。

## 构建命令

在 NCS 3.2.x 工具链环境中构建：

```powershell
nrfutil toolchain-manager launch --ncs-version v3.2.1 -- west build -b nrf54l15dk/nrf54l15/cpuapp "C:\ncs\v3.2.1\prj\lbs_v32" --sysbuild -d "C:\ncs\v3.2.1\prj\lbs_v32\build" -p always
```

构建完成后，常用产物包括：

- `build/merged.hex`：完整烧录镜像。
- `build/dfu_application.zip`：手机端 OTA 升级包。

## 烧录命令

```powershell
nrfutil toolchain-manager launch --ncs-version v3.2.1 -- west flash -d "C:\ncs\v3.2.1\prj\lbs_v32\build" --erase
```

## OTA 注意事项

如果 OTA 时手机端 APP 进度直接到 `100%`，但设备端固件没有升级，通常说明新固件包生成有问题，而不是手机端传输流程真正完成升级。

常见处理方式：

1. 删除工程原始 `build` 文件夹。
2. 重新 pristine build。
3. 使用重新生成的 `build/dfu_application.zip` 做 OTA。
4. 确认 `VERSION` 文件中的版本号比设备当前运行版本更新。

当前工程中的 `新固件` 文件夹已经包含一个可用于 OTA 升级测试的 `dfu_application.zip`，可以直接用手机端 APP 选择该文件进行升级验证。
