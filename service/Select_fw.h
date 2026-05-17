#ifndef SELECT_FW_H
#define SELECT_FW_H

/*
 * Boot_SelectFW() — 開機韌體選擇與跳轉（重構版）
 *   - 若 cmd == BTLD_FORCE_BANK1/2 → 清除 cmd，強制跳入指定 Bank（不返回）
 *   - 若 cmd == BTLD_UPDATE_METER  → 清除 cmd，返回 main 進入 OTA 接收迴圈
 *   - 否則健康檢查 + rollback 判斷，跳入有效 Bank（不返回）
 */
void Boot_SelectFW(void);

#endif /* SELECT_FW_H */
