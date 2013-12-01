/****************************************************************************
 *
 *		Copyright(c) 2011-2012 Yamaha Corporation. All rights reserved.
 *
 *		Module		: mcdevprof.h
 *
 *		Description	: MC Driver device profile header
 *
 *		Version		: 3.1.0 	2012.02.22
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

#ifndef _MCDEVPROF_H_
#define _MCDEVPROF_H_

#include "mctypedef.h"
#include "mcdriver.h"
#include "mcresctrl.h"

typedef enum
{
	eMCDRV_DEV_ID_79H	= 0,
	eMCDRV_DEV_ID_71H,
	eMCDRV_DEV_ID_46_14H,
	eMCDRV_DEV_ID_44_14H,
	eMCDRV_DEV_ID_46_15H,
	eMCDRV_DEV_ID_44_15H,
	eMCDRV_DEV_ID_46_16H,
	eMCDRV_DEV_ID_44_16H
} MCDRV_DEV_ID;

typedef enum
{
	eMCDRV_FUNC_LI1	= 0,
	eMCDRV_FUNC_LI2,
	eMCDRV_FUNC_CPMODE,
	eMCDRV_FUNC_HPIDLE,
	eMCDRV_FUNC_DNG,
	eMCDRV_FUNC_RANGE,
	eMCDRV_FUNC_BYPASS,
	eMCDRV_FUNC_ADC1,
	eMCDRV_FUNC_PAD2,
	eMCDRV_FUNC_DBEX,
	eMCDRV_FUNC_GPMODE,
	eMCDRV_FUNC_PM,
	eMCDRV_FUNC_DTMF,
	eMCDRV_FUNC_MIX2,
	eMCDRV_FUNC_CDSP,
	eMCDRV_FUNC_DITSWAP,
	eMCDRV_FUNC_DACSWAP,
	eMCDRV_FUNC_IRQ,
	eMCDRV_FUNC_DIO2,
	eMCDRV_FUNC_POW
} MCDRV_FUNC_KIND;

typedef enum
{
	eMCDRV_SLAVE_ADDR_DIG	= 0,
	eMCDRV_SLAVE_ADDR_ANA,
	eMCDRV_SLAVE_ADDR_POW,
	eMCDRV_SLAVE_ADDR_RTC,
	eMCDRV_SLAVE_ADDR_DVS,
	eMCDRV_SLAVE_ADDR_ROM
} MCDRV_SLAVE_ADDR_KIND;



void	McDevProf_SetDevId(MCDRV_DEV_ID eDevId);
MCDRV_DEV_ID	McDevProf_GetDevId(void);

UINT8	McDevProf_IsValid(MCDRV_FUNC_KIND eFuncKind);
UINT8	McDevProf_GetSlaveAddr(MCDRV_SLAVE_ADDR_KIND eSlaveAddrKind);
void	McDevProf_MaskPathInfo(MCDRV_PATH_INFO*	psPathInfo);

UINT8	McDevProf_GetBDSPAddr(UINT32 dUpdateInfo);
UINT32	McDevProf_GetDRCDataSize(void);
UINT8	McDevProf_GetSRCRateSet(void);

MCDRV_REG_ACCSESS	McDevProf_GetRegAccess(const MCDRV_REG_INFO* psRegInfo);



#endif	/*	_MCDEVPROF_H_	*/
