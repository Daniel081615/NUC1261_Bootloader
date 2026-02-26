/******************************************************************************
 * @file     targetdev.c
 * @brief    ISP 支援功能
 * @version  0x31
 ******************************************************************************/

#include "targetdev.h"
#include "isp_user.h"
#define 	CONFIG0_DFEN 		0x01  // Data Flash Enable
#define 	META_DFBA				0x0001E800

#define CONFIG0_DFEN_Msk    (1ul << 1)
/**
 * @brief 取得 APROM 實際大小（自動偵測 128K/256K）
 * @return APROM 大小（bytes）
 */
//uint32_t GetApromSize(void)
//{
//    uint32_t size = 0x20000, data;
//    int result;

//    do {
//        result = FMC_Read_User(size, &data);
//        if (result < 0) return size;
//        else size *= 2;
//    } while (1);
//}



/**
 * @brief 取得 Data Flash 起始位址與大小
 * @param[out] addr 起始位址
 * @param[out] size 大小（bytes）
 */
void GetDataFlashInfo(uint32_t *addr, uint32_t *size)
{
    uint32_t uData;
    *size = 0;
    FMC_Read_User(Config0, &uData);

    if ((uData & CONFIG0_DFEN) == 0) { // DFEN enable
        FMC_Read_User(Config1, &uData);
        if (uData > g_apromSize || (uData & 0x7FF)) uData = g_apromSize;
        *addr = uData;
        *size = g_apromSize - uData;
    } else {
        *addr = g_apromSize;
        *size = 0;
    }
}

/*** (C) COPYRIGHT 2019 Nuvoton Technology Corp. ***/