# System Overview — NUC1261 OTA Bootloader

## 模組清單

### App 層
| 檔案 | 責任 |
|---|---|
| `app/main.c` | 系統初始化 (時鐘、WDT、UART)、主迴圈調度 OTA 接收 → Patch |
| `app/BootloaderProcess.c` | OTA 接收狀態機：IDLE → READY → RECEIVING → DONE/ERROR |

### Service 層（Middleware）
| 檔案 | 責任 |
|---|---|
| `service/Select_fw.c` | 開機韌體選擇：讀 FW_Info + Bank_MetaInfo，選 active bank，執行 JumpToApp |
| `service/flash_service.c` | 高層 Flash 操作：EraseBank、WriteFirmware、VerifyBankCRC、ReadFWInfo、BankMeta 讀寫 |
| `service/ota_patch_meta.c` | OTA metadata page 解析與驗證 (magic、版本、offset 合法性) |
| `service/ota_offset_patcher.c` | Offset 式 patch 執行：讀 metadata → 逐 page 修補 → 計算 CRC → 寫 BankMeta → JumpToApp |
| `service/ota_scheduler.h` | OTA 協定常數：命令碼、payload offset、chunk 大小、timeout |
| `service/patch_engine.c` | **DEPRECATED** — 已棄用，待從 Keil project 移除 |

### Driver 層
| 檔案 | 責任 |
|---|---|
| `driver/uart_drv.c` | UART1 ISR (RX/TX)、Ring Buffer、封包組裝、BL_UART_Poll/SendRsp API |
| `driver/bl_fmc_adapter.c` | 將 BSP_Flash_* 橋接為 IFmcDriver_t（dependency injection 介面） |
| `driver/crc_user.c` | 使用硬體 CRC 引擎計算 CRC32 |

### BSP 層
| 檔案 | 責任 |
|---|---|
| `bsp/bsp_flash.c` | Flash 底層：ErasePage、WriteWords、ReadWords、JumpToApp (VTOR 設定 + 系統重置) |
| `bsp/MeterV52PinConfig.c` | GPIO 腳位多工設定 (UART0/1、RS485 nRTS) |

### Common（跨層共用）
| 檔案 | 責任 |
|---|---|
| `common/fw_info.h` | 資料結構定義：FW_Info_t、Bank_MetaInfo_t、FW_BANK / BANK_USAGE / FW_HEALTH enum |
| `common/MyDef.h` | 全域常數：APROM_SIZE、LED 巨集、UART buffer 大小、OTA_RECV_TIMEOUT_MS |

---

## 記憶體佈局

```
0x00020000  ─── APROM 頂端
0x0001F800  Data Flash (FW_INFO_BASE) — FW_Info_t：active_bank + cmd + usage + health
0x0001E800  Bank1 META_BASE — Bank_MetaInfo_t (16 bytes)
0x00010000  Bank1 APROM_BASE — App Code (最大 57 KB)
0x0000E800  Bank0 META_BASE — Bank_MetaInfo_t (16 bytes)
0x00002000  Bank0 APROM_BASE — App Code (最大 57 KB)
0x00000000  Bootloader (8 KB，固定，不被 OTA 覆蓋)
```

OTA payload 在 Flash 中的結構（接收後）：
```
bank_base + payload_size - 2048  OtaPatchMeta_t header (20 B) + uint32_t offsets[] (最多 507 個)
bank_base + fw_image_size        0xFF padding（補齊至 page 邊界）
bank_base                        原始韌體 (compiled at base 0x0000)
```

---

## 資料流

### 開機選擇
```
main()
  → Boot_SelectFW()
      → FlashService_ReadFWInfo()          // 讀 Data Flash FW_Info_t
      → FlashService_ReadBankMeta(0/1)     // 讀兩個 bank 的 Bank_MetaInfo_t
      → 選出 ACTIVE / VALID bank
      → FlashService_JumpToApp()           // 設 VTOR → 觸發系統重置
```
若無有效韌體或 `cmd == BTLD_UPDATE_METER`，繼續進入 OTA 接收迴圈。

### OTA 接收
```
UART1 RX Interrupt
  → HOSTRxQ ring buffer (100 B)
  → 偵測 0x55 header + 0x0A tail + checksum
  → HostToken[100] (完整封包)
  → HostTokenReady = TRUE

BootloaderProcess() 主迴圈
  → BL_WDT_Reset()
  → BL_UART_Poll()           // 消費 ring buffer，組裝封包
  → checksum 驗證 + DeviceID 過濾
  → switch(cmd):
      ENTER_REQ         → HandleEnterReq()    // 選 inactive bank，回應 [FLAG, bank]
      UPDATE_CHILD_REQ  → HandleUpdateReq()   // 驗參數、Erase bank
      STORE_CHILD_REQ   → HandleStoreReq()    // 寫 chunk，末包驗 transport_crc32
      METER_CMD_ALIVE   → 回應 SYS_INFO
  → OTA_DONE 後返回 main()
     （BankID / NewBankMeta / OtaPayloadSize 此時有效）
```

### OTA Patch 套用
```
OtaOffsetPatcher_Apply(ctx)
  → meta_page_addr = bank_base + payload_size - 2048
  → OtaPatchMeta_ReadAndValidate()        // ← 必須先讀 metadata 到 RAM（見 R4）
  → FlashService_UpdateBankMeta(INCOMING) // erase BankMeta page → 寫 INCOMING 標記
  → for each page in [0 .. fwsize):
      → 若無 offset 落入此 page → skip（節省 erase/write）
      → BSP_Flash_ReadWords() → page_buf
      → 套用 offset：*(u32 at off) = original + bank_base （memcpy 保護對齊）
      → BSP_Flash_ErasePage() + BSP_Flash_WriteWords()
  → FlashService_GetCRC32(bank_base, fwsize)       // post-patch CRC（不含 metadata page）
  → FlashService_UpdateBankMeta(VALID, fw_crc32)
  → FlashService_UpdateFWInfo(active = target_bank)
  → FlashService_JumpToApp()
```

---

## 協定格式（UART Frame，100 bytes 固定長度）

```
[0x55] [DeviceID] [CMD] [Payload 95 B] [Checksum] [0x0A]
  0       1        2      3..97           98         99
```

| CMD | 方向 | 說明 |
|---|---|---|
| `0x20` ENTER_REQ | Host→BL | 請求進入 OTA 模式 |
| `0x21` ENTER_RSP | BL→Host | `[FLAG_OTA_UPDATE, target_bank]`（2 bytes） |
| `0x23` UPDATE_CHILD_REQ | Host→BL | payload_size / fw_image_size / transport_crc32 / version / meta_version |
| `0x24` STORE_CHILD_REQ | Host→BL | chunk_offset (4 B) + 88 B 韌體資料 |
| `0x21` STATUS_RSP | BL→Host | 正常回應（含 rx_offset 進度） |
| `0x22` ERROR_RSP | BL→Host | 錯誤回應 `0xFF` |

---

## 風險點標記

### ⚠ R1 — ISR 封包驗證邏輯過重
**位置**：`driver/uart_drv.c` — `UART1_IRQHandler()`

ISR 中進行 header/tail 偵測、checksum 累加、封包複製至 HostToken。若 ISR 執行時間過長，可能在處理期間丟失後續 byte。

**建議**：ISR 僅負責填充 ring buffer，封包組裝移至主迴圈 `BL_UART_Poll()`。

---

### ✅ R2 — 共用變數競態（Global Variable Race）— 已部分修正
**位置**：`driver/uart_drv.c:17` / `driver/uart_drv.h:33` — `HostTokenReady`

**已修正**：`HostTokenReady` 已加上 `volatile` 限定詞（`volatile _Bool HostTokenReady`）。

#### 競態場景詳解

**Error A — 編譯器快取 HostTokenReady（最高風險，已修正）**

未加 `volatile` 前，Keil armclang 在 `-O1` 以上會將緊湊迴圈中的讀取提升到暫存器：

```c
// 編譯後等效（最佳化後可能產生）
r0 = HostTokenReady;     // 讀一次後放入暫存器
while (!r0) continue;    // 永遠以暫存器值判斷，永遠不見 ISR 寫入
```

**症狀**：BL 收到第一個封包後永久卡在 `continue` 迴圈，不再回應任何指令。
**根本原因**：C11 標準不保證非 volatile 變數對中斷可見。
**修正**：`volatile _Bool HostTokenReady` 強制每次從記憶體讀取。

---

**Error B — HostToken[] 寫入順序（編譯器重排，低風險）**

ISR 執行順序：
```c
// ISR
for (i = 0; i < 100; i++) HostToken[i] = HOSTRxQ[i];  // 寫 100 bytes
HostTokenReady = TRUE;                                   // 寫旗標
```
主迴圈執行順序：
```c
// Main
if (HostTokenReady)       // 讀旗標
    pkt = BL_UART_GetPacket();  // 讀 HostToken[]
```

`HostToken[]` 未宣告 `volatile`。理論上編譯器可將 `HostToken[]` 讀取提升到 `HostTokenReady` 判斷之前（指令重排）。Cortex-M0 硬體為嚴格順序執行（in-order），不存在 CPU 層級重排，但編譯器重排仍可能發生。

**症狀**：主迴圈讀到上一個封包的殘值，誤發舊命令（checksum 通過但 CMD 欄位是舊值）。

**建議**：在 `BL_UART_HasPacket()` 之後、讀取 `HostToken[]` 之前插入 `__DSB()` memory barrier；或宣告 `HostToken[100]` 為 `volatile`（代價是每個 byte 存取都不可最佳化，效能稍降但 100 bytes 影響可忽略）。

---

**Error C — HOSTRxQ_cnt 非原子遞增（ISR 內部，低風險）**

ISR 中對 `HOSTRxQ_cnt` 的操作：
```c
HOSTRxQ_cnt++;   // 讀-改-寫，3 個指令
```
Cortex-M0 無硬體原子指令（無 LDREX/STREX）。若此 ISR 被更高優先級中斷搶佔（在本專案中只有 SysTick 可能搶佔 UART1 IRQ，但 SysTick 不存取 HOSTRxQ），則不存在實際競態。

**目前安全**，但若日後新增其他存取 HOSTRxQ 的 ISR，必須重新評估。

---

**Error D — HasPacket/GetPacket 非原子（理論上的 race window）**

```c
if (!BL_UART_HasPacket()) continue;   // 讀 HostTokenReady = 1
// ← 此處 ISR 完成新封包寫入 HostToken[]，覆蓋舊封包 ←
pkt = BL_UART_GetPacket();            // 清旗標，返回 HostToken（已被新封包覆蓋）
```

主迴圈會拿到「新封包」而非「觸發 HasPacket 那個封包」。新封包仍是完整封包，不會造成 checksum 失敗。實際觸發需要兩封包之間隔 < 幾個指令周期（< 100 ns），而 57600 baud 下一個完整封包需 ~15.6 ms。**實際上不可能發生**，記錄供參考。

---

### ⚠ R3 — 共用頁面緩衝區（Shared Buffer）
**位置**：`Aprom_Page_Buff[2048]`（`BootloaderProcess.c`）與 `Next_Aprom_Page_Buff[2048]`

兩個 buffer 被 BL 接收流程與 Patcher 共用。**目前安全**（`BootloaderProcess()` 返回後才呼叫 Patcher，不重疊）。記錄此依賴，未來重構若引入非同步呼叫需重新評估。

---

### ⚠ R4 — Flash 寫入的 Power-Loss 安全性與臨界順序
**位置**：`service/ota_offset_patcher.c`

**臨界順序**：當 `payload_size == BSP_BANK_SIZE` 時，OTA metadata page 與 BankMeta page 共用同一 flash 位址。必須先讀 metadata 至 RAM，再呼叫 `UpdateBankMeta(INCOMING)`（此呼叫會 erase 該 page）。

步驟 2~3 之間斷電：BankMeta 狀態為 INCOMING，重開機後 `Select_fw.c` 不選此 bank；OTA 資料殘留在 flash，下次 OTA 時 `HandleUpdateReq()` 先 erase 整個 bank 再重寫。整體設計安全，但記錄此假設。

---

### ✅ R5 — WDT 模組設定與 Patcher 餵狗 — 已修正
**位置**：`app/main.c`、`service/ota_offset_patcher.c`

**已修正**：`WDT_Init()` 已加入 `main()` 的 `SYS_Init()` 之後。

#### WDT 參數說明

| 參數 | 值 | 說明 |
|---|---|---|
| 時鐘源 | LIRC (10 kHz) | 在 `SYS_Init()` 中設定：`CLK_CLKSEL1_WDTSEL_LIRC` |
| 逾時 | `WDT_TIMEOUT_2POW16` | 2^16 / 10000 Hz = **6.55 秒** |
| Reset 延遲 | `WDT_RESET_DELAY_18CLK` | 逾時後 18 LIRC 周期（1.8 ms）才觸發 reset |
| Reset 啟用 | `TRUE` | 逾時後強制系統重置 |

**為何 6.55 秒合理**：OTA 封包逾時 (`OTA_RECV_TIMEOUT_MS`) 為 5000 ms；狀態機在等待下一個封包時若超過 5 秒會回 IDLE，期間 `BL_WDT_Reset()` 在主迴圈頂端持續被呼叫（每次迴圈約 1 ms 內）。故 WDT 在正常 OTA 接收時絕不逾時。

**Patcher 迴圈餵狗**：`ota_offset_patcher.c` 已在以下三個位置呼叫 `WDT_RESET_COUNTER()`：
- 標記 INCOMING 之後（進入 page 迴圈前）
- 每個 page 迴圈頂端（每頁 erase+write 約 5~10 ms，遠低於 6.55 s）
- page 迴圈結束後（進入 CRC 計算前）

57 KB fw（29 pages）總 patch 時間 < 300 ms，WDT 不會逾時。

---

### ⚠ R6 — UART 逾時後 Flash 狀態不一致（OTA Timeout）
**位置**：`app/BootloaderProcess.c:211~215`

`BL_OTA_RECEIVING` 狀態超過 5000 ms → 狀態機回 IDLE，但**已寫入的 bank 資料不 erase**。Bank 的 usage 仍為 EMPTY 或 INCOMING（取決於是否完成 UpdateReq），`Select_fw.c` 不選此 bank，整體安全。下次 OTA 的 `HandleUpdateReq()` 會 erase bank 後重寫。

---

### ⚠ R7 — Bank_MetaInfo_t 結構大小變更（Binary Compatibility）
**位置**：`common/fw_info.h`

重構後 struct 從 20 bytes（含 `region_table_addr`）縮為 16 bytes。Flash 中舊格式資料以新格式解讀，`health` 與 `fw_crc32` 欄位錯位，可能誤判 bank 狀態。

**必要動作**：燒錄新 bootloader 後，執行一次完整 OTA 重建 bank metadata。

---

### ⚠ R8 — Cortex-M0 Unaligned Access（維護注意）
**位置**：`service/ota_offset_patcher.c` — offset 套用邏輯

Cortex-M0 不支援非對齊 32-bit 存取（HardFault）。目前透過 `memcpy` 讀寫 patch 位置，offset validation 確保每個 offset 為 4-byte 對齊。**維護時不得將 memcpy 改為直接指標解引用。**

---

## 待辦事項

| 項目 | 說明 |
|---|---|
| Keil 新增 | `service/ota_patch_meta.c`、`service/ota_offset_patcher.c` |
| Keil 移除 | `service/patch_engine.c` |
| Host 端 | UPDATE_META 封包格式更新（payload_size / fw_image_size / transport_crc32 / meta_version） |
| Host 端 | ENTER_RSP 解析：byte[1] = BL 選定的 target bank |
| Host 端工具 | binary diff 工具：比較 `base_0000.bin` vs `base_2000.bin`，輸出 delta==0x2000 的 offset 陣列 |
| Host 端工具 | OTA 封包產生：`OtaPatchMeta_t` header + offsets[] 附加於 fw binary 後方 |
| Host 端工具 | transport_crc32 = CRC32(整個 payload_size bytes) |
| 初次燒錄後 | 執行完整 OTA 重建 16-byte BankMeta（解決 R7 結構不相容問題） |
