#ifndef		__HOSTPROCESS_H__
#define		__HOSTPROCESS_H__

#include	"NUC1261.h"

//	Memory Addr
#define BANK1_BASE					0x00002000
#define BANK2_BASE					0x00010000
#define FW_INFO_BASE				BANK2_BASE + BANK_SIZE
#define BANK_SIZE						BANK2_BASE - BANK1_BASE
#define DUAL_BANK						2
#define BANK1			0
#define BANK2			1
#define FW     		0
#define META 	1
//	Update Pack Length
#define UART_PACKET_PAYLOAD_LEN 	92


#define BANK1_META_BASE  		BANK2_BASE - FMC_FLASH_PAGE_SIZE
#define BANK2_META_BASE			(BANK2_BASE+BANK_SIZE) - FMC_FLASH_PAGE_SIZE

#define	VectorTableSize	 0xC0		// 192byte
#define	RegionTableSize	 0x20		// 32byte
#define PageInstructNum  (FMC_FLASH_PAGE_SIZE / sizeof(uint16_t))

/***	@Thumb PC	decode define ***/
//@LSLS 	0x0081	=>	Size of JmpAdrNum *4
//@ADR		0xa001	=>	JmpTbl StartAddr
//@LDR		0x5840	=>	Load JmpTbl Addr
//@MOV		0x4687
#define LDR_r0_sp_OPCODE			0x9800
#define	LDR_r0_INSTR					0x4800
#define LDR_r3_INSTR					0x68E0
#define ADR_r0_INSTR					0xA000
#define CMP_r0_INSTR					0x2800

#define JmpTbl_LSLS_INSTR1		0x0081
#define JmpTbl_ADR_INSTR2			0xA000
#define	JmpTbl_LDR_INSTR3			0x5840
#define	JmpTbl_MOV_INSTR4			0x4687
#define MOV_r8_r8							0x46c0

#define LDR_r0_INSTR_Msk			0xff00
#define LDR_r0_OFFSET_Msk			0x00ff

#define LDR_r3_INSTR_Msk			0xfff0
#define LDR_r3_OFFSET_Msk			0x000f

#define ADR_r0_INSTR_Msk			0xfff0
#define ADR_r0_OFFSET_Msk			0x000f

#define CMP_r0_INSTR_Msk			0xff00
#define CMP_r0_OFFSET_Msk			0x00ff

#define ALIGN_4Byte_Msk				0xfffffffc

/*** 	@fgFromHostFlag : Flag From Host ***/
#define FLAG_OTA_UPDATE		BIT7

/***	@fgToHostFlag ***/

#define TO_CENTER_INIT_METER			BIT6
#define TO_CENTER_CHG_USER_INFO		BIT7

typedef enum OTA_CMD_TOKEN
{	
		CMD_UPDATE_META_INFO   = 0x01,
    CMD_UPDATE_FW        			= 0x02,
		CMD_RESEND_PACKET					= 0x03,
	
} OTA_CMD_TOKEN;

enum DEFINE_RS485_CENTER_TOKEN 
{
		CTR_ALIVE					=	0x10,		//0x10			
		CTR_OTA_UPDATE		=	0x20,

		RSP_CTR_ACK				= 0x30,		//0x30
		RSP_CTR_OTA_INFO	= 0x40,
}	;


enum DEFINE_RS485_METER_TOKEN 
{
		METER_CMD_ALIVE				=	0x10,	//0x10
		METER_CMD_OTA_UPDATE	=	0x20,

		METER_RSP_ACK					= 0x30,	//0x30
		METER_RSP_SYS_INFO		= 0x31,
		METER_RSP_OTA_INFO		= 0x40,
} ;


//	FWMetadata
typedef struct {
    uint8_t flags;
		uint32_t Version;
    uint32_t Size;
		uint32_t RegionTable_Addr;
    uint32_t fw_crc32;
	
    uint8_t trial_counter;
} Bank_MetaInfo;

typedef struct {
		uint8_t		Bank;
		uint8_t 	cmd;
} FW_Info;

//	Extern
extern uint8_t BankID;
extern const uint32_t Fw_BaseAddr[DUAL_BANK][2];
extern FW_Info	NowFwInfo;

extern void BootloaderProcess(void);
extern void FwInfoUpdate(FW_Info *fwinfo);
extern void	PatchProcess(void);



#endif