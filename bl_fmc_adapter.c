#include "bl_fmc_adapter.h"
#include "bsp_flash.h"

static int32_t Adapter_Init(void)
{
    BSP_Flash_Init();
    return 0;
}

static int32_t Adapter_ErasePage(uint32_t addr)
{
    return (int32_t)BSP_Flash_ErasePage(addr);
}

static int32_t Adapter_WriteWords(uint32_t addr, const uint32_t *data, uint32_t cnt)
{
    return (int32_t)BSP_Flash_WriteWords(addr, data, cnt);
}

const IFmcDriver_t g_bl_fmc_driver = {
    .Init       = Adapter_Init,
    .ErasePage  = Adapter_ErasePage,
    .WriteWords = Adapter_WriteWords,
    .ReadWords  = BSP_Flash_ReadWords,
    .GetCRC32   = BSP_Flash_GetCRC32,
    .JumpToApp  = BSP_Flash_JumpToApp,
};
