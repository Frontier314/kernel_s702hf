/****************************************************************************
 *
 *		Copyright(c) 2011 Yamaha Corporation. All rights reserved.
 *
 *		Module		: mcpacking.h
 *
 *		Description	: MC Driver Packet packing header
 *
 *		Version		: 3.0.0 	2011.11.17
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.	In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *	claim that you wrote the original software. If you use this software
 *	in a product, an acknowledgment in the product documentation would be
 *	appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *	misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ****************************************************************************/

#ifndef _MCPACKING_H_
#define _MCPACKING_H_

#include "mctypedef.h"
#include "mcdriver.h"

/* volume update */
typedef enum
{
	eMCDRV_VOLUPDATE_MUTE,
	eMCDRV_VOLUPDATE_ALL
} MCDRV_VOLUPDATE_MODE;

/*	power setting	*/
#define	MCDRV_POWINFO_DIGITAL_DP0			((UINT32)0x0001)
#define	MCDRV_POWINFO_DIGITAL_DP1			((UINT32)0x0002)
#define	MCDRV_POWINFO_DIGITAL_DP2			((UINT32)0x0004)
#define	MCDRV_POWINFO_DIGITAL_DPB			((UINT32)0x0008)
#define	MCDRV_POWINFO_DIGITAL_DPC			((UINT32)0x0010)
#define	MCDRV_POWINFO_DIGITAL_DPDI0			((UINT32)0x0020)
#define	MCDRV_POWINFO_DIGITAL_DPDI1			((UINT32)0x0040)
#define	MCDRV_POWINFO_DIGITAL_DPDI2			((UINT32)0x0080)
#define	MCDRV_POWINFO_DIGITAL_DPPDM			((UINT32)0x0100)
#define	MCDRV_POWINFO_DIGITAL_DPCD			((UINT32)0x0200)
#define	MCDRV_POWINFO_DIGITAL_DPBDSP		((UINT32)0x0400)
#define	MCDRV_POWINFO_DIGITAL_DPADIF		((UINT32)0x0800)
#define	MCDRV_POWINFO_DIGITAL_PLLRST0		((UINT32)0x1000)
typedef struct
{
	UINT32	dDigital;
	UINT8	abAnalog[6];
} MCDRV_POWER_INFO;

/* power update */
typedef struct
{
	UINT32	dDigital;
	UINT8	abAnalog[6];
} MCDRV_POWER_UPDATE;

#define	MCDRV_POWUPDATE_DIGITAL_ALL			(0xFFFFFFFFUL)
#define	MCDRV_POWUPDATE_ANALOG0_ALL			(0x0F)
#define	MCDRV_POWUPDATE_ANALOG0_IN			(0x0D)
#define	MCDRV_POWUPDATE_ANALOG0_OUT			(0x02)
#define	MCDRV_POWUPDATE_ANALOG1_ALL			(0xFF)
#define	MCDRV_POWUPDATE_ANALOG1_IN			(0xC0)
#define	MCDRV_POWUPDATE_ANALOG1_OUT			(0x3F)
#define	MCDRV_POWUPDATE_ANALOG2_ALL			(0xBF)
#define	MCDRV_POWUPDATE_ANALOG2_IN			(0x00)
#define	MCDRV_POWUPDATE_ANALOG2_OUT			(0xBF)
#define	MCDRV_POWUPDATE_ANALOG2_LO1L		(0x01)
#define	MCDRV_POWUPDATE_ANALOG2_LO1R		(0x02)
#define	MCDRV_POWUPDATE_ANALOG2_LO2L		(0x04)
#define	MCDRV_POWUPDATE_ANALOG2_LO2R		(0x08)
#define	MCDRV_POWUPDATE_ANALOG3_ALL			(0x1F)
#define	MCDRV_POWUPDATE_ANALOG3_IN			(0x1F)
#define	MCDRV_POWUPDATE_ANALOG3_OUT			(0x00)
#define	MCDRV_POWUPDATE_ANALOG4_ALL			(0xF0)
#define	MCDRV_POWUPDATE_ANALOG4_IN			(0xF0)
#define	MCDRV_POWUPDATE_ANALOG4_OUT			(0x00)
#define	MCDRV_POWUPDATE_ANALOG5_ALL			(0x20)
#define	MCDRV_POWUPDATE_ANALOG5_IN			(0x20)
#define	MCDRV_POWUPDATE_ANALOG5_OUT			(0x00)


SINT32		McPacket_AddInit			(void);
SINT32		McPacket_AddVol				(UINT32 dUpdate, MCDRV_VOLUPDATE_MODE eMode, UINT32* pdSVolDoneParam);
SINT32		McPacket_AddPowerUp			(const MCDRV_POWER_INFO* psPowerInfo, const MCDRV_POWER_UPDATE* psPowerUpdate);
SINT32		McPacket_AddPowerDown		(const MCDRV_POWER_INFO* psPowerInfo, const MCDRV_POWER_UPDATE* psPowerUpdate);
SINT32		McPacket_AddPathSet			(void);
SINT32		McPacket_StopDI2			(void);
SINT32		McPacket_AddMixSet			(void);
SINT32		McPacket_AddStart			(void);
SINT32		McPacket_AddStop			(void);
SINT32		McPacket_AddDigitalIO		(UINT32 dUpdateInfo);
SINT32		McPacket_AddDAC				(UINT32 dUpdateInfo);
SINT32		McPacket_AddADC				(UINT32 dUpdateInfo);
SINT32		McPacket_AddADCEx			(UINT32 dUpdateInfo);
SINT32		McPacket_AddSP				(void);
SINT32		McPacket_AddDNG				(UINT32 dUpdateInfo);
SINT32		McPacket_AddAE				(UINT32 dUpdateInfo);
SINT32		McPacket_AddAeEx			(void);
SINT32		McPacket_AddPDM				(UINT32 dUpdateInfo);
SINT32		McPacket_AddPDMEx			(UINT32 dUpdateInfo);
SINT32		McPacket_AddDTMF			(UINT32 dUpdateInfo);
SINT32		McPacket_AddGPMode			(void);
SINT32		McPacket_AddGPMask			(void);
SINT32		McPacket_AddGPSet			(void);
SINT32		McPacket_AddSysEq			(UINT32 dUpdateInfo);
SINT32		McPacket_AddClockSwitch		(void);
SINT32		McPacket_AddDitSwap			(UINT32 dUpdateInfo);
SINT32		McPacket_AddHSDet			(void);
SINT32		McPacket_AddMKDetEnable		(void);


#endif /* _MCPACKING_H_ */
