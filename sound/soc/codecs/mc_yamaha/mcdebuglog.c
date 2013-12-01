/****************************************************************************
 *
 *		Copyright(c) 2011 Yamaha Corporation. All rights reserved.
 *
 *		Module		: mcdebuglog.c
 *
 *		Description	: MC Driver debug log
 *
 *		Version		: 3.0.0 	2011.11.14
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


#include "mcdebuglog.h"
#include "mcresctrl.h"

#if MCDRV_DEBUG_LEVEL

#include "mcdefs.h"
#include "mcdevprof.h"
#include "mcservice.h"
#include "mcmachdep.h"



typedef char	CHAR;

static CHAR	gsbLogString[8192];
static CHAR	gsbLFCode[]	= "\n";

static const CHAR	gsbCmdName[][20]	=
{
	"init",
	"term",
	"read_reg",
	"write_reg",
	"get_path",
	"set_path",
	"get_volume",
	"set_volume",
	"get_digitalio",
	"set_digitalio",
	"get_dac",
	"set_dac",
	"get_adc",
	"set_adc",
	"get_sp",
	"set_sp",
	"get_dng",
	"set_dng",
	"set_ae",
	"set_aeex",
	"set_cdsp",
	"",
	"set_cdspparam",
	"",
	"get_pdm",
	"set_pdm",
	"set_dtmf",
	"config_gp",
	"mask_gp",
	"getset_gp",
	"get_peak",
	"irq",
	"update_clock",
	"switch_clock",
	"get_syseq",
	"set_syseq",
	"get_dtmf",
	"set_ex_param",
	"get_ditswap",
	"set_ditswap",
	"get_adc_ex",
	"set_adc_ex",
	"get_pdm_ex",
	"set_pdm_ex",
	"get_pll",
	"set_pll"
};


static void	OutputRegDump(void);
static void	GetRegDump_B(CHAR* psbLogString, UINT8 bSlaveAddr, UINT8 bADRAddr, UINT8 bWINDOWAddr, UINT8 bRegType);

static void	MakeInitInfoLog(const MCDRV_INIT_INFO* pParam);
static void	MakeRegInfoLog(const MCDRV_REG_INFO* pParam);
static void	MakePLLInfoLog(const MCDRV_PLL_INFO* pParam);
static void	MakeClockInfoLog(const MCDRV_CLOCK_INFO* pParam);
static void	MakeClockSwInfoLog(const MCDRV_CLKSW_INFO* pParam);
static void	MakePathInfoLog(const MCDRV_PATH_INFO* pParam);
static void	MakeVolInfoLog(const MCDRV_VOL_INFO* pParam);
static void	MakeDIOInfoLog(const MCDRV_DIO_INFO* pParam);
static void	MakeDACInfoLog(const MCDRV_DAC_INFO* pParam);
static void	MakeADCInfoLog(const MCDRV_ADC_INFO* pParam);
static void	MakeADCExInfoLog(const MCDRV_ADC_EX_INFO* pParam);
static void	MakeSpInfoLog(const MCDRV_SP_INFO* pParam);
static void	MakeDNGInfoLog(const MCDRV_DNG_INFO* pParam);
static void	MakeAEInfoLog(const MCDRV_AE_INFO* pParam);
static void	MakeSetCDSPLog(const UINT8* pParam);
static void	MakeCDSPParamLog(const MCDRV_CDSPPARAM* pParam);
static void	MakeCDSPCBLog(const void* pParam);
static void	MakePDMInfoLog(const MCDRV_PDM_INFO* pParam);
static void	MakePDMExInfoLog(const MCDRV_PDM_EX_INFO* pParam);
static void	MakeDTMFInfoLog(const MCDRV_DTMF_INFO* pParam);
static void	MakeGPModeLog(const MCDRV_GP_MODE* pParam);
static void	MakeGPMaskLog(const UINT8* pParam);
static void	MakeGetSetGPLog(const UINT8* pParam);
static void	MakePeakLog(const MCDRV_PEAK* pParam);
static void	MakeSysEQInfoLog(const MCDRV_SYSEQ_INFO* pParam);
static void	MakeDITSwapInfoLog(const MCDRV_DITSWAP_INFO* pParam);

/****************************************************************************
 *	McDebugLog_CmdIn
 *
 *	Description:
 *			Output Function entrance log.
 *	Arguments:
 *			dCmd		Command ID
 *			pvParam		pointer to parameter
 *			dUpdateInfo	Update info
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McDebugLog_CmdIn
(
	UINT32		dCmd,
	const void*	pvParam,
	UINT32		dUpdateInfo
)
{
	CHAR	sbStr[80];
	UINT8	bLevel	= MCDRV_DEBUG_LEVEL;

	if(dCmd >= sizeof(gsbCmdName)/sizeof(gsbCmdName[0]))
	{
		return;
	}

	strcpy(gsbLogString, gsbCmdName[dCmd]);
	strcat(gsbLogString, " In");

	if(bLevel < 2)
	{
		strcat(gsbLogString, gsbLFCode);
		machdep_DebugPrint((UINT8*)(void*)gsbLogString);
		return;
	}

	switch(dCmd)
	{
	case	MCDRV_INIT:
		MakeInitInfoLog((MCDRV_INIT_INFO*)pvParam);
		break;
	case	MCDRV_READ_REG:
	case	MCDRV_WRITE_REG:
		MakeRegInfoLog((MCDRV_REG_INFO*)pvParam);
		break;
	case	MCDRV_SET_PLL:
		MakePLLInfoLog((MCDRV_PLL_INFO*)pvParam);
		break;
	case	MCDRV_UPDATE_CLOCK:
		MakeClockInfoLog((MCDRV_CLOCK_INFO*)pvParam);
		break;
	case	MCDRV_SWITCH_CLOCK:
		MakeClockSwInfoLog((MCDRV_CLKSW_INFO*)pvParam);
		break;
	case	MCDRV_SET_PATH:
		MakePathInfoLog((MCDRV_PATH_INFO*)pvParam);
		break;
	case	MCDRV_SET_VOLUME:
		MakeVolInfoLog((MCDRV_VOL_INFO*)pvParam);
		break;
	case	MCDRV_SET_DIGITALIO:
		MakeDIOInfoLog((MCDRV_DIO_INFO*)pvParam);
		sprintf(sbStr, " dPrm=%08lX", dUpdateInfo);
		strcat(gsbLogString, sbStr);
		break;
	case	MCDRV_SET_DAC:
		MakeDACInfoLog((MCDRV_DAC_INFO*)pvParam);
		sprintf(sbStr, " dPrm=%08lX", dUpdateInfo);
		strcat(gsbLogString, sbStr);
		break;
	case	MCDRV_SET_ADC:
		MakeADCInfoLog((MCDRV_ADC_INFO*)pvParam);
		sprintf(sbStr, " dPrm=%08lX", dUpdateInfo);
		strcat(gsbLogString, sbStr);
		break;
	case	MCDRV_SET_ADC_EX:
		MakeADCExInfoLog((MCDRV_ADC_EX_INFO*)pvParam);
		sprintf(sbStr, " dPrm=%08lX", dUpdateInfo);
		strcat(gsbLogString, sbStr);
		break;
	case	MCDRV_SET_SP:
		MakeSpInfoLog((MCDRV_SP_INFO*)pvParam);
		break;
	case	MCDRV_SET_DNG:
		MakeDNGInfoLog((MCDRV_DNG_INFO*)pvParam);
		sprintf(sbStr, " dPrm=%08lX", dUpdateInfo);
		strcat(gsbLogString, sbStr);
		break;
	case	MCDRV_SET_AUDIOENGINE:
		MakeAEInfoLog((MCDRV_AE_INFO*)pvParam);
		sprintf(sbStr, " dPrm=%08lX", dUpdateInfo);
		strcat(gsbLogString, sbStr);
		break;
	case	MCDRV_SET_AUDIOENGINE_EX:
		MakeSetCDSPLog(pvParam);
		sprintf(sbStr, " dPrm=%08lX", dUpdateInfo);
		strcat(gsbLogString, sbStr);
		break;
	case	MCDRV_SET_EX_PARAM:
		MakeCDSPParamLog((MCDRV_CDSPPARAM*)pvParam);
		break;
	case	MCDRV_SET_CDSP:
		MakeSetCDSPLog(pvParam);
		sprintf(sbStr, " dPrm=%08lX", dUpdateInfo);
		strcat(gsbLogString, sbStr);
		break;
	case	MCDRV_SET_CDSP_PARAM:
		MakeCDSPParamLog((MCDRV_CDSPPARAM*)pvParam);
		break;
	case	MCDRV_SET_PDM:
		MakePDMInfoLog((MCDRV_PDM_INFO*)pvParam);
		sprintf(sbStr, " dPrm=%08lX", dUpdateInfo);
		strcat(gsbLogString, sbStr);
		break;
	case	MCDRV_SET_PDM_EX:
		MakePDMExInfoLog((MCDRV_PDM_EX_INFO*)pvParam);
		sprintf(sbStr, " dPrm=%08lX", dUpdateInfo);
		strcat(gsbLogString, sbStr);
		break;
	case	MCDRV_SET_DTMF:
		MakeDTMFInfoLog((MCDRV_DTMF_INFO*)pvParam);
		sprintf(sbStr, " dPrm=%08lX", dUpdateInfo);
		strcat(gsbLogString, sbStr);
		break;
	case	MCDRV_CONFIG_GP:
		MakeGPModeLog((MCDRV_GP_MODE*)pvParam);
		break;
	case	MCDRV_MASK_GP:
		MakeGPMaskLog((UINT8*)pvParam);
		sprintf(sbStr, " dPrm=%08lX", dUpdateInfo);
		strcat(gsbLogString, sbStr);
		break;
	case	MCDRV_GETSET_GP:
		MakeGetSetGPLog((UINT8*)pvParam);
		sprintf(sbStr, " dPrm=%08lX", dUpdateInfo);
		strcat(gsbLogString, sbStr);
		break;
	case	MCDRV_SET_SYSEQ:
		MakeSysEQInfoLog((MCDRV_SYSEQ_INFO*)pvParam);
		sprintf(sbStr, " dPrm=%08lX", dUpdateInfo);
		strcat(gsbLogString, sbStr);
		break;
	case	MCDRV_SET_DITSWAP:
		MakeDITSwapInfoLog((MCDRV_DITSWAP_INFO*)pvParam);
		sprintf(sbStr, " dPrm=%08lX", dUpdateInfo);
		strcat(gsbLogString, sbStr);
		break;

	default:
		break;
	}

	strcat(gsbLogString, gsbLFCode);
	machdep_DebugPrint((UINT8*)(void*)gsbLogString);
}

/****************************************************************************
 *	McDebugLog_CmdOut
 *
 *	Description:
 *			Output Function exit log.
 *	Arguments:
 *			dCmd		Command ID
 *			psdRet		retrun value
 *			pvParam		pointer to parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McDebugLog_CmdOut
(
	UINT32			dCmd,
	const SINT32*	psdRet,
	const void*		pvParam
)
{
	CHAR	sbStr[80];
	UINT8	bLevel	= MCDRV_DEBUG_LEVEL;

	if(dCmd >= sizeof(gsbCmdName)/sizeof(gsbCmdName[0]))
	{
		return;
	}

	strcpy(gsbLogString, gsbCmdName[dCmd]);
	strcat(gsbLogString, " Out");
	if(psdRet != NULL)
	{
		sprintf(sbStr, " ret=%ld", *psdRet);
		strcat(gsbLogString, sbStr);
	}

	if(bLevel < 2)
	{
		strcat(gsbLogString, gsbLFCode);
		machdep_DebugPrint((UINT8*)(void*)gsbLogString);
		return;
	}

	switch(dCmd)
	{
	case	MCDRV_READ_REG:
		MakeRegInfoLog((MCDRV_REG_INFO*)pvParam);
		break;
	case	MCDRV_GET_PLL:
		MakePLLInfoLog((MCDRV_PLL_INFO*)pvParam);
		break;
	case	MCDRV_GET_PATH:
		MakePathInfoLog((MCDRV_PATH_INFO*)pvParam);
		break;
	case	MCDRV_GET_VOLUME:
		MakeVolInfoLog((MCDRV_VOL_INFO*)pvParam);
		break;
	case	MCDRV_GET_DIGITALIO:
		MakeDIOInfoLog((MCDRV_DIO_INFO*)pvParam);
		break;
	case	MCDRV_GET_DAC:
		MakeDACInfoLog((MCDRV_DAC_INFO*)pvParam);
		break;
	case	MCDRV_GET_ADC:
		MakeADCInfoLog((MCDRV_ADC_INFO*)pvParam);
		break;
	case	MCDRV_GET_ADC_EX:
		MakeADCExInfoLog((MCDRV_ADC_EX_INFO*)pvParam);
		break;
	case	MCDRV_GET_SP:
		MakeSpInfoLog((MCDRV_SP_INFO*)pvParam);
		break;
	case	MCDRV_GET_DNG:
		MakeDNGInfoLog((MCDRV_DNG_INFO*)pvParam);
		break;
	case	MCDRV_GET_PDM:
		MakePDMInfoLog((MCDRV_PDM_INFO*)pvParam);
		break;
	case	MCDRV_GET_PDM_EX:
		MakePDMExInfoLog((MCDRV_PDM_EX_INFO*)pvParam);
		break;
	case	MCDRV_GET_DTMF:
		MakeDTMFInfoLog((MCDRV_DTMF_INFO*)pvParam);
		break;
	case	MCDRV_GETSET_GP:
		MakeGetSetGPLog((UINT8*)pvParam);
		break;
	case	MCDRV_GET_PEAK:
		MakePeakLog((MCDRV_PEAK*)pvParam);
		break;
	case	MCDRV_GET_SYSEQ:
		MakeSysEQInfoLog((MCDRV_SYSEQ_INFO*)pvParam);
		break;
	case	MCDRV_GET_DITSWAP:
		MakeDITSwapInfoLog((MCDRV_DITSWAP_INFO*)pvParam);
		break;

	default:
		break;
	}
	strcat(gsbLogString, gsbLFCode);
	machdep_DebugPrint((UINT8*)(void*)gsbLogString);

	if(bLevel < 3)
	{
		return;
	}

	OutputRegDump();
}

/****************************************************************************
 *	McDebugLog_FuncIn
 *
 *	Description:
 *			Output Function entrance log.
 *	Arguments:
 *			pbFuncName	function name
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McDebugLog_FuncIn
(
	void*	pvFuncName
)
{
	strcpy(gsbLogString, (CHAR*)pvFuncName);
	strcat(gsbLogString, " In");

	strcat(gsbLogString, gsbLFCode);
	machdep_DebugPrint((UINT8*)(void*)gsbLogString);
}

/****************************************************************************
 *	McDebugLog_FuncOut
 *
 *	Description:
 *			Output Function exit log.
 *	Arguments:
 *			pbFuncName	function name
 *			psdRet		retrun value
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McDebugLog_FuncOut
(
	void*			pvFuncName,
	const SINT32*	psdRet
)
{
	CHAR	sbStr[80];

	strcpy(gsbLogString, (CHAR*)pvFuncName);
	strcat(gsbLogString, " Out");
	if(psdRet != NULL)
	{
		sprintf(sbStr, " ret=%ld", *psdRet);
		strcat(gsbLogString, sbStr);
	}

	strcat(gsbLogString, gsbLFCode);
	machdep_DebugPrint((UINT8*)(void*)gsbLogString);
}


/****************************************************************************
 *	OutputRegDump
 *
 *	Description:
 *			Output Register dump.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	OutputRegDump
(
	void
)
{
	UINT16	i;
	CHAR	sbStr[10];
	CHAR	sbLogString[2500]="";
	UINT8	bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG);
	MCDRV_REG_INFO	sRegInfo;

	/*	A_ADR	*/
	sRegInfo.bRegType	= MCDRV_REGTYPE_A;
	strcpy(sbLogString, "A_ADR:");
	for(i = 0; i < 256UL; i++)
	{
		sRegInfo.bAddress	= (UINT8)i;
		if((McResCtrl_GetRegAccess(&sRegInfo) & eMCDRV_CAN_READ) != 0)
		{
			sprintf(sbStr, "[%d]=%02X", i, McSrv_ReadI2C(bSlaveAddr, i));
			strcat(sbLogString, sbStr);
			if(i < 255UL)
			{
				strcat(sbLogString, " ");
			}
		}
	}
	strcat(sbLogString, gsbLFCode);
	machdep_DebugPrint((UINT8*)(void*)sbLogString);

	/*	BASE_ADR	*/
	strcpy(sbLogString, "BASE_ADR:");
	GetRegDump_B(sbLogString, bSlaveAddr, MCI_BASE_ADR, MCI_BASE_WINDOW, MCDRV_REGTYPE_B_BASE);
	strcat(sbLogString, gsbLFCode);
	machdep_DebugPrint((UINT8*)(void*)sbLogString);

	/*	ANA_ADR	*/
	strcpy(sbLogString, "ANA_ADR:");
	GetRegDump_B(sbLogString, McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_ANA), MCI_ANA_ADR, MCI_ANA_WINDOW, MCDRV_REGTYPE_B_ANALOG);
	strcat(sbLogString, gsbLFCode);
	machdep_DebugPrint((UINT8*)(void*)sbLogString);

	/*	CD_ADR	*/
	strcpy(sbLogString, "CD_ADR:");
	GetRegDump_B(sbLogString, McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_ANA), MCI_CD_ADR, MCI_CD_WINDOW, MCDRV_REGTYPE_B_CODEC);
	strcat(sbLogString, gsbLFCode);
	machdep_DebugPrint((UINT8*)(void*)sbLogString);

	/*	MIX_ADR	*/
	strcpy(sbLogString, "MIX_ADR:");
	GetRegDump_B(sbLogString, bSlaveAddr, MCI_MIX_ADR, MCI_MIX_WINDOW, MCDRV_REGTYPE_B_MIXER);
	strcat(sbLogString, gsbLFCode);
	machdep_DebugPrint((UINT8*)(void*)sbLogString);

	/*	CDSP_ADR	*/
	if(McDevProf_IsValid(eMCDRV_FUNC_CDSP) != 0)
	{
		strcpy(sbLogString, "CDSP_ADR:");
		GetRegDump_B(sbLogString, bSlaveAddr, MCI_CDSP_ADR, MCI_CDSP_WINDOW, MCDRV_REGTYPE_B_CDSP);
		strcat(sbLogString, gsbLFCode);
		machdep_DebugPrint((UINT8*)(void*)sbLogString);
	}

#if 0
	/*	AE_ADR	*/
	strcpy(sbLogString, "AE_ADR:");
	GetRegDump_B(sbLogString, bSlaveAddr, MCI_AE_ADR, MCI_AE_WINDOW, MCDRV_REGTYPE_B_AE);
	strcat(sbLogString, gsbLFCode);
	machdep_DebugPrint((UINT8*)(void*)sbLogString);
#endif
}

/****************************************************************************
 *	GetRegDump_B
 *
 *	Description:
 *			Get B-ADR Register dump string.
 *	Arguments:
 *			psbLogString	string buffer
 *			bSlaveAddr		Slave address
 *			bADRAddr		ADR address
 *			bWINDOWAddr		WINDOW address
 *			bRegType		register type
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	GetRegDump_B
(
	CHAR*	psbLogString,
	UINT8	bSlaveAddr,
	UINT8	bADRAddr,
	UINT8	bWINDOWAddr,
	UINT8	bRegType
)
{
	UINT16	i;
	CHAR	sbStr[10];
	UINT8	abData[2];
	MCDRV_REG_INFO	sRegInfo;

	abData[0]	= bADRAddr<<1;
	sRegInfo.bRegType	= bRegType;

	for(i = 0; i < 256UL; i++)
	{
		sRegInfo.bAddress	= (UINT8)i;
		if((McResCtrl_GetRegAccess(&sRegInfo) & eMCDRV_CAN_READ) != 0)
		{
			abData[1]	= (UINT8)i;
			McSrv_WriteI2C(bSlaveAddr, abData, 2);
			sprintf(sbStr, "[%d]=%02X", i, McSrv_ReadI2C(bSlaveAddr, bWINDOWAddr));
			strcat(psbLogString, sbStr);
			if(i < 255UL)
			{
				strcat(psbLogString, " ");
			}
		}
	}
}

/****************************************************************************
 *	MakeInitInfoLog
 *
 *	Description:
 *			Make Init Parameter log.
 *	Arguments:
 *			pParam	pointer to parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	MakeInitInfoLog
(
	const MCDRV_INIT_INFO*	pParam
)
{
	CHAR	sbStr[80];

	if(pParam == NULL)
	{
		strcat(gsbLogString, " param=NULL");
		return;
	}

	sprintf(sbStr, " bCkSel=%02X", pParam->bCkSel);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bDivR0=%02X", pParam->bDivR0);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bDivF0=%02X", pParam->bDivF0);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bDivR1=%02X", pParam->bDivR1);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bDivF1=%02X", pParam->bDivF1);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bRange0=%02X", pParam->bRange0);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bRange1=%02X", pParam->bRange1);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bBypass=%02X", pParam->bBypass);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bDioSdo0Hiz=%02X", pParam->bDioSdo0Hiz);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bDioSdo1Hiz=%02X", pParam->bDioSdo1Hiz);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bDioSdo2Hiz=%02X", pParam->bDioSdo2Hiz);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bDioClk0Hiz=%02X", pParam->bDioClk0Hiz);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bDioClk1Hiz=%02X", pParam->bDioClk1Hiz);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bDioClk2Hiz=%02X", pParam->bDioClk2Hiz);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bPcmHiz=%02X", pParam->bPcmHiz);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bLineIn1Dif=%02X", pParam->bLineIn1Dif);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bLineIn2Dif=%02X", pParam->bLineIn2Dif);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bLineOut1Dif=%02X", pParam->bLineOut1Dif);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bLineOut2Dif=%02X", pParam->bLineOut2Dif);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bSpmn=%02X", pParam->bSpmn);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bMic1Sng=%02X", pParam->bMic1Sng);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bMic2Sng=%02X", pParam->bMic2Sng);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bMic3Sng=%02X", pParam->bMic3Sng);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bPowerMode=%02X", pParam->bPowerMode);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bSpHiz=%02X", pParam->bSpHiz);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bLdo=%02X", pParam->bLdo);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bPad0Func=%02X", pParam->bPad0Func);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bPad1Func=%02X", pParam->bPad1Func);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bPad2Func=%02X", pParam->bPad2Func);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bAvddLev=%02X", pParam->bAvddLev);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bVrefLev=%02X", pParam->bVrefLev);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bDclGain=%02X", pParam->bDclGain);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bDclLimit=%02X", pParam->bDclLimit);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bCpMod=%02X", pParam->bCpMod);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bSdDs=%02X", pParam->bSdDs);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bHpIdle=%02X", pParam->bHpIdle);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bCLKI1=%02X", pParam->bCLKI1);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bMbsDisch=%02X", pParam->bMbsDisch);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bReserved=%02X", pParam->bReserved);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " sWaitTime.dAdHpf=%08lX", pParam->sWaitTime.dAdHpf);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " sWaitTime.dMic1Cin=%08lX", pParam->sWaitTime.dMic1Cin);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " sWaitTime.dMic2Cin=%08lX", pParam->sWaitTime.dMic2Cin);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " sWaitTime.dMic3Cin=%08lX", pParam->sWaitTime.dMic3Cin);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " sWaitTime.dLine1Cin=%08lX", pParam->sWaitTime.dLine1Cin);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " sWaitTime.dLine2Cin=%08lX", pParam->sWaitTime.dLine2Cin);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " sWaitTime.dVrefRdy1=%08lX", pParam->sWaitTime.dVrefRdy1);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " sWaitTime.dVrefRdy2=%08lX", pParam->sWaitTime.dVrefRdy2);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " sWaitTime.dHpRdy=%08lX", pParam->sWaitTime.dHpRdy);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " sWaitTime.dSpRdy=%08lX", pParam->sWaitTime.dSpRdy);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " sWaitTime.dPdm=%08lX", pParam->sWaitTime.dPdm);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " sWaitTime.dAnaRdyInterval=%08lX", pParam->sWaitTime.dAnaRdyInterval);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " sWaitTime.dSvolInterval=%08lX", pParam->sWaitTime.dSvolInterval);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " sWaitTime.dAnaRdyTimeOut=%08lX", pParam->sWaitTime.dAnaRdyTimeOut);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " sWaitTime.dSvolTimeOut=%08lX", pParam->sWaitTime.dSvolTimeOut);
	strcat(gsbLogString, sbStr);
}

/****************************************************************************
 *	MakeRegInfoLog
 *
 *	Description:
 *			Make Reg Info Parameter log.
 *	Arguments:
 *			pParam	pointer to parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	MakeRegInfoLog
(
	const MCDRV_REG_INFO* pParam
)
{
	CHAR	sbStr[80];

	if(pParam == NULL)
	{
		strcat(gsbLogString, " param=NULL");
		return;
	}

	sprintf(sbStr, " bRegType=%02X", pParam->bRegType);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bAddress=%02X", pParam->bAddress);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bData=%02X", pParam->bData);
	strcat(gsbLogString, sbStr);
}

/****************************************************************************
 *	MakePLLInfoLog
 *
 *	Description:
 *			Make PLL Info Parameter log.
 *	Arguments:
 *			pParam	pointer to parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	MakePLLInfoLog
(
	const MCDRV_PLL_INFO* pParam
)
{
	CHAR	sbStr[80];

	if(pParam == NULL)
	{
		strcat(gsbLogString, " param=NULL");
		return;
	}

	sprintf(sbStr, " bMode0=%02X", pParam->bMode0);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bPrevDiv0=%02X", pParam->bPrevDiv0);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " wFbDiv0=%04X", pParam->wFbDiv0);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " wFrac0=%04X", pParam->wFrac0);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bMode1=%02X", pParam->bMode1);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bPrevDiv1=%02X", pParam->bPrevDiv1);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " wFbDiv1=%04X", pParam->wFbDiv1);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " wFrac1=%04X", pParam->wFrac1);
	strcat(gsbLogString, sbStr);
}

/****************************************************************************
 *	MakeClockInfoLog
 *
 *	Description:
 *			Make Clock Info Parameter log.
 *	Arguments:
 *			pParam	pointer to parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	MakeClockInfoLog
(
	const MCDRV_CLOCK_INFO* pParam
)
{
	CHAR	sbStr[80];

	if(pParam == NULL)
	{
		strcat(gsbLogString, " param=NULL");
		return;
	}

	sprintf(sbStr, " bCkSel=%02X", pParam->bCkSel);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bDivR0=%02X", pParam->bDivR0);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bDivF0=%02X", pParam->bDivF0);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bDivR1=%02X", pParam->bDivR1);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bDivF1=%02X", pParam->bDivF1);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bRange0=%02X", pParam->bRange0);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bRange1=%02X", pParam->bRange1);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bBypass=%02X", pParam->bBypass);
	strcat(gsbLogString, sbStr);
}

/****************************************************************************
 *	MakeClockSwInfoLog
 *
 *	Description:
 *			Make Clock Switch Info Parameter log.
 *	Arguments:
 *			pParam	pointer to parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	MakeClockSwInfoLog
(
	const MCDRV_CLKSW_INFO* pParam
)
{
	CHAR	sbStr[80];

	if(pParam == NULL)
	{
		strcat(gsbLogString, " param=NULL");
		return;
	}

	sprintf(sbStr, " bClkSrc=%02X", pParam->bClkSrc);
	strcat(gsbLogString, sbStr);
}

/****************************************************************************
 *	MakePathInfoLog
 *
 *	Description:
 *			Make Path Info Parameter log.
 *	Arguments:
 *			pParam	pointer to parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	MakePathInfoLog
(
	const MCDRV_PATH_INFO* pParam
)
{
	UINT8	bBlock, bCh;
	CHAR	sbStr[80];

	if(pParam == NULL)
	{
		strcat(gsbLogString, " param=NULL");
		return;
	}

	for(bCh = 0; bCh < HP_PATH_CHANNELS; bCh++)
	{
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++)
		{
			sprintf(sbStr, " asHpOut[%d].abSrcOnOff[%d]=%02X", bCh, bBlock, pParam->asHpOut[bCh].abSrcOnOff[bBlock]);
			strcat(gsbLogString, sbStr);
		}
	}
	for(bCh = 0; bCh < SP_PATH_CHANNELS; bCh++)
	{
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++)
		{
			sprintf(sbStr, " asSpOut[%d].abSrcOnOff[%d]=%02X", bCh, bBlock, pParam->asSpOut[bCh].abSrcOnOff[bBlock]);
			strcat(gsbLogString, sbStr);
		}
	}
	for(bCh = 0; bCh < RC_PATH_CHANNELS; bCh++)
	{
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++)
		{
			sprintf(sbStr, " asRcOut[%d].abSrcOnOff[%d]=%02X", bCh, bBlock, pParam->asRcOut[bCh].abSrcOnOff[bBlock]);
			strcat(gsbLogString, sbStr);
		}
	}
	for(bCh = 0; bCh < LOUT1_PATH_CHANNELS; bCh++)
	{
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++)
		{
			sprintf(sbStr, " asLout1[%d].abSrcOnOff[%d]=%02X", bCh, bBlock, pParam->asLout1[bCh].abSrcOnOff[bBlock]);
			strcat(gsbLogString, sbStr);
		}
	}
	for(bCh = 0; bCh < LOUT2_PATH_CHANNELS; bCh++)
	{
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++)
		{
			sprintf(sbStr, " asLout2[%d].abSrcOnOff[%d]=%02X", bCh, bBlock, pParam->asLout2[bCh].abSrcOnOff[bBlock]);
			strcat(gsbLogString, sbStr);
		}
	}
	for(bCh = 0; bCh < PEAK_PATH_CHANNELS; bCh++)
	{
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++)
		{
			sprintf(sbStr, " asPeak[%d].abSrcOnOff[%d]=%02X", bCh, bBlock, pParam->asPeak[bCh].abSrcOnOff[bBlock]);
			strcat(gsbLogString, sbStr);
		}
	}
	for(bCh = 0; bCh < DIT0_PATH_CHANNELS; bCh++)
	{
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++)
		{
			sprintf(sbStr, " asDit0[%d].abSrcOnOff[%d]=%02X", bCh, bBlock, pParam->asDit0[bCh].abSrcOnOff[bBlock]);
			strcat(gsbLogString, sbStr);
		}
	}
	for(bCh = 0; bCh < DIT1_PATH_CHANNELS; bCh++)
	{
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++)
		{
			sprintf(sbStr, " asDit1[%d].abSrcOnOff[%d]=%02X", bCh, bBlock, pParam->asDit1[bCh].abSrcOnOff[bBlock]);
			strcat(gsbLogString, sbStr);
		}
	}
	for(bCh = 0; bCh < DIT2_PATH_CHANNELS; bCh++)
	{
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++)
		{
			sprintf(sbStr, " asDit2[%d].abSrcOnOff[%d]=%02X", bCh, bBlock, pParam->asDit2[bCh].abSrcOnOff[bBlock]);
			strcat(gsbLogString, sbStr);
		}
	}
	for(bCh = 0; bCh < DAC_PATH_CHANNELS; bCh++)
	{
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++)
		{
			sprintf(sbStr, " asDac[%d].abSrcOnOff[%d]=%02X", bCh, bBlock, pParam->asDac[bCh].abSrcOnOff[bBlock]);
			strcat(gsbLogString, sbStr);
		}
	}
	for(bCh = 0; bCh < AE_PATH_CHANNELS; bCh++)
	{
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++)
		{
			sprintf(sbStr, " asAe[%d].abSrcOnOff[%d]=%02X", bCh, bBlock, pParam->asAe[bCh].abSrcOnOff[bBlock]);
			strcat(gsbLogString, sbStr);
		}
	}
	for(bCh = 0; bCh < CDSP_PATH_CHANNELS; bCh++)
	{
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++)
		{
			sprintf(sbStr, " asCdsp[%d].abSrcOnOff[%d]=%02X", bCh, bBlock, pParam->asCdsp[bCh].abSrcOnOff[bBlock]);
			strcat(gsbLogString, sbStr);
		}
	}
	for(bCh = 0; bCh < ADC0_PATH_CHANNELS; bCh++)
	{
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++)
		{
			sprintf(sbStr, " asAdc0[%d].abSrcOnOff[%d]=%02X", bCh, bBlock, pParam->asAdc0[bCh].abSrcOnOff[bBlock]);
			strcat(gsbLogString, sbStr);
		}
	}
	for(bCh = 0; bCh < ADC1_PATH_CHANNELS; bCh++)
	{
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++)
		{
			sprintf(sbStr, " asAdc1[%d].abSrcOnOff[%d]=%02X", bCh, bBlock, pParam->asAdc1[bCh].abSrcOnOff[bBlock]);
			strcat(gsbLogString, sbStr);
		}
	}
	for(bCh = 0; bCh < MIX_PATH_CHANNELS; bCh++)
	{
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++)
		{
			sprintf(sbStr, " asMix[%d].abSrcOnOff[%d]=%02X", bCh, bBlock, pParam->asMix[bCh].abSrcOnOff[bBlock]);
			strcat(gsbLogString, sbStr);
		}
	}
	for(bCh = 0; bCh < BIAS_PATH_CHANNELS; bCh++)
	{
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++)
		{
			sprintf(sbStr, " asBias[%d].abSrcOnOff[%d]=%02X", bCh, bBlock, pParam->asBias[bCh].abSrcOnOff[bBlock]);
			strcat(gsbLogString, sbStr);
		}
	}
	for(bCh = 0; bCh < MIX2_PATH_CHANNELS; bCh++)
	{
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++)
		{
			sprintf(sbStr, " asMix2[%d].abSrcOnOff[%d]=%02X", bCh, bBlock, pParam->asMix2[bCh].abSrcOnOff[bBlock]);
			strcat(gsbLogString, sbStr);
		}
	}
}

/****************************************************************************
 *	MakeVolInfoLog
 *
 *	Description:
 *			Make Volume Info Parameter log.
 *	Arguments:
 *			pParam	pointer to parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	MakeVolInfoLog
(
	const MCDRV_VOL_INFO* pParam
)
{
	UINT8	bCh;
	CHAR	sbStr[80];

	if(pParam == NULL)
	{
		strcat(gsbLogString, " param=NULL");
		return;
	}

	for(bCh = 0; bCh < AD0_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_Ad0[%d]=%04X", bCh, (UINT16)pParam->aswD_Ad0[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < AENG6_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_Aeng6[%d]=%04X", bCh, (UINT16)pParam->aswD_Aeng6[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < PDM_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_Pdm[%d]=%04X", bCh, (UINT16)pParam->aswD_Pdm[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < DTMF_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_Dtmfb[%d]=%04X", bCh, (UINT16)pParam->aswD_Dtmfb[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < DIO0_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_Dir0[%d]=%04X", bCh, (UINT16)pParam->aswD_Dir0[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < DIO1_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_Dir1[%d]=%04X", bCh, (UINT16)pParam->aswD_Dir1[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < DIO2_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_Dir2[%d]=%04X", bCh, (UINT16)pParam->aswD_Dir2[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < AD0_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_Ad0Att[%d]=%04X", bCh, (UINT16)pParam->aswD_Ad0Att[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < DIO0_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_Dir0Att[%d]=%04X", bCh, (UINT16)pParam->aswD_Dir0Att[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < DIO1_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_Dir1Att[%d]=%04X", bCh, (UINT16)pParam->aswD_Dir1Att[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < DIO2_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_Dir2Att[%d]=%04X", bCh, (UINT16)pParam->aswD_Dir2Att[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < PDM_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_SideTone[%d]=%04X", bCh, (UINT16)pParam->aswD_SideTone[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < DTFM_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_DtmfAtt[%d]=%04X", bCh, (UINT16)pParam->aswD_DtmfAtt[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < DAC_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_DacMaster[%d]=%04X", bCh, (UINT16)pParam->aswD_DacMaster[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < DAC_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_DacVoice[%d]=%04X", bCh, (UINT16)pParam->aswD_DacVoice[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < DAC_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_DacAtt[%d]=%04X", bCh, (UINT16)pParam->aswD_DacAtt[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < DIO0_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_Dit0[%d]=%04X",bCh, (UINT16)pParam->aswD_Dit0[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < DIO1_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_Dit1[%d]=%04X", bCh, (UINT16)pParam->aswD_Dit1[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < DIO2_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_Dit2[%d]=%04X", bCh, (UINT16)pParam->aswD_Dit2[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < AD0_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswA_Ad0[%d]=%04X", bCh, (UINT16)pParam->aswA_Ad0[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < AD1_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswA_Ad1[%d]=%04X", bCh, (UINT16)pParam->aswA_Ad1[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh <LIN1_VOL_CHANNELS ; bCh++)
	{
		sprintf(sbStr, " aswA_Lin1[%d]=%04X", bCh, (UINT16)pParam->aswA_Lin1[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < LIN2_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswA_Lin2[%d]=%04X", bCh, (UINT16)pParam->aswA_Lin2[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < MIC1_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswA_Mic1[%d]=%04X", bCh, (UINT16)pParam->aswA_Mic1[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < MIC2_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswA_Mic2[%d]=%04X", bCh, (UINT16)pParam->aswA_Mic2[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < MIC3_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswA_Mic3[%d]=%04X", bCh, (UINT16)pParam->aswA_Mic3[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < HP_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswA_Hp[%d]=%04X", bCh, (UINT16)pParam->aswA_Hp[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < SP_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswA_Sp[%d]=%04X", bCh, (UINT16)pParam->aswA_Sp[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < RC_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswA_Rc[%d]=%04X", bCh, (UINT16)pParam->aswA_Rc[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < LOUT1_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswA_Lout1[%d]=%04X", bCh, (UINT16)pParam->aswA_Lout1[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < LOUT2_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswA_Lout2[%d]=%04X", bCh, (UINT16)pParam->aswA_Lout2[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < MIC1_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswA_Mic1Gain[%d]=%04X", bCh, (UINT16)pParam->aswA_Mic1Gain[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < MIC2_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswA_Mic2Gain[%d]=%04X", bCh, (UINT16)pParam->aswA_Mic2Gain[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < MIC3_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswA_Mic3Gain[%d]=%04X", bCh, (UINT16)pParam->aswA_Mic3Gain[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < HPGAIN_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswA_HpGain[%d]=%04X", bCh, (UINT16)pParam->aswA_HpGain[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < AD1_DVOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_Adc1[%d]=%04X", bCh, (UINT16)pParam->aswD_Adc1[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < AD1_DVOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_Adc1Att[%d]=%04X", bCh, (UINT16)pParam->aswD_Adc1Att[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < DIO2_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_Dit21[%d]=%04X", bCh, (UINT16)pParam->aswD_Dit21[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < MIX2_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_Adc0AeMix[%d]=%04X", bCh, (UINT16)pParam->aswD_Adc0AeMix[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < MIX2_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_Adc1AeMix[%d]=%04X", bCh, (UINT16)pParam->aswD_Adc1AeMix[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < MIX2_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_Dir0AeMix[%d]=%04X", bCh, (UINT16)pParam->aswD_Dir0AeMix[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < MIX2_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_Dir1AeMix[%d]=%04X", bCh, (UINT16)pParam->aswD_Dir1AeMix[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < MIX2_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_Dir2AeMix[%d]=%04X", bCh, (UINT16)pParam->aswD_Dir2AeMix[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < MIX2_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_PdmAeMix[%d]=%04X", bCh, (UINT16)pParam->aswD_PdmAeMix[bCh]);
		strcat(gsbLogString, sbStr);
	}
	for(bCh = 0; bCh < MIX2_VOL_CHANNELS; bCh++)
	{
		sprintf(sbStr, " aswD_MixAeMix[%d]=%04X", bCh, (UINT16)pParam->aswD_MixAeMix[bCh]);
		strcat(gsbLogString, sbStr);
	}
}

/****************************************************************************
 *	MakeDIOInfoLog
 *
 *	Description:
 *			Make Digital I/O Info Parameter log.
 *	Arguments:
 *			pParam	pointer to parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	MakeDIOInfoLog
(
	const MCDRV_DIO_INFO* pParam
)
{
	CHAR	sbStr[80];
	UINT8	bPort, bCh;

	if(pParam == NULL)
	{
		strcat(gsbLogString, " param=NULL");
		return;
	}

	for(bPort = 0; bPort < IOPORT_NUM; bPort++)
	{
		sprintf(sbStr, " asPortInfo[%d].sDioCommon.bMasterSlave=%02X", bPort, pParam->asPortInfo[bPort].sDioCommon.bMasterSlave);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " asPortInfo[%d].sDioCommon.bAutoFs=%02X", bPort, pParam->asPortInfo[bPort].sDioCommon.bAutoFs);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " asPortInfo[%d].sDioCommon.bFs=%02X", bPort, pParam->asPortInfo[bPort].sDioCommon.bFs);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " asPortInfo[%d].sDioCommon.bBckFs=%02X", bPort, pParam->asPortInfo[bPort].sDioCommon.bBckFs);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " asPortInfo[%d].sDioCommon.bInterface=%02X", bPort, pParam->asPortInfo[bPort].sDioCommon.bInterface);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " asPortInfo[%d].sDioCommon.bBckInvert=%02X", bPort, pParam->asPortInfo[bPort].sDioCommon.bBckInvert);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " asPortInfo[%d].sDioCommon.bPcmHizTim=%02X", bPort, pParam->asPortInfo[bPort].sDioCommon.bPcmHizTim);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " asPortInfo[%d].sDioCommon.bPcmClkDown=%02X", bPort, pParam->asPortInfo[bPort].sDioCommon.bPcmClkDown);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " asPortInfo[%d].sDioCommon.bPcmFrame=%02X", bPort, pParam->asPortInfo[bPort].sDioCommon.bPcmFrame);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " asPortInfo[%d].sDioCommon.bPcmHighPeriod=%02X", bPort, pParam->asPortInfo[bPort].sDioCommon.bPcmHighPeriod);
		strcat(gsbLogString, sbStr);

		sprintf(sbStr, " asPortInfo[%d].sDir.wSrcRate=%04X", bPort, pParam->asPortInfo[bPort].sDir.wSrcRate);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " asPortInfo[%d].sDir.sDaFormat.bBitSel=%02X", bPort, pParam->asPortInfo[bPort].sDir.sDaFormat.bBitSel);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " asPortInfo[%d].sDir.sDaFormat.bMode=%02X", bPort, pParam->asPortInfo[bPort].sDir.sDaFormat.bMode);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " asPortInfo[%d].sDir.sPcmFormat.bMono=%02X", bPort, pParam->asPortInfo[bPort].sDir.sPcmFormat.bMono);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " asPortInfo[%d].sDir.sPcmFormat.bOrder=%02X", bPort, pParam->asPortInfo[bPort].sDir.sPcmFormat.bOrder);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " asPortInfo[%d].sDir.sPcmFormat.bLaw=%02X", bPort, pParam->asPortInfo[bPort].sDir.sPcmFormat.bLaw);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " asPortInfo[%d].sDir.sPcmFormat.bBitSel=%02X", bPort, pParam->asPortInfo[bPort].sDir.sPcmFormat.bBitSel);
		strcat(gsbLogString, sbStr);
		for(bCh = 0; bCh < DIO_CHANNELS; bCh++)
		{
			sprintf(sbStr, " asPortInfo[%d].sDir.abSlot[%d]=%02X", bPort, bCh, pParam->asPortInfo[bPort].sDir.abSlot[bCh]);
			strcat(gsbLogString, sbStr);
		}

		sprintf(sbStr, " asPortInfo[%d].sDit.wSrcRate=%04X", bPort, pParam->asPortInfo[bPort].sDit.wSrcRate);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " asPortInfo[%d].sDit.sDaFormat.bBitSel=%02X", bPort, pParam->asPortInfo[bPort].sDit.sDaFormat.bBitSel);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " asPortInfo[%d].sDit.sDaFormat.bMode=%02X", bPort, pParam->asPortInfo[bPort].sDit.sDaFormat.bMode);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " asPortInfo[%d].sDit.sPcmFormat.bMono=%02X", bPort, pParam->asPortInfo[bPort].sDit.sPcmFormat.bMono);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " asPortInfo[%d].sDit.sPcmFormat.bOrder=%02X", bPort, pParam->asPortInfo[bPort].sDit.sPcmFormat.bOrder);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " asPortInfo[%d].sDit.sPcmFormat.bLaw=%02X", bPort, pParam->asPortInfo[bPort].sDit.sPcmFormat.bLaw);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " asPortInfo[%d].sDit.sPcmFormat.bBitSel=%02X", bPort, pParam->asPortInfo[bPort].sDit.sPcmFormat.bBitSel);
		strcat(gsbLogString, sbStr);
		for(bCh = 0; bCh < DIO_CHANNELS; bCh++)
		{
			sprintf(sbStr, " asPortInfo[%d].sDit.abSlot[%d]=%02X", bPort, bCh, pParam->asPortInfo[bPort].sDit.abSlot[bCh]);
			strcat(gsbLogString, sbStr);
		}
	}
}

/****************************************************************************
 *	MakeDACInfoLog
 *
 *	Description:
 *			Make DAC Info Parameter log.
 *	Arguments:
 *			pParam	pointer to parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	MakeDACInfoLog
(
	const MCDRV_DAC_INFO* pParam
)
{
	CHAR	sbStr[80];

	if(pParam == NULL)
	{
		strcat(gsbLogString, " param=NULL");
		return;
	}

	sprintf(sbStr, " bMasterSwap=%02X", pParam->bMasterSwap);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bVoiceSwap=%02X", pParam->bVoiceSwap);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bDcCut=%02X", pParam->bDcCut);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bDacSwap=%02X", pParam->bDacSwap);
	strcat(gsbLogString, sbStr);
}

/****************************************************************************
 *	MakeADCInfoLog
 *
 *	Description:
 *			Make ADC Info Parameter log.
 *	Arguments:
 *			pParam	pointer to parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	MakeADCInfoLog
(
	const MCDRV_ADC_INFO* pParam
)
{
	CHAR	sbStr[80];

	if(pParam == NULL)
	{
		strcat(gsbLogString, " param=NULL");
		return;
	}

	sprintf(sbStr, " bAgcAdjust=%02X", pParam->bAgcAdjust);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bAgcOn=%02X", pParam->bAgcOn);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bMono=%02X", pParam->bMono);
	strcat(gsbLogString, sbStr);
}

/****************************************************************************
 *	MakeADCExInfoLog
 *
 *	Description:
 *			Make ADC Ex Info Parameter log.
 *	Arguments:
 *			pParam	pointer to parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	MakeADCExInfoLog
(
	const MCDRV_ADC_EX_INFO* pParam
)
{
	CHAR	sbStr[80];

	if(pParam == NULL)
	{
		strcat(gsbLogString, " param=NULL");
		return;
	}

	sprintf(sbStr, "bAgcEnv =%02X", pParam->bAgcEnv);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, "bAgcLvl =%02X", pParam->bAgcLvl);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, "bAgcUpTim =%02X", pParam->bAgcUpTim);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, "bAgcDwTim =%02X", pParam->bAgcDwTim);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, "bAgcCutLvl =%02X", pParam->bAgcCutLvl);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, "bAgcCutTim =%02X", pParam->bAgcCutTim);
	strcat(gsbLogString, sbStr);
}

/****************************************************************************
 *	MakeSpInfoLog
 *
 *	Description:
 *			Make Sp Info Parameter log.
 *	Arguments:
 *			pParam	pointer to parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	MakeSpInfoLog
(
	const MCDRV_SP_INFO* pParam
)
{
	CHAR	sbStr[80];

	if(pParam == NULL)
	{
		strcat(gsbLogString, " param=NULL");
		return;
	}

	sprintf(sbStr, " bSwap=%02X", pParam->bSwap);
	strcat(gsbLogString, sbStr);
}

/****************************************************************************
 *	MakeDNGInfoLog
 *
 *	Description:
 *			Make DNG Info Parameter log.
 *	Arguments:
 *			pParam	pointer to parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	MakeDNGInfoLog
(
	const MCDRV_DNG_INFO* pParam
)
{
	CHAR	sbStr[80];
	UINT8	bItem;

	if(pParam == NULL)
	{
		strcat(gsbLogString, " param=NULL");
		return;
	}

	for(bItem = 0; bItem < DNG_ITEM_NUM; bItem++)
	{
		sprintf(sbStr, " abOnOff[%d]=%02X", bItem, pParam->abOnOff[bItem]);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " abThreshold[%d]=%02X", bItem, pParam->abThreshold[bItem]);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " abHold[%d]=%02X", bItem, pParam->abHold[bItem]);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " abAttack[%d]=%02X", bItem, pParam->abAttack[bItem]);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " abRelease[%d]=%02X", bItem, pParam->abRelease[bItem]);
		strcat(gsbLogString, sbStr);
		sprintf(sbStr, " abTarget[%d]=%02X", bItem, pParam->abTarget[bItem]);
		strcat(gsbLogString, sbStr);
	}
	sprintf(sbStr, " bFw=%02X", pParam->bFw);
	strcat(gsbLogString, sbStr);
}

/****************************************************************************
 *	MakeAEInfoLog
 *
 *	Description:
 *			Make AudioEngine Info Parameter log.
 *	Arguments:
 *			pParam	pointer to parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	MakeAEInfoLog
(
	const MCDRV_AE_INFO* pParam
)
{
	CHAR	sbStr[80];
	UINT16	wIdx;

	if(pParam == NULL)
	{
		strcat(gsbLogString, " param=NULL");
		return;
	}

	sprintf(sbStr, " bOnOff=%02X", pParam->bOnOff);
	strcat(gsbLogString, sbStr);

	for(wIdx = 0; wIdx < BEX_PARAM_SIZE; wIdx++)
	{
		sprintf(sbStr, " abBex[%d]=%02X", wIdx, pParam->abBex[wIdx]);
		strcat(gsbLogString, sbStr);
	}
	for(wIdx = 0; wIdx < WIDE_PARAM_SIZE; wIdx++)
	{
		sprintf(sbStr, " abWide[%d]=%02X", wIdx, pParam->abWide[wIdx]);
		strcat(gsbLogString, sbStr);
	}
	for(wIdx = 0; wIdx < DRC_PARAM_SIZE; wIdx++)
	{
		sprintf(sbStr, " abDrc[%d]=%02X", wIdx, pParam->abDrc[wIdx]);
		strcat(gsbLogString, sbStr);
	}

	machdep_DebugPrint((UINT8*)(void*)gsbLogString);
	gsbLogString[0]	= 0;

	for(wIdx = 0; wIdx < EQ5_PARAM_SIZE; wIdx++)
	{
		sprintf(sbStr, " abEq5[%d]=%02X", wIdx, pParam->abEq5[wIdx]);
		strcat(gsbLogString, sbStr);
	}
	for(wIdx = 0; wIdx < EQ3_PARAM_SIZE; wIdx++)
	{
		sprintf(sbStr, " abEq3[%d]=%02X", wIdx, pParam->abEq3[wIdx]);
		strcat(gsbLogString, sbStr);
	}
	for(wIdx = 0; wIdx < DRC2_PARAM_SIZE; wIdx++)
	{
		sprintf(sbStr, " abDrc2[%d]=%02X", wIdx, pParam->abDrc2[wIdx]);
		strcat(gsbLogString, sbStr);
	}
}

/****************************************************************************
 *	MakeSetCDSPLog
 *
 *	Description:
 *			Make C-DSP Parameter log.
 *	Arguments:
 *			pParam	parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	MakeSetCDSPLog
(
	const UINT8*	pParam
)
{
	CHAR	sbStr[80];

	if(pParam == NULL)
	{
		strcat(gsbLogString, " param=NULL");
		return;
	}

	sprintf(sbStr, " param=%08p", pParam);
	strcat(gsbLogString, sbStr);
}

/****************************************************************************
 *	MakeCDSPParamLog
 *
 *	Description:
 *			Make CDSP Parameter log.
 *	Arguments:
 *			pParam	pointer to parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	MakeCDSPParamLog
(
	const MCDRV_CDSPPARAM* pParam
)
{
	CHAR	sbStr[80];
	UINT8	bIdx;

	if(pParam == NULL)
	{
		strcat(gsbLogString, " param=NULL");
		return;
	}

	sprintf(sbStr, " bCommand=%02X", pParam->bCommand);
	strcat(gsbLogString, sbStr);
	for(bIdx = 0; bIdx < 16; bIdx++)
	{
		sprintf(sbStr, " abParam[%d]=%02X", bIdx, pParam->abParam[bIdx]);
		strcat(gsbLogString, sbStr);
	}
}

/****************************************************************************
 *	MakePDMInfoLog
 *
 *	Description:
 *			Make PDM Info Parameter log.
 *	Arguments:
 *			pParam	pointer to parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	MakePDMInfoLog
(
	const MCDRV_PDM_INFO* pParam
)
{
	CHAR	sbStr[80];

	if(pParam == NULL)
	{
		strcat(gsbLogString, " param=NULL");
		return;
	}

	sprintf(sbStr, " bClk=%02X", pParam->bClk);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bAgcAdjust=%02X", pParam->bAgcAdjust);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bAgcOn=%02X", pParam->bAgcOn);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bPdmEdge=%02X", pParam->bPdmEdge);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bPdmWait=%02X", pParam->bPdmWait);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bPdmSel=%02X", pParam->bPdmSel);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bMono=%02X", pParam->bMono);
	strcat(gsbLogString, sbStr);
}

/****************************************************************************
 *	MakePDMExInfoLog
 *
 *	Description:
 *			Make PDM Ex Info Parameter log.
 *	Arguments:
 *			pParam	pointer to parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	MakePDMExInfoLog
(
	const MCDRV_PDM_EX_INFO* pParam
)
{
	CHAR	sbStr[80];

	if(pParam == NULL)
	{
		strcat(gsbLogString, " param=NULL");
		return;
	}

	sprintf(sbStr, "bAgcEnv =%02X", pParam->bAgcEnv);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, "bAgcLvl =%02X", pParam->bAgcLvl);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, "bAgcUpTim =%02X", pParam->bAgcUpTim);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, "bAgcDwTim =%02X", pParam->bAgcDwTim);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, "bAgcCutLvl =%02X", pParam->bAgcCutLvl);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, "bAgcCutTim =%02X", pParam->bAgcCutTim);
	strcat(gsbLogString, sbStr);
}

/****************************************************************************
 *	MakeDTMFInfoLog
 *
 *	Description:
 *			Make DTMF Info Parameter log.
 *	Arguments:
 *			pParam	pointer to parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	MakeDTMFInfoLog
(
	const MCDRV_DTMF_INFO* pParam
)
{
	CHAR	sbStr[80];

	if(pParam == NULL)
	{
		strcat(gsbLogString, " param=NULL");
		return;
	}

	sprintf(sbStr, " bSinGenHost=%02X", pParam->bSinGenHost);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bOnOff=%02X", pParam->bOnOff);
	strcat(gsbLogString, sbStr);

	sprintf(sbStr, " sParam.wSinGen0Freq=%04X", pParam->sParam.wSinGen0Freq);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " sParam.wSinGen1Freq=%04X", pParam->sParam.wSinGen1Freq);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " sParam.swSinGen0Vol=%02X", pParam->sParam.swSinGen0Vol);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " sParam.swSinGen1Vol=%02X", pParam->sParam.swSinGen1Vol);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " sParam.bSinGenGate=%02X", pParam->sParam.bSinGenGate);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " sParam.bSinGenLoop=%02X", pParam->sParam.bSinGenLoop);
	strcat(gsbLogString, sbStr);
}

/****************************************************************************
 *	MakeGPModeLog
 *
 *	Description:
 *			Make GPIO mode Info Parameter log.
 *	Arguments:
 *			pParam	pointer to parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	MakeGPModeLog
(
	const MCDRV_GP_MODE* pParam
)
{
	CHAR	sbStr[80];
	UINT8	bPadNo;

	if(pParam == NULL)
	{
		strcat(gsbLogString, " param=NULL");
		return;
	}

	for(bPadNo = 0; bPadNo < GPIO_PAD_NUM; bPadNo++)
	{
		sprintf(sbStr, " abGpDdr[%d]=%02X", bPadNo, pParam->abGpDdr[bPadNo]);
		strcat(gsbLogString, sbStr);
	}
	for(bPadNo = 0; bPadNo < GPIO_PAD_NUM; bPadNo++)
	{
		sprintf(sbStr, " abGpMode[%d]=%02X", bPadNo, pParam->abGpMode[bPadNo]);
		strcat(gsbLogString, sbStr);
	}
	for(bPadNo = 0; bPadNo < GPIO_PAD_NUM; bPadNo++)
	{
		sprintf(sbStr, " abGpHost[%d]=%02X", bPadNo, pParam->abGpHost[bPadNo]);
		strcat(gsbLogString, sbStr);
	}
	for(bPadNo = 0; bPadNo < GPIO_PAD_NUM; bPadNo++)
	{
		sprintf(sbStr, " abGpInvert[%d]=%02X", bPadNo, pParam->abGpInvert[bPadNo]);
		strcat(gsbLogString, sbStr);
	}
}

/****************************************************************************
 *	MakeGPMaskLog
 *
 *	Description:
 *			Make GPIO Mask Info Parameter log.
 *	Arguments:
 *			pParam	pointer to parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	MakeGPMaskLog
(
	const UINT8* pParam
)
{
	CHAR	sbStr[80];

	if(pParam == NULL)
	{
		strcat(gsbLogString, " param=NULL");
		return;
	}

	sprintf(sbStr, " mask=%02X", *pParam);
	strcat(gsbLogString, sbStr);
}

/****************************************************************************
 *	MakeGetSetGPLog
 *
 *	Description:
 *			Make Get/Set GPIO Info Parameter log.
 *	Arguments:
 *			pParam	pointer to parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	MakeGetSetGPLog
(
	const UINT8* pParam
)
{
	CHAR	sbStr[80];

	if(pParam == NULL)
	{
		strcat(gsbLogString, " param=NULL");
		return;
	}

	sprintf(sbStr, " HiLow=%02X", *pParam);
	strcat(gsbLogString, sbStr);
}

/****************************************************************************
 *	MakePeakLog
 *
 *	Description:
 *			Make Peak Parameter log.
 *	Arguments:
 *			pParam	pointer to parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	MakePeakLog
(
	const MCDRV_PEAK* pParam
)
{
	CHAR	sbStr[80];
	UINT8	bIdx;

	if(pParam == NULL)
	{
		strcat(gsbLogString, " param=NULL");
		return;
	}

	for(bIdx = 0; bIdx < PEAK_CHANNELS; bIdx++)
	{
		sprintf(sbStr, " aswPeak[%d]=%02X", bIdx, pParam->aswPeak[bIdx]);
		strcat(gsbLogString, sbStr);
	}
}

/****************************************************************************
 *	MakeSysEQInfoLog
 *
 *	Description:
 *			Make System EQ Info Parameter log.
 *	Arguments:
 *			pParam	pointer to parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	MakeSysEQInfoLog
(
	const MCDRV_SYSEQ_INFO* pParam
)
{
	CHAR	sbStr[80];
	UINT8	bIdx;

	if(pParam == NULL)
	{
		strcat(gsbLogString, " param=NULL");
		return;
	}

	sprintf(sbStr, " bOnOff=%02X", pParam->bOnOff);
	strcat(gsbLogString, sbStr);

	for(bIdx = 0; bIdx < 15; bIdx++)
	{
		sprintf(sbStr, " abParam[%d]=%02X", bIdx, pParam->abParam[bIdx]);
		strcat(gsbLogString, sbStr);
	}
}

/****************************************************************************
 *	MakeDITSwapInfoLog
 *
 *	Description:
 *			Make DIT Swap Info Parameter log.
 *	Arguments:
 *			pParam	pointer to parameter
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	MakeDITSwapInfoLog
(
	const MCDRV_DITSWAP_INFO* pParam
)
{
	CHAR	sbStr[80];

	if(pParam == NULL)
	{
		strcat(gsbLogString, " param=NULL");
		return;
	}

	sprintf(sbStr, " bSwapL=%02X", pParam->bSwapL);
	strcat(gsbLogString, sbStr);
	sprintf(sbStr, " bSwapR=%02X", pParam->bSwapR);
	strcat(gsbLogString, sbStr);
}




#endif	/*	MCDRV_DEBUG_LEVEL	*/
