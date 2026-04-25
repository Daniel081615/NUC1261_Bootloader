#include "fw_metadata.h"
#include "fmc_user.h"

void BankMetaUpdate(Bank_MetaInfo *meta)
{
    uint32_t MetaBase = Fw_BaseAddr[BankID][META];
    FMC_Erase(MetaBase);
    FMC_Proc(FMC_ISPCMD_PROGRAM, MetaBase, MetaBase + sizeof(Bank_MetaInfo), (uint32_t *)meta);
}

void FwInfoUpdate(FW_Info *fwinfo)
{
    uint32_t FwInfoBase = FW_INFO_BASE;
    FMC_Erase(FwInfoBase);
    FMC_Proc(FMC_ISPCMD_PROGRAM, FwInfoBase, FwInfoBase + sizeof(FW_Info), (uint32_t *)fwinfo);
}
