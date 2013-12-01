/*
 * MC ASoC codec driver
 *
 * Copyright (c) 2011-2012 Yamaha Corporation
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "mc_asoc_priv.h"

#ifdef CONFIG_SND_SOC_MC_YAMAHA_DEBUG

static void mc_asoc_dump_init_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_INIT_INFO *info = (MCDRV_INIT_INFO *)pvPrm;

	dbg_info("bCkSel      = 0x%02x\n", info->bCkSel);
	dbg_info("bDivR0      = 0x%02x\n", info->bDivR0);
	dbg_info("bDivF0      = 0x%02x\n", info->bDivF0);
	dbg_info("bDivR1      = 0x%02x\n", info->bDivR1);
	dbg_info("bDivF1      = 0x%02x\n", info->bDivF1);
	dbg_info("bRange0     = 0x%02x\n", info->bRange0);
	dbg_info("bRange1     = 0x%02x\n", info->bRange1);
	dbg_info("bBypass     = 0x%02x\n", info->bBypass);
	dbg_info("bDioSdo0Hiz = 0x%02x\n", info->bDioSdo0Hiz);
	dbg_info("bDioSdo1Hiz = 0x%02x\n", info->bDioSdo1Hiz);
	dbg_info("bDioSdo2Hiz = 0x%02x\n", info->bDioSdo2Hiz);
	dbg_info("bDioClk0Hiz = 0x%02x\n", info->bDioClk0Hiz);
	dbg_info("bDioClk1Hiz = 0x%02x\n", info->bDioClk1Hiz);
	dbg_info("bDioClk2Hiz = 0x%02x\n", info->bDioClk2Hiz);

	dbg_info("bPcmHiz     = 0x%02x\n", info->bPcmHiz);
	dbg_info("bLineIn1Dif = 0x%02x\n", info->bLineIn1Dif);
	dbg_info("bLineIn2Dif = 0x%02x\n", info->bLineIn2Dif);
	dbg_info("bLineOut1Dif= 0x%02x\n", info->bLineOut1Dif);
	dbg_info("bLineOut2Dif= 0x%02x\n", info->bLineOut2Dif);
	dbg_info("bSpmn       = 0x%02x\n", info->bSpmn);
	dbg_info("bMic1Sng    = 0x%02x\n", info->bMic1Sng);
	dbg_info("bMic2Sng    = 0x%02x\n", info->bMic2Sng);
	dbg_info("bMic3Sng    = 0x%02x\n", info->bMic3Sng);
	dbg_info("bPowerMode  = 0x%02x\n", info->bPowerMode);
	dbg_info("bSpHiz      = 0x%02x\n", info->bSpHiz);
	dbg_info("bLdo        = 0x%02x\n", info->bLdo);
	dbg_info("bPad0Func   = 0x%02x\n", info->bPad0Func);
	dbg_info("bPad1Func   = 0x%02x\n", info->bPad1Func);
	dbg_info("bPad2Func   = 0x%02x\n", info->bPad2Func);
	dbg_info("bAvddLev    = 0x%02x\n", info->bAvddLev);
	dbg_info("bVrefLev    = 0x%02x\n", info->bVrefLev);
	dbg_info("bDclGain    = 0x%02x\n", info->bDclGain);
	dbg_info("bDclLimit   = 0x%02x\n", info->bDclLimit);
	dbg_info("bCpMod      = 0x%02x\n", info->bCpMod);
	dbg_info("bSdDs       = 0x%02x\n", info->bSdDs);

	dbg_info("sWaitTime.dAdHpf   = 0x%08x\n", (unsigned int)info->sWaitTime.dAdHpf);
	dbg_info("sWaitTime.dMic1Cin = 0x%08x\n", (unsigned int)info->sWaitTime.dMic1Cin);
	dbg_info("sWaitTime.dMic2Cin = 0x%08x\n", (unsigned int)info->sWaitTime.dMic2Cin);
	dbg_info("sWaitTime.dMic3Cin = 0x%08x\n", (unsigned int)info->sWaitTime.dMic3Cin);
	dbg_info("sWaitTime.dLine1Cin= 0x%08x\n", (unsigned int)info->sWaitTime.dLine1Cin);
	dbg_info("sWaitTime.dLine2Cin= 0x%08x\n", (unsigned int)info->sWaitTime.dLine2Cin);
	dbg_info("sWaitTime.dVrefRdy1= 0x%08x\n", (unsigned int)info->sWaitTime.dVrefRdy1);
	dbg_info("sWaitTime.dVrefRdy2= 0x%08x\n", (unsigned int)info->sWaitTime.dVrefRdy2);
	dbg_info("sWaitTime.dHpRdy   = 0x%08x\n", (unsigned int)info->sWaitTime.dHpRdy);
	dbg_info("sWaitTime.dSpRdy   = 0x%08x\n", (unsigned int)info->sWaitTime.dSpRdy);
	dbg_info("sWaitTime.dPdm     = 0x%08x\n", (unsigned int)info->sWaitTime.dPdm);
	dbg_info("sWaitTime.dAnaRdyInterval = 0x%08x\n", (unsigned int)info->sWaitTime.dAnaRdyInterval);
	dbg_info("sWaitTime.dSvolInterval   = 0x%08x\n", (unsigned int)info->sWaitTime.dSvolInterval);
	dbg_info("sWaitTime.dAnaRdyTimeOut  = 0x%08x\n", (unsigned int)info->sWaitTime.dAnaRdyTimeOut);
	dbg_info("sWaitTime.dSvolTimeOut    = 0x%08x\n", (unsigned int)info->sWaitTime.dSvolTimeOut);
}

static void mc_asoc_dump_reg_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_REG_INFO *info = (MCDRV_REG_INFO *)pvPrm;

	dbg_info("bRegType = 0x%02x\n", info->bRegType);
	dbg_info("bAddress = 0x%02x\n", info->bAddress);
	dbg_info("bData    = 0x%02x\n", info->bData);
}

static void mc_asoc_dump_array(const char *name,
			     const unsigned char *data, size_t len)
{
	char str[128], *p;
#if 1
	int n = len;
#else
	int n = (len <= 282) ? len : 282;
#endif
	int i;

	p = str;
	dbg_info("%s[] = %d byte\n", name, n);
	for (i = 0; i < n; i++) {
		p += sprintf(p, "0x%02x ", data[i]);
#if 1
		if ((i % 16 == 15) || (i == n - 1)) {
#else
		if ((i % 8 == 7) || (i == n - 1)) {
#endif
			dbg_info("%s\n", str);
			p = str;
		}
	}
}

#define DEF_PATH(p) {offsetof(MCDRV_PATH_INFO, p), #p}

static void mc_asoc_dump_path_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_PATH_INFO *info = (MCDRV_PATH_INFO *)pvPrm;
	int i;
	UINT8	mask	= (dPrm==0)?0xFF:dPrm;

	struct path_table {
		size_t offset;
		char *name;
	};

	struct path_table table[] = {
		DEF_PATH(asHpOut[0]), DEF_PATH(asHpOut[1]),
		DEF_PATH(asSpOut[0]), DEF_PATH(asSpOut[1]),
		DEF_PATH(asRcOut[0]),
		DEF_PATH(asLout1[0]), DEF_PATH(asLout1[1]),
		DEF_PATH(asLout2[0]), DEF_PATH(asLout2[1]),
//		DEF_PATH(asPeak[0]),
		DEF_PATH(asDit0[0]),
		DEF_PATH(asDit1[0]),
		DEF_PATH(asDit2[0]),
		DEF_PATH(asDac[0]), DEF_PATH(asDac[1]),
		DEF_PATH(asAe[0]),
		DEF_PATH(asCdsp[0]), DEF_PATH(asCdsp[1]),
		DEF_PATH(asCdsp[2]), DEF_PATH(asCdsp[3]),
		DEF_PATH(asAdc0[0]), DEF_PATH(asAdc0[1]),
		DEF_PATH(asAdc1[0]),
		DEF_PATH(asMix[0]),
		DEF_PATH(asBias[0]),
//		DEF_PATH(asMix2[0]),
	};

#define N_PATH_TABLE (sizeof(table) / sizeof(struct path_table))

	for (i = 0; i < N_PATH_TABLE; i++) {
		MCDRV_CHANNEL *ch = (MCDRV_CHANNEL *)((void *)info + table[i].offset);
#if 1
		dbg_info("%s.abSrcOnOff\t= 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
			table[i].name,
			ch->abSrcOnOff[0] & mask,
			ch->abSrcOnOff[1] & mask,
			ch->abSrcOnOff[2] & mask,
			ch->abSrcOnOff[3] & mask,
			ch->abSrcOnOff[4] & mask,
			ch->abSrcOnOff[5] & mask,
			ch->abSrcOnOff[6] & mask
			);
#else
		int j;
		for (j = 0; j < SOURCE_BLOCK_NUM; j++) {
			if (ch->abSrcOnOff[j] != 0) {
				dbg_info("%s.abSrcOnOff[%d] = 0x%02x\n",
					 table[i].name, j, ch->abSrcOnOff[j]);
			}
		}
#endif
	}
}

#define DEF_VOL(v) {offsetof(MCDRV_VOL_INFO, v), #v}

static void mc_asoc_dump_vol_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_VOL_INFO *info = (MCDRV_VOL_INFO *)pvPrm;
	int i;

	struct vol_table {
		size_t offset;
		char *name;
	};

	struct vol_table table[] = {
		DEF_VOL(aswD_Ad0[0]), DEF_VOL(aswD_Ad0[1]),
		DEF_VOL(aswD_Reserved1[0]),
		DEF_VOL(aswD_Aeng6[0]), DEF_VOL(aswD_Aeng6[1]),
		DEF_VOL(aswD_Pdm[0]), DEF_VOL(aswD_Pdm[1]),
		DEF_VOL(aswD_Dtmfb[0]), DEF_VOL(aswD_Dtmfb[1]),
		DEF_VOL(aswD_Dir0[0]), DEF_VOL(aswD_Dir0[1]),
		DEF_VOL(aswD_Dir1[0]), DEF_VOL(aswD_Dir1[1]),
		DEF_VOL(aswD_Dir2[0]), DEF_VOL(aswD_Dir2[1]),
		DEF_VOL(aswD_Ad0Att[0]), DEF_VOL(aswD_Ad0Att[1]),
		DEF_VOL(aswD_Reserved2[0]),
		DEF_VOL(aswD_Dir0Att[0]), DEF_VOL(aswD_Dir0Att[1]),
		DEF_VOL(aswD_Dir1Att[0]), DEF_VOL(aswD_Dir1Att[1]),
		DEF_VOL(aswD_Dir2Att[0]), DEF_VOL(aswD_Dir2Att[1]),
		DEF_VOL(aswD_SideTone[0]), DEF_VOL(aswD_SideTone[1]),
		DEF_VOL(aswD_DtmfAtt[0]), DEF_VOL(aswD_DtmfAtt[1]),
		DEF_VOL(aswD_DacMaster[0]), DEF_VOL(aswD_DacMaster[1]),
		DEF_VOL(aswD_DacVoice[0]), DEF_VOL(aswD_DacVoice[1]),
		DEF_VOL(aswD_DacAtt[0]), DEF_VOL(aswD_DacAtt[1]),
		DEF_VOL(aswD_Dit0[0]), DEF_VOL(aswD_Dit0[1]),
		DEF_VOL(aswD_Dit1[0]), DEF_VOL(aswD_Dit1[1]),
		DEF_VOL(aswD_Dit2[0]), DEF_VOL(aswD_Dit2[1]),
		DEF_VOL(aswA_Ad0[0]), DEF_VOL(aswA_Ad0[1]),
		DEF_VOL(aswA_Ad1[0]),
		DEF_VOL(aswA_Lin1[0]), DEF_VOL(aswA_Lin1[1]),
		DEF_VOL(aswA_Lin2[0]), DEF_VOL(aswA_Lin2[1]),
		DEF_VOL(aswA_Mic1[0]),
		DEF_VOL(aswA_Mic2[0]),
		DEF_VOL(aswA_Mic3[0]),
		DEF_VOL(aswA_Hp[0]), DEF_VOL(aswA_Hp[1]),
		DEF_VOL(aswA_Sp[0]), DEF_VOL(aswA_Sp[1]),
		DEF_VOL(aswA_Rc[0]),
		DEF_VOL(aswA_Lout1[0]), DEF_VOL(aswA_Lout1[1]),
		DEF_VOL(aswA_Lout2[0]), DEF_VOL(aswA_Lout2[1]),
		DEF_VOL(aswA_Mic1Gain[0]),
		DEF_VOL(aswA_Mic2Gain[0]),
		DEF_VOL(aswA_Mic3Gain[0]),
		DEF_VOL(aswA_HpGain[0]),
		DEF_VOL(aswD_Adc1[0]), DEF_VOL(aswD_Adc1[1]),
		DEF_VOL(aswD_Adc1Att[0]), DEF_VOL(aswD_Adc1Att[1]),
		DEF_VOL(aswD_Dit21[0]), DEF_VOL(aswD_Dit21[1]),
//		DEF_VOL(aswD_Adc0AeMix[0]), DEF_VOL(aswD_Adc0AeMix[1]),
//		DEF_VOL(aswD_Adc1AeMix[0]), DEF_VOL(aswD_Adc1AeMix[1]),
//		DEF_VOL(aswD_Dir0AeMix[0]), DEF_VOL(aswD_Dir0AeMix[1]),
//		DEF_VOL(aswD_Dir1AeMix[0]), DEF_VOL(aswD_Dir1AeMix[1]),
//		DEF_VOL(aswD_Dir2AeMix[0]), DEF_VOL(aswD_Dir2AeMix[1]),
//		DEF_VOL(aswD_PdmAeMix[0]), DEF_VOL(aswD_PdmAeMix[1]),
//		DEF_VOL(aswD_MixAeMix[0]), DEF_VOL(aswD_MixAeMix[1]),
	};

#define N_VOL_TABLE (sizeof(table) / sizeof(struct vol_table))

	for (i = 0; i < N_VOL_TABLE; i++) {
		SINT16 vol = *(SINT16 *)((void *)info + table[i].offset);
		if (vol & 0x0001) {
			dbg_info("%s = 0x%04x\n", table[i].name, (vol & 0xfffe));
		}
	}
}

static void mc_asoc_dump_dio_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_DIO_INFO *info = (MCDRV_DIO_INFO *)pvPrm;
	MCDRV_DIO_PORT *port;
	UINT32 update;
	int i;

	for (i = 0; i < IOPORT_NUM; i++) {
		dbg_info("asPortInfo[%d]:\n", i);
		port = &info->asPortInfo[i];
		update = dPrm >> (i*3);
		if (update & MCDRV_DIO0_COM_UPDATE_FLAG) {
			dbg_info("sDioCommon.bMasterSlave = 0x%02x\n",
				 port->sDioCommon.bMasterSlave);
			dbg_info("           bAutoFs = 0x%02x\n",
				 port->sDioCommon.bAutoFs);
			dbg_info("           bFs = 0x%02x\n",
				 port->sDioCommon.bFs);
			dbg_info("           bBckFs = 0x%02x\n",
				 port->sDioCommon.bBckFs);
			dbg_info("           bInterface = 0x%02x\n",
				 port->sDioCommon.bInterface);
			dbg_info("           bBckInvert = 0x%02x\n",
				 port->sDioCommon.bBckInvert);
			dbg_info("           bPcmHizTim = 0x%02x\n",
				 port->sDioCommon.bPcmHizTim);
			dbg_info("           bPcmClkDown = 0x%02x\n",
				 port->sDioCommon.bPcmClkDown);
			dbg_info("           bPcmFrame = 0x%02x\n",
				 port->sDioCommon.bPcmFrame);
			dbg_info("           bPcmHighPeriod = 0x%02x\n",
				 port->sDioCommon.bPcmHighPeriod);
		}
		if (update & MCDRV_DIO0_DIR_UPDATE_FLAG) {
			dbg_info("sDir.wSrcRate = 0x%04x\n",
				 port->sDir.wSrcRate);
			dbg_info("     sDaFormat.bBitSel = 0x%02x\n",
				 port->sDir.sDaFormat.bBitSel);
			dbg_info("               bMode = 0x%02x\n",
				 port->sDir.sDaFormat.bMode);
			dbg_info("     sPcmFormat.bMono = 0x%02x\n",
				 port->sDir.sPcmFormat.bMono);
			dbg_info("                bOrder = 0x%02x\n",
				 port->sDir.sPcmFormat.bOrder);
			dbg_info("                bLaw = 0x%02x\n",
				 port->sDir.sPcmFormat.bLaw);
			dbg_info("                bBitSel = 0x%02x\n",
				 port->sDir.sPcmFormat.bBitSel);
			dbg_info("     abSlot[] = {0x%02x, 0x%02x}\n",
				 port->sDir.abSlot[0], port->sDir.abSlot[1]);
		}
		if (update & MCDRV_DIO0_DIT_UPDATE_FLAG) {
			dbg_info("sDit.wSrcRate = 0x%04x\n",
				 port->sDit.wSrcRate);
			dbg_info("     sDaFormat.bBitSel = 0x%02x\n",
				 port->sDit.sDaFormat.bBitSel);
			dbg_info("               bMode = 0x%02x\n",
				 port->sDit.sDaFormat.bMode);
			dbg_info("     sPcmFormat.bMono = 0x%02x\n",
				 port->sDit.sPcmFormat.bMono);
			dbg_info("                bOrder = 0x%02x\n",
				 port->sDit.sPcmFormat.bOrder);
			dbg_info("                bLaw = 0x%02x\n",
				 port->sDit.sPcmFormat.bLaw);
			dbg_info("                bBitSel = 0x%02x\n",
				 port->sDit.sPcmFormat.bBitSel);
			dbg_info("     abSlot[] = {0x%02x, 0x%02x}\n",
				 port->sDit.abSlot[0], port->sDit.abSlot[1]);
		}
	}
}

static void mc_asoc_dump_dac_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_DAC_INFO *info = (MCDRV_DAC_INFO *)pvPrm;

	if (dPrm & MCDRV_DAC_MSWP_UPDATE_FLAG) {
		dbg_info("bMasterSwap = 0x%02x\n", info->bMasterSwap);
	}
	if (dPrm & MCDRV_DAC_VSWP_UPDATE_FLAG) {
		dbg_info("bVoiceSwap = 0x%02x\n", info->bVoiceSwap);
	}
	if (dPrm & MCDRV_DAC_HPF_UPDATE_FLAG) {
		dbg_info("bDcCut = 0x%02x\n", info->bDcCut);
	}
	if (dPrm & MCDRV_DAC_DACSWP_UPDATE_FLAG) {
		dbg_info("bDacSwap = 0x%02x\n", info->bDacSwap);
	}
}

static void mc_asoc_dump_adc_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_ADC_INFO *info = (MCDRV_ADC_INFO *)pvPrm;

	if (dPrm & MCDRV_ADCADJ_UPDATE_FLAG) {
		dbg_info("bAgcAdjust = 0x%02x\n", info->bAgcAdjust);
	}
	if (dPrm & MCDRV_ADCAGC_UPDATE_FLAG) {
		dbg_info("bAgcOn = 0x%02x\n", info->bAgcOn);
	}
	if (dPrm & MCDRV_ADCMONO_UPDATE_FLAG) {
		dbg_info("bMono = 0x%02x\n", info->bMono);
	}
}

static void mc_asoc_dump_sp_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_SP_INFO *info = (MCDRV_SP_INFO *)pvPrm;

	dbg_info("bSwap = 0x%02x\n", info->bSwap);
}

static void mc_asoc_dump_dng_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_DNG_INFO *info = (MCDRV_DNG_INFO *)pvPrm;

	if (dPrm & MCDRV_DNGSW_HP_UPDATE_FLAG) {
		dbg_info("HP:abOnOff = 0x%02x\n", info->abOnOff[0]);
	}
	if (dPrm & MCDRV_DNGTHRES_HP_UPDATE_FLAG) {
		dbg_info("HP:abThreshold = 0x%02x\n", info->abThreshold[0]);
	}
	if (dPrm & MCDRV_DNGHOLD_HP_UPDATE_FLAG) {
		dbg_info("HP:abHold = 0x%02x\n", info->abHold[0]);
	}
	if (dPrm & MCDRV_DNGATK_HP_UPDATE_FLAG) {
		dbg_info("HP:abAttack = 0x%02x\n", info->abAttack[0]);
	}
	if (dPrm & MCDRV_DNGREL_HP_UPDATE_FLAG) {
		dbg_info("HP:abRelease = 0x%02x\n", info->abRelease[0]);
	}
	if (dPrm & MCDRV_DNGTARGET_HP_UPDATE_FLAG) {
		dbg_info("HP:abTarget = 0x%02x\n", info->abTarget[0]);
	}
	
	if (dPrm & MCDRV_DNGSW_SP_UPDATE_FLAG) {
		dbg_info("SP:abOnOff = 0x%02x\n", info->abOnOff[1]);
	}
	if (dPrm & MCDRV_DNGTHRES_SP_UPDATE_FLAG) {
		dbg_info("SP:abThreshold = 0x%02x\n", info->abThreshold[1]);
	}
	if (dPrm & MCDRV_DNGHOLD_SP_UPDATE_FLAG) {
		dbg_info("SP:abHold = 0x%02x\n", info->abHold[1]);
	}
	if (dPrm & MCDRV_DNGATK_SP_UPDATE_FLAG) {
		dbg_info("SP:abAttack = 0x%02x\n", info->abAttack[1]);
	}
	if (dPrm & MCDRV_DNGREL_SP_UPDATE_FLAG) {
		dbg_info("SP:abRelease = 0x%02x\n", info->abRelease[1]);
	}
	if (dPrm & MCDRV_DNGTARGET_SP_UPDATE_FLAG) {
		dbg_info("SP:abTarget = 0x%02x\n", info->abTarget[1]);
	}

	if (dPrm & MCDRV_DNGSW_RC_UPDATE_FLAG) {
		dbg_info("RC:abOnOff = 0x%02x\n", info->abOnOff[2]);
	}
	if (dPrm & MCDRV_DNGTHRES_RC_UPDATE_FLAG) {
		dbg_info("RC:abThreshold = 0x%02x\n", info->abThreshold[2]);
	}
	if (dPrm & MCDRV_DNGHOLD_RC_UPDATE_FLAG) {
		dbg_info("RC:abHold = 0x%02x\n", info->abHold[2]);
	}
	if (dPrm & MCDRV_DNGATK_RC_UPDATE_FLAG) {
		dbg_info("RC:abAttack = 0x%02x\n", info->abAttack[2]);
	}
	if (dPrm & MCDRV_DNGREL_RC_UPDATE_FLAG) {
		dbg_info("RC:abRelease = 0x%02x\n", info->abRelease[2]);
	}
	if (dPrm & MCDRV_DNGTARGET_RC_UPDATE_FLAG) {
		dbg_info("RC:abTarget = 0x%02x\n", info->abTarget[2]);
	}

	if (dPrm & MCDRV_DNGFW_FLAG) {
		dbg_info("   bFw = 0x%02x\n", info->bFw);
	}
}

static void mc_asoc_dump_ae_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_AE_INFO *info = (MCDRV_AE_INFO *)pvPrm;

	dbg_info("bOnOff = 0x%02x\n", info->bOnOff);
	if (dPrm & MCDRV_AEUPDATE_FLAG_BEX) {
		mc_asoc_dump_array("abBex", info->abBex, BEX_PARAM_SIZE);
	}
	if (dPrm & MCDRV_AEUPDATE_FLAG_WIDE) {
		mc_asoc_dump_array("abWide", info->abWide, WIDE_PARAM_SIZE);
	}
	if (dPrm & MCDRV_AEUPDATE_FLAG_DRC) {
		mc_asoc_dump_array("abDrc", info->abDrc, DRC_PARAM_SIZE);
		mc_asoc_dump_array("abDrc2", info->abDrc2, DRC2_PARAM_SIZE);
	}
	if (dPrm & MCDRV_AEUPDATE_FLAG_EQ5) {
		mc_asoc_dump_array("abEq5", info->abEq5, EQ5_PARAM_SIZE);
	}
	if (dPrm & MCDRV_AEUPDATE_FLAG_EQ3) {
		mc_asoc_dump_array("abEq3", info->abEq3, EQ3_PARAM_SIZE);
	}
}

static void mc_asoc_dump_ae_ex(const void *pvPrm, UINT32 dPrm)
{
	UINT8 *info = (UINT8 *)pvPrm;
	char str[128], *p;
	int i;

	p = str;
	dbg_info("AE_EX PROGRAM = %ld byte\n", dPrm);
	for (i = 0; i < dPrm && i < 16; i++) {
		p += sprintf(p, "0x%02x ", info[i]);
		if ((i % 16 == 15) || (i == dPrm - 1)) {
			dbg_info("%s\n", str);
			p = str;
		}
	}
	if(dPrm > 16) {
		dbg_info(":\n:\n");
	}
}

static void mc_asoc_dump_cdsp(const void *pvPrm, UINT32 dPrm)
{
	UINT8 *info = (UINT8 *)pvPrm;
	char str[128], *p;
	int i;

	p = str;
	dbg_info("CDSP PROGRAM = %ld byte\n", dPrm);
	for (i = 0; i < dPrm && i < 16; i++) {
		p += sprintf(p, "0x%02x ", info[i]);
		if ((i % 16 == 15) || (i == dPrm - 1)) {
			dbg_info("%s\n", str);
			p = str;
		}
	}
	if(dPrm > 16) {
		dbg_info(":\n:\n");
	}
}

static void mc_asoc_dump_cdspparam(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_CDSPPARAM *info = (MCDRV_CDSPPARAM *)pvPrm;

	dbg_info("bCommand = 0x%02x\n", info->bCommand);

#if 1
	dbg_info("abParam = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
		info->abParam[0],
		info->abParam[1],
		info->abParam[2],
		info->abParam[3],
		info->abParam[4],
		info->abParam[5],
		info->abParam[6],
		info->abParam[7],
		info->abParam[8],
		info->abParam[9],
		info->abParam[10],
		info->abParam[11],
		info->abParam[12],
		info->abParam[13],
		info->abParam[14],
		info->abParam[15]
		);
#else
	int i;
	for (i = 0; i < 16; i++){
		dbg_info("abParam[%d] = 0x%02x\n", i, info->abParam[i]);
	}
#endif
}

static void mc_asoc_dump_pdm_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_PDM_INFO *info = (MCDRV_PDM_INFO *)pvPrm;

	if (dPrm & MCDRV_PDMCLK_UPDATE_FLAG) {
		dbg_info("bClk = 0x%02x\n", info->bClk);
	}
	if (dPrm & MCDRV_PDMADJ_UPDATE_FLAG) {
		dbg_info("bAgcAdjust = 0x%02x\n", info->bAgcAdjust);
	}
	if (dPrm & MCDRV_PDMAGC_UPDATE_FLAG) {
		dbg_info("bAgcOn = 0x%02x\n", info->bAgcOn);
	}
	if (dPrm & MCDRV_PDMEDGE_UPDATE_FLAG) {
		dbg_info("bPdmEdge = 0x%02x\n", info->bPdmEdge);
	}
	if (dPrm & MCDRV_PDMWAIT_UPDATE_FLAG) {
		dbg_info("bPdmWait = 0x%02x\n", info->bPdmWait);
	}
	if (dPrm & MCDRV_PDMSEL_UPDATE_FLAG) {
		dbg_info("bPdmSel = 0x%02x\n", info->bPdmSel);
	}
	if (dPrm & MCDRV_PDMMONO_UPDATE_FLAG) {
		dbg_info("bMono = 0x%02x\n", info->bMono);
	}
}

static void mc_asoc_dump_dtmf_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_DTMF_INFO *info = (MCDRV_DTMF_INFO *)pvPrm;
	
	if (dPrm & MCDRV_DTMFHOST_UPDATE_FLAG) {
		dbg_info("bSinGenHost = 0x%02x\n", info->bSinGenHost);
	}
	if (dPrm & MCDRV_DTMFONOFF_UPDATE_FLAG) {
		dbg_info("bOnOff = 0x%02x\n", info->bOnOff);
	}
	if (dPrm & MCDRV_DTMFFREQ0_UPDATE_FLAG) {
		dbg_info("sParam.wSinGen0Freq = 0x%02x\n", info->sParam.wSinGen0Freq);
	}
	if (dPrm & MCDRV_DTMFFREQ1_UPDATE_FLAG) {
		dbg_info("sParam.wSinGen1Freq = 0x%02x\n", info->sParam.wSinGen1Freq);
	}
	if (dPrm & MCDRV_DTMFVOL0_UPDATE_FLAG) {
		dbg_info("sParam.swSinGen0Vol = 0x%02x\n", info->sParam.swSinGen0Vol);
	}
	if (dPrm & MCDRV_DTMFVOL1_UPDATE_FLAG) {
		dbg_info("sParam.swSinGen1Vol = 0x%02x\n", info->sParam.swSinGen1Vol);
	}
	if (dPrm & MCDRV_DTMFGATE_UPDATE_FLAG) {
		dbg_info("sParam.bSinGenGate = 0x%02x\n", info->sParam.bSinGenGate);
	}
	if (dPrm & MCDRV_DTMFLOOP_UPDATE_FLAG) {
		dbg_info("sParam.bSinGenLoop = 0x%02x\n", info->sParam.bSinGenLoop);
	}
}

static void mc_asoc_dump_clksw_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_CLKSW_INFO *info = (MCDRV_CLKSW_INFO *)pvPrm;

	dbg_info("bClkSrc = 0x%02x\n", info->bClkSrc);
}

static void mc_asoc_dump_syseq_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_SYSEQ_INFO *info = (MCDRV_SYSEQ_INFO *)pvPrm;
	int i;

	if (dPrm & MCDRV_SYSEQ_ONOFF_UPDATE_FLAG) {
		dbg_info("bOnOff = 0x%02x\n", info->bOnOff);
	}
	if (dPrm & MCDRV_SYSEQ_PARAM_UPDATE_FLAG) {
		for (i = 0; i < 15; i++){
			dbg_info("abParam[%d] = 0x%02x\n", i, info->abParam[i]);
		}
	}
}

static void mc_asoc_dump_exparam(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_EXPARAM *info = (MCDRV_EXPARAM *)pvPrm;

	dbg_info("bCommand = 0x%02x\n", info->bCommand);

#if 1
	dbg_info("abParam = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x \n",
		info->abParam[0],
		info->abParam[1],
		info->abParam[2],
		info->abParam[3],
		info->abParam[4],
		info->abParam[5],
		info->abParam[6],
		info->abParam[7],
		info->abParam[8],
		info->abParam[9],
		info->abParam[10],
		info->abParam[11],
		info->abParam[12],
		info->abParam[13],
		info->abParam[14],
		info->abParam[15]
		);
#else
	int i;
	for (i = 0; i < 16; i++){
		dbg_info("abParam[%d] = 0x%02x\n", i, info->abParam[i]);
	}
#endif
}

static void mc_asoc_dump_ditswap_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_DITSWAP_INFO *info = (MCDRV_DITSWAP_INFO *)pvPrm;
	
	if (dPrm & MCDRV_DITSWAP_L_UPDATE_FLAG) {
		dbg_info("bSwapL = 0x%02x\n", info->bSwapL);
	}
	if (dPrm & MCDRV_DITSWAP_R_UPDATE_FLAG) {
		dbg_info("bSwapR = 0x%02x\n", info->bSwapR);
	}
}

static void mc_asoc_dump_agc_ex_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_ADC_EX_INFO *info = (MCDRV_ADC_EX_INFO *)pvPrm;

	if (dPrm & MCDRV_AGCENV_UPDATE_FLAG) {
		dbg_info("bAgcEnv = 0x%02x\n", info->bAgcEnv);
	}
	if (dPrm & MCDRV_AGCLVL_UPDATE_FLAG) {
		dbg_info("bAgcLvl = 0x%02x\n", info->bAgcLvl);
	}
	if (dPrm & MCDRV_AGCUPTIM_UPDATE_FLAG) {
		dbg_info("bAgcUpTim = 0x%02x\n", info->bAgcUpTim);
	}
	if (dPrm & MCDRV_AGCDWTIM_UPDATE_FLAG) {
		dbg_info("bAgcDwTim = 0x%02x\n", info->bAgcDwTim);
	}
	if (dPrm & MCDRV_AGCCUTLVL_UPDATE_FLAG) {
		dbg_info("bAgcCutLvl = 0x%02x\n", info->bAgcCutLvl);
	}
	if (dPrm & MCDRV_AGCCUTTIM_UPDATE_FLAG) {
		dbg_info("bAgcCutTim = 0x%02x\n", info->bAgcCutTim);
	}
}

static void mc_asoc_dump_pll_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_PLL_INFO *info = (MCDRV_PLL_INFO *)pvPrm;

	dbg_info("bMode0 = 0x%02x\n", info->bMode0);
	dbg_info("bPrevDiv0 = 0x%02x\n", info->bPrevDiv0);
	dbg_info("wFbDiv0 = 0x%04x\n", info->wFbDiv0);
	dbg_info("wFrac0 = 0x%04x\n", info->wFrac0);
	dbg_info("bMode1 = 0x%02x\n", info->bMode1);
	dbg_info("bPrevDiv1 = 0x%02x\n", info->bPrevDiv1);
	dbg_info("wFbDiv1 = 0x%04x\n", info->wFbDiv1);
	dbg_info("wFrac1 = 0x%04x\n", info->wFrac1);
}

static void mc_asoc_dump_hsdet_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_HSDET_INFO *info = (MCDRV_HSDET_INFO *)pvPrm;

	if (dPrm & MCDRV_ENPLUGDET_UPDATE_FLAG) {
		dbg_info("bEnPlugDet = 0x%02x\n", info->bEnPlugDet);
	}
	if (dPrm & MCDRV_ENPLUGDETDB_UPDATE_FLAG) {
		dbg_info("bEnPlugDetDb = 0x%02x\n", info->bEnPlugDetDb);
	}
	if (dPrm & MCDRV_ENDLYKEYOFF_UPDATE_FLAG) {
		dbg_info("bEnDlyKeyOff = 0x%02x\n", info->bEnDlyKeyOff);
	}
	if (dPrm & MCDRV_ENDLYKEYON_UPDATE_FLAG) {
		dbg_info("bEnDlyKeyOn = 0x%02x\n", info->bEnDlyKeyOn);
	}
	if (dPrm & MCDRV_ENMICDET_UPDATE_FLAG) {
		dbg_info("bEnMicDet = 0x%02x\n", info->bEnMicDet);
	}
	if (dPrm & MCDRV_ENKEYOFF_UPDATE_FLAG) {
		dbg_info("bEnKeyOff = 0x%02x\n", info->bEnKeyOff);
	}
	if (dPrm & MCDRV_ENKEYON_UPDATE_FLAG) {
		dbg_info("bEnKeyOn = 0x%02x\n", info->bEnKeyOn);
	}
	if (dPrm & MCDRV_HSDETDBNC_UPDATE_FLAG) {
		dbg_info("bHsDetDbnc = 0x%02x\n", info->bHsDetDbnc);
	}
	if (dPrm & MCDRV_KEYOFFMTIM_UPDATE_FLAG) {
		dbg_info("bKeyOffMtim = 0x%02x\n", info->bKeyOffMtim);
	}
	if (dPrm & MCDRV_KEYONMTIM_UPDATE_FLAG) {
		dbg_info("bKeyOnMtim = 0x%02x\n", info->bKeyOnMtim);
	}
	if (dPrm & MCDRV_KEY0OFFDLYTIM_UPDATE_FLAG) {
		dbg_info("bKey0OffDlyTim = 0x%02x\n", info->bKey0OffDlyTim);
	}
	if (dPrm & MCDRV_KEY1OFFDLYTIM_UPDATE_FLAG) {
		dbg_info("bKey1OffDlyTim = 0x%02x\n", info->bKey1OffDlyTim);
	}
	if (dPrm & MCDRV_KEY2OFFDLYTIM_UPDATE_FLAG) {
		dbg_info("bKey2OffDlyTim = 0x%02x\n", info->bKey2OffDlyTim);
	}
	if (dPrm & MCDRV_KEY0ONDLYTIM_UPDATE_FLAG) {
		dbg_info("bKey0OnDlyTim = 0x%02x\n", info->bKey0OnDlyTim);
	}
	if (dPrm & MCDRV_KEY1ONDLYTIM_UPDATE_FLAG) {
		dbg_info("bKey1OnDlyTim = 0x%02x\n", info->bKey1OnDlyTim);
	}
	if (dPrm & MCDRV_KEY2ONDLYTIM_UPDATE_FLAG) {
		dbg_info("bKey2OnDlyTim = 0x%02x\n", info->bKey2OnDlyTim);
	}
	if (dPrm & MCDRV_KEY0ONDLYTIM2_UPDATE_FLAG) {
		dbg_info("bKey0OnDlyTim2 = 0x%02x\n", info->bKey0OnDlyTim2);
	}
	if (dPrm & MCDRV_KEY1ONDLYTIM2_UPDATE_FLAG) {
		dbg_info("bKey1OnDlyTim2 = 0x%02x\n", info->bKey1OnDlyTim2);
	}
	if (dPrm & MCDRV_KEY2ONDLYTIM2_UPDATE_FLAG) {
		dbg_info("bKey2OnDlyTim2 = 0x%02x\n", info->bKey2OnDlyTim2);
	}
	if (dPrm & MCDRV_IRQTYPE_UPDATE_FLAG) {
		dbg_info("bIrqType = 0x%02x\n", info->bIrqType);
	}
	if (dPrm & MCDRV_DETINV_UPDATE_FLAG) {
		dbg_info("bDetInInv = 0x%02x\n", info->bDetInInv);
	}
	if (dPrm & MCDRV_HSDETMODE_UPDATE_FLAG) {
		dbg_info("bHsDetMode = 0x%02x\n", info->bHsDetMode);
	}
	if (dPrm & MCDRV_SPERIOD_UPDATE_FLAG) {
		dbg_info("bSperiod = 0x%02x\n", info->bSperiod);
	}
	if (dPrm & MCDRV_LPERIOD_UPDATE_FLAG) {
		dbg_info("bLperiod = 0x%02x\n", info->bLperiod);
	}
	if (dPrm & MCDRV_DBNCNUMPLUG_UPDATE_FLAG) {
		dbg_info("bDbncNumPlug = 0x%02x\n", info->bDbncNumPlug);
	}
	if (dPrm & MCDRV_DBNCNUMMIC_UPDATE_FLAG) {
		dbg_info("bDbncNumMic = 0x%02x\n", info->bDbncNumMic);
	}
	if (dPrm & MCDRV_DBNCNUMKEY_UPDATE_FLAG) {
		dbg_info("bDbncNumKey = 0x%02x\n", info->bDbncNumKey);
	}
	if (dPrm & MCDRV_DLYIRQSTOP_UPDATE_FLAG) {
		dbg_info("bDlyIrqStop = 0x%02x\n", info->bDlyIrqStop);
	}
	if (dPrm & MCDRV_CBFUNC_UPDATE_FLAG) {
		dbg_info("cbfunc = %8p\n", info->cbfunc);
	}
}

struct mc_asoc_dump_func {
	char *name;
	void (*func)(const void *, UINT32);
};

struct mc_asoc_dump_func mc_asoc_dump_func_map[] = {
	{"MCDRV_INIT", mc_asoc_dump_init_info},
	{"MCDRV_TERM", NULL},
	{"MCDRV_READ_REG", mc_asoc_dump_reg_info},
	{"MCDRV_WRITE_REG", mc_asoc_dump_reg_info},
	{"MCDRV_GET_PATH", NULL},
	{"MCDRV_SET_PATH", mc_asoc_dump_path_info},
	{"MCDRV_GET_VOLUME", NULL},
	{"MCDRV_SET_VOLUME", mc_asoc_dump_vol_info},
	{"MCDRV_GET_DIGITALIO", NULL},
	{"MCDRV_SET_DIGITALIO", mc_asoc_dump_dio_info},
	{"MCDRV_GET_DAC", NULL},
	{"MCDRV_SET_DAC", mc_asoc_dump_dac_info},
	{"MCDRV_GET_ADC", NULL},
	{"MCDRV_SET_ADC", mc_asoc_dump_adc_info},
	{"MCDRV_GET_SP", NULL},
	{"MCDRV_SET_SP", mc_asoc_dump_sp_info},
	{"MCDRV_GET_DNG", NULL},
	{"MCDRV_SET_DNG", mc_asoc_dump_dng_info},
	{"MCDRV_SET_AUDIOENGINE", mc_asoc_dump_ae_info},
	{"MCDRV_SET_AUDIOENGINE_EX", mc_asoc_dump_ae_ex},
	{"MCDRV_SET_CDSP", mc_asoc_dump_cdsp},
	{"MCDRV_GET_CDSP_PARAM", NULL},
	{"MCDRV_SET_CDSP_PARAM", mc_asoc_dump_cdspparam},
	{"MCDRV_REGISTER_CDSP_CB", NULL},
	{"MCDRV_GET_PDM", NULL},
	{"MCDRV_SET_PDM", mc_asoc_dump_pdm_info},
	{"MCDRV_SET_DTMF", mc_asoc_dump_dtmf_info},
	{"MCDRV_CONFIG_GP", NULL},
	{"MCDRV_MASK_GP", NULL},
	{"MCDRV_GETSET_GP", NULL},
	{"MCDRV_GET_PEAK", NULL},
	{"MCDRV_IRQ", NULL},
	{"MCDRV_UPDATE_CLOCK", NULL},
	{"MCDRV_SWITCH_CLOCK", mc_asoc_dump_clksw_info},
	{"MCDRV_GET_SYSEQ", NULL},
	{"MCDRV_SET_SYSEQ", mc_asoc_dump_syseq_info},
	{"MCDRV_GET_DTMF", NULL},
	{"MCDRV_SET_EX_PARAM", mc_asoc_dump_exparam},
	{"MCDRV_GET_DITSWAP", NULL},
	{"MCDRV_SET_DITSWAP", mc_asoc_dump_ditswap_info},
	{"MCDRV_GET_ADC_EX", NULL},
	{"MCDRV_SET_ADC_EX", mc_asoc_dump_agc_ex_info},
	{"MCDRV_GET_PDM_EX", NULL},
	{"MCDRV_SET_PDM_EX", mc_asoc_dump_agc_ex_info},
	{"MCDRV_GET_PLL", NULL},
	{"MCDRV_SET_PLL", mc_asoc_dump_pll_info},
	{"MCDRV_GET_HSDET", NULL},
	{"MCDRV_SET_HSDET", mc_asoc_dump_hsdet_info},
};

SINT32 McDrv_Ctrl_dbg(UINT32 dCmd, void *pvPrm, UINT32 dPrm)
{
	SINT32 err;

	dbg_info("calling %s:\n", mc_asoc_dump_func_map[dCmd].name);
    printk("[%s][%d]dCmd = %#x\n",__func__, __LINE__, dCmd);
	if (mc_asoc_dump_func_map[dCmd].func) {
		mc_asoc_dump_func_map[dCmd].func(pvPrm, dPrm);
	}

	err = McDrv_Ctrl(dCmd, pvPrm, dPrm);
	dbg_info("err = %d\n", (int)err);

	if(dCmd == MCDRV_SET_VOLUME) {
		McDrv_Ctrl(MCDRV_GET_VOLUME, pvPrm, 0);
		mc_asoc_dump_vol_info(pvPrm, 0xFF);
	}
	if(dCmd == MCDRV_GET_PATH) {
		mc_asoc_dump_path_info(pvPrm, 0xFF);
	}
	if(dCmd == MCDRV_SET_PATH) {
		McDrv_Ctrl(MCDRV_GET_PATH, pvPrm, 0);
		mc_asoc_dump_path_info(pvPrm, 0x55);
	}

	return err;
}

#endif /* CONFIG_SND_SOC_MC_YAMAHA_DEBUG */
