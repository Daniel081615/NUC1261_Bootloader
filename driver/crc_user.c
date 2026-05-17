#include "crc_user.h"

uint32_t CRC32_Calculator(const uint32_t *pData, uint32_t len)
{
    uint32_t i, crc_result;

    CRC_Open(CRC_32, CRC_CHECKSUM_COM | CRC_CHECKSUM_RVS | CRC_WDATA_RVS,
             0xFFFFFFFF, CRC_CPU_WDATA_32);

    for (i = 0; i < (len / 4); i++) {
        CRC->DAT = pData[i];
    }

    crc_result = CRC_GetChecksum();
    return crc_result;
}
