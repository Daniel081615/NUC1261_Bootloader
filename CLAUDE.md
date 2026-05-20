# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

OTA Bootloader for **Nuvoton NUC1261** (ARM Cortex-M0) embedded in a dual-bank meter system. The bootloader resides in the lowest 8 KB of APROM and is responsible for:
1. Selecting and jumping to the active firmware bank on boot
2. Receiving OTA firmware updates over UART/RS485 from a host (Center)
3. **Patching** the received binary using a pre-computed offset table so a single `.bin` compiled at base 0x0 runs correctly in either Bank0 or Bank1

## Memory Layout

```
0x0001F800  Data Flash — BSP_FW_INFO_BASE: stores FW_Info_t (8 bytes)
0x0001D800  Bank1 Meta page (last 2 KB page of Bank1)
0x00010000  Bank1 APROM (56 KB = 0xE000)
0x0000F800  Bank0 Meta page (last 2 KB page before Bank1 starts)
0x00002000  Bank0 APROM (56 KB = 0xE000)
0x00000000  Bootloader (8 KB)
```

Each bank's last 2 KB flash page stores `Bank_MetaInfo_t` (16 bytes): usage, health, trial_counter, version, fw_size, fw_crc32.

`FW_Info_t` (8 bytes) in Data Flash holds: active_bank, cmd, bank0_usage, bank1_usage, health, trial_counter. Defined in [common/fw_info.h](common/fw_info.h).

## Build Environment

- **Toolchain**: Keil MDK 5.x (`armclang`) — project at [Keil/NUC1261_Bootloader.uvprojx](Keil/NUC1261_Bootloader.uvprojx)
- **Target**: NUC1261, 72 MHz (PLL from HXT), 128 KB APROM
- **Scatter file**: [Keil/Objects/NUC1261_Bootloader.sct](Keil/Objects/NUC1261_Bootloader.sct) — bootloader must fit in 0x2000 (8 KB)
- **Board variant flags**: [bsp/bsp_config.h](bsp/bsp_config.h) — `MeterV5_2` (PF3/PF4 HXT), `RS485` (UART1 AUD mode)

**Required Keil build flags** (Options for Target):
- C/C++: *One ELF Section per Function*
- Linker: *Use Memory Layout from Target Dialog*, *Make RW Sections Position Independent*, *Make RO Sections Position Independent*

There is no CLI build — build via Keil IDE only.

## Architecture & Layered Structure

Dependency flows strictly top-to-bottom. No layer may include headers from a layer above it.

```
app/          → hal/, service/           (main.c, BootloaderProcess.c)
service/      → hal/, bsp/, common/      (Select_fw, flash_service, ota_*)
hal/          → bsp/, driver/            (hal_sys, hal_uart, hal_gpio)
driver/       → bsp/                     (uart_drv)
bsp/          → vendor NUC1261 SDK only  (bsp_flash, bsp_uart, bsp_init, ...)
common/       → stdint.h only            (fw_info.h — shared with App)
```

### Source File Map

| Layer | File | Responsibility |
|---|---|---|
| app | `main.c` | Composition root: wires `BL_ProtocolOps_t` DI struct, calls `Boot_SelectFW()`, main loop |
| app | `BootloaderProcess.c/.h` | OTA state machine (`IDLE→READY→RECEIVING→DONE`), frame parsing, DI interface |
| service | `Select_fw.c/.h` | Boot bank selection, trial_counter increment, rollback logic, JumpToApp |
| service | `flash_service.c/.h` | Flash abstraction: FW_Info r/w, BankMeta r/w, erase/write/CRC, JumpToApp |
| service | `ota_offset_patcher.c/.h` | Post-OTA patch via offset list; updates BankMeta; triggers JumpToApp |
| service | `ota_patch_meta.c/.h` | Reads/validates the metadata page appended to every OTA payload |
| service | `ota_scheduler.h` | OTA command codes, frame constants, payload field offsets |
| hal | `hal_sys.c/.h` | `HAL_System_Init`, `HAL_GetDeviceID`, `HAL_WDT_Feed`, `HAL_LED_RToggle` |
| hal | `hal_uart.c/.h` | `HAL_UART_Init/Poll/HasPacket/GetPacket/SendRsp`, `HAL_SysTick_Init/GetTickMs` |
| hal | `hal_gpio.c/.h` | GPIO read/write abstraction |
| driver | `uart_drv.c/.h` | Ring-buffer ISR, frame assembly, RS485 direction control (no NUC1261.h) |
| bsp | `bsp_flash.c/.h` | FMC page erase/write/read/CRC32, JumpToApp, CONFIG verify+fix |
| bsp | `bsp_uart.c/.h` | UART peripheral init, TX/RX register ops |
| bsp | `bsp_gpio.c/.h` | GPIO pin config, read/write |
| bsp | `bsp_init.c/.h` | `BSP_Init()`: SYS_Init, WDT, clock, LED, device-ID GPIO sample |
| bsp | `bsp_config.h` | Board variant flags (`MeterV5_2`, `RS485`) |
| common | `fw_info.h` | `FW_Info_t`, `Bank_MetaInfo_t`, `FW_BankId_t`, `FW_BtldCmd_t` enums — shared with App |

## Architecture & Data Flow

### Boot sequence (`main.c` → `Select_fw.c`)
1. `HAL_System_Init()` — pin-mux → clock (72 MHz PLL) → WDT (6.5 s, LIRC) → LED → DeviceID sample; internally calls `BSP_Flash_ConfigVerifyAndFix()` to ensure Data Flash base = `BSP_FW_INFO_BASE`
2. `HAL_UART_Init(device_id)` + `HAL_SysTick_Init()`
3. `BootloaderProcess_Init(device_id, &g_bl_ops)` — inject DI function pointers
4. `FlashService_Init()` — opens FMC access
5. `Boot_SelectFW()` — reads `FW_Info_t`; handles forced-jump, OTA trigger, rollback, trial_counter; calls `FlashService_JumpToApp()` (does not return on normal boot)
6. Falls through to `while(1)` only when `cmd == BTLD_UPDATE_METER` or no valid bank

### UART protocol
- Fixed 100-byte frames: `[0x55][DeviceID][CMD][...payload (96 bytes)...][Checksum][0x0A]`
- **UART0** (PD0/PD1): Host (Center) communication
- **UART1** (PE13/PE12, nRTS PE11): Meter downstream, RS485 half-duplex (AUD mode)
- Ring-buffer ISR in `uart_drv.c`; `HAL_UART_Poll()` drains buffer and assembles frames

### OTA update flow (`BootloaderProcess.c`)

State machine: `IDLE → READY → RECEIVING → DONE`

| Step | Host → Bootloader | Bootloader action |
|---|---|---|
| 1 | `OTA_CMD_ENTER_REQ (0x20)` | BL auto-selects inactive bank; replies `OTA_CMD_ENTER_RSP (0x21)` with `{FLAG_OTA_UPDATE, target_bank}` |
| 2 | `OTA_CMD_UPDATE_CHILD_REQ (0x23)` | Parses payload_size/fw_image_size/transport_crc32/version/meta_version; erases target bank; replies `OTA_CMD_STATUS_RSP (0x21)` |
| 3 | `OTA_CMD_STORE_CHILD_REQ (0x24)` × N | Writes 88-byte chunk at `chunk_offset`; replies `rx_offset` progress until `rx_offset >= payload_size` |
| 4 | (last STORE completes) | Verifies transport CRC32 over full payload; pass → `BL_OTA_DONE`; fail → erase bank, back to `IDLE` |
| — | `METER_CMD_ALIVE (0x10)` | Replies `METER_RSP_SYS_INFO (0x31)` with `{FLAG_OTA_UPDATE, 0xFF}` (backward-compat) |

**UpdateChild payload layout** (bytes from `pkt[3]`):

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | `payload_size` — full OTA payload (fw + padding + metadata page, page-aligned) |
| 4 | 4 | `fw_image_size` — raw fw binary bytes (excludes metadata page) |
| 8 | 4 | `transport_crc32` — CRC32 of entire payload |
| 12 | 4 | `version` — firmware version |
| 16 | 2 | `meta_version` — must equal `OTA_META_VERSION` (1) |

### Patch engine (`ota_offset_patcher.c` + `ota_patch_meta.c`)

The OTA payload's **last page** (`payload_size - PAGE_SIZE`) is an `OtaPatchMeta_t` header (20 bytes) followed by a sorted list of 4-byte offsets pointing to every relocatable word in the fw image.

```
OtaPatchMeta_t (20 bytes):
  magic (4)        = 0x50415441 ('ATAP' LE)
  meta_version (4) = 1
  fwsize (4)       = raw fw image size
  fwCrc32 (4)      = pre-patch CRC32 (source base 0x0)
  offset_count (4) = number of offsets
followed by: uint32_t offsets[offset_count]
Max offsets: (2048 - 20) / 4 = 507
```

`OtaOffsetPatcher_Apply()` steps:
1. Read metadata page into `s_meta_buf` (RAM) — **must** precede any erase
2. Mark target bank `BANK_USAGE_INCOMING` (power-loss idempotent)
3. For each fw page containing at least one offset: read → `word += bank_base` for each offset → erase → write back
4. Calculate post-patch CRC32 over `fwsize` bytes → store in `Bank_MetaInfo_t.fw_crc32`
5. Mark bank `BANK_USAGE_VALID`; update `FW_Info_t.active_bank`; call `FlashService_JumpToApp()` (does not return)
6. On any error: erase target bank; return negative error code; `main.c` loops back to `BootloaderProcess()`

### Boot_SelectFW rollback logic (`Select_fw.c`)
1. `BTLD_FORCE_BANK1/BANK2` → jump directly (maintenance mode)
2. `BTLD_UPDATE_METER` → clear cmd, return to `main.c` → enter OTA loop
3. Active bank `EMPTY` or `INCOMING` → fallback to other bank if `VALID/ACTIVE`
4. `trial_counter >= 3` and other bank `HEALTH_CONFIRMED` → switch (rollback)
5. Increment `trial_counter`; jump to selected bank

## Key Constraints

- No `malloc`/`free` — all buffers statically declared (`s_chunk_buf`, `s_page_buf`, `s_meta_buf`, ring buffers)
- `SYS_UnlockReg()` must precede any FMC or CONFIG write (handled inside BSP layer)
- Bootloader image must stay within 8 KB (0x0000–0x1FFF)
- ISR handlers must not call blocking functions
- Metadata page must be read into RAM before any bank erase (step 1 of `OtaOffsetPatcher_Apply`)
- `fw_info.h` is shared between Bootloader and App — struct changes require recompiling both

## Dependency Injection Pattern

`main.c` is the **composition root**: it constructs `g_bl_ops` (`BL_ProtocolOps_t`) wiring HAL + FlashService functions into `BootloaderProcess`. `BootloaderProcess.c` never calls HAL directly — only through `s_ops->...`. This makes the OTA protocol logic testable without hardware.

## Hardware Variant

[bsp/bsp_config.h](bsp/bsp_config.h) controls compilation:
- `#define MeterV5_2` — uses PF3/PF4 for HXT crystal (vs PF0/PF1 X32 on other variants)
- `#define RS485` — enables UART1 RS485 AUD mode (hardware direction control via nRTS PE11)

DeviceID is read as a 6-bit value from GPIO PB2–PB7 at startup inside `BSP_Init()`.
