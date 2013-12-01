/****************************************************************************
 *
 *		Copyright(c) 2011-2012 Yamaha Corporation. All rights reserved.
 *
 *		Module		: mcmachdep.h
 *
 *		Description	: MC Driver machine dependent part header
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

#ifndef _MCMACHDEP_H_
#define _MCMACHDEP_H_

#include "mctypedef.h"


#define	MCDRV_DEBUG_LEVEL		(0)
#define	MCDRV_USE_CDSP_DRIVER	(1)


void	machdep_SystemInit	( void );
void	machdep_SystemTerm	( void );
void	machdep_ClockStart	( void );
void	machdep_ClockStop	( void );
void	machdep_WriteI2C	( UINT8 bSlaveAdr, const UINT8 *pbData, UINT32 dSize );
UINT8	machdep_ReadI2C		( UINT8 bSlaveAdr, UINT32 dAddress );
void	machdep_WriteSPI	( const UINT8 *pbData, UINT32 dSize );
UINT8	machdep_ReadSPI		( UINT32 dAddress );
void	machdep_Sleep		( UINT32 dSleepTime );
void	machdep_Lock		( void );
void	machdep_Unlock		( void );
void	machdep_DebugPrint	( UINT8 *pbLogString );


#endif /* _MCMACHDEP_H_ */
