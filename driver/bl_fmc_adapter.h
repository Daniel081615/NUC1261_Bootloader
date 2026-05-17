#ifndef BL_FMC_ADAPTER_H
#define BL_FMC_ADAPTER_H

#include "flash_service.h"

/* BSP_Flash_* → IFmcDriver_t 的唯一橋接點 */
extern const IFmcDriver_t g_bl_fmc_driver;

#endif /* BL_FMC_ADAPTER_H */
