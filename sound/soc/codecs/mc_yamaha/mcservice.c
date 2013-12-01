/****************************************************************************
 *
 *		Copyright(c) 2011-2012 Yamaha Corporation. All rights reserved.
 *
 *		Module		: mcservice.c
 *
 *		Description	: MC Driver service routine
 *
 *		Version		: 3.0.0 	2012.01.17
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


#include "mcdefs.h"
#include "mcservice.h"
#include "mcmachdep.h"
#include "mcresctrl.h"
#include "mcdevprof.h"
#if (MCDRV_DEBUG_LEVEL>=4)
#include "mcdebuglog.h"
#endif


/****************************************************************************
 *	McSrv_SystemInit
 *
 *	Description:
 *			Initialize the system.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McSrv_SystemInit
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McSrv_SystemInit");
#endif

	machdep_SystemInit();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McSrv_SystemInit", 0);
#endif
}

/****************************************************************************
 *	McSrv_SystemTerm
 *
 *	Description:
 *			Terminate the system.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McSrv_SystemTerm
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McSrv_SystemTerm");
#endif

	machdep_SystemTerm();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McSrv_SystemTerm", 0);
#endif
}

/****************************************************************************
 *	McSrv_ClockStart
 *
 *	Description:
 *			Start clock.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McSrv_ClockStart
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McSrv_ClockStart");
#endif

	machdep_ClockStart();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McSrv_ClockStart", 0);
#endif
}

/****************************************************************************
 *	McSrv_ClockStop
 *
 *	Description:
 *			Stop clock.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McSrv_ClockStop
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McSrv_ClockStop");
#endif

	machdep_ClockStop();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McSrv_ClockStop", 0);
#endif
}

/****************************************************************************
 *	McSrv_WriteI2C
 *
 *	Description:
 *			Write data to register.
 *	Arguments:
 *			bSlaveAddr	slave address
 *			pbData		data
 *			dSize		data size
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McSrv_WriteI2C
(
	UINT8	bSlaveAddr,
	UINT8	*pbData,
	UINT32	dSize
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McSrv_WriteI2C");
#endif

	McSrv_DisableIrq();
	machdep_WriteI2C( bSlaveAddr, pbData, dSize );
	McSrv_EnableIrq();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McSrv_WriteI2C", 0);
#endif
}

/****************************************************************************
 *	McSrv_ReadI2C
 *
 *	Function:
 *			Read a byte data from the register.
 *	Arguments:
 *			bSlaveAddr	slave address
 *			dRegAddr	address of register
 *	Return:
 *			read data
 *
 ****************************************************************************/
UINT8	McSrv_ReadI2C
(
	UINT8	bSlaveAddr,
	UINT32	dRegAddr
)
{
	UINT8	bReg;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("McSrv_ReadI2C");
#endif

	McSrv_DisableIrq();
	bReg	= machdep_ReadI2C( bSlaveAddr, dRegAddr );
	McSrv_EnableIrq();

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= (SINT32)bReg;
	McDebugLog_FuncOut("McSrv_ReadI2C", &sdRet);
#endif

	return bReg;
}

/****************************************************************************
 *	McSrv_WriteSPI
 *
 *	Description:
 *			Write data to register.
 *	Arguments:
 *			pbData		data
 *			dSize		data size
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McSrv_WriteSPI
(
	UINT8	*pbData,
	UINT32	dSize
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McSrv_WriteSPI");
#endif

	McSrv_DisableIrq();
	machdep_WriteSPI( pbData, dSize );
	McSrv_EnableIrq();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McSrv_WriteSPI", 0);
#endif
}

/****************************************************************************
 *	McSrv_ReadSPI
 *
 *	Function:
 *			Read a byte data from the register.
 *	Arguments:
 *			dRegAddr	address of register
 *	Return:
 *			read data
 *
 ****************************************************************************/
UINT8	McSrv_ReadSPI
(
	UINT32	dRegAddr
)
{
	UINT8	bReg;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("McSrv_ReadSPI");
#endif

	McSrv_DisableIrq();
	bReg	= machdep_ReadSPI( dRegAddr );
	McSrv_EnableIrq();

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= (SINT32)bReg;
	McDebugLog_FuncOut("McSrv_ReadSPI", &sdRet);
#endif

	return bReg;
}

/***************************************************************************
 *	McSrv_Sleep
 *
 *	Function:
 *			Sleep for a specified interval.
 *	Arguments:
 *			dSleepTime	sleep time [us]
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McSrv_Sleep
(
	UINT32	dSleepTime
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McSrv_Sleep");
#endif

	machdep_Sleep( dSleepTime );

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McSrv_Sleep", 0);
#endif
}

/****************************************************************************
 *	McSrv_Lock
 *
 *	Description:
 *			Lock a call of the driver.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McSrv_Lock
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McSrv_Lock");
#endif

	machdep_Lock();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McSrv_Lock", 0);
#endif
}

/***************************************************************************
 *	McSrv_Unlock
 *
 *	Function:
 *			Unlock a call of the driver.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McSrv_Unlock
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McSrv_Unlock");
#endif

	machdep_Unlock();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McSrv_Unlock", 0);
#endif
}

/***************************************************************************
 *	McSrv_MemCopy
 *
 *	Function:
 *			Copy memory.
 *	Arguments:
 *			pbSrc	copy source
 *			pbDest	copy destination
 *			dSize	size
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McSrv_MemCopy
(
	const UINT8	*pbSrc,
	UINT8		*pbDest,
	UINT32		dSize
)
{
	UINT32	i;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McSrv_MemCopy");
#endif

	for ( i = 0; i < dSize; i++ )
	{
		pbDest[i] = pbSrc[i];
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McSrv_MemCopy", 0);
#endif
}

/***************************************************************************
 *	McSrv_DisableIrq
 *
 *	Function:
 *			Disable interrupt.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McSrv_DisableIrq
(
	void
)
{
#if 0
	UINT8	abData[2];
#endif

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McSrv_DisableIrq");
#endif

#if 0
	if(McResCtrl_IsEnableIRQ() != 0)
	{
		if(1L == McResCtrl_IncIrqDisableCount())
		{
			abData[0]	= MCI_RST;
#if 0
			abData[1]	= machdep_ReadI2C(McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG), abData[0]);
			abData[1]	&= (UINT8)~MCB_EIRQ;
#else
			abData[1]	= 0;
#endif
			machdep_WriteI2C(McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG), abData, 2);
		}
	}
#endif

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McSrv_DisableIrq", 0);
#endif
}

/***************************************************************************
 *	McSrv_EnableIrq
 *
 *	Function:
 *			Enable interrupt.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McSrv_EnableIrq
(
	void
)
{
#if 0
	UINT8	abData[2];
#endif

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McSrv_EnableIrq");
#endif

#if 0
	if(McResCtrl_IsEnableIRQ() != 0)
	{
		if(0L == McResCtrl_DecIrqDisableCount())
		{
			abData[0]	= MCI_RST;
#if 0
			abData[1]	= machdep_ReadI2C(McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG), abData[0]);
#else
			abData[1]	= MCB_EIRQ;
#endif
			machdep_WriteI2C(McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG), abData, 2);
		}
	}
#endif

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McSrv_EnableIrq", 0);
#endif
}

