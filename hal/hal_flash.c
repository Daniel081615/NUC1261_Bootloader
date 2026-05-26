#include "hal_flash.h"
#include "bsp_flash.h"

void HAL_Flash_Init(void)
{
    BSP_Flash_Init();
}

HAL_Flash_Status HAL_Flash_ErasePage(uint32_t addr)
{
    return (HAL_Flash_Status)BSP_Flash_ErasePage(addr);
}

HAL_Flash_Status HAL_Flash_WriteWords(uint32_t addr, const uint32_t *data,
                                      uint32_t word_count)
{
    return (HAL_Flash_Status)BSP_Flash_WriteWords(addr, data, word_count);
}

void HAL_Flash_ReadWords(uint32_t addr, uint32_t *data, uint32_t word_count)
{
    BSP_Flash_ReadWords(addr, data, word_count);
}

uint32_t HAL_Flash_GetCRC32(uint32_t addr, uint32_t byte_len)
{
    return BSP_Flash_GetCRC32(addr, byte_len);
}

void HAL_Flash_JumpToApp(uint32_t app_base)
{
    BSP_Flash_JumpToApp(app_base);
}
