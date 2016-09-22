/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*******************************************************************************
 *
 * Filename:
 * ---------
 *   AudDrv_Ana.c
 *
 * Project:
 * --------
 *   MT6583  Audio Driver ana Register setting
 *
 * Description:
 * ------------
 *   Audio register
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
 *
 *
 *******************************************************************************/


/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "AudDrv_Common.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"

/*
  *define this to use wrapper to control
*/

#ifndef CONFIG_MTK_FPGA
#define AUDIO_USING_WRAP_DRIVER
#endif

#ifdef AUDIO_USING_WRAP_DRIVER
#include <mach/mt_pmic_wrap.h>
#endif

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/

void Ana_Set_Reg(uint32 offset, uint32 value, uint32 mask)
{
	/*
	* set pmic register or analog CONTROL_IFACE_PATH
	*/
	int ret = 0;
	uint32 Reg_Value = 0;
	PRINTK_ANA_REG("Ana_Set_Reg offset= 0x%x , value = 0x%x mask = 0x%x\n", offset, value, mask);
#ifdef AUDIO_USING_WRAP_DRIVER
	Reg_Value = Ana_Get_Reg(offset);
	Reg_Value &= (~mask);
	Reg_Value |= (value & mask);
	ret = pwrap_write(offset, Reg_Value);
	Reg_Value = Ana_Get_Reg(offset);
	if ((Reg_Value & mask) != (value & mask)) {
		pr_debug("Ana_Set_Reg offset= 0x%x , value = 0x%x mask = 0x%x ret = %d Reg_Value = 0x%x\n",
			 offset, value, mask, ret, Reg_Value);
	}

#endif
}
EXPORT_SYMBOL(Ana_Set_Reg);

uint32 Ana_Get_Reg(uint32 offset)
{
	/*
	* get pmic register
	*/
	int ret = 0;
	uint32 Rdata = 0;
#ifdef AUDIO_USING_WRAP_DRIVER
	ret = pwrap_read(offset, &Rdata);
#endif
	PRINTK_ANA_REG("Ana_Get_Reg offset= 0x%x  Rdata = 0x%x ret = %d\n", offset, Rdata, ret);
	return Rdata;
}
EXPORT_SYMBOL(Ana_Get_Reg);

void Ana_Log_Print(void)
{
	AudDrv_ANA_Clk_On();
	pr_debug("AFE_UL_DL_CON0  = 0x%x\n", Ana_Get_Reg(AFE_UL_DL_CON0));
	pr_debug("AFE_DL_SRC2_CON0_H  = 0x%x\n", Ana_Get_Reg(AFE_DL_SRC2_CON0_H));
	pr_debug("AFE_DL_SRC2_CON0_L  = 0x%x\n", Ana_Get_Reg(AFE_DL_SRC2_CON0_L));
	pr_debug("AFE_DL_SDM_CON0  = 0x%x\n", Ana_Get_Reg(AFE_DL_SDM_CON0));
	pr_debug("AFE_DL_SDM_CON1  = 0x%x\n", Ana_Get_Reg(AFE_DL_SDM_CON1));
	pr_debug("AFE_UL_SRC0_CON0_H  = 0x%x\n", Ana_Get_Reg(AFE_UL_SRC0_CON0_H));
	pr_debug("AFE_UL_SRC0_CON0_L  = 0x%x\n", Ana_Get_Reg(AFE_UL_SRC0_CON0_L));
	pr_debug("AFE_UL_SRC1_CON0_H  = 0x%x\n", Ana_Get_Reg(AFE_UL_SRC1_CON0_H));
	pr_debug("AFE_UL_SRC1_CON0_L  = 0x%x\n", Ana_Get_Reg(AFE_UL_SRC1_CON0_L));
	pr_debug("PMIC_AFE_TOP_CON0  = 0x%x\n", Ana_Get_Reg(PMIC_AFE_TOP_CON0));
	pr_debug("AFE_AUDIO_TOP_CON0  = 0x%x\n", Ana_Get_Reg(AFE_AUDIO_TOP_CON0));
	pr_debug("PMIC_AFE_TOP_CON0  = 0x%x\n", Ana_Get_Reg(PMIC_AFE_TOP_CON0));
	pr_debug("AFE_DL_SRC_MON0  = 0x%x\n", Ana_Get_Reg(AFE_DL_SRC_MON0));
	pr_debug("AFE_DL_SDM_TEST0  = 0x%x\n", Ana_Get_Reg(AFE_DL_SDM_TEST0));
	pr_debug("AFE_MON_DEBUG0  = 0x%x\n", Ana_Get_Reg(AFE_MON_DEBUG0));
	pr_debug("AUDRC_TUNE_MON0  = 0x%x\n", Ana_Get_Reg(AUDRC_TUNE_MON0));
	pr_debug("AFE_UP8X_FIFO_CFG0  = 0x%x\n", Ana_Get_Reg(AFE_UP8X_FIFO_CFG0));
	pr_debug("AFE_UP8X_FIFO_LOG_MON0  = 0x%x\n", Ana_Get_Reg(AFE_UP8X_FIFO_LOG_MON0));
	pr_debug("AFE_UP8X_FIFO_LOG_MON1  = 0x%x\n", Ana_Get_Reg(AFE_UP8X_FIFO_LOG_MON1));
	pr_debug("AFE_DL_DC_COMP_CFG0  = 0x%x\n", Ana_Get_Reg(AFE_DL_DC_COMP_CFG0));
	pr_debug("AFE_DL_DC_COMP_CFG1  = 0x%x\n", Ana_Get_Reg(AFE_DL_DC_COMP_CFG1));
	pr_debug("AFE_DL_DC_COMP_CFG2  = 0x%x\n", Ana_Get_Reg(AFE_DL_DC_COMP_CFG2));
	pr_debug("AFE_PMIC_NEWIF_CFG0  = 0x%x\n", Ana_Get_Reg(AFE_PMIC_NEWIF_CFG0));
	pr_debug("AFE_PMIC_NEWIF_CFG1  = 0x%x\n", Ana_Get_Reg(AFE_PMIC_NEWIF_CFG1));
	pr_debug("AFE_PMIC_NEWIF_CFG2  = 0x%x\n", Ana_Get_Reg(AFE_PMIC_NEWIF_CFG2));
	pr_debug("AFE_PMIC_NEWIF_CFG3  = 0x%x\n", Ana_Get_Reg(AFE_PMIC_NEWIF_CFG3));
	pr_debug("AFE_SGEN_CFG0  = 0x%x\n", Ana_Get_Reg(AFE_SGEN_CFG0));
	pr_debug("AFE_SGEN_CFG1  = 0x%x\n", Ana_Get_Reg(AFE_SGEN_CFG1));
	pr_debug("AFE_ADDA2_UL_SRC_CON0_H  = 0x%x\n", Ana_Get_Reg(AFE_ADDA2_UL_SRC_CON0_H));
	pr_debug("AFE_ADDA2_UL_SRC_CON0_L  = 0x%x\n", Ana_Get_Reg(AFE_ADDA2_UL_SRC_CON0_L));
	pr_debug("AFE_UL_SRC_CON1_H  = 0x%x\n", Ana_Get_Reg(AFE_UL_SRC_CON1_H));

	pr_debug("AFE_ADDA2_UL_SRC_CON1_L  = 0x%x\n", Ana_Get_Reg(AFE_ADDA2_UL_SRC_CON1_L));
	pr_debug("AFE_ADDA2_UP8X_FIFO_LOG_MON0  = 0x%x\n", Ana_Get_Reg(AFE_ADDA2_UP8X_FIFO_LOG_MON0));
	pr_debug("AFE_ADDA2_UP8X_FIFO_LOG_MON1  = 0x%x\n", Ana_Get_Reg(AFE_ADDA2_UP8X_FIFO_LOG_MON1));
	pr_debug("AFE_ADDA2_PMIC_NEWIF_CFG0  = 0x%x\n", Ana_Get_Reg(AFE_ADDA2_PMIC_NEWIF_CFG0));
	pr_debug("AFE_ADDA2_PMIC_NEWIF_CFG1  = 0x%x\n", Ana_Get_Reg(AFE_ADDA2_PMIC_NEWIF_CFG1));
	pr_debug("AFE_ADDA2_PMIC_NEWIF_CFG2  = 0x%x\n", Ana_Get_Reg(AFE_ADDA2_PMIC_NEWIF_CFG2));
	pr_debug("AFE_MIC_ARRAY_CFG  = 0x%x\n", Ana_Get_Reg(AFE_MIC_ARRAY_CFG));
	pr_debug("AFE_ADC_ASYNC_FIFO_CFG  = 0x%x\n", Ana_Get_Reg(AFE_ADC_ASYNC_FIFO_CFG));
	pr_debug("AFE_ANC_CFG0  = 0x%x\n", Ana_Get_Reg(AFE_ANC_CFG0));
	pr_debug("AFE_ANC_CFG1  = 0x%x\n", Ana_Get_Reg(AFE_ANC_CFG1));
	pr_debug("AFE_ANC_COEF_B00  = 0x%x\n", Ana_Get_Reg(AFE_ANC_COEF_B00));
	pr_debug("AFE_ANC_COEF_ADDR  = 0x%x\n", Ana_Get_Reg(AFE_ANC_COEF_ADDR));
	pr_debug("AFE_ANC_COEF_WDATA  = 0x%x\n", Ana_Get_Reg(AFE_ANC_COEF_WDATA));
	pr_debug("AFE_ANC_COEF_RDATA  = 0x%x\n", Ana_Get_Reg(AFE_ANC_COEF_RDATA));
	pr_debug("AUDRC_TUNE_UL2_MON0  = 0x%x\n", Ana_Get_Reg(AUDRC_TUNE_UL2_MON0));
	pr_debug("AFE_MBIST_CFG0  = 0x%x\n", Ana_Get_Reg(AFE_MBIST_CFG0));
	pr_debug("AFE_MBIST_CFG1  = 0x%x\n", Ana_Get_Reg(AFE_MBIST_CFG1));
	pr_debug("AFE_MBIST_CFG2  = 0x%x\n", Ana_Get_Reg(AFE_MBIST_CFG2));
	pr_debug("AFE_MBIST_CFG3  = 0x%x\n", Ana_Get_Reg(AFE_MBIST_CFG3));
	pr_debug("AFE_VOW_TOP  = 0x%x\n", Ana_Get_Reg(AFE_VOW_TOP));
	pr_debug("AFE_VOW_CFG0  = 0x%x\n", Ana_Get_Reg(AFE_VOW_CFG0));
	pr_debug("AFE_VOW_CFG1  = 0x%x\n", Ana_Get_Reg(AFE_VOW_CFG1));
	pr_debug("AFE_VOW_CFG2  = 0x%x\n", Ana_Get_Reg(AFE_VOW_CFG2));
	pr_debug("AFE_VOW_CFG3  = 0x%x\n", Ana_Get_Reg(AFE_VOW_CFG3));
	pr_debug("AFE_VOW_CFG4  = 0x%x\n", Ana_Get_Reg(AFE_VOW_CFG4));
	pr_debug("AFE_VOW_CFG5  = 0x%x\n", Ana_Get_Reg(AFE_VOW_CFG5));
	pr_debug("AFE_VOW_MON0  = 0x%x\n", Ana_Get_Reg(AFE_VOW_MON0));
	pr_debug("AFE_VOW_MON1  = 0x%x\n", Ana_Get_Reg(AFE_VOW_MON1));
	pr_debug("AFE_VOW_MON2  = 0x%x\n", Ana_Get_Reg(AFE_VOW_MON2));
	pr_debug("AFE_VOW_MON3  = 0x%x\n", Ana_Get_Reg(AFE_VOW_MON3));
	pr_debug("AFE_VOW_MON4  = 0x%x\n", Ana_Get_Reg(AFE_VOW_MON4));
	pr_debug("AFE_VOW_MON5  = 0x%x\n", Ana_Get_Reg(AFE_VOW_MON5));

	pr_debug("AFE_CLASSH_CFG0  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG0));
	pr_debug("AFE_CLASSH_CFG1  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG1));
	pr_debug("AFE_CLASSH_CFG2  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG2));
	pr_debug("AFE_CLASSH_CFG3  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG3));
	pr_debug("AFE_CLASSH_CFG4  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG4));
	pr_debug("AFE_CLASSH_CFG5  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG5));
	pr_debug("AFE_CLASSH_CFG6  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG6));
	pr_debug("AFE_CLASSH_CFG7  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG7));
	pr_debug("AFE_CLASSH_CFG8  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG8));
	pr_debug("AFE_CLASSH_CFG9  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG9));
	pr_debug("AFE_CLASSH_CFG10 = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG10));
	pr_debug("AFE_CLASSH_CFG11 = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG11));
	pr_debug("AFE_CLASSH_CFG12  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG12));
	pr_debug("AFE_CLASSH_CFG13  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG13));
	pr_debug("AFE_CLASSH_CFG14  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG14));
	pr_debug("AFE_CLASSH_CFG15  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG15));
	pr_debug("AFE_CLASSH_CFG16  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG16));
	pr_debug("AFE_CLASSH_CFG17  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG17));
	pr_debug("AFE_CLASSH_CFG18  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG18));
	pr_debug("AFE_CLASSH_CFG19  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG19));
	pr_debug("AFE_CLASSH_CFG20  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG20));
	pr_debug("AFE_CLASSH_CFG21  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG21));
	pr_debug("AFE_CLASSH_CFG22  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG22));
	pr_debug("AFE_CLASSH_CFG23  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG23));
	pr_debug("AFE_CLASSH_CFG24  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG24));
	pr_debug("AFE_CLASSH_CFG25  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG25));
	pr_debug("AFE_CLASSH_CFG26  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG26));
	pr_debug("AFE_CLASSH_CFG27  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG27));
	pr_debug("AFE_CLASSH_CFG28  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG28));
	pr_debug("AFE_CLASSH_CFG29  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG29));
	pr_debug("AFE_CLASSH_CFG30  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG30));
	pr_debug("AFE_CLASSH_MON00  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_MON00));
	pr_debug("AFE_CLASSH_MON1  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_MON1));
	pr_debug("AFE_DCCLK_CFG0  = 0x%x\n", Ana_Get_Reg(AFE_DCCLK_CFG0));
	pr_debug("AFE_ANC_COEF_MON1  = 0x%x\n", Ana_Get_Reg(AFE_ANC_COEF_MON1));
	pr_debug("AFE_ANC_COEF_MON2  = 0x%x\n", Ana_Get_Reg(AFE_ANC_COEF_MON2));
	pr_debug("AFE_ANC_COEF_MON3  = 0x%x\n", Ana_Get_Reg(AFE_ANC_COEF_MON3));

	pr_debug("TOP_STATUS  = 0x%x\n", Ana_Get_Reg(TOP_STATUS));
	pr_debug("TOP_CKPDN_CON0  = 0x%x\n", Ana_Get_Reg(TOP_CKPDN_CON0));
	pr_debug("TOP_CKPDN_CON1  = 0x%x\n", Ana_Get_Reg(TOP_CKPDN_CON1));
	pr_debug("TOP_CKPDN_CON2  = 0x%x\n", Ana_Get_Reg(TOP_CKPDN_CON2));
	pr_debug("TOP_CKSEL_CON  = 0x%x\n", Ana_Get_Reg(TOP_CKSEL_CON));
	pr_debug("TOP_CLKSQ  = 0x%x\n", Ana_Get_Reg(TOP_CLKSQ));
	pr_debug("ZCD_CON0  = 0x%x\n", Ana_Get_Reg(ZCD_CON0));
	pr_debug("ZCD_CON1  = 0x%x\n", Ana_Get_Reg(ZCD_CON1));
	pr_debug("ZCD_CON2  = 0x%x\n", Ana_Get_Reg(ZCD_CON2));
	pr_debug("ZCD_CON3  = 0x%x\n", Ana_Get_Reg(ZCD_CON3));
	pr_debug("ZCD_CON4  = 0x%x\n", Ana_Get_Reg(ZCD_CON4));
	pr_debug("ZCD_CON5  = 0x%x\n", Ana_Get_Reg(ZCD_CON5));

	pr_debug("AUDDAC_CFG0  = 0x%x\n", Ana_Get_Reg(AUDDAC_CFG0));
	pr_debug("AUDBUF_CFG0  = 0x%x\n", Ana_Get_Reg(AUDBUF_CFG0));
	pr_debug("AUDBUF_CFG1  = 0x%x\n", Ana_Get_Reg(AUDBUF_CFG1));
	pr_debug("AUDBUF_CFG2  = 0x%x\n", Ana_Get_Reg(AUDBUF_CFG2));
	pr_debug("AUDBUF_CFG3  = 0x%x\n", Ana_Get_Reg(AUDBUF_CFG3));
	pr_debug("AUDBUF_CFG4  = 0x%x\n", Ana_Get_Reg(AUDBUF_CFG4));
	pr_debug("AUDBUF_CFG5  = 0x%x\n", Ana_Get_Reg(AUDBUF_CFG5));
	pr_debug("AUDBUF_CFG6  = 0x%x\n", Ana_Get_Reg(AUDBUF_CFG6));
	pr_debug("AUDBUF_CFG7  = 0x%x\n", Ana_Get_Reg(AUDBUF_CFG7));
	pr_debug("AUDBUF_CFG8  = 0x%x\n", Ana_Get_Reg(AUDBUF_CFG8));
	pr_debug("IBIASDIST_CFG0  = 0x%x\n", Ana_Get_Reg(IBIASDIST_CFG0));
	pr_debug("AUDCLKGEN_CFG0  = 0x%x\n", Ana_Get_Reg(AUDCLKGEN_CFG0));
	pr_debug("AUDLDO_CFG0  = 0x%x\n", Ana_Get_Reg(AUDLDO_CFG0));
	pr_debug("AUDDCDC_CFG1  = 0x%x\n", Ana_Get_Reg(AUDDCDC_CFG1));
	pr_debug("AUDNVREGGLB_CFG0  = 0x%x\n", Ana_Get_Reg(AUDNVREGGLB_CFG0));
	pr_debug("AUD_NCP0  = 0x%x\n", Ana_Get_Reg(AUD_NCP0));
	pr_debug("AUD_ZCD_CFG0  = 0x%x\n", Ana_Get_Reg(AUD_ZCD_CFG0));
	pr_debug("AUDPREAMP_CFG0  = 0x%x\n", Ana_Get_Reg(AUDPREAMP_CFG0));
	pr_debug("AUDPREAMP_CFG1  = 0x%x\n", Ana_Get_Reg(AUDPREAMP_CFG1));
	pr_debug("AUDPREAMP_CFG2  = 0x%x\n", Ana_Get_Reg(AUDPREAMP_CFG2));
	pr_debug("AUDADC_CFG0  = 0x%x\n", Ana_Get_Reg(AUDADC_CFG0));
	pr_debug("AUDADC_CFG1  = 0x%x\n", Ana_Get_Reg(AUDADC_CFG1));
	pr_debug("AUDADC_CFG2  = 0x%x\n", Ana_Get_Reg(AUDADC_CFG2));
	pr_debug("AUDADC_CFG3  = 0x%x\n", Ana_Get_Reg(AUDADC_CFG3));
	pr_debug("AUDADC_CFG4  = 0x%x\n", Ana_Get_Reg(AUDADC_CFG4));
	pr_debug("AUDADC_CFG5  = 0x%x\n", Ana_Get_Reg(AUDADC_CFG5));

	pr_debug("AUDDIGMI_CFG0  = 0x%x\n", Ana_Get_Reg(AUDDIGMI_CFG0));
	pr_debug("AUDDIGMI_CFG1  = 0x%x\n", Ana_Get_Reg(AUDDIGMI_CFG1));
	pr_debug("AUDMICBIAS_CFG0  = 0x%x\n", Ana_Get_Reg(AUDMICBIAS_CFG0));
	pr_debug("AUDMICBIAS_CFG1  = 0x%x\n", Ana_Get_Reg(AUDMICBIAS_CFG1));
	pr_debug("AUDENCSPARE_CFG0  = 0x%x\n", Ana_Get_Reg(AUDENCSPARE_CFG0));
	pr_debug("AUDPREAMPGAIN_CFG0  = 0x%x\n", Ana_Get_Reg(AUDPREAMPGAIN_CFG0));
	pr_debug("AUDVOWPLL_CFG0  = 0x%x\n", Ana_Get_Reg(AUDVOWPLL_CFG0));
	pr_debug("AUDVOWPLL_CFG1  = 0x%x\n", Ana_Get_Reg(AUDVOWPLL_CFG1));
	pr_debug("AUDVOWPLL_CFG2  = 0x%x\n", Ana_Get_Reg(AUDVOWPLL_CFG2));
	pr_debug("AUDENCSPARE_CFG0  = 0x%x\n", Ana_Get_Reg(AUDENCSPARE_CFG0));

	pr_debug("AUDLDO_NVREG_CFG0  = 0x%x\n", Ana_Get_Reg(AUDLDO_NVREG_CFG0));
	pr_debug("AUDLDO_NVREG_CFG1  = 0x%x\n", Ana_Get_Reg(AUDLDO_NVREG_CFG1));
	pr_debug("AUDLDO_NVREG_CFG2  = 0x%x\n", Ana_Get_Reg(AUDLDO_NVREG_CFG2));

	pr_debug("ANALDO_CON3  = 0x%x\n", Ana_Get_Reg(ANALDO_CON3));

#ifdef CONFIG_MTK_SPEAKER
	pr_debug("TOP_CKPDN_CON0  = 0x%x\n", Ana_Get_Reg(SPK_TOP_CKPDN_CON0));
	pr_debug("TOP_CKPDN_CON1  = 0x%x\n", Ana_Get_Reg(SPK_TOP_CKPDN_CON1));
	pr_debug("VSBST_CON5  = 0x%x\n", Ana_Get_Reg(VSBST_CON5));
	pr_debug("VSBST_CON8  = 0x%x\n", Ana_Get_Reg(VSBST_CON8));
	pr_debug("VSBST_CON10  = 0x%x\n", Ana_Get_Reg(VSBST_CON10));
	pr_debug("VSBST_CON12  = 0x%x\n", Ana_Get_Reg(VSBST_CON12));
	pr_debug("VSBST_CON20  = 0x%x\n", Ana_Get_Reg(VSBST_CON20));

	pr_debug("SPK_CON0  = 0x%x\n", Ana_Get_Reg(SPK_CON0));
	pr_debug("SPK_CON2  = 0x%x\n", Ana_Get_Reg(SPK_CON2));
	pr_debug("SPK_CON9  = 0x%x\n", Ana_Get_Reg(SPK_CON9));
	pr_debug("SPK_CON12  = 0x%x\n", Ana_Get_Reg(SPK_CON12));
	pr_debug("SPK_CON13  = 0x%x\n", Ana_Get_Reg(SPK_CON13));
	pr_debug("SPK_CON14  = 0x%x\n", Ana_Get_Reg(SPK_CON14));
	pr_debug("SPK_CON16  = 0x%x\n", Ana_Get_Reg(SPK_CON16));
#endif
	pr_debug("MT6332_AUXADC_CON12  = 0x%x\n", Ana_Get_Reg(MT6332_AUXADC_CON12));
	pr_debug("MT6332_AUXADC_CON13  = 0x%x\n", Ana_Get_Reg(MT6332_AUXADC_CON13));
	pr_debug("MT6332_AUXADC_CON33  = 0x%x\n", Ana_Get_Reg(MT6332_AUXADC_CON33));
	pr_debug("MT6332_AUXADC_CON36  = 0x%x\n", Ana_Get_Reg(MT6332_AUXADC_CON36));
	AudDrv_ANA_Clk_Off();
	pr_debug("-Ana_Log_Print\n");
}
EXPORT_SYMBOL(Ana_Log_Print);


