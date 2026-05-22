# NUC1261 Dual-Bank OTA Bootloader

OTA Bootloader for Nuvoton NUC1261 (ARM Cortex-M0) in a dual-bank meter system.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Memory Map](#2-memory-map)
3. [Layered Architecture](#3-layered-architecture)
4. [Directory Structure](#4-directory-structure)
5. [Boot Sequence](#5-boot-sequence)
6. [OTA Update Protocol](#6-ota-update-protocol)
7. [Offset-Based Patch Mechanism](#7-offset-based-patch-mechanism)
8. [Flash Data Structures](#8-flash-data-structures)
9. [Build Instructions](#9-build-instructions)
10. [How to Extend or Modify](#10-how-to-extend-or-modify)
11. [Debugging Guide](#11-debugging-guide)

---

## 1. Overview

The bootloader occupies the lowest 8 KB of APROM (0x0000–0x1FFF). On every reset it:

1. **Selects** the healthy firmware bank and jumps to it (normal boot path)
2. **Receives** OTA firmware over UART from a Center host when triggered
3. **Patches** the received binary — a single `.bin` compiled at base address `0x0` is relocated at run-time to whichever bank (Bank0 @ 0x2000 or Bank1 @ 0x10000) it was written to

The patching strategy uses a **pre-computed offset table** appended as the last page of the OTA payload. This replaces the previous heuristic scan approach and is deterministic regardless of compiler optimisation level.

---

## 2. Memory Map

```
Address       Region              Size    Description
-----------   ------------------  ------  ------------------------------------------
0x0001F800    Data Flash          8 B     FW_Info_t: active_bank, cmd, usage, health
0x0001D800    Bank1 Meta page     2 KB    Bank_MetaInfo_t for Bank1 (last page)
0x00010000    Bank1 APROM         56 KB   App firmware (second slot)
0x0000F800    Bank0 Meta page     2 KB    Bank_MetaInfo_t for Bank0 (last page)
0x00002000    Bank0 APROM         56 KB   App firmware (first slot)
0x00000000    Bootloader          8 KB    This project
```

**Notes:**
- The Bank Meta page is the **last 2 KB page** inside each bank's address range. App code must not use those pages.
- During OTA, the **last page of the OTA payload** (`payload_size - 2048`) is the patch metadata page. It is separate from the Bank Meta page unless `payload_size == BANK_SIZE` (56 KB exactly).
- `FW_Info_t` lives in Data Flash (separate from APROM); address `0x1F800` is the Data Flash Base Address configured in CONFIG1.

---

## 3. Layered Architecture

Dependencies flow strictly **top to bottom**. No file may `#include` a header from a layer above it.

```
+------------------------------------------+
|  app/                                    |  Business logic, composition root
|  main.c  BootloaderProcess.c             |  Includes: hal/, service/, common/
+------------------------------------------+
|  service/                                |  Platform-agnostic services
|  Select_fw  flash_service  ota_*         |  Includes: hal/, bsp/, common/
+------------------------------------------+
|  hal/                                    |  Hardware-neutral portable interface
|  hal_sys  hal_uart  hal_gpio             |  Includes: bsp/, driver/
+------------------------------------------+
|  driver/                                 |  MCU peripheral wrappers (no NUC1261.h)
|  uart_drv                                |  Includes: bsp/
+------------------------------------------+
|  bsp/                                    |  Board Support Package
|  bsp_flash  bsp_uart  bsp_init  ...      |  Includes: NUC1261 vendor SDK only
+------------------------------------------+
|  common/                                 |  Shared types (BL + App)
|  fw_info.h                               |  Includes: stdint.h only
+------------------------------------------+
```

**Key rule:** `app/` and `service/` files must never include `bsp/bsp_flash.h`, `driver/uart_drv.h`, or any NUC1261 vendor header. If a higher layer needs a low-level operation, add an API to `hal/` or `service/`.

---

## 4. Directory Structure

```
OTABootLoader/
|
+-- app/
|   +-- main.c                  Composition root; wires DI struct; boot + OTA loop
|   +-- BootloaderProcess.c     OTA state machine (IDLE->READY->RECEIVING->DONE)
|   +-- BootloaderProcess.h     BL_ProtocolOps_t DI interface + OTA command defs
|
+-- service/
|   +-- Select_fw.c/.h          Boot_SelectFW(): bank selection, rollback, jump
|   +-- flash_service.c/.h      Flash abstraction: FW_Info, BankMeta, erase/write/CRC
|   +-- ota_offset_patcher.c/.h Post-OTA patch engine; calls JumpToApp on success
|   +-- ota_patch_meta.c/.h     OtaPatchMeta_t parser/validator
|   +-- ota_scheduler.h         OTA command codes & payload field offsets (header-only)
|
+-- hal/
|   +-- hal_sys.c/.h            HAL_System_Init, HAL_GetDeviceID, HAL_WDT_Feed
|   +-- hal_uart.c/.h           HAL_UART_Init/Poll/HasPacket/GetPacket/SendRsp/GetTickMs
|   +-- hal_gpio.c/.h           HAL_GPIO_Read/Write
|
+-- driver/
|   +-- uart_drv.c              Ring-buffer ISR, frame assembly, RS485 DE control
|   +-- uart_drv.h              UART driver API (no NUC1261.h exposed)
|
+-- bsp/
|   +-- bsp_init.c/.h           BSP_Init(): clock, WDT, LED, DeviceID sample
|   +-- bsp_flash.c/.h          FMC erase/write/read/CRC32, JumpToApp, CONFIG fix
|   +-- bsp_uart.c/.h           UART register init, TX/RX
|   +-- bsp_gpio.c/.h           GPIO pin config, read/write
|   +-- bsp_config.h            Board variant flags (MeterV5_2, RS485)
|   +-- MeterV52PinConfig.c/.h  Board-specific pin mux table
|
+-- common/
|   +-- fw_info.h               FW_Info_t, Bank_MetaInfo_t, enums
|                               (shared between Bootloader and App projects)
|
+-- Keil/
|   +-- NUC1261_Bootloader.uvprojx   Keil project file
|   +-- Objects/NUC1261_Bootloader.sct  Scatter file (BL must fit in 8 KB)
|
+-- CLAUDE.md                   AI assistant project context
+-- README.md                   This file
```

---

## 5. Boot Sequence

```
Reset
 |
 v
HAL_System_Init()
 +-- BSP_Init() -> SYS_Init (72 MHz PLL), WDT (6.5 s LIRC), LED, DeviceID GPIO
 |
HAL_UART_Init(device_id) + HAL_SysTick_Init()
 |
BootloaderProcess_Init(device_id, &g_bl_ops)   <- inject DI
 |
FlashService_Init()   <- open FMC access
 |
Boot_SelectFW()                          <- return = enter OTA loop (single meaning)
 +--[BTLD_FORCE_BANK1/BANK2]      -> JumpToApp (maintenance, does not return)
 +--[BTLD_UPDATE_METER]           -> clear cmd, return -> enter OTA loop
 +--[BTLD_PATCH]                  -> clear cmd, scan banks for INCOMING
 |    +--[INCOMING found]         -> OtaOffsetPatcher_Apply()
 |    |    +--[success]           -> JumpToApp (does not return)
 |    |    +--[failure]           -> return -> enter OTA loop
 |    +--[not found]              -> return -> enter OTA loop
 +--[active bank EMPTY/INCOMING]  -> try fallback to other bank if VALID/ACTIVE
 +--[trial_counter >= 3, other    -> rollback to CONFIRMED bank
      bank HEALTH_CONFIRMED]
 +--[normal]                      -> increment trial_counter -> JumpToApp
                                                                (does not return)
                                        |
                              OTA loop: |
                              while(1): |
                               BootloaderProcess()      <- blocks until OTA_DONE
                               OtaOffsetPatcher_Apply()
                                +--[success] -> JumpToApp (does not return)
                                +--[failure] -> erase bank, loop again
```

### trial_counter and rollback

Every normal boot increments `trial_counter` in `Bank_MetaInfo_t`. The App must set `health = FW_HEALTH_CONFIRMED` and reset `trial_counter` after validating its own operation. If the App crashes before confirming, after 3 resets the bootloader switches to the other bank (if that bank has `HEALTH_CONFIRMED`).

---

## 6. OTA Update Protocol

### Frame format (fixed 100 bytes)

```
Byte  0    : 0x55 (SOF)
Byte  1    : DeviceID (6-bit; BL validates against sampled GPIO)
Byte  2    : CMD
Bytes 3-95 : Payload (93 bytes; unused bytes ignored)
Byte  96   : Checksum = sum(bytes[1..95])
Byte  97   : 0x0A (EOF)
```

Actual data window: `OTA_CHUNK_SIZE = 88` bytes per STORE frame.

### Command codes

| CMD                        | Value | Direction   | Description |
|----------------------------|-------|-------------|-------------|
| `OTA_CMD_ENTER_REQ`        | 0x20  | Host -> BL  | Request enter OTA mode |
| `OTA_CMD_ENTER_RSP`        | 0x21  | BL -> Host  | ACK with target bank |
| `OTA_CMD_STATUS_RSP`       | 0x21  | BL -> Host  | Progress/done reply |
| `OTA_CMD_UPDATE_CHILD_REQ` | 0x23  | Host -> BL  | Send OTA metadata |
| `OTA_CMD_STORE_CHILD_REQ`  | 0x24  | Host -> BL  | Send firmware chunk |
| `OTA_CMD_ERROR_RSP`        | 0xFF  | BL -> Host  | Error |
| `METER_CMD_ALIVE`          | 0x10  | Host -> BL  | Heartbeat (backward-compat) |
| `METER_RSP_SYS_INFO`       | 0x31  | BL -> Host  | Heartbeat reply |

### OTA flow (3 steps)

**Step 1 — Enter**

Host sends `OTA_CMD_ENTER_REQ`. BL reads `FW_Info_t.active_bank`, selects the **inactive** bank, and replies:

```
OTA_CMD_ENTER_RSP payload[2] = { 0x80 (FLAG_OTA_UPDATE), target_bank }
```

State: IDLE -> READY. Timeout: 5 seconds.

**Step 2 — Update Metadata**

Host sends `OTA_CMD_UPDATE_CHILD_REQ` (payload from `pkt[3]`):

| Offset | Bytes | Field | Description |
|--------|-------|-------|-------------|
| 0 | 4 | `payload_size` | Total OTA payload (page-aligned, fw + padding + meta page) |
| 4 | 4 | `fw_image_size` | Raw firmware binary size (excluding metadata page) |
| 8 | 4 | `transport_crc32` | CRC32 of the entire OTA payload |
| 12 | 4 | `version` | Firmware version number |
| 16 | 2 | `meta_version` | Must equal 1 (OTA_META_VERSION) |

BL validates fields, erases target bank, and replies `OTA_CMD_STATUS_RSP` (0x00 = OK, 0xFF = error).

State: READY -> RECEIVING.

**Step 3 — Store Chunks**

Host sends `OTA_CMD_STORE_CHILD_REQ` repeatedly (payload from `pkt[3]`):

| Offset | Bytes | Field | Description |
|--------|-------|-------|-------------|
| 0 | 4 | `chunk_offset` | Byte offset of this chunk within the OTA payload |
| 4 | 88 | `data` | Firmware data (last chunk padded to 4-byte boundary with 0xFF) |

BL replies with updated `rx_offset` (4 bytes) until `rx_offset >= payload_size`, then:
- **CRC32 match**: fills `NewBankMeta`, state = DONE, replies 0x00, returns to `main.c`
- **CRC32 mismatch**: erases bank, replies 0xFF, state = IDLE

---

## 7. Offset-Based Patch Mechanism

### Why patching is needed

The App firmware is compiled with base address `0x00000000`. Every absolute address embedded in the binary (vector table, function pointers, jump tables) must be relocated before execution at `0x2000` (Bank0) or `0x10000` (Bank1).

### OTA payload layout

```
+----------------------------+
|  fw image (fw_image_size)  |
+----------------------------+
|  padding (0xFF fill)       |  aligned to 2 KB page boundary
+----------------------------+
|  metadata page (2048 B)    |  always the last page of the payload
+----------------------------+
```

### Metadata page format

```
Offset  Size   Field           Description
------  -----  --------------  -------------------------------------------
0       4      magic           0x50415441 ('ATAP' little-endian)
4       4      meta_version    1 (OTA_META_VERSION)
8       4      fwsize          Raw fw image size in bytes
12      4      fwCrc32         CRC32 of fw at source base 0x0
16      4      offset_count    Number of 4-byte offsets that follow
20      4*N    offsets[N]      Sorted byte offsets of words to relocate
```

Maximum offsets: `(2048 - 20) / 4 = 507` per metadata page.

### Patch algorithm

```
1. Read metadata page into RAM (s_meta_buf)
   -- Must happen before any erase; handles case where meta page
   -- overlaps with Bank Meta page at payload_size == BANK_SIZE

2. Write Bank_MetaInfo_t.usage = BANK_USAGE_INCOMING
   -- Marks bank as in-progress; power-safe (BL skips INCOMING banks on boot)

3. For each firmware page i = 0 .. (fw_page_count - 1):
     if no offsets fall in this page: continue
     read 2 KB page -> s_page_buf
     for each offset in page:
         word_idx = (offset - page_start) / 4
         s_page_buf[word_idx] += bank_base     (add 0x2000 or 0x10000)
     erase page
     write s_page_buf back

4. fw_crc32 = CRC32(bank_base, fwsize)
   Bank_MetaInfo_t.fw_crc32 = fw_crc32
   Bank_MetaInfo_t.usage    = BANK_USAGE_VALID

5. FW_Info_t.active_bank = target_bank
   FlashService_JumpToApp(bank_base)    <- does not return

Error path: FlashService_EraseBank(target_bank); return error_code
   main.c loops back to BootloaderProcess() for retry
```

### Host-side responsibilities

1. Compile App with `--RO-base 0x00000000 --RW-base 0x00000000`
2. Scan the `.bin` for all relocatable address words; record their byte offsets
3. Build metadata page: fill `OtaPatchMeta_t`, append sorted offsets, pad to 2 KB with 0xFF
4. Concatenate: `[fw image] + [padding] + [metadata page]`
5. Compute `transport_crc32 = CRC32(entire_payload)`
6. Send `payload_size`, `fw_image_size`, `transport_crc32` in the UPDATE frame

---

## 8. Flash Data Structures

All structs use `#pragma pack(push, 1)`.

### `FW_Info_t` — 8 bytes at Data Flash 0x1F800

```c
typedef struct {
    uint8_t active_bank;    // 0 = Bank0, 1 = Bank1
    uint8_t cmd;            // FW_BtldCmd_t: controls BL on next reset
    uint8_t bank0_usage;    // FW_BankUsage_t
    uint8_t bank1_usage;    // FW_BankUsage_t
    uint8_t health;         // FW_Health_t: whether active bank is App-confirmed
    uint8_t trial_counter;  // boot attempts without App confirmation
    uint8_t reserved[2];
} FW_Info_t;
```

### `Bank_MetaInfo_t` — 16 bytes at Bank_META_BASE

```c
typedef struct {
    uint8_t  usage;          // FW_BankUsage_t
    uint8_t  health;         // FW_Health_t
    uint8_t  trial_counter;
    uint8_t  reserved;
    uint32_t version;        // firmware version
    uint32_t fw_size;        // raw fw bytes (excludes metadata page)
    uint32_t fw_crc32;       // post-patch CRC32 (written by OtaOffsetPatcher)
} Bank_MetaInfo_t;
```

### Key enum values

| Constant | Value | Meaning |
|----------|-------|---------|
| `BTLD_CMD_NONE` | 0xFF | Normal boot |
| `BTLD_UPDATE_METER` | 0xA1 | App requests OTA entry |
| `BTLD_PATCH` | 0xA2 | Resume interrupted patch on INCOMING bank (skip OTA receive) |
| `BTLD_FORCE_BANK1` | 0x11 | Force jump to Bank0 (maintenance) |
| `BTLD_FORCE_BANK2` | 0x12 | Force jump to Bank1 (maintenance) |
| `BANK_USAGE_EMPTY` | 0xFF | Flash erased / unused |
| `BANK_USAGE_INCOMING` | 0x03 | OTA in progress |
| `BANK_USAGE_VALID` | 0x06 | OTA + patch complete, CRC verified |
| `BANK_USAGE_ACTIVE` | 0x01 | App confirmed healthy |
| `FW_HEALTH_CONFIRMED` | 0xA5 | App confirmed after self-test |
| `FW_HEALTH_UNVERIFIED` | 0x00 | Not yet confirmed |

---

## 9. Build Instructions

### Prerequisites

- Keil MDK 5.x with NUC1261 device pack installed
- Nu-Link Pro debugger/programmer

### Build steps

1. Open `Keil/NUC1261_Bootloader.uvprojx`
2. Verify *Options for Target*:
   - **C/C++ tab**: One ELF Section per Function — ON
   - **Linker tab**: Use Memory Layout from Target Dialog — ON
   - **Linker tab**: Make RW Sections Position Independent — ON
   - **Linker tab**: Make RO Sections Position Independent — ON
3. Build (F7). Output: `Keil/Objects/NUC1261_Bootloader.bin`
4. Verify `.bin` size <= 8192 bytes

### Flash CONFIG registers (must be set via programmer)

The bootloader requires specific CONFIG values in the NUC1261 flash. These are **not** set by software at runtime — they must be programmed once using **NuMicro ICP Programming Tool** or the Nu-Link programmer before or during the first flash.

| Register | Value | Meaning |
|----------|-------|---------|
| CONFIG0  | `0xFFFFFFFE` | CBS bits [1:0] = `00b` → APROM + IAP mode |
| CONFIG1  | `0x0001F800` | DFBA = Data Flash base address |

**Using NuMicro ICP Programming Tool:**
1. Connect Nu-Link Pro
2. Select chip: NUC1261
3. Go to *Config* tab → set CONFIG0 = `0xFFFFFFFE`, CONFIG1 = `0x0001F800`
4. Click *Program* → the tool writes CONFIG + firmware in one operation

**If the chip gets locked** (Nu-Link shows "Target chip is locked"):  
Click *Yes* to chip erase — this resets CONFIG to factory defaults (`0xFFFFFFFF`).  
Re-program the firmware and CONFIG immediately after.

---

### Board variant

Edit [bsp/bsp_config.h](bsp/bsp_config.h):

```c
#define MeterV5_2   // HXT on PF3/PF4 — comment out for PF0/PF1 variant
#define RS485       // UART1 RS485 AUD mode — comment out for full-duplex
```

---

## 10. How to Extend or Modify

### Adding a new OTA command

1. Add value to `OtaCommand_t` in [service/ota_scheduler.h](service/ota_scheduler.h)
2. Add `static void HandleXxxReq(...)` in [app/BootloaderProcess.c](app/BootloaderProcess.c)
3. Add `case` in the dispatch switch inside `BootloaderProcess()`

### Adding a new boot behaviour (cmd)

1. Add value to `FW_BtldCmd_t` in [common/fw_info.h](common/fw_info.h)
2. Handle the new `cmd` at the top of `Boot_SelectFW()` in [service/Select_fw.c](service/Select_fw.c)
3. Recompile both Bootloader and App (they share `fw_info.h`)

### Porting to a different MCU

1. Rewrite `bsp/` — only this layer includes vendor SDK headers
2. Rewrite `driver/uart_drv.c` — ISR and RS485 control are MCU-specific
3. `hal/`, `service/`, `app/`, `common/` require no changes if the HAL API contract is preserved

### Changing flash page size

1. `BSP_FLASH_PAGE_SIZE` in [bsp/bsp_flash.h](bsp/bsp_flash.h)
2. `FLASH_SVC_PAGE_SIZE` in [service/flash_service.h](service/flash_service.h)
3. `OTA_META_PAGE_SIZE` in [service/ota_patch_meta.h](service/ota_patch_meta.h)
4. Recompute `OTA_META_MAX_OFFSETS = (new_page_size - 20) / 4`

---

## 11. Debugging Guide

### BL does not jump to App on normal boot

Read 8 bytes at Data Flash 0x1F800 (`FW_Info_t`):
- `active_bank` must be 0 or 1 (0xFF means empty)
- `cmd` must be 0xFF (`BTLD_CMD_NONE`)

Read `Bank_MetaInfo_t` at `Bank0_META_BASE (0xF800)` or `Bank1_META_BASE (0x1D800)`:
- `usage` must be `0x01 (ACTIVE)` or `0x06 (VALID)`
- `trial_counter` must be < 3

### OTA update times out / no response

- Confirm host frame `pkt[1]` equals the DeviceID (6-bit GPIO PB2–PB7)
- Verify checksum: `sum(pkt[1..95]) == pkt[96]`
- Check UART baud rate in `bsp_uart.c`

### Patch fails

Check the error code returned by `OtaOffsetPatcher_Apply()`:

| Code | Meaning |
|------|---------|
| -1 (`OTA_META_ERR_PARAM`) | NULL pointer or unaligned `payload_size` |
| -2 (`OTA_META_ERR_MAGIC`) | Metadata page not written or corrupted |
| -3 (`OTA_META_ERR_VERSION`) | `meta_version` != 1 |
| -4 (`OTA_META_ERR_FWSIZE`) | `fwsize` in metadata != `fw_image_size` sent in UPDATE frame |
| -5 (`OTA_META_ERR_COUNT`) | `offset_count` > 507 |
| -6 (`OTA_META_ERR_OFFSET`) | Offset not 4-byte aligned or exceeds `fwsize` |

### Unexpected reset loop (trial_counter)

If the App never calls the confirmation sequence, `trial_counter` reaches 3 on the 4th boot and BL attempts rollback. To recover manually:
1. Write `BTLD_FORCE_BANK1 (0x11)` or `BTLD_FORCE_BANK2 (0x12)` to `FW_Info_t.cmd` in Data Flash
2. Reset the board — BL will jump directly to that bank

### WDT timeout during OTA

WDT timeout = 6.5 seconds. `HAL_WDT_Feed()` is called at the top of `BootloaderProcess()` and at each page during `OtaOffsetPatcher_Apply()`. If a single chunk takes > 6.5 s to arrive, the MCU will reset. Ensure the host sends frames within the `OTA_RECV_TIMEOUT_MS (5000 ms)` window.
