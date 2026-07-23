# nRF54L15 外部 Flash OTA 示例（NCS 3.2.x）

本分支用于保存 nRF54L15 DK 上的 Bluetooth LBS + MCUboot OTA 示例工程。当前工程使用外部 SPI NOR flash 作为 MCUboot secondary slot，用于接收蓝牙 OTA 下载的新固件。

本工程已在 NCS 3.2.3 环境中使用 nRF54L DK 测试。

## 工程内容

- `src/main.c`：外部 flash 读写测试、Bluetooth LBS 广播、BLE OTA，以及 Button0/Button1 低功耗控制逻辑。
- `prj.conf`：应用镜像配置，启用 SPI NOR、flash map、BLE、LBS 和 MCUmgr Bluetooth OTA。
- `sysbuild.conf`：启用 sysbuild 下的 MCUboot，并配置 external flash 作为 OTA secondary slot。
- `sysbuild/mcuboot/prj.conf`：MCUboot 子镜像配置域，只影响 bootloader。
- `boards/nrf54l15dk_nrf54l15_cpuapp.overlay`：启用 DK 板载外部 flash，并作为 Partition Manager external flash。
- `pm_static.yml` / `pm_static_nrf54l15dk_nrf54l15_cpuapp.yml`：固定 Partition Manager memory layout。
- `新固件/dfu_application.zip`：用于手机端 OTA 测试的新固件包。

## 功能说明

当前程序会周期性擦写外部 flash 的 `ido_storage_partition` 分区，并通过 BLE LBS 服务广播。

按键低功耗逻辑：

- Button0：进入低功耗空闲状态，主循环停止，BLE 停止广播，已有连接会断开，外部 flash 进入 DPD，SPI 外设进入 suspend。
- Button1：退出低功耗空闲状态，恢复 SPI 和外部 flash，重新开始 BLE 广播，主循环继续运行。

## OTA 说明

本工程的 OTA secondary slot 位于外部 SPI NOR flash。手机端通过 MCUmgr Bluetooth SMP 传输新固件，应用侧 MCUmgr 写入外部 flash，复位后由 MCUboot 完成镜像升级。

`新固件/dfu_application.zip` 是已经生成好的测试 OTA 包，可以直接使用 nRF Connect 或 nRF Device Manager 进行升级测试。

## MCUboot 配置域说明

NCS 使用 sysbuild 时，应用和 MCUboot 是两个不同的配置域：

- 应用配置域：`prj.conf`
- MCUboot 配置域：`sysbuild/mcuboot/prj.conf`
- MCUboot overlay：`sysbuild/mcuboot/app.overlay` 和 `sysbuild/mcuboot/boards/*.overlay`

不要把 MCUboot 专用 Kconfig 配置随意放到应用 `prj.conf` 中。MCUboot 的 slot、签名、swap、启动地址、镜像校验等配置会直接影响启动和 OTA 行为，修改前需要确认该配置的作用和影响。

## 外部 Flash 关键配置

应用侧需要启用 SPI NOR 和 flash map：

```text
CONFIG_FLASH=y
CONFIG_FLASH_MAP=y
CONFIG_FLASH_PAGE_LAYOUT=y
CONFIG_SPI=y
CONFIG_SPI_NOR=y
CONFIG_SPI_NOR_SFDP_DEVICETREE=y
CONFIG_SPI_NOR_FLASH_LAYOUT_PAGE_SIZE=4096
```

应用侧和 MCUboot 侧都需要保证外部 flash devicetree 配置一致，否则 OTA 写入 external secondary slot 时可能失败。

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

## 已知问题修复：BLE 广播启动失败（err -11 / EAGAIN）

现象：设备启动后 BLE 广播一直报 `Advertising failed to start (err -11)`，扫不到任何广播。

根因：工程开启了 `CONFIG_BT_SETTINGS=y`（BLE 绑定信息持久化），此时 Zephyr 的 `bt_init()` 在首次启动、设备还没有已保存的身份地址（ID address）时，会打印一行 `No ID address. App must call settings_load()` 后直接返回，**不会**设置 `BT_DEV_READY` 标志位——它是故意等应用层调用 `settings_load()` 之后才由 settings 子系统补上这个标志。`bluetooth_init()` 里原来只调用了 `bt_enable(NULL)`，从未调用 `settings_load()`，导致 `BT_DEV_READY` 永远不会被置位，`bt_le_adv_start()` 因此必然返回 `-EAGAIN`（即 -11）。

修复：在 `bt_enable()` 成功后补上 `settings_load()` 调用（`src/main.c` 的 `bluetooth_init()`）：

```c
err = bt_enable(NULL);
if (err) {
	printk("Bluetooth init failed (err %d)\n", err);
	return err;
}

if (IS_ENABLED(CONFIG_SETTINGS)) {
	settings_load();
}

err = bt_lbs_init(&lbs_callbacks);
```

同时需要包含头文件 `#include <zephyr/settings/settings.h>`。

## OTA 注意事项

如果 OTA 时手机端 APP 进度直接到 `100%`，但设备端固件没有升级，通常说明新固件包生成或版本号配置有问题，而不是手机端传输流程真正完成升级。

常见处理方式：

1. 删除工程原始 `build` 文件夹。
2. 修改 `VERSION` 文件，确保新固件版本号高于设备当前运行版本。
3. 重新 pristine build。
4. 使用重新生成的 `build/dfu_application.zip` 做 OTA。
