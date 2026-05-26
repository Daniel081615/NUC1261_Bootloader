#ifndef SELECT_FW_H
#define SELECT_FW_H

/*
 * Boot_SelectFW() — 開機韌體選擇與跳轉
 *   - BTLD_FORCE_BANK1/2 → 強制跳入指定 Bank（不返回）
 *   - BTLD_PATCH         → 對 INCOMING bank 執行 offset patch（成功不返回；失敗返回）
 *   - BTLD_UPDATE_METER  → 清除 cmd，返回
 *   - 否則健康檢查 + rollback，跳入有效 Bank（不返回）
 *
 * 返回只有一個語意：進入 OTA 接收主迴圈。
 */
void Boot_SelectFW(void);

#endif /* SELECT_FW_H */
