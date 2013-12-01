/****************************************************************************
 *
 *		Copyright (c) 2011 Yamaha Corporation
 *
 *		Module		: mccdspos.h
 *
 *		Description	: C-DSP Operating System
 *
 *		Version		: 3.0.0 	2011.01.25
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

#ifndef __MCCDSPOS_H__
#define __MCCDSPOS_H__

static const unsigned char gabCdspOs[] =
{
	0x00, 0x78,		/* LoadAddress */
	0x0C, 0x00,		/* Size */
/* data */
	0x2D, 0xAE, 0xC3, 0xB2, 0x61, 0xB0, 0xF5, 0xB1, 0xF5, 0x8E, 0xF5, 0x8F, 0x6F, 0xB7, 0x7D, 0xB7, 
	0x5B, 0xB7, 0xBD, 0xB7, 0x63, 0xB8, 0x47, 0xB8, 
};

#endif	/* __MCCDSPOS_H__ */
