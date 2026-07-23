# nRF54L15 外部 Flash OTA 示例（NCS 3.4.x / 设备树管理 flash 分区）

本分支是 [NCS3.2.X 分支](https://github.com/Davidduan1987/ota_example/tree/NCS3.2.X) 的**设备树（Devicetree fixed-partitions）管理版本**，运行在 **NCS 3.4.0** 上。

功能和 NCS3.2.X 分支完全一样（Bluetooth LBS 广播 + 外部 SPI NOR flash 作为 MCUboot secondary slot 的 BLE OTA），唯一的区别是 flash 分区不再由 Partition Manager（`pm_static.yml`）管理，改成 NCS 3.3+ 推荐的设备树 `fixed-partitions` 方式。

本工程已在 NCS 3.4.0 环境中使用 nRF54L15 DK 测试。

## 与 NCS3.2.X 分支的关系

| | NCS3.2.X | NCS3.4.X（本分支） |
|---|---|---|
| NCS 版本 | 3.2.1 | 3.4.0 |
| flash 分区管理方式 | Partition Manager（`pm_static*.yml`） | 设备树 `fixed-partitions`（`boards/*.overlay`） |
| 应用逻辑 / `prj.conf` | 相同 | 相同（未改动） |
| flash 物理分区边界（地址/大小） | 相同 | **完全相同**（这是能互相 OTA 的前提，见最后一节） |

`pm_static.yml` / `pm_static_nrf54l15dk_nrf54l15_cpuapp.yml` 保留在仓库里（改名为 `.orig`），仅作为迁移前的参考，构建时不再生效。

## 从 Partition Manager 迁移到设备树管理分区：具体改了什么

NCS 上 sysbuild 会同时构建两个独立镜像域——**app** 和 **mcuboot**，每个域有自己独立的 devicetree。用 Partition Manager 时，两边的分区表由 PM 自动算好、自动同步；改成设备树之后，**这个自动同步没有了，两边的分区表必须手动改、手动保持一致**。这是整个迁移过程中最容易踩坑的地方。

### 1. `sysbuild.conf`

关闭 Partition Manager，转为纯设备树管理：

```diff
-#external flash
-#SB_CONFIG_PARTITION_MANAGER=y
-SB_CONFIG_PM_EXTERNAL_FLASH_MCUBOOT_SECONDARY=y
-SB_CONFIG_PM_OVERRIDE_EXTERNAL_DRIVER_CHECK=y
+#flash layout is managed via devicetree fixed-partitions (see boards/*.overlay), not Partition Manager
+#SB_CONFIG_PARTITION_MANAGER=n
```

### 2. App 镜像的板级 overlay（`boards/nrf54l15dk_nrf54l15_cpuapp.overlay`）

在内部 RRAM（`&cpuapp_rram`）下用 `fixed-partitions` 显式定义 `boot_partition`（mcuboot）/ `slot0_partition`（image-0）/ `storage_partition` / `ring_storage`，地址和大小跟原来 PM 算出来的完全对齐（`pm_static_nrf54l15dk_nrf54l15_cpuapp.yml` 里能查到原始数值）。同时要 `/delete-node/` 掉板级默认 dtsi 里预置的几个默认分区节点，避免地址冲突。

外部 flash（`&mx25r64`）下同样用 `fixed-partitions` 定义 `slot1_partition`（image-1，OTA 副槨）和 `ido_storage_partition`（用户数据区），替换掉原来 PM 模式下的 `nordic,pm-ext-flash = &mx25r64;` chosen 属性（设备树模式不需要这个）。

### 3. MCUboot 镜像自己的 overlay（`sysbuild/mcuboot/boards/nrf54l15dk_nrf54l15_cpuapp.overlay` + `sysbuild/mcuboot/app.overlay`）

**这一步是最容易漏、也是后果最严重的一步**：mcuboot 是单独的镖像，有自己独立的 devicetree，**不会**自动继承 app 那边写的分区定义。它需要一份自己的、内容基本相同的 `boot_partition`/`slot0_partition` 定义。

- 同样加上 `fixed-partitions`（`boot_partition`、`slot0_partition`，地址范围必须跟 app 那边完全一样），以及外部 flash 的 `slot1_partition`。
- **必须显式声明 `zephyr,code-partition = &boot_partition;`**。原因：标准 nRF54L15 DK 板级文件的默认 `zephyr,code-partition` 指向的是 `slot0_partition`（这是为普通应用镖像准备的默认值）。mcuboot 自己的构建如果不显式覆盖这个 chosen 属性，就会**继承这个默认值，导致 mcuboot 自己被链接到 slot0 的地址（跟 app 撞地址）而不是 0x0**。这个 bug 的症状非常隐蔽——编译、烧录都不会报错，烧进去之后板子表现为"完全没有任何串口输出、像是死机"，需要用调试器接上去读 PC 才能发现 mcuboot 实际执行的代码地址不对。
- 去掉 `sysbuild/mcuboot/app.overlay` 里的 `nordic,pm-ext-flash = &ext_flash;`（PM 专用 chosen，设备树模式下没有意义，且 `ext_flash` 这个 label 在纯设备树模式下未必存在，留着可能导致编译报未定义引用）。
- **给外部 flash 节点加上 `t-reset-recovery = <100000>;`**：标准 nRF54L15 DK 板级文件给 `mx25r64` 节点配置了 `reset-gpios`，但**没有配置 `t-reset-recovery`**。Zephyr 的 `spi_nor` 驱动在初始化时会先把这个复位引脚拉低再立刻拉高（触发一次硬件复位），如果没有 `t-reset-recovery` 属性，驱动**不会在复位后等待任何恢复时间**就立刻发命令读 JEDEC ID——这个时间点芯片内部电路还没稳定，读回来的 ID 永远是 `00 00 00`，导致 mcuboot 在 flash 探测阶段直接 panic（`Device id 00 00 00 does not match config c2 28 17` → `Image in the primary slot is not valid!`）。这是官方板级文件本身的一个配置缺口，只要工程里用到了这颗外部 flash 的 `reset-gpios`，就必须自己在 overlay 里把这个恢复时间补上。

### 4. 应用逻辑（`src/main.c` / `prj.conf`）

`prj.conf` 里 flash 相关的 Kconfig（`CONFIG_FLASH`、`CONFIG_SPI_NOR`、`CONFIG_SPI_NOR_SFDP_DEVICETREE` 等）**完全不需要改**，PM 和设备树两种模式用的是同一套驱动 Kconfig，区别只在分区表的来源。

本分支同时也带着 NCS3.2.X 分支里已经修好的那个 BLE 广播 bug 的修复（`bt_enable()` 后补 `settings_load()`，详见 NCS3.2.X 分支的 README）——这个 bug 跟 PM/设备树的迁移完全无关，纯粹是应用代码本身缺了一行调用。

## 迁移注意事项清单

1. **mcuboot 和 app 各有一份独立的分区定义，必须手动保持地址/大小一致**，改了一边一定要记得改另一边。设备树模式下没有 Partition Manager 帮你自动同步。
2. **mcuboot 自己的 overlay 里必须显式写 `zephyr,code-partition = &boot_partition;`**，不要依赖板级默认值——默认值是给 app 用的，不是给 mcuboot 用的。
3. 如果工程用到了外部 flash 芯片的硬件 `reset-gpios`，检查一下板级文件有没有配套的 `t-reset-recovery`，没有就自己在 overlay 里加上，否则复位后读芯片 ID 会稳定失败。
4. 迁移后的物理分区边界（mcuboot / slot0 / slot1 / storage 的起始地址和大小）要跟原 PM 版本**逐字节对齐**，这是下一节"两边能互相 OTA"的前提——地址对不上，mcuboot 校验镖像时会直接判定为无效镖像。

## OTA 互操作性验证：NCS3.2.X（PM）可以直接 OTA 升级到本分支（设备树）

已经做过真实的 BLE OTA 测试验证（不是理论推算，是实际走通的）：

1. 用 NCS3.2.X 分支（Partition Manager 管理）编译出的签名镖像（`build/dfu_application.zip` 里的 `lbs_v32.signed.bin`）
2. 通过真实的 BLE SMP（mcumgr 协议）传输，推送到一块**当前正跑着本分支（设备树管理）编译出的 mcuboot + app** 的 nRF54L15 DK 上
3. mcuboot 完成 swap、新镖像正常启动、`settings_load`/BLE 广播/flash 读写测试全部正常，SMP 确认（confirm）成功，不会在下次复位时回滚

结论：**只要两边的 flash 物理分区边界完全一致，mcuboot 并不关心这个边界是 Partition Manager 算出来的还是设备树里手写的**——swap、校验、启动这些逻辑只认最终解析出的物理地址。这也是为什么上面"迁移注意事项"里反复强调"分区边界要跟原版本逐字节对齐"。

（测试用的 BLE 客户端：Windows 上 Nordic 官方的 `mcumgr` Go CLI 不支持 BLE 传输，改用了开源的 Python `smpclient`/`smpmgr`，底层通过 `bleak` 调用 Windows 自带的 WinRT 蓝牙 API，走的是 PC 本机蓝牙硬件，跟手机上用 nRF Connect 做 DFU 是同一套协议。）

## 工程内容

- `src/main.c`：外部 flash 读写测试、Bluetooth LBS 广播、BLE OTA，以及 Button0/Button1 低功耗控制逻辑。
- `prj.conf`：应用镜像配置，启用 SPI NOR、flash map、BLE、LBS 和 MCUmgr Bluetooth OTA。
- `sysbuild.conf`：启用 sysbuild 下的 MCUboot，flash 布局由设备树管理（不再启用 Partition Manager）。
- `sysbuild/mcuboot/prj.conf` / `sysbuild/mcuboot/boards/*.conf`：MCUboot 子镖像配置域，只影响 bootloader。
- `boards/nrf54l15dk_nrf54l15_cpuapp.overlay`：应用镖像的设备树分区定义（内部 RRAM + 外部 SPI NOR flash）。
- `sysbuild/mcuboot/boards/nrf54l15dk_nrf54l15_cpuapp.overlay`：MCUboot 镖像自己的设备树分区定义，必须与上面保持地址/大小一致。
- `pm_static.yml.orig` / `pm_static_nrf54l15dk_nrf54l15_cpuapp.yml.orig`：迁移前 Partition Manager 的原始分区数值，仅供参考对照。
- `新固件/54L_application.zip`：用于手机端 OTA 测试的新固件包。

## 功能说明

当前程序会周期性擦写外部 flash 的 `ido_storage_partition` 分区，并通过 BLE LBS 服务广播。

按键低功耗逻辑：

- Button0：进入低功耗空闲状态，主循环停止，BLE 停止广播，已有连接会断开，外部 flash 进入 DPD，SPI 外设进入 suspend。
- Button1：退出低功耗空闲状态，恢复 SPI 和外部 flash，重新开始 BLE 广播，主循环继续运行。

## OTA 说明

本工程的 OTA secondary slot 位于外部 SPI NOR flash。手机端通过 MCUmgr Bluetooth SMP 传输新固件，应用侧 MCUmgr 写入外部 flash，复位后由 MCUboot 完成镖像升级。

`新固件/54L_application.zip` 是已经生成好的测试 OTA 包，可以直接使用 nRF Connect 或 nRF Device Manager 进行升级测试。

## MCUboot 配置域说明

NCS 使用 sysbuild 时，应用和 MCUboot 是两个不同的配置域：

- 应用配置域：`prj.conf` + `boards/*.overlay`
- MCUboot 配置域：`sysbuild/mcuboot/prj.conf` + `sysbuild/mcuboot/boards/*.conf` + `sysbuild/mcuboot/boards/*.overlay`

不要把 MCUboot 专用 Kconfig 配置随意放到应用 `prj.conf` 中。MCUboot 的 slot、签名、swap、启动地址、镖像校验等配置会直接影响启动和 OTA 行为，修改前需要确认该配置的作用和影响。

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

应用侧和 MCUboot 侧都需要保证外部 flash devicetree 配置一致（分区边界 + `t-reset-recovery`），否则 OTA 写入 external secondary slot 时可能失败，或者 mcuboot 启动阶段探测外部 flash 直接 panic。

## 构建命令

在 NCS 3.4.0 工具链环境中构建：

```powershell
nrfutil toolchain-manager launch --ncs-version v3.4.0 -- west build -b nrf54l15dk/nrf54l15/cpuapp "C:\ncs\v3.4.0\IDO\lbs_v34" --sysbuild -d "C:\ncs\v3.4.0\IDO\lbs_v34\build" -p always
```

构建完成后，常用产物包括：

- `build/merged.hex`：完整烧录镖像。
- `build/dfu_application.zip`：手机端 OTA 升级包。

## 烧录命令

```powershell
nrfutil toolchain-manager launch --ncs-version v3.4.0 -- west flash -d "C:\ncs\v3.4.0\IDO\lbs_v34\build" --erase
```

## OTA 注意事项

如果 OTA 时手机端 APP 进度直接到 `100%`，但设备端固件没有升级，通常说明新固件包生成或版本号配置有问题，而不是手机端传输流程真正完成升级。

常见处理方式：

1. 删除工程原始 `build` 文件夹。
2. 修改 `VERSION` 文件，确保新固件版本号高于设备当前运行版本。
3. 重新 pristine build。
4. 使用重新生成的 `build/dfu_application.zip` 做 OTA。
