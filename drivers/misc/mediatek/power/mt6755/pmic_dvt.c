#include <generated/autoconf.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/wakelock.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/aee.h>
#include <linux/xlog.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/earlysuspend.h>
#include <linux/seq_file.h>
#include <linux/time.h>

#include <asm/uaccess.h>

#include <mach/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>
#include <mach/mt_pm_ldo.h>
#include <mach/eint.h>
#include <mach/mt_pmic_wrap.h>
#include <mach/mt_gpio.h>
#include <mach/mtk_rtc.h>
#include <mach/mt_spm_mtcmos.h>

#include <mach/battery_common.h>
//#include <mach/pmic_mt6325_sw.h>
#include <mach/mt6311.h>

#include <cust_pmic.h>
#include <cust_battery_meter.h>

#include <linux/rtc.h>

#include <pmic_dvt.h>

#define pmic_xprintk(level,  format, args...) do { \
	if (1) {\
			printk(KERN_EMERG "[PMIC_DVT]%s %d: " format , \
				__func__, __LINE__ , ## args); \
		} \
	} while (0)

#ifdef PMICDBG
#undef PMICDBG
#endif
#define PMICDBG(level, fmt, args...) pmic_xprintk(level, fmt, ## args)


extern kal_uint32 PMIC_IMM_GetOneChannelValueMD(kal_uint8 dwChannel, int deCount, int trimd);
extern kal_uint32 upmu_get_reg_value(kal_uint32 reg);
extern void upmu_set_reg_value(kal_uint32 reg, kal_uint32 reg_val);

extern kal_uint16 mt6351_set_register_value(PMU_FLAGS_LIST_ENUM flagname,kal_uint32 val);
extern kal_uint16 mt6351_get_register_value(PMU_FLAGS_LIST_ENUM flagname);

#define BIF_DVT_ENABLE
//argus fix build error without battery
int Enable_BATDRV_LOG;

void setdummyloadSrclkenMode(kal_uint8 mode);
void setSrclkenMode(kal_uint8 mode);
extern const PMU_FLAG_TABLE_ENTRY pmu_flags_table[];
int pcap_15_8 = 0;
int pcap_7_0 = 0;
int pcreg_15_8 = 0;
int pcreg_7_0 = 0;
int nvmcap_15_8 = 0;
int nvmcap_7_0 = 0;
int nvmreg_15_8 = 0;
int nvmreg_7_0 = 0;
int sccap_15_8 = 0;
int sccap_7_0 = 0;
int screg_15_8 = 0;
int screg_7_0 = 0;

static unsigned int pmic_gray_code[128] = {
	0x0000000,
	0x0000001,
	0x0000011,
	0x0000010,
	0x0000110,
	0x0000111,
	0x0000101,
	0x0000100,
	0x0001100,
	0x0001101,
	0x0001111,
	0x0001110,
	0x0001010,
	0x0001011,
	0x0001001,
	0x0001000,
	0x0011000,
	0x0011001,
	0x0011011,
	0x0011010,
	0x0011110,
	0x0011111,
	0x0011101,
	0x0011100,
	0x0010100,
	0x0010101,
	0x0010111,
	0x0010110,
	0x0010010,
	0x0010011,
	0x0010001,
	0x0010000,
	0x0110000,
	0x0110001,
	0x0110011,
	0x0110010,
	0x0110110,
	0x0110111,
	0x0110101,
	0x0110100,
	0x0111100,
	0x0111101,
	0x0111111,
	0x0111110,
	0x0111010,
	0x0111011,
	0x0111001,
	0x0111000,
	0x0101000,
	0x0101001,
	0x0101011,
	0x0101010,
	0x0101110,
	0x0101111,
	0x0101101,
	0x0101100,
	0x0100100,
	0x0100101,
	0x0100111,
	0x0100110,
	0x0100010,
	0x0100011,
	0x0100001,
	0x0100000,
	0x1100000,
	0x1100001,
	0x1100011,
	0x1100010,
	0x1100110,
	0x1100111,
	0x1100101,
	0x1100100,
	0x1101100,
	0x1101101,
	0x1101111,
	0x1101110,
	0x1101010,
	0x1101011,
	0x1101001,
	0x1101000,
	0x1111000,
	0x1111001,
	0x1111011,
	0x1111010,
	0x1111110,
	0x1111111,
	0x1111101,
	0x1111100,
	0x1110100,
	0x1110101,
	0x1110111,
	0x1110110,
	0x1110010,
	0x1110011,
	0x1110001,
	0x1110000,
	0x1010000,
	0x1010001,
	0x1010011,
	0x1010010,
	0x1010110,
	0x1010111,
	0x1010101,
	0x1010100,
	0x1011100,
	0x1011101,
	0x1011111,
	0x1011110,
	0x1011010,
	0x1011011,
	0x1011001,
	0x1011000,
	0x1001000,
	0x1001001,
	0x1001011,
	0x1001010,
	0x1001110,
	0x1001111,
	0x1001101,
	0x1001100,
	0x1000100,
	0x1000101,
	0x1000111,
	0x1000110,
	0x1000010,
	0x1000011,
	0x1000001,
	0x1000000,
};

void pmic_auxadc1_basic111(void)
{

	int i=0;
	int ret;

	printk("[PMIC_DVT] [pmic_auxadc1_basic111]\n");
	for (i=0;i<16;i++)
	{
		printk("[PMIC_DVT] [%d]=%d\n",i,PMIC_IMM_GetOneChannelValue(i,0,0));
	}
/*
	printk("[PMIC_DVT] [md0]=%d\n",PMIC_IMM_GetOneChannelValueMD(0,0,0));
	printk("[PMIC_DVT] [md1]=%d\n",PMIC_IMM_GetOneChannelValueMD(1,0,0));
	printk("[PMIC_DVT] [md4]=%d\n",PMIC_IMM_GetOneChannelValueMD(4,0,0));
	printk("[PMIC_DVT] [md7]=%d\n",PMIC_IMM_GetOneChannelValueMD(7,0,0));
	printk("[PMIC_DVT] [md8]=%d\n",PMIC_IMM_GetOneChannelValueMD(8,0,0));
*/
}


#define AUXADC_EFUSE_GAIN 0xff
#define AUXADC_EFUSE_OFFSET 0xff

/*
//for ch7 15bit case , output is 15bit.
Y[14:0]= Y0[14:0]*(1+GE[11:0]/32768)+ OE[10:0]

//for other 12bit case , output is 12bit.
Y1[14:0]= {Y0[11:0],3'h0 }*(1+GE[11:0]/32768)+ OE[10:0]
Y[11:0]=Y1[14:3]

*/

kal_uint8 pmic_auxadc_trim_check(kal_uint32 trimdata,kal_uint32 rawdata,kal_uint8 type,kal_uint32 offset,kal_uint32 gain)
{
	kal_uint32 trimdata2,rawdata2,rawdata3,rawdata4;
	kal_uint32 rawdatabackup;

	if (type==0)// 15bit
	{
		trimdata2=rawdata+offset+(rawdata*gain/32768);
		 printk("[PMIC_DVT] [pmic_auxadc_trim_check][15bit] rawdata=%d trimdata=%d trimdata2=%d diff=%d offset=%d gain=%d\n", rawdata,trimdata,trimdata2,trimdata2-trimdata,offset,gain);
	}
	else
	{
		rawdata2=rawdata;
		rawdata3=rawdata2+offset+(rawdata2*gain/32768);
		rawdata4=rawdata3>>3;
		printk("[PMIC_DVT] [pmic_auxadc_trim_check][12bit] rawdata=%d:%d:%d:%d trimdata=%d diff=%d offset=%d gain=%d\n",
			rawdata,rawdata2,rawdata3,rawdata4,trimdata,rawdata4-trimdata,offset,gain);
	}

}



void pmic_auxadc2_trim111(int ch)
{
	int i=0,j=ch;
	kal_uint32 trimdata,rawdata;
	kal_uint32 gain0=0x60,offset0=0x60;
	kal_uint32 gain1=0xff,offset1=0xff;
	kal_uint32 gain2=0x3ff,offset2=0x3ff;
	kal_uint32 gain3=0xfff,offset3=0x7ff;

	printk("[PMIC_DVT] [pmic_auxadc2_trim111]\n");

	pmic_set_register_value(PMIC_EFUSE_GAIN_CH0_TRIM,gain0);
	pmic_set_register_value(PMIC_EFUSE_OFFSET_CH0_TRIM,offset0);

	pmic_set_register_value(PMIC_EFUSE_GAIN_CH4_TRIM,gain1);
	pmic_set_register_value(PMIC_EFUSE_OFFSET_CH4_TRIM,offset1);

	pmic_set_register_value(PMIC_EFUSE_GAIN_CH7_TRIM,gain2);
	pmic_set_register_value(PMIC_EFUSE_OFFSET_CH7_TRIM,offset2);

	pmic_set_register_value(PMIC_AUXADC_SW_GAIN_TRIM,gain3);
	pmic_set_register_value(PMIC_AUXADC_SW_OFFSET_TRIM,offset3);

	for (i=0;i<4;i++)
	{
		printk("[PMIC_DVT] [trim_sel]:%d\n",i);
		pmic_set_register_value(PMIC_AUXADC_TRIM_CH0_SEL,i);
		pmic_set_register_value(PMIC_AUXADC_TRIM_CH1_SEL,i);
		pmic_set_register_value(PMIC_AUXADC_TRIM_CH2_SEL,i);
		pmic_set_register_value(PMIC_AUXADC_TRIM_CH3_SEL,i);
		pmic_set_register_value(PMIC_AUXADC_TRIM_CH4_SEL,i);
		pmic_set_register_value(PMIC_AUXADC_TRIM_CH5_SEL,i);
		pmic_set_register_value(PMIC_AUXADC_TRIM_CH6_SEL,i);
		pmic_set_register_value(PMIC_AUXADC_TRIM_CH7_SEL,i);
		pmic_set_register_value(PMIC_AUXADC_TRIM_CH8_SEL,i);
		pmic_set_register_value(PMIC_AUXADC_TRIM_CH9_SEL,i);
		pmic_set_register_value(PMIC_AUXADC_TRIM_CH10_SEL,i);
		pmic_set_register_value(PMIC_AUXADC_TRIM_CH11_SEL,i);
		//for (j=0;j<11;j++)
		{

			trimdata=PMIC_IMM_GetOneChannelValue(j,0,0);
			rawdata=pmic_get_register_value(PMIC_AUXADC_ADC_OUT_RAW);

			if (j==0 || j==1 || j==7)
			{
				if(i==0)
				{
					pmic_auxadc_trim_check(trimdata,rawdata,0,offset0,gain0);
				}
				else if(i==1)
				{
					pmic_auxadc_trim_check(trimdata,rawdata,0,offset1,gain1);
				}
				else if(i==2)
				{
					pmic_auxadc_trim_check(trimdata,rawdata,0,offset2,gain2);
				}
				else if(i==3)
				{
					pmic_auxadc_trim_check(trimdata,rawdata,0,offset3,gain3);
				}
			}
			else
			{
				if(i==0)
				{
					pmic_auxadc_trim_check(trimdata,rawdata,1,offset0,gain0);
				}
				else if(i==1)
				{
					pmic_auxadc_trim_check(trimdata,rawdata,1,offset1,gain1);
				}
				else if(i==2)
				{
					pmic_auxadc_trim_check(trimdata,rawdata,1,offset2,gain2);
				}
				else if(i==3)
				{
					pmic_auxadc_trim_check(trimdata,rawdata,1,offset3,gain3);
				}
			}
		}
	}

}

void pmic_set_32k(void)
{
	pmic_set_register_value(PMIC_RG_AUXADC_CK_TSTSEL,1);
}


kal_int32 pmicno[20];
kal_int32 pmicval[20];
kal_int32 pmiciv[20];

void pmic_priority_check(kal_uint16 ap,kal_uint16 md)
{
	kal_uint32 i=0;

	kal_uint8 idx=1;



	for(i=0;i<20;i++)
	{
		pmicno[i]=0;
		pmicval[i]=0;
		pmiciv[i]=0;
	}

	i=0;

	do
	{
		if(pmic_get_register_value(PMIC_AUXADC_ADC_RDY_CH0_BY_AP) == 1 && pmicno[0]==0 && (ap&(1<<0))   )
		{
			pmicval[0]=pmic_get_register_value(PMIC_AUXADC_ADC_OUT_CH0_BY_AP);
			pmicno[0]=idx;
			pmiciv[0]=i;
			idx=idx+1;
		}

		if(pmic_get_register_value(PMIC_AUXADC_ADC_RDY_CH1_BY_AP) == 1&& pmicno[1]==0 && (ap&(1<<1))   )
		{
			pmicval[1]=pmic_get_register_value(PMIC_AUXADC_ADC_OUT_CH1_BY_AP);
			pmicno[1]=idx;
			pmiciv[1]=i;
			idx=idx+1;
		}
		if(pmic_get_register_value(PMIC_AUXADC_ADC_RDY_CH2) == 1&& pmicno[2]==0&& (ap&(1<<2))   )
		{
			pmicval[2]=pmic_get_register_value(PMIC_AUXADC_ADC_OUT_CH2);
			pmicno[2]=idx;
			pmiciv[2]=i;
			idx=idx+1;
		}

		if(pmic_get_register_value(PMIC_AUXADC_ADC_RDY_CH3) == 1&& pmicno[3]==0&& (ap&(1<<3))   )
		{
			pmicval[3]=pmic_get_register_value(PMIC_AUXADC_ADC_OUT_CH3);
			pmicno[3]=idx;
			pmiciv[3]=i;
			idx=idx+1;
		}

		if(pmic_get_register_value(PMIC_AUXADC_ADC_RDY_CH4) == 1&& pmicno[4]==0&& (ap&(1<<4))   )
		{
			pmicval[4]=pmic_get_register_value(PMIC_AUXADC_ADC_OUT_CH4);
			pmicno[4]=idx;
			pmiciv[4]=i;
			idx=idx+1;
		}

		if(pmic_get_register_value(PMIC_AUXADC_ADC_RDY_CH5) == 1&& pmicno[5]==0&& (ap&(1<<5))   )
		{
			pmicval[5]=pmic_get_register_value(PMIC_AUXADC_ADC_OUT_CH5);
			pmicno[5]=idx;
			pmiciv[5]=i;
			idx=idx+1;
		}

		if(pmic_get_register_value(PMIC_AUXADC_ADC_RDY_CH6) == 1&& pmicno[6]==0&& (ap&(1<<6))   )
		{
			pmicval[6]=pmic_get_register_value(PMIC_AUXADC_ADC_OUT_CH6);
			pmicno[6]=idx;
			pmiciv[6]=i;
			idx=idx+1;
		}

		if(pmic_get_register_value(PMIC_AUXADC_ADC_RDY_CH7_BY_AP) == 1&& pmicno[7]==0&& (ap&(1<<7))   )
		{
			pmicval[7]=pmic_get_register_value(PMIC_AUXADC_ADC_OUT_CH7_BY_AP);
			pmicno[7]=idx;
			pmiciv[7]=i;
			idx=idx+1;
		}

		if(pmic_get_register_value(PMIC_AUXADC_ADC_RDY_CH8) == 1&& pmicno[8]==0&& (ap&(1<<8))   )
		{
			pmicval[8]=pmic_get_register_value(PMIC_AUXADC_ADC_OUT_CH8);
			pmicno[8]=idx;
			pmiciv[8]=i;
			idx=idx+1;
		}

		if(pmic_get_register_value(PMIC_AUXADC_ADC_RDY_CH9) == 1&& pmicno[9]==0&& (ap&(1<<9))   )
		{
			pmicval[9]=pmic_get_register_value(PMIC_AUXADC_ADC_OUT_CH9);
			pmicno[9]=idx;
			pmiciv[9]=i;
			idx=idx+1;
		}

		if(pmic_get_register_value(PMIC_AUXADC_ADC_RDY_CH10) == 1&& pmicno[10]==0&& (ap&(1<<10))   )
		{
			pmicval[10]=pmic_get_register_value(PMIC_AUXADC_ADC_OUT_CH10);
			pmicno[10]=idx;
			pmiciv[10]=i;
			idx=idx+1;
		}

		if(pmic_get_register_value(PMIC_AUXADC_ADC_RDY_CH11) == 1&& pmicno[11]==0&& (ap&(1<<11))   )
		{
			pmicval[11]=pmic_get_register_value(PMIC_AUXADC_ADC_OUT_CH11);
			pmicno[11]=idx;
			pmiciv[11]=i;
			idx=idx+1;
		}

		if(pmic_get_register_value(PMIC_AUXADC_ADC_RDY_CH12_15) == 1&& pmicno[12]==0&& (ap&(1<<12))   )
		{
			pmicval[12]=pmic_get_register_value(PMIC_AUXADC_ADC_OUT_CH12_15);
			pmicno[12]=idx;
			pmiciv[12]=i;
			idx=idx+1;
		}

		if(pmic_get_register_value(PMIC_AUXADC_ADC_RDY_CH0_BY_MD) == 1&& pmicno[13]==0&& (md&(1<<0))   )
		{
			pmicval[13]=pmic_get_register_value(PMIC_AUXADC_ADC_OUT_CH0_BY_MD);
			pmicno[13]=idx;
			pmiciv[13]=i;
			idx=idx+1;
		}

		if(pmic_get_register_value(PMIC_AUXADC_ADC_RDY_CH1_BY_MD) == 1&& pmicno[14]==0&& (md&(1<<1))   )
		{
			pmicval[14]=pmic_get_register_value(PMIC_AUXADC_ADC_OUT_CH1_BY_MD);
			pmicno[14]=idx;
			pmiciv[14]=i;
			idx=idx+1;
		}

		if(pmic_get_register_value(PMIC_AUXADC_ADC_RDY_CH4_BY_MD) == 1&& pmicno[15]==0&& (md&(1<<4))   )
		{
			pmicval[15]=pmic_get_register_value(PMIC_AUXADC_ADC_OUT_CH4_BY_MD);
			pmicno[15]=idx;
			pmiciv[15]=i;
			idx=idx+1;
		}

		if(pmic_get_register_value(PMIC_AUXADC_ADC_RDY_CH7_BY_MD) == 1&& pmicno[16]==0&& (md&(1<<7))   )
		{
			pmicval[16]=pmic_get_register_value(PMIC_AUXADC_ADC_OUT_CH7_BY_MD);
			pmicno[16]=idx;
			pmiciv[16]=i;
			idx=idx+1;
		}

		if(pmic_get_register_value(PMIC_AUXADC_ADC_RDY_CH7_BY_GPS) == 1&& pmicno[17]==0&& (md&(1<<8))   )
		{
			pmicval[17]=pmic_get_register_value(PMIC_AUXADC_ADC_OUT_CH7_BY_GPS);
			pmicno[17]=idx;
			pmiciv[17]=i;
			idx=idx+1;
		}

		i++;
	}while(i<5000);

	printk("[PMIC_DVT] [ch00_AP]=%2d data=%d i=%d\n",pmicno[0],pmicval[0],pmiciv[0]);
	printk("[PMIC_DVT] [ch01_AP]=%2d data=%d i=%d\n",pmicno[1],pmicval[1],pmiciv[1]);
	printk("[PMIC_DVT] [ch02_AP]=%2d data=%d i=%d\n",pmicno[2],pmicval[2],pmiciv[2]);
	printk("[PMIC_DVT] [ch03_AP]=%2d data=%d i=%d\n",pmicno[3],pmicval[3],pmiciv[3]);
	printk("[PMIC_DVT] [ch04_AP]=%2d data=%d i=%d\n",pmicno[4],pmicval[4],pmiciv[4]);
	printk("[PMIC_DVT] [ch05_AP]=%2d data=%d i=%d\n",pmicno[5],pmicval[5],pmiciv[5]);
	printk("[PMIC_DVT] [ch06_AP]=%2d data=%d i=%d\n",pmicno[6],pmicval[6],pmiciv[6]);
	printk("[PMIC_DVT] [ch07_AP]=%2d data=%d i=%d\n",pmicno[7],pmicval[7],pmiciv[7]);
	printk("[PMIC_DVT] [ch08_AP]=%2d data=%d i=%d\n",pmicno[8],pmicval[8],pmiciv[8]);
	printk("[PMIC_DVT] [ch09_AP]=%2d data=%d i=%d\n",pmicno[9],pmicval[9],pmiciv[9]);
	printk("[PMIC_DVT] [ch10_AP]=%2d data=%d i=%d\n",pmicno[10],pmicval[10],pmiciv[10]);
	printk("[PMIC_DVT] [ch11_AP]=%2d data=%d i=%d\n",pmicno[11],pmicval[11],pmiciv[11]);
	printk("[PMIC_DVT] [ch12_AP]=%2d data=%d i=%d\n",pmicno[12],pmicval[12],pmiciv[12]);
	printk("[PMIC_DVT] [ch00_MD]=%2d data=%d i=%d\n",pmicno[13],pmicval[13],pmiciv[13]);
	printk("[PMIC_DVT] [ch01_MD]=%2d data=%d i=%d\n",pmicno[14],pmicval[14],pmiciv[14]);
	printk("[PMIC_DVT] [ch04_MD]=%2d data=%d i=%d\n",pmicno[15],pmicval[15],pmiciv[15]);
	printk("[PMIC_DVT] [ch07_MD]=%2d data=%d i=%d\n",pmicno[16],pmicval[16],pmiciv[16]);
	printk("[PMIC_DVT] [ch07_GPS]=%2d data=%d i=%d\n",pmicno[17],pmicval[17],pmiciv[17]);



}



void pmic_auxadc3_prio111(void)
{
	kal_int32 ret=0;
	pmic_set_32k();
	printk("[PMIC_DVT] [pmic_auxadc3_prio111] %d\n",upmu_get_reg_value(0xe18));
	ret=pmic_config_interface(PMIC_AUXADC_RQST0_SET,0xffff,0xffff,0);
	ret=pmic_config_interface(PMIC_AUXADC_RQST1_SET,0x193,0xffff,0);

	udelay(100);

	pmic_priority_check(0x1fff,0x193);

}

void pmic_auxadc3_prio112(void)
{
	kal_int32 ret=0;
	pmic_set_32k();
	printk("[PMIC_DVT] [pmic_auxadc3_prio112]\n");
	ret=pmic_config_interface(PMIC_AUXADC_RQST0_SET,0x7f,0xffff,0);
	udelay(1);
	ret=pmic_config_interface(PMIC_AUXADC_RQST1_SET,0x190,0xffff,0);

	udelay(100);

	pmic_priority_check(0x7f,0x190);

}

void pmic_auxadc3_prio113(void)
{
	kal_int32 ret=0;
	pmic_set_32k();
	printk("[PMIC_DVT] [pmic_auxadc3_prio113]\n");
	ret=pmic_config_interface(PMIC_AUXADC_RQST1_SET,0x100,0xffff,0);
	udelay(1);
	ret=pmic_config_interface(PMIC_AUXADC_RQST1_SET,0x0080,0xffff,0);

	udelay(100);

	pmic_priority_check(0x00,0x0180);

}

void pmic_auxadc3_prio114(void)
{
	kal_int32 ret=0;
	kal_uint32 i=0,j=0;
	kal_uint8 v1,v2;
	pmic_set_32k();
	printk("[PMIC_DVT] [pmic_auxadc3_prio114]\n");
	ret=pmic_config_interface(PMIC_AUXADC_RQST1_SET,0x100,0xffff,0);

	udelay(100);
	while(pmic_get_register_value(PMIC_AUXADC_ADC_RDY_CH7_BY_GPS) != 1 )
	{
		i++;
	}

	ret=pmic_config_interface(PMIC_AUXADC_RQST1_SET,0x80,0xffff,0);
	while(pmic_get_register_value(PMIC_AUXADC_ADC_RDY_CH7_BY_MD) != 1 )
	{
		j++;
	}

	v1= pmic_get_register_value(PMIC_AUXADC_ADC_OUT_CH7_BY_GPS);
	v2= pmic_get_register_value(PMIC_AUXADC_ADC_OUT_CH7_BY_MD);


	printk("[PMIC_DVT] [pmic_auxadc3_prio114]i=%d j=%d v1=%d v2;%d\n",i,j,v1,v2);


}

void pmic_auxadc3_prio115(void)
{
	kal_int32 ret=0;
	pmic_set_32k();
	printk("[PMIC_DVT] [pmic_auxadc3_prio115]\n");

	ret=pmic_config_interface(PMIC_AUXADC_RQST1_SET,0x180,0xffff,0);

	ret=pmic_config_interface(PMIC_AUXADC_RQST0_SET,0xffd,0xffff,0);
	udelay(1);
	ret=pmic_config_interface(PMIC_AUXADC_RQST0_SET,0x2,0xffff,0);

	udelay(100);
	pmic_priority_check(0xfff,0x180);
}

//LBAT1.1.1 : Test low battery voltage interrupt
void pmic_lbat111(void)
{
	int i=0;

	setSrclkenMode(1);
	printk("[PMIC_DVT] [LBAT1.1.1 : Test low battery voltage interrupt]\n");
	// 0:set interrupt
	pmic_set_register_value(PMIC_RG_INT_EN_BAT_L,1);

	// 1:setup max voltage threshold 4.2v
	//pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MAX,0x0c91);

	// 2.setup min voltage threshold 3.4v
	//pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MIN,0x018e);
	pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MIN,0x0aa0);

	// 3.setup detection period
	pmic_set_register_value(PMIC_AUXADC_LBAT_DET_PRD_19_16,0x0);
	pmic_set_register_value(PMIC_AUXADC_LBAT_DET_PRD_15_0,0x7d0);

	// 4.setup max/min debounce time
	//pmic_set_register_value(PMIC_AUXADC_LBAT_DEBT_MAX,0x3);
	pmic_set_register_value(PMIC_AUXADC_LBAT_DEBT_MIN,0x3);

	// 5.turn on IRQ
	//pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MAX,0x0);
	pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MIN,0x1);

	// 6. turn on LowBattery Detection
	//pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MAX,0);
	pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MIN,1);


	while(pmic_get_register_value(PMIC_RG_INT_STATUS_BAT_L)!=1)
	{
		msleep(1000);
		printk("[PMIC_DVT] debounce count:%d %d %d\n",pmic_get_register_value(PMIC_AUXADC_LBAT_DEBOUNCE_COUNT_MIN)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_RDY_LBAT)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_OUT_LBAT));
		i++;
		if (i>=50)
			break;
	}


	// 7. Monitor Debounce counts

	pmic_get_register_value(PMIC_AUXADC_LBAT_DEBOUNCE_COUNT_MIN);

	//ReadReg(AUXADC_LBAT_DEBOUNCE_COUNT_MAX);
	// ReadReg(BAUXADC_LBAT_DEBOUNCE_COUNT_MIN);
	// 8. Read LowBattery Detect Value
	//while(! ReadReg(BRW_AUXADC_ADC_RDY_LBAT));
	//ReadReg(BRW_AUXADC_ADC_OUT_LBAT_7_0);
	//ReadReg(BRW_AUXADC_ADC_OUT_LBAT_9_8);

	// 9. Test on VBAT = 3.5 -> 3.4 -> 3.3 and receive interrupt

	//10. Clear interrupt after receiving interrupt
	//pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MAX,0);
	pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MIN,0);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MAX,0);
	pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MIN,0);
	pmic_set_register_value(PMIC_RG_INT_EN_BAT_L,0);
	//pmic_set_register_value(PMIC_RG_INT_EN_BAT_H,0);


}



//LBAT1.1.2 : Test high battery voltage interrupt
void pmic_lbat112(void)
{
	int i=0;
	setSrclkenMode(1);
	printk("[PMIC_DVT] [LBAT1.1.2 : Test high battery voltage interrupt ]\n");
	// 0:set interrupt
	//pmic_set_register_value(PMIC_RG_INT_EN_BAT_L,1);
	pmic_set_register_value(PMIC_RG_INT_EN_BAT_H,1);

	// 1:setup max voltage threshold 4.2v
	//pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MAX,0x0c91);
	pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MAX,0x0a60);

	// 2.setup min voltage threshold 3.4v
	//pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MIN,0x018e);

	// 3.setup detection period
	pmic_set_register_value(PMIC_AUXADC_LBAT_DET_PRD_19_16,0x0);
	pmic_set_register_value(PMIC_AUXADC_LBAT_DET_PRD_15_0,0x7d0);

	// 4.setup max/min debounce time
	pmic_set_register_value(PMIC_AUXADC_LBAT_DEBT_MAX,0x3);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_DEBT_MIN,0x3);

	// 5.turn on IRQ
	pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MAX,0x1);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MIN,0x1);

	// 6. turn on LowBattery Detection
	pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MAX,1);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MIN,0);

	// 7. Monitor Debounce counts
	while(pmic_get_register_value(PMIC_RG_INT_STATUS_BAT_H)!=1)
	{
		msleep(1000);
		printk("[PMIC_DVT] debounce count:%d %d %d\n",pmic_get_register_value(PMIC_AUXADC_LBAT_DEBOUNCE_COUNT_MAX)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_RDY_LBAT)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_OUT_LBAT));

		i++;
		if (i>=50)
			break;
	}


	// 9. Test on VBAT = 3.5 -> 3.4 -> 3.3 and receive interrupt

	//10. Clear interrupt after receiving interrupt
	pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MAX,0);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MIN,0);
	pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MAX,0);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MIN,0);
	pmic_set_register_value(PMIC_RG_INT_EN_BAT_H,0);
	//pmic_set_register_value(PMIC_RG_INT_EN_BAT_L,0);


}



//LBAT1.1.3 : test low battery voltage interrupt  in sleep mode
void pmic_lbat113(void)
{
	int i=0;
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_STRUP_AUXADC_RSTB_SW,1);
	pmic_set_register_value(PMIC_STRUP_AUXADC_RSTB_SEL,1);

	printk("[PMIC_DVT] [LBAT1.1.3 : test low battery voltage interrupt  in sleep mode]\n");
	// 0:set interrupt
	pmic_set_register_value(PMIC_RG_INT_EN_BAT_L,1);

	// 1:setup max voltage threshold 4.2v
	//pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MAX,0x0c91);

	// 2.setup min voltage threshold 3.4v
	pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MIN,0x0aa0);

	// 3.setup detection period
	pmic_set_register_value(PMIC_AUXADC_LBAT_DET_PRD_19_16,0x0);
	pmic_set_register_value(PMIC_AUXADC_LBAT_DET_PRD_15_0,0x7d0);

	// 4.setup max/min debounce time
	//pmic_set_register_value(PMIC_AUXADC_LBAT_DEBT_MAX,0x3);
	pmic_set_register_value(PMIC_AUXADC_LBAT_DEBT_MIN,0x3);

	// 5.turn on IRQ
	//pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MAX,0x0);
	pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MIN,0x1);

	// 6. turn on LowBattery Detection
	//pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MAX,0);
	pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MIN,1);


	while(pmic_get_register_value(PMIC_RG_INT_STATUS_BAT_L)!=1)
	{
		msleep(1000);
		printk("[PMIC_DVT] debounce count:%d %d %d\n",pmic_get_register_value(PMIC_AUXADC_LBAT_DEBOUNCE_COUNT_MIN)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_RDY_LBAT)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_OUT_LBAT));
		i++;
		if (i>=50)
			break;

	}

	// 7. Monitor Debounce counts

	pmic_get_register_value(PMIC_AUXADC_LBAT_DEBOUNCE_COUNT_MIN);

	//ReadReg(AUXADC_LBAT_DEBOUNCE_COUNT_MAX);
	// ReadReg(BAUXADC_LBAT_DEBOUNCE_COUNT_MIN);
	// 8. Read LowBattery Detect Value
	//while(! ReadReg(BRW_AUXADC_ADC_RDY_LBAT));
	//ReadReg(BRW_AUXADC_ADC_OUT_LBAT_7_0);
	//ReadReg(BRW_AUXADC_ADC_OUT_LBAT_9_8);

	// 9. Test on VBAT = 3.5 -> 3.4 -> 3.3 and receive interrupt

	//10. Clear interrupt after receiving interrupt
	//pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MAX,0);
	pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MIN,0);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MAX,0);
	pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MIN,0);
	pmic_set_register_value(PMIC_RG_INT_EN_BAT_L,0);
	//pmic_set_register_value(PMIC_RG_INT_EN_BAT_H,0);

}

//LBAT1.1.4 : test high battery voltage interrupt in sleep mode
void pmic_lbat114(void)
{
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_STRUP_AUXADC_RSTB_SW,1);
	pmic_set_register_value(PMIC_STRUP_AUXADC_RSTB_SEL,1);

	printk("[PMIC_DVT] [LBAT1.1.4 : test high battery voltage interrupt in sleep mode ]\n");
	// 0:set interrupt
	//pmic_set_register_value(PMIC_RG_INT_EN_BAT_L,1);
	pmic_set_register_value(PMIC_RG_INT_EN_BAT_H,1);

	// 1:setup max voltage threshold 4.2v
	pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MAX,0x0a60);

	// 2.setup min voltage threshold 3.4v
	//pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MIN,0x018e);

	// 3.setup detection period
	pmic_set_register_value(PMIC_AUXADC_LBAT_DET_PRD_19_16,0x0);
	pmic_set_register_value(PMIC_AUXADC_LBAT_DET_PRD_15_0,0x7d0);

	// 4.setup max/min debounce time
	pmic_set_register_value(PMIC_AUXADC_LBAT_DEBT_MAX,0x3);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_DEBT_MIN,0x3);

	// 5.turn on IRQ
	pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MAX,0x1);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MIN,0x1);

	// 6. turn on LowBattery Detection
	pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MAX,1);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MIN,0);

	// 7. Monitor Debounce counts
	while(pmic_get_register_value(PMIC_RG_INT_STATUS_BAT_H)!=1)
	{
		msleep(1000);
		printk("[PMIC_DVT] debounce count:%d %d %d\n",pmic_get_register_value(PMIC_AUXADC_LBAT_DEBOUNCE_COUNT_MAX)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_RDY_LBAT)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_OUT_LBAT));
	}


	// 9. Test on VBAT = 3.5 -> 3.4 -> 3.3 and receive interrupt

	//10. Clear interrupt after receiving interrupt
	pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MAX,0);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MIN,0);
	pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MAX,0);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MIN,0);
	pmic_set_register_value(PMIC_RG_INT_EN_BAT_H,0);
	//pmic_set_register_value(PMIC_RG_INT_EN_BAT_L,0);

}

//LBAT2 1.1.1 : Test low battery voltage interrupt
void pmic_lbat2111(void)
{
	int i=0;

	setSrclkenMode(1);
	printk("[PMIC_DVT] [LBAT2 1.1.1 : Test low battery voltage interrupt]\n");
	// 0:set interrupt
	pmic_set_register_value(PMIC_RG_INT_EN_BAT_L,1);

	// 1:setup max voltage threshold 4.2v
	//pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MAX,0x0c91);

	// 2.setup min voltage threshold 3.4v
	pmic_set_register_value(PMIC_AUXADC_LBAT2_VOLT_MIN,0x0aa0);

	// 3.setup detection period
	pmic_set_register_value(PMIC_AUXADC_LBAT2_DET_PRD_19_16,0x0);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_DET_PRD_15_0,0x7d0);

	// 4.setup max/min debounce time
	//pmic_set_register_value(PMIC_AUXADC_LBAT_DEBT_MAX,0x3);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_DEBT_MIN,0x3);

	// 5.turn on IRQ
	//pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MAX,0x0);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_IRQ_EN_MIN,0x1);

	// 6. turn on LowBattery Detection
	//pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MAX,0);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_EN_MIN,1);


	while(pmic_get_register_value(PMIC_RG_INT_STATUS_BAT_L)!=1)
	{
		msleep(1000);
		printk("[PMIC_DVT] debounce count:%d %d %d\n",pmic_get_register_value(PMIC_AUXADC_LBAT2_DEBOUNCE_COUNT_MIN)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_RDY_LBAT2)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_OUT_LBAT2));
		i++;
		if (i>=50)
			break;

	}


	// 7. Monitor Debounce counts

	pmic_get_register_value(PMIC_AUXADC_LBAT2_DEBOUNCE_COUNT_MIN);

	//ReadReg(AUXADC_LBAT_DEBOUNCE_COUNT_MAX);
	// ReadReg(BAUXADC_LBAT_DEBOUNCE_COUNT_MIN);
	// 8. Read LowBattery Detect Value
	//while(! ReadReg(BRW_AUXADC_ADC_RDY_LBAT));
	//ReadReg(BRW_AUXADC_ADC_OUT_LBAT_7_0);
	//ReadReg(BRW_AUXADC_ADC_OUT_LBAT_9_8);

	// 9. Test on VBAT = 3.5 -> 3.4 -> 3.3 and receive interrupt

	//10. Clear interrupt after receiving interrupt
	//pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MAX,0);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_IRQ_EN_MIN,0);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MAX,0);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_EN_MIN,0);
	pmic_set_register_value(PMIC_RG_INT_EN_BAT_L,0);
	//pmic_set_register_value(PMIC_RG_INT_EN_BAT_H,0);


}



//LBAT1.1.2 : Test high battery voltage interrupt
void pmic_lbat2112(void)
{
	int i=0;

	setSrclkenMode(1);
	printk("[PMIC_DVT] [LBAT2 1.1.2 : Test high battery voltage interrupt ]\n");
	// 0:set interrupt
	//pmic_set_register_value(PMIC_RG_INT_EN_BAT_L,1);
	pmic_set_register_value(PMIC_RG_INT_EN_BAT_H,1);

	// 1:setup max voltage threshold 4.2v
	pmic_set_register_value(PMIC_AUXADC_LBAT2_VOLT_MAX,0x0a60);

	// 2.setup min voltage threshold 3.4v
	//pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MIN,0x018e);

	// 3.setup detection period
	pmic_set_register_value(PMIC_AUXADC_LBAT2_DET_PRD_19_16,0x0);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_DET_PRD_15_0,0x7d0);

	// 4.setup max/min debounce time
	pmic_set_register_value(PMIC_AUXADC_LBAT2_DEBT_MAX,0x3);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_DEBT_MIN,0x3);

	// 5.turn on IRQ
	pmic_set_register_value(PMIC_AUXADC_LBAT2_IRQ_EN_MAX,0x1);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MIN,0x1);

	// 6. turn on LowBattery Detection
	pmic_set_register_value(PMIC_AUXADC_LBAT2_EN_MAX,1);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MIN,0);

	// 7. Monitor Debounce counts
	while(pmic_get_register_value(PMIC_RG_INT_STATUS_BAT_H)!=1)
	{
		msleep(1000);
		printk("[PMIC_DVT] debounce count:%d %d %d\n",pmic_get_register_value(PMIC_AUXADC_LBAT2_DEBOUNCE_COUNT_MAX)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_RDY_LBAT2)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_OUT_LBAT2));

		i++;
		if (i>=50)
			break;
	}


	// 9. Test on VBAT = 3.5 -> 3.4 -> 3.3 and receive interrupt

	//10. Clear interrupt after receiving interrupt
	pmic_set_register_value(PMIC_AUXADC_LBAT2_IRQ_EN_MAX,0);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MIN,0);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_EN_MAX,0);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MIN,0);
	pmic_set_register_value(PMIC_RG_INT_EN_BAT_H,0);
	//pmic_set_register_value(PMIC_RG_INT_EN_BAT_L,0);



}

//LBAT1.1.3 : test low battery voltage interrupt  in sleep mode
void pmic_lbat2113(void)
{
	int i=0;
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_STRUP_AUXADC_RSTB_SW,1);
	pmic_set_register_value(PMIC_STRUP_AUXADC_RSTB_SEL,1);

	printk("[PMIC_DVT] [LBAT1.1.3 : test low battery voltage interrupt  in sleep mode]\n");
	// 0:set interrupt
	pmic_set_register_value(PMIC_RG_INT_EN_BAT_L,1);

	// 1:setup max voltage threshold 4.2v
	//pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MAX,0x0c91);

	// 2.setup min voltage threshold 3.4v
	pmic_set_register_value(PMIC_AUXADC_LBAT2_VOLT_MIN,0x0aa0);

	// 3.setup detection period
	pmic_set_register_value(PMIC_AUXADC_LBAT2_DET_PRD_19_16,0x0);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_DET_PRD_15_0,0x7d0);

	// 4.setup max/min debounce time
	//pmic_set_register_value(PMIC_AUXADC_LBAT_DEBT_MAX,0x3);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_DEBT_MIN,0x3);

	// 5.turn on IRQ
	//pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MAX,0x0);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_IRQ_EN_MIN,0x1);

	// 6. turn on LowBattery Detection
	//pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MAX,0);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_EN_MIN,1);


	while(pmic_get_register_value(PMIC_RG_INT_STATUS_BAT_L)!=1)
	{
		msleep(1000);
		printk("[PMIC_DVT] debounce count:%d %d %d\n",pmic_get_register_value(PMIC_AUXADC_LBAT2_DEBOUNCE_COUNT_MIN)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_RDY_LBAT2)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_OUT_LBAT2));
		i++;
		if (i>=50)
			break;

	}

	// 7. Monitor Debounce counts

	pmic_get_register_value(PMIC_AUXADC_LBAT2_DEBOUNCE_COUNT_MIN);

	//ReadReg(AUXADC_LBAT_DEBOUNCE_COUNT_MAX);
	// ReadReg(BAUXADC_LBAT_DEBOUNCE_COUNT_MIN);
	// 8. Read LowBattery Detect Value
	//while(! ReadReg(BRW_AUXADC_ADC_RDY_LBAT));
	//ReadReg(BRW_AUXADC_ADC_OUT_LBAT_7_0);
	//ReadReg(BRW_AUXADC_ADC_OUT_LBAT_9_8);

	// 9. Test on VBAT = 3.5 -> 3.4 -> 3.3 and receive interrupt

	//10. Clear interrupt after receiving interrupt
	//pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MAX,0);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_IRQ_EN_MIN,0);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MAX,0);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_EN_MIN,0);
	pmic_set_register_value(PMIC_RG_INT_EN_BAT_L,0);
	//pmic_set_register_value(PMIC_RG_INT_EN_BAT_H,0);

}

//LBAT2 1.1.4 : test high battery voltage interrupt in sleep mode
void pmic_lbat2114(void)
{

	int i=0;
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_STRUP_AUXADC_RSTB_SW,1);
	pmic_set_register_value(PMIC_STRUP_AUXADC_RSTB_SEL,1);

	printk("[PMIC_DVT] [LBAT2 1.1.4 : test high battery voltage interrupt in sleep mode ]\n");
	// 0:set interrupt
	//pmic_set_register_value(PMIC_RG_INT_EN_BAT_L,1);
	pmic_set_register_value(PMIC_RG_INT_EN_BAT_H,1);

	// 1:setup max voltage threshold 4.2v
	pmic_set_register_value(PMIC_AUXADC_LBAT2_VOLT_MAX,0x0a60);

	// 2.setup min voltage threshold 3.4v
	//pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MIN,0x018e);

	// 3.setup detection period
	pmic_set_register_value(PMIC_AUXADC_LBAT2_DET_PRD_19_16,0x0);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_DET_PRD_15_0,0x7d0);

	// 4.setup max/min debounce time
	pmic_set_register_value(PMIC_AUXADC_LBAT2_DEBT_MAX,0x3);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_DEBT_MIN,0x3);

	// 5.turn on IRQ
	pmic_set_register_value(PMIC_AUXADC_LBAT2_IRQ_EN_MAX,0x1);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MIN,0x1);

	// 6. turn on LowBattery Detection
	pmic_set_register_value(PMIC_AUXADC_LBAT2_EN_MAX,1);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MIN,0);

	// 7. Monitor Debounce counts
	while(pmic_get_register_value(PMIC_RG_INT_STATUS_BAT_H)!=1)
	{
		msleep(1000);
		printk("[PMIC_DVT] debounce count:%d %d %d\n",pmic_get_register_value(PMIC_AUXADC_LBAT2_DEBOUNCE_COUNT_MAX)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_RDY_LBAT2)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_OUT_LBAT2));

		i++;
		if (i>=50)
			break;
	}


	// 9. Test on VBAT = 3.5 -> 3.4 -> 3.3 and receive interrupt

	//10. Clear interrupt after receiving interrupt
	pmic_set_register_value(PMIC_AUXADC_LBAT2_IRQ_EN_MAX,0);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MIN,0);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_EN_MAX,0);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MIN,0);
	pmic_set_register_value(PMIC_RG_INT_EN_BAT_H,0);
	//pmic_set_register_value(PMIC_RG_INT_EN_BAT_L,0);

}

//THR1.1.1 : Thermal interrupt for over temperature
void pmic_thr111(void)
{
	printk("[PMIC_DVT] [THR1.1.1 : Thermal interrupt for over temperature]\n");
	// 0:set interrupt
	pmic_set_register_value(PMIC_RG_INT_EN_THR_L,1);

	// 1:setup max voltage threshold 4.2v
	//pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MAX,0x0c91);

	// 2.setup min voltage threshold 3.4v
	pmic_set_register_value(PMIC_AUXADC_THR_VOLT_MIN,0x018e);

	// 3.setup detection period
	pmic_set_register_value(PMIC_AUXADC_THR_DET_PRD_19_16,0x0);
	pmic_set_register_value(PMIC_AUXADC_THR_DET_PRD_15_0,0x00a);

	// 4.setup max/min debounce time
	//pmic_set_register_value(PMIC_AUXADC_LBAT_DEBT_MAX,0x3);
	pmic_set_register_value(PMIC_AUXADC_THR_DEBT_MIN,0x3);

	// 5.turn on IRQ
	//pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MAX,0x0);
	pmic_set_register_value(PMIC_AUXADC_THR_IRQ_EN_MIN,0x1);

	// 6. turn on LowBattery Detection
	//pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MAX,0);
	pmic_set_register_value(PMIC_AUXADC_THR_EN_MIN,1);


	while(pmic_get_register_value(PMIC_RG_INT_STATUS_THR_L)!=1)
	{
		msleep(1000);
		printk("[PMIC_DVT] debounce count:%d %d %d\n",pmic_get_register_value(PMIC_AUXADC_THR_DEBOUNCE_COUNT_MIN)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_RDY_THR_HW)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_OUT_THR_HW));
	}


	// 7. Monitor Debounce counts

	pmic_get_register_value(PMIC_AUXADC_THR_DEBOUNCE_COUNT_MIN);

	//ReadReg(AUXADC_LBAT_DEBOUNCE_COUNT_MAX);
	// ReadReg(BAUXADC_LBAT_DEBOUNCE_COUNT_MIN);
	// 8. Read LowBattery Detect Value
	//while(! ReadReg(BRW_AUXADC_ADC_RDY_LBAT));
	//ReadReg(BRW_AUXADC_ADC_OUT_LBAT_7_0);
	//ReadReg(BRW_AUXADC_ADC_OUT_LBAT_9_8);

	// 9. Test on VBAT = 3.5 -> 3.4 -> 3.3 and receive interrupt

	//10. Clear interrupt after receiving interrupt
	//pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MAX,0);
	pmic_set_register_value(PMIC_AUXADC_THR_IRQ_EN_MIN,0);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MAX,0);
	pmic_set_register_value(PMIC_AUXADC_THR_EN_MIN,0);
	pmic_set_register_value(PMIC_RG_INT_EN_THR_L,0);
	//pmic_set_register_value(PMIC_RG_INT_EN_BAT_H,0);


}



//THR1.1.2 : Thermal interrupt for under temperature
void pmic_thr112(void)
{
	printk("[PMIC_DVT] [THR1.1.2 : Thermal interrupt for under temperature ]\n");
	// 0:set interrupt
	//pmic_set_register_value(PMIC_RG_INT_EN_BAT_L,1);
	pmic_set_register_value(PMIC_RG_INT_EN_THR_H,1);

	// 1:setup max voltage threshold 4.2v
	pmic_set_register_value(PMIC_AUXADC_THR_VOLT_MAX,0x0c91);

	// 2.setup min voltage threshold 3.4v
	//pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MIN,0x018e);

	// 3.setup detection period
	pmic_set_register_value(PMIC_AUXADC_THR_DET_PRD_19_16,0x0);
	pmic_set_register_value(PMIC_AUXADC_THR_DET_PRD_15_0,0x00a);

	// 4.setup max/min debounce time
	pmic_set_register_value(PMIC_AUXADC_THR_DEBT_MAX,0x3);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_DEBT_MIN,0x3);

	// 5.turn on IRQ
	pmic_set_register_value(PMIC_AUXADC_THR_IRQ_EN_MAX,0x0);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MIN,0x1);

	// 6. turn on LowBattery Detection
	pmic_set_register_value(PMIC_AUXADC_THR_EN_MAX,1);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MIN,0);

	// 7. Monitor Debounce counts
	while(pmic_get_register_value(PMIC_RG_INT_STATUS_THR_H)!=1)
	{
		msleep(1000);
		printk("[PMIC_DVT] debounce count:%d %d %d\n",pmic_get_register_value(PMIC_AUXADC_THR_DEBOUNCE_COUNT_MIN)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_RDY_THR_HW)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_OUT_THR_HW));
	}


	// 9. Test on VBAT = 3.5 -> 3.4 -> 3.3 and receive interrupt

	//10. Clear interrupt after receiving interrupt
	pmic_set_register_value(PMIC_AUXADC_THR_IRQ_EN_MAX,0);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MIN,0);
	pmic_set_register_value(PMIC_AUXADC_THR_EN_MAX,0);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MIN,0);
	pmic_set_register_value(PMIC_RG_INT_EN_THR_H,0);
	//pmic_set_register_value(PMIC_RG_INT_EN_BAT_L,0);



}

//THR1.1.3 : Thermal interrupt  in sleep mode
void pmic_thr113(void)
{
	pmic_set_register_value(PMIC_STRUP_AUXADC_RSTB_SW,1);
	pmic_set_register_value(PMIC_STRUP_AUXADC_RSTB_SEL,1);

	printk("[PMIC_DVT] [THR1.1.3 : Thermal interrupt min  in sleep mode]\n");
	// 0:set interrupt
	pmic_set_register_value(PMIC_RG_INT_EN_THR_L,1);

	// 1:setup max voltage threshold 4.2v
	//pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MAX,0x0c91);

	// 2.setup min voltage threshold 3.4v
	pmic_set_register_value(PMIC_AUXADC_THR_VOLT_MIN,0x018e);

	// 3.setup detection period
	pmic_set_register_value(PMIC_AUXADC_THR_DET_PRD_19_16,0x0);
	pmic_set_register_value(PMIC_AUXADC_THR_DET_PRD_15_0,0x00a);

	// 4.setup max/min debounce time
	//pmic_set_register_value(PMIC_AUXADC_LBAT_DEBT_MAX,0x3);
	pmic_set_register_value(PMIC_AUXADC_THR_DEBT_MIN,0x3);

	// 5.turn on IRQ
	//pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MAX,0x0);
	pmic_set_register_value(PMIC_AUXADC_THR_IRQ_EN_MIN,0x1);

	// 6. turn on LowBattery Detection
	//pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MAX,0);
	pmic_set_register_value(PMIC_AUXADC_THR_EN_MIN,1);


	while(pmic_get_register_value(PMIC_RG_INT_STATUS_THR_L)!=1)
	{
		msleep(1000);
		printk("[PMIC_DVT] debounce count:%d %d %d\n",pmic_get_register_value(PMIC_AUXADC_THR_DEBOUNCE_COUNT_MIN)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_RDY_THR_HW)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_OUT_THR_HW));
	}

	// 7. Monitor Debounce counts

	pmic_get_register_value(PMIC_AUXADC_THR_DEBOUNCE_COUNT_MIN);

	//ReadReg(AUXADC_LBAT_DEBOUNCE_COUNT_MAX);
	// ReadReg(BAUXADC_LBAT_DEBOUNCE_COUNT_MIN);
	// 8. Read LowBattery Detect Value
	//while(! ReadReg(BRW_AUXADC_ADC_RDY_LBAT));
	//ReadReg(BRW_AUXADC_ADC_OUT_LBAT_7_0);
	//ReadReg(BRW_AUXADC_ADC_OUT_LBAT_9_8);

	// 9. Test on VBAT = 3.5 -> 3.4 -> 3.3 and receive interrupt

	//10. Clear interrupt after receiving interrupt
	//pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MAX,0);
	pmic_set_register_value(PMIC_AUXADC_THR_IRQ_EN_MIN,0);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MAX,0);
	pmic_set_register_value(PMIC_AUXADC_THR_EN_MIN,0);
	pmic_set_register_value(PMIC_RG_INT_EN_THR_L,0);
	//pmic_set_register_value(PMIC_RG_INT_EN_BAT_H,0);

}

//THR1.1.4 : Thermal interrupt in sleep mode
void pmic_thr114(void)
{
	pmic_set_register_value(PMIC_STRUP_AUXADC_RSTB_SW,1);
	pmic_set_register_value(PMIC_STRUP_AUXADC_RSTB_SEL,1);

	printk("[PMIC_DVT] [THR1.1.4 : Thermal interrupt in sleep mode ]\n");
	// 0:set interrupt
	//pmic_set_register_value(PMIC_RG_INT_EN_BAT_L,1);
	pmic_set_register_value(PMIC_RG_INT_EN_THR_H,1);

	// 1:setup max voltage threshold 4.2v
	pmic_set_register_value(PMIC_AUXADC_THR_VOLT_MAX,0x0c91);

	// 2.setup min voltage threshold 3.4v
	//pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MIN,0x018e);

	// 3.setup detection period
	pmic_set_register_value(PMIC_AUXADC_THR_DET_PRD_19_16,0x0);
	pmic_set_register_value(PMIC_AUXADC_THR_DET_PRD_15_0,0x00a);

	// 4.setup max/min debounce time
	pmic_set_register_value(PMIC_AUXADC_THR_DEBT_MAX,0x3);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_DEBT_MIN,0x3);

	// 5.turn on IRQ
	pmic_set_register_value(PMIC_AUXADC_THR_IRQ_EN_MAX,0x0);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MIN,0x1);

	// 6. turn on LowBattery Detection
	pmic_set_register_value(PMIC_AUXADC_THR_EN_MAX,1);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MIN,0);

	// 7. Monitor Debounce counts
	while(pmic_get_register_value(PMIC_RG_INT_STATUS_THR_H)!=1)
	{
		msleep(1000);
		printk("[PMIC_DVT] debounce count:%d %d %d\n",pmic_get_register_value(PMIC_AUXADC_THR_DEBOUNCE_COUNT_MIN)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_RDY_THR_HW)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_OUT_THR_HW));
	}


	// 9. Test on VBAT = 3.5 -> 3.4 -> 3.3 and receive interrupt

	//10. Clear interrupt after receiving interrupt
	pmic_set_register_value(PMIC_AUXADC_THR_IRQ_EN_MAX,0);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MIN,0);
	pmic_set_register_value(PMIC_AUXADC_THR_EN_MAX,0);
	//pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MIN,0);
	pmic_set_register_value(PMIC_RG_INT_EN_THR_H,0);
	//pmic_set_register_value(PMIC_RG_INT_EN_BAT_L,0);

}


//ACC1.1.1 accdet auto sampling
void pmic_acc111(void)
{
	kal_uint32 ret_data;
	kal_uint32 i=0,j=0;
	printk("[PMIC_DVT] [ACC1.1.1 accdet auto sampling]\n");
	pmic_set_register_value(PMIC_AUXADC_ACCDET_AUTO_SPL,1);

	while(i<20)
	{
		while(pmic_get_register_value(PMIC_AUXADC_ADC_RDY_CH5) != 1 );
		ret_data = pmic_get_register_value(PMIC_AUXADC_ADC_OUT_CH5);
		printk("[PMIC_DVT] [ACC1.1.1 accdet auto sampling]idx=%d data=%d \n",i,ret_data);
		i++;
		msleep(10);
	}

}

//IMP1.1.1 Impedance measurement for BATSNS
void pmic_IMP111(void)
{
	kal_uint32 ret_data;
	kal_uint32 i=0;
	//setSrclkenMode(1);
	printk("[PMIC_DVT] [IMP1.1.1 Impedance measurement for BATSNS]:%d \n",pmic_get_register_value(PMIC_RG_INT_STATUS_AUXADC_IMP));

	//clear IRQ
	pmic_set_register_value(PMIC_AUXADC_CLR_IMP_CNT_STOP,1);
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_IRQ_CLR,1);

	//restore to initial state
	pmic_set_register_value(PMIC_AUXADC_CLR_IMP_CNT_STOP,0);
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_IRQ_CLR,0);

	//set issue interrupt
	pmic_set_register_value(PMIC_RG_INT_EN_AUXADC_IMP,1);

	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_CHSEL,0);
	pmic_set_register_value(PMIC_AUXADC_IMP_AUTORPT_EN,1);
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_CNT,4);
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_MODE,1);

	while(pmic_get_register_value(PMIC_RG_INT_STATUS_AUXADC_IMP)!=1)
	{
		//polling IRQ
		printk("[PMIC_DVT] rdy bit:%d \n",pmic_get_register_value(PMIC_RG_INT_STATUS_AUXADC_IMP));

	}

	printk("[PMIC_DVT] rdy bit:%d i:%d\n",pmic_get_register_value(PMIC_RG_INT_STATUS_AUXADC_IMP),i);
/*
	//clear IRQ
	pmic_set_register_value(PMIC_AUXADC_CLR_IMP_CNT_STOP,1);
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_IRQ_CLR,1);

	//restore to initial state
	pmic_set_register_value(PMIC_AUXADC_CLR_IMP_CNT_STOP,0);
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_IRQ_CLR,0);

	//turn off interrupt
	pmic_set_register_value(PMIC_RG_INT_EN_AUXADC_IMP,0);
*/
}


//IMP1.1.2 Impedance measurement for ISENSE
void pmic_IMP112(void)
{
	kal_uint32 ret_data;
	kal_uint32 i=0;
	//setSrclkenMode(1);
	printk("[PMIC_DVT] [IMP1.1.2 Impedance measurement for ISENSE] %d \n",pmic_get_register_value(PMIC_RG_INT_STATUS_AUXADC_IMP));

	//clear IRQ
	pmic_set_register_value(PMIC_AUXADC_CLR_IMP_CNT_STOP,1);
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_IRQ_CLR,1);

	//restore to initial state
	pmic_set_register_value(PMIC_AUXADC_CLR_IMP_CNT_STOP,0);
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_IRQ_CLR,0);

	//set issue interrupt
	pmic_set_register_value(PMIC_RG_INT_EN_AUXADC_IMP,1);

	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_CHSEL,1);
	pmic_set_register_value(PMIC_AUXADC_IMP_AUTORPT_EN,1);
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_CNT,4);
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_MODE,1);

/*
	while(pmic_get_register_value(PMIC_RG_INT_STATUS_AUXADC_IMP)!=1 && i<15)
	{
		//polling IRQ
		msleep(1000);
		printk("[PMIC_DVT] rdy bit:%d \n",pmic_get_register_value(PMIC_RG_INT_STATUS_AUXADC_IMP));
		i++;

	}

	printk("[PMIC_DVT] rdy bit:%d i:%d\n",pmic_get_register_value(PMIC_RG_INT_STATUS_AUXADC_IMP),i);


	//clear IRQ
	pmic_set_register_value(PMIC_AUXADC_CLR_IMP_CNT_STOP,1);
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_IRQ_CLR,1);

	//restore to initial state
	pmic_set_register_value(PMIC_AUXADC_CLR_IMP_CNT_STOP,0);
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_IRQ_CLR,0);

	//turn off interrupt
	pmic_set_register_value(PMIC_RG_INT_EN_AUXADC_IMP,0);
*/
}

//CCK.1.1 Real-time & wakeup measurement
void pmic_cck11(void)
{
	kal_uint32 ret_data;
	kal_uint32 i=0;
	printk("[PMIC_DVT] [CCK.1.1 Real-time & wakeup measurement ]\n");

	pmic_set_register_value(PMIC_RG_BUCK_OSC_SEL_SW,0); // 0: power on; 1: power down smps aud clock
	pmic_set_register_value(PMIC_RG_BUCK_OSC_SEL_MODE,1); // SW mode
	mdelay(1);


	setSrclkenMode(1);

	//set issue interrupt
	pmic_set_register_value(PMIC_AUXADC_MDRT_DET_WKUP_EN,1);
	mdelay(1);


	//define period of measurement
	//pmic_set_register_value(PMIC_AUXADC_MDRT_DET_PRD,10);

	//setting of enabling measurement
	pmic_set_register_value(PMIC_AUXADC_MDRT_DET_EN,0);



	setSrclkenMode(0);

	setSrclkenMode(1);


	printk("[PMIC_DVT] [wake up]\n");
	while(i<20)
	{
		printk("[PMIC_DVT] i:%d rdy:%d out:%d\n",i,pmic_get_register_value(PMIC_AUXADC_ADC_RDY_MDRT)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_OUT_MDRT));
		udelay(1000);
		i++;

	}



	i=0;
// Check AUXADC_MDRT_DET function
           //define period of measurement
           pmic_set_register_value(PMIC_AUXADC_MDRT_DET_PRD,10); // 5ms, AUXADC_ADC_OUT_MDRT will update periodically based on this time

           //setting of enabling measurement
           pmic_set_register_value(PMIC_AUXADC_MDRT_DET_EN,1);


	printk("[PMIC_DVT] [real time]\n");
	while(i<20)
	{
		printk("[PMIC_DVT] i:%d rdy:%d out:%d\n",i,pmic_get_register_value(PMIC_AUXADC_ADC_RDY_MDRT)
						  		 ,pmic_get_register_value(PMIC_AUXADC_ADC_OUT_MDRT));
		udelay(1000);
		i++;

	}

}


void pmic_cck13(void)
{
                kal_uint32 ret_data;
                kal_uint32 i=0;
                printk("[PMIC_DVT] [CCK.1.3 Real-time & wakeup measurement ]\n");

                //pmic_set_register_value(PMIC_RG_BUCK_OSC_SEL_SW,0); // 0: power on; 1: power down smps aud clock
                //pmic_set_register_value(PMIC_RG_BUCK_OSC_SEL_MODE,1); // SW mode
                //mdelay(1);


                //setSrclkenMode(1);

                //set issue interrupt
                pmic_set_register_value(PMIC_AUXADC_MDRT_DET_WKUP_EN,1);
                mdelay(1);


                //define period of measurement
                //pmic_set_register_value(PMIC_AUXADC_MDRT_DET_PRD,10);

                //setting of enabling measurement
                //pmic_set_register_value(PMIC_AUXADC_MDRT_DET_EN,0);


                //setSrclkenMode(0);

                //setSrclkenMode(1);

                pmic_set_register_value(PMIC_AUXADC_MDRT_DET_WKUP_START_SEL,1); // SW mode
                mdelay(10);
                pmic_set_register_value(PMIC_AUXADC_MDRT_DET_WKUP_START,0);
                mdelay(10);
               pmic_set_register_value(PMIC_AUXADC_MDRT_DET_WKUP_START,1);
                mdelay(10);
                pmic_set_register_value(PMIC_AUXADC_MDRT_DET_WKUP_START,0);
                mdelay(10);


                printk("[PMIC_DVT] [wake up]\n");
                while(i<20)
                {
                                printk("[PMIC_DVT] i:%d rdy:%d out:%d\n",i,pmic_get_register_value(PMIC_AUXADC_ADC_RDY_MDRT)
                                                                                                                                ,pmic_get_register_value(PMIC_AUXADC_ADC_OUT_MDRT));
                                udelay(1000);
                                i++;

                }

}


void pmic_cck14(void)
{
                kal_uint32 ret_data;
                kal_uint32 i=0;
                printk("[PMIC_DVT] [CCK.1.4 Real-time & wakeup measurement ]\n");


                //set issue interrupt
                pmic_set_register_value(PMIC_AUXADC_MDRT_DET_WKUP_EN,1);

                pmic_set_register_value(PMIC_RG_OSC_SEL_HW_SRC_SEL,3);

                pmic_set_register_value(PMIC_RG_SRCLKEN_IN0_HW_MODE,0);
                pmic_set_register_value(PMIC_RG_SRCLKEN_IN0_EN,1);
                pmic_set_register_value(PMIC_RG_SRCLKEN_IN1_EN,1);

                pmic_set_register_value(PMIC_RG_SRCLKEN_IN0_HW_MODE,0);
                pmic_set_register_value(PMIC_RG_SRCLKEN_IN0_EN,0);
                pmic_set_register_value(PMIC_RG_SRCLKEN_IN1_EN,0);

               mdelay(2);

                pmic_set_register_value(PMIC_RG_SRCLKEN_IN0_HW_MODE,0);
                pmic_set_register_value(PMIC_RG_SRCLKEN_IN0_EN,1);
                pmic_set_register_value(PMIC_RG_SRCLKEN_IN1_EN,1);


                printk("[PMIC_DVT] [wake up]\n");
                while(i<20)
                {
                                printk("[PMIC_DVT] i:%d rdy:%d out:%d\n",i,pmic_get_register_value(PMIC_AUXADC_ADC_RDY_MDRT)
                                                                                                                                ,pmic_get_register_value(PMIC_AUXADC_ADC_OUT_MDRT));
                                udelay(1000);
                                i++;

                }

}


void pmic_cck15(void)
{
                kal_uint32 ret_data;
                kal_uint32 i=0;
                printk("[PMIC_DVT] [CCK.1.1 Real-time & wakeup measurement ]\n");


                //set issue interrupt
                pmic_set_register_value(PMIC_AUXADC_MDRT_DET_WKUP_EN,1);

                        // Allowing SPI R/W for Sleep mode (For DVT verification only)
                        pmic_set_register_value(PMIC_RG_SLP_RW_EN,1);
                        pmic_set_register_value(PMIC_RG_G_SMPS_AUD_CK_PDN_HWEN,0);
                        pmic_set_register_value(PMIC_RG_G_SMPS_AUD_CK_PDN,0);

                               pmic_set_register_value(PMIC_RG_OSC_SEL_HW_SRC_SEL,3);
                              pmic_set_register_value(PMIC_AUXADC_SRCLKEN_SRC_SEL,1); // Make AUXADC source SRCLKEN_IN1

                               pmic_set_register_value(PMIC_RG_SRCLKEN_IN1_HW_MODE,0);
                               pmic_set_register_value(PMIC_RG_SRCLKEN_IN1_EN,1);

                 pmic_set_register_value(PMIC_RG_SRCLKEN_IN1_HW_MODE,0);
                pmic_set_register_value(PMIC_RG_SRCLKEN_IN1_EN,0);

               mdelay(2);

                 pmic_set_register_value(PMIC_RG_SRCLKEN_IN1_HW_MODE,0);
                pmic_set_register_value(PMIC_RG_SRCLKEN_IN1_EN,1);


                printk("[PMIC_DVT] [wake up]\n");
                while(i<20)
                {
                                printk("[PMIC_DVT] i:%d rdy:%d out:%d\n",i,pmic_get_register_value(PMIC_AUXADC_ADC_RDY_MDRT)
                                                                                                                                ,pmic_get_register_value(PMIC_AUXADC_ADC_OUT_MDRT));
                                udelay(1000);
                                i++;

                }

}

void pmic_cck16(void)
{


                kal_uint32 ret_data;
                kal_uint32 i=0;
                printk("[PMIC_DVT] [CCK.1.6 Real-time & wakeup measurement ]\n");


               // set to avoid SRCLKEN toggling to reset auxadc in sleep mode
               pmic_set_register_value(PMIC_STRUP_AUXADC_START_SEL,1);
               pmic_set_register_value(PMIC_STRUP_AUXADC_RSTB_SW,1);
               pmic_set_register_value(PMIC_STRUP_AUXADC_RSTB_SEL,1);

               //set issue interrupt
               pmic_set_register_value(PMIC_AUXADC_MDRT_DET_WKUP_EN,1);

               setSrclkenMode(1);

               setSrclkenMode(0);

               setSrclkenMode(1);


               printk("[PMIC_DVT] [wake up]\n");
               while(i<20)
               {
                                printk("[PMIC_DVT] i:%d rdy:%d out:%d\n",i,pmic_get_register_value(PMIC_AUXADC_ADC_RDY_MDRT)
                                                                                                                                ,pmic_get_register_value(PMIC_AUXADC_ADC_OUT_MDRT));
                                udelay(1000);
                                i++;

               }
}







void pmic_scan2(void)
{
	printk("[PMIC_DVT] [scan]0x%x \n",upmu_get_reg_value(0xf0a));
       printk("[PMIC_DVT] dbg[%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_ADC_RDY_MDBG),
                    pmic_get_register_value(PMIC_AUXADC_ADC_OUT_MDBG));

	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_00),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_00));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_01),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_01));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_02),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_02));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_03),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_03));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_04),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_04));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_05),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_05));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_06),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_06));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_07),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_07));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_08),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_08));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_09),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_09));

	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_10),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_10));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_11),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_11));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_12),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_12));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_13),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_13));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_14),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_14));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_15),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_15));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_16),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_16));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_17),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_17));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_18),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_18));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_19),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_19));

	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_20),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_20));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_21),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_21));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_22),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_22));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_23),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_23));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_24),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_24));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_25),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_25));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_26),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_26));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_27),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_27));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_28),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_28));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_29),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_29));


	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_30),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_30));
	printk("[PMIC_DVT] [%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_BUF_RDY_31),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_31));

}


void pmic_scan(void)
{
	printk("[PMIC_DVT] [scan]0x%x \n",upmu_get_reg_value(0xf0a));
       printk("[PMIC_DVT] dbg[%d]::%d\n",pmic_get_register_value(PMIC_AUXADC_ADC_RDY_MDBG),
                    pmic_get_register_value(PMIC_AUXADC_ADC_OUT_MDBG));

	printk("[PMIC_DVT] [00]:%2d:%d  [01]:%2d:%d  [02]:%2d:%d  [03]:%2d:%d  [04]:%2d:%d  \n",
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_00),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_00),
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_01),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_01),
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_02),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_02),
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_03),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_03),
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_04),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_04));

	printk("[PMIC_DVT] [05]:%2d:%d  [06]:%2d:%d  [07]:%2d:%d  [08]:%2d:%d  [09]:%2d:%d  \n",
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_05),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_05),
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_06),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_06),
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_07),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_07),
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_08),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_08),
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_09),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_09));

	printk("[PMIC_DVT] [10]:%2d:%d  [11]:%2d:%d  [12]:%2d:%d  [13]:%2d:%d  [14]:%2d:%d  \n",
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_10),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_10),
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_11),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_11),
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_12),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_12),
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_13),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_13),
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_14),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_14));

	printk("[PMIC_DVT] [15]:%2d:%d  [16]:%2d:%d  [17]:%2d:%d  [18]:%2d:%d  [19]:%2d:%d  \n",
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_15),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_15),
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_16),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_16),
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_17),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_17),
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_18),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_18),
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_19),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_19));

	printk("[PMIC_DVT] [20]:%2d:%d  [21]:%2d:%d  [22]:%2d:%d  [23]:%2d:%d  [24]:%2d:%d  \n",
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_20),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_20),
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_21),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_21),
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_22),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_22),
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_23),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_23),
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_24),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_24));

	printk("[PMIC_DVT] [25]:%2d:%d  [26]:%2d:%d  [27]:%2d:%d  [28]:%2d:%d  [29]:%2d:%d  \n",
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_25),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_25),
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_26),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_26),
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_27),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_27),
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_28),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_28),
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_29),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_29));

	printk("[PMIC_DVT] [30]:%2d:%d  [31]:%2d:%d   \n",
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_30),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_30),
		pmic_get_register_value(PMIC_AUXADC_BUF_RDY_31),
		pmic_get_register_value(PMIC_AUXADC_BUF_OUT_31));






}



//CCK.1.2 Background measurement
void pmic_cck12(void)
{
	kal_uint32 ret_data;
	kal_uint32 i=0;
	printk("[PMIC_DVT] [CCK.1.2 Background measurement]\n");

	//set issue interrupt
	//pmic_set_register_value(PMIC_AUXADC_MDRT_DET_WKUP_EN,1);

	//define period of measurement
	//pmic_set_register_value(PMIC_AUXADC_MDRT_DET_PRD,0x1);
	pmic_set_register_value(PMIC_AUXADC_MDBG_DET_PRD,0x20);

	//setting of enabling measurement
	//pmic_set_register_value(PMIC_AUXADC_MDRT_DET_EN,1);
	pmic_set_register_value(PMIC_AUXADC_MDBG_DET_EN,1);

/*
	while(pmic_get_register_value(PMIC_AUXADC_ADC_RDY_MDRT)!=1)
	{
	}
*/

	for(i=0;i<100;i++)
	{
	      mdelay(16);
   	      pmic_scan();

	}

}



void pmic_efuse11(void)
{
	kal_uint32 i;
	printk("[PMIC_DVT] 1.1 Compare EFUSE bit #0 ~ 511 which are HW auto-loaded \n");

	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_0_15,pmic_get_register_value(PMIC_RG_OTP_DOUT_0_15));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_16_31,pmic_get_register_value(PMIC_RG_OTP_DOUT_16_31));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_32_47,pmic_get_register_value(PMIC_RG_OTP_DOUT_32_47));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_48_63,pmic_get_register_value(PMIC_RG_OTP_DOUT_48_63));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_64_79,pmic_get_register_value(PMIC_RG_OTP_DOUT_64_79));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_80_95,pmic_get_register_value(PMIC_RG_OTP_DOUT_80_95));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_96_111,pmic_get_register_value(PMIC_RG_OTP_DOUT_96_111));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_112_127,pmic_get_register_value(PMIC_RG_OTP_DOUT_112_127));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_128_143,pmic_get_register_value(PMIC_RG_OTP_DOUT_128_143));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_144_159,pmic_get_register_value(PMIC_RG_OTP_DOUT_144_159));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_160_175,pmic_get_register_value(PMIC_RG_OTP_DOUT_160_175));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_176_191,pmic_get_register_value(PMIC_RG_OTP_DOUT_176_191));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_192_207,pmic_get_register_value(PMIC_RG_OTP_DOUT_192_207));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_208_223,pmic_get_register_value(PMIC_RG_OTP_DOUT_208_223));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_224_239,pmic_get_register_value(PMIC_RG_OTP_DOUT_224_239));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_240_255,pmic_get_register_value(PMIC_RG_OTP_DOUT_240_255));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_256_271,pmic_get_register_value(PMIC_RG_OTP_DOUT_256_271));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_272_287,pmic_get_register_value(PMIC_RG_OTP_DOUT_272_287));

	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_288_303,pmic_get_register_value(PMIC_RG_OTP_VAL_288_303));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_304_319,pmic_get_register_value(PMIC_RG_OTP_VAL_304_319));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_320_335,pmic_get_register_value(PMIC_RG_OTP_VAL_320_335));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_336_351,pmic_get_register_value(PMIC_RG_OTP_VAL_336_351));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_352_367,pmic_get_register_value(PMIC_RG_OTP_VAL_352_367));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_368_383,pmic_get_register_value(PMIC_RG_OTP_VAL_368_383));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_384_399,pmic_get_register_value(PMIC_RG_OTP_VAL_384_399));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_400_415,pmic_get_register_value(PMIC_RG_OTP_VAL_400_415));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_416_431,pmic_get_register_value(PMIC_RG_OTP_VAL_416_431));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_432_447,pmic_get_register_value(PMIC_RG_OTP_VAL_432_447));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_448_463,pmic_get_register_value(PMIC_RG_OTP_VAL_448_463));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_464_479,pmic_get_register_value(PMIC_RG_OTP_VAL_464_479));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_480_495,pmic_get_register_value(PMIC_RG_OTP_VAL_480_495));
	printk("[PMIC_DVT] 0x%x =0x%x\n",MT6351_OTP_DOUT_496_511,pmic_get_register_value(PMIC_RG_OTP_VAL_496_511));


}


void pmic_efuse21(void)
{
	kal_uint32 i,j;
	kal_uint32 val;
	printk("[PMIC_DVT] 2.1 Compare EFUSE bit #512 ~ 1023 which are SW triggered read  \n");
	pmic_set_register_value(PMIC_RG_EFUSE_CK_PDN_HWEN,0);
	pmic_set_register_value(PMIC_RG_EFUSE_CK_PDN,0);
	pmic_set_register_value(PMIC_RG_OTP_RD_SW,1);

	for (i=0;i<0x1f;i++)
	{
		pmic_set_register_value(PMIC_RG_OTP_PA,i*2);

		if(pmic_get_register_value(PMIC_RG_OTP_RD_TRIG)==0)
		{
			pmic_set_register_value(PMIC_RG_OTP_RD_TRIG,1);
		}
		else
		{
			pmic_set_register_value(PMIC_RG_OTP_RD_TRIG,0);
		}

		while(pmic_get_register_value(PMIC_RG_OTP_RD_BUSY)==1)
		{
		}

		//printk("[PMIC_DVT] [%d -%d] RG_OTP_PA:0x%x =0x%x\n",512+16*i,527+16*i,i*2,pmic_get_register_value(PMIC_RG_OTP_DOUT_SW));
		//printk("[PMIC_DVT] [%d] PMIC_RG_OTP_DOUT_SW=0x%x\n",i,pmic_get_register_value(PMIC_RG_OTP_DOUT_SW));

		val=pmic_get_register_value(PMIC_RG_OTP_DOUT_SW);

		for (j=0;j<16;j++)
		{
			kal_uint16 x;
			if(val & (1<<j))
			{
				x=1;
			}
			else
			{
				x=0;
			}

			printk("[PMIC_DVT] [%d] =%d\n",512+16*i+j,x);
		}







	}

	pmic_set_register_value(PMIC_RG_EFUSE_CK_PDN,1);
	pmic_set_register_value(PMIC_RG_EFUSE_CK_PDN_HWEN,1);
}



void pmic_ldo_vio18(void)
{
	int x,y;
	upmu_set_reg_value(0x000e, 0x7ff3);
	upmu_set_reg_value(0x000a, 0x8000);
	upmu_set_reg_value(0x0a80, 0x0000);

	printk("[PMIC_DVT] VIO18 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VIO18_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VIO18_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VIO18_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VIO18_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VIO18_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VIO18_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VIO18_EN)==1)
		{
			printk("[PMIC_DVT] VIO18 pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);



	printk("[PMIC_DVT] VIO18 Ldo on/off control HW mode\n");
	pmic_set_register_value(PMIC_RG_VIO18_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VIO18_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VIO18_EN)==1)
		{
			printk("[PMIC_DVT] VIO18 pass\n");
		}
	}


}



void pmic_ldo1_dummy_load_control(void)
{
	int i,x,y;

	printk("[PMIC_DVT] VA18 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VEMC_DUMMY_LOAD_CTRL,0);
	pmic_set_register_value(PMIC_RG_VEMC_DUMMY_LOAD_EN,0);
	mdelay(50);
	for (i = 0;i < 5;i++)
	{
		pmic_set_register_value(PMIC_RG_VEMC_DUMMY_LOAD, i);
		printk("[PMIC_DVT] dummy load value:%x\n",pmic_get_register_value(PMIC_DA_QI_VEMC_DUMMY_LOAD));
	}
	printk("[PMIC_DVT] VEMC Ldo dummy load control hw mode\n");
	pmic_set_register_value(PMIC_RG_VEMC_DUMMY_LOAD_CTRL,1);
	pmic_set_register_value(PMIC_RG_VEMC_DUMMY_LOAD_EN,1);

	setdummyloadSrclkenMode(0);
	for (i = 0;i < 5;i++)
	{
		pmic_set_register_value(PMIC_RG_VEMC_DUMMY_LOAD, i);
		printk("[PMIC_DVT] w/o srclken dummy load value:%x\n",pmic_get_register_value(PMIC_DA_QI_VEMC_DUMMY_LOAD));
	}
	setdummyloadSrclkenMode(1);
	for (i = 0;i < 5;i++)
	{
		pmic_set_register_value(PMIC_RG_VEMC_DUMMY_LOAD, i);
		printk("[PMIC_DVT] with srclken dummy load value:%x\n",pmic_get_register_value(PMIC_DA_QI_VEMC_DUMMY_LOAD));
	}

}

void pmic_ldo1_sw_on_control(void)
{
	int x,y;
	printk("[PMIC_DVT] 1 Ldo on/off control sw mode \n");

	printk("[PMIC_DVT] VA18 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VA18_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VA18_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VA18_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VA18_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VA18_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VA18_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VA18_EN)==1)
		{
			printk("[PMIC_DVT] VA18 pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);


	printk("[PMIC_DVT] VTCXO24 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VTCXO24_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VTCXO24_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VTCXO24_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VTCXO24_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VTCXO24_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VTCXO24_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VTCXO24_EN)==1)
		{
			printk("[PMIC_DVT] VTCXO24 pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);


	printk("[PMIC_DVT] VTCXO28 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VTCXO28_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VTCXO28_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VTCXO28_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VTCXO28_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VTCXO28_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VTCXO28_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VTCXO28_EN)==1)
		{
			printk("[PMIC_DVT] VTCXO28 pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VCN28 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VCN28_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VCN28_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VCN28_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VCN28_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VCN28_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VCN28_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VCN28_EN)==1)
		{
			printk("[PMIC_DVT] VCN28 pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VCAMA Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VCAMA_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VCAMA_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VCAMA_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VCAMA_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VCAMA_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VCAMA_EN)==1)
		{
			printk("[PMIC_DVT] VCAMA pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VUSB33 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VUSB33_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VUSB33_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VUSB33_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VUSB33_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VUSB33_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VUSB33_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VUSB33_EN)==1)
		{
			printk("[PMIC_DVT] VUSB33 pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VSIM1 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VSIM1_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VSIM1_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VSIM1_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VSIM1_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VSIM1_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VSIM1_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VSIM1_EN)==1)
		{
			printk("[PMIC_DVT] VSIM1 pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);


	printk("[PMIC_DVT] VSIM2 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VSIM2_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VSIM2_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VSIM2_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VSIM2_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VSIM2_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VSIM2_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VSIM2_EN)==1)
		{
			printk("[PMIC_DVT] VSIM2 pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VEMC Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VEMC_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VEMC_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VEMC_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VEMC_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VEMC_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VEMC_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VEMC_EN)==1)
		{
			printk("[PMIC_DVT] VEMC pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VMCH Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VMCH_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VMCH_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VMCH_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VMCH_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VMCH_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VMCH_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VMCH_EN)==1)
		{
			printk("[PMIC_DVT] VMCH pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VIO28 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VIO28_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VIO28_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VIO28_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VIO28_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VIO28_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VIO28_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VIO28_EN)==1)
		{
			printk("[PMIC_DVT] VIO28 pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VIBR Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VIBR_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VIBR_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VIBR_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VIBR_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VIBR_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VIBR_EN)==1)
		{
			printk("[PMIC_DVT] VIBR pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VCAMD Ldo on/off control sw mode\n");
//ARGUS 	pmic_set_register_value(PMIC_RG_VCAMD_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VCAMD_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VCAMD_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VCAMD_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VCAMD_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VCAMD_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VCAMD_EN)==1)
		{
			printk("[PMIC_DVT] VCAMD pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);


	printk("[PMIC_DVT] VRF18 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VRF18_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VRF18_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VRF18_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VRF18_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VRF18_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VRF18_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VRF18_EN)==1)
		{
			printk("[PMIC_DVT] VRF18 pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VIO18 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VIO18_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VIO18_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VIO18_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VIO18_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VIO18_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VIO18_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VIO18_EN)==1)
		{
			printk("[PMIC_DVT] VIO18 pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VCN18 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VCN18_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VCN18_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VCN18_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VCN18_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VCN18_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VCN18_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VCN18_EN)==1)
		{
			printk("[PMIC_DVT] VCN18 pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VCAMIO Ldo on/off control sw mode\n");
//ARGUS	pmic_set_register_value(PMIC_RG_VCAMIO_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VCAMIO_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VCAMIO_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VCAMIO_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VCAMIO_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VCAMIO_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VCAMIO_EN)==1)
		{
			printk("[PMIC_DVT] VCAMIO pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VSRAM Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VSRAM_PROC_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VSRAM_PROC_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VSRAM_PROC_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VSRAM_PROC_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VSRAM_PROC_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VSRAM_PROC_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VSRAM_PROC_EN)==1)
		{
			printk("[PMIC_DVT] VSRAM_PROC pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VXO22 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VXO22_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VXO22_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VXO22_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VXO22_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VXO22_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VXO22_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VXO22_EN)==1)
		{
			printk("[PMIC_DVT] VXO22 pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VRF12 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VRF12_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VRF12_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VRF12_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VRF12_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VRF12_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VRF12_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VRF12_EN)==1)
		{
			printk("[PMIC_DVT] VRF12 pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VA10 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VA10_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VA10_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VA10_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VA10_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VA10_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VA10_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VA10_EN)==1)
		{
			printk("[PMIC_DVT] VA10 pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VDRAM Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VDRAM_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VDRAM_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VDRAM_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VDRAM_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VDRAM_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VDRAM_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VDRAM_EN)==1)
		{
			printk("[PMIC_DVT] VDRAM pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VMIPI Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VMIPI_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VMIPI_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VMIPI_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VMIPI_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VMIPI_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VMIPI_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VMIPI_EN)==1)
		{
			printk("[PMIC_DVT] VMIPI pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VGP3 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VGP3_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VGP3_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VGP3_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VGP3_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VGP3_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VGP3_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VGP3_EN)==1)
		{
			printk("[PMIC_DVT] VGP3 pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VBIF28 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VBIF28_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VBIF28_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VBIF28_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VBIF28_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VBIF28_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VBIF28_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VBIF28_EN)==1)
		{
			printk("[PMIC_DVT] VBIF28 pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VEFUSE Ldo on/off control sw mode\n");
//ARGUS	pmic_set_register_value(PMIC_RG_VEFUSE_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VEFUSE_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VEFUSE_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VEFUSE_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VEFUSE_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VEFUSE_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VEFUSE_EN)==1)
		{
			printk("[PMIC_DVT] VEFUSE pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VCN33 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VCN33_ON_CTRL_BT,0);
	pmic_set_register_value(PMIC_RG_VCN33_EN_BT,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VCN33_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VCN33_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VCN33_EN_BT,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VCN33_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VCN33_EN)==1)
		{
			printk("[PMIC_DVT] VCN33 pass\n");
		}
	}
	pmic_set_register_value(PMIC_RG_VCN33_EN_BT,0);
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VCN33 WIFI Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VCN33_ON_CTRL_WIFI,0);
	pmic_set_register_value(PMIC_RG_VCN33_EN_WIFI,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VCN33_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VCN33_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VCN33_EN_WIFI,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VCN33_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VCN33_EN)==1)
		{
			printk("[PMIC_DVT] VCN33 WIFI pass\n");
		}
	}
	pmic_set_register_value(PMIC_RG_VCN33_EN_WIFI,0);
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VLDO28 USER 0Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VLDO28_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VLDO28_EN_0,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VLDO28_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VLDO28_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VLDO28_EN_0,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VLDO28_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VLDO28_EN)==1)
		{
			printk("[PMIC_DVT] VLDO28 USER 0 pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VLDO28 USER 1Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VLDO28_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VLDO28_EN_1,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VLDO28_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VLDO28_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VLDO28_EN_1,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VLDO28_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VLDO28_EN)==1)
		{
			printk("[PMIC_DVT] VLDO28 USER 1 pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VMC Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VMC_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VMC_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VMC_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VMC_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VMC_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VMC_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VMC_EN)==1)
		{
			printk("[PMIC_DVT] VMC pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

#if defined MT6328
	//VTREF
	printk("[PMIC_DVT] VTREF Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_TREF_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_TREF_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_TREF_EN);
	if(pmic_get_register_value(PMIC_DA_QI_TREF_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_TREF_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_TREF_EN);
		if(pmic_get_register_value(PMIC_DA_QI_TREF_EN)==1)
		{
			printk("[PMIC_DVT] TREF pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VCAMAF Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VCAMAF_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VCAMAF_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VCAMAF_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VCAMAF_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VCAMAF_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VCAMAF_EN)==1)
		{
			printk("[PMIC_DVT] VCAMAF pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VGP3 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VGP3_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VGP3_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VGP3_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VGP3_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VGP3_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VGP3_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VGP3_EN)==1)
		{
			printk("[PMIC_DVT] VGP3 pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);

	printk("[PMIC_DVT] VM Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VM_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VM_EN,0);
	mdelay(50);
	x=pmic_get_register_value(PMIC_DA_QI_VM_EN);
	if(pmic_get_register_value(PMIC_DA_QI_VM_EN)==0)
	{
		pmic_set_register_value(PMIC_RG_VM_EN,1);
		mdelay(50);
		y=pmic_get_register_value(PMIC_DA_QI_VM_EN);
		if(pmic_get_register_value(PMIC_DA_QI_VM_EN)==1)
		{
			printk("[PMIC_DVT] VM pass\n");
		}
	}
	printk("[PMIC_DVT] disable:%d enable:%d\n",x,y);
#endif /*End of MT6328 */

}

void setdummyloadSrclkenMode(kal_uint8 mode)
{
	//Switch to SW mode
	pmic_set_register_value(PMIC_RG_SRCLKEN_IN0_HW_MODE,0);

	// Allowing SPI R/W for Sleep mode (For DVT verification only)
	pmic_set_register_value(PMIC_RG_SLP_RW_EN,1);
	pmic_set_register_value(PMIC_RG_G_SMPS_AUD_CK_PDN_HWEN,0);
	pmic_set_register_value(PMIC_RG_G_SMPS_AUD_CK_PDN,0);


	pmic_set_register_value(PMIC_RG_SRCLKEN_IN0_EN,mode);
	pmic_set_register_value(PMIC_RG_SRCLKEN_IN1_EN,mode);
	pmic_set_register_value(PMIC_RG_VEMC_DUMMY_LOAD_SRCLKEN_SEL,mode);
	mdelay(50);
}


void setSrclkenMode(kal_uint8 mode)
{
	//Switch to SW mode
	pmic_set_register_value(PMIC_RG_SRCLKEN_IN0_HW_MODE,0);

	// Allowing SPI R/W for Sleep mode (For DVT verification only)
	pmic_set_register_value(PMIC_RG_SLP_RW_EN,1);
	pmic_set_register_value(PMIC_RG_G_SMPS_AUD_CK_PDN_HWEN,0);
	pmic_set_register_value(PMIC_RG_G_SMPS_AUD_CK_PDN,0);


	pmic_set_register_value(PMIC_RG_SRCLKEN_IN0_EN,mode);
	pmic_set_register_value(PMIC_RG_SRCLKEN_IN1_EN,mode);
	mdelay(50);
}

void pmic_vtcxo_1(void)
{
	int x,y;

	printk("[PMIC_DVT] VTCXO 1 Ldo on/off control sw mode\n");

	printk("[PMIC_DVT] [0x%x]=0x%x\n",0xa02,upmu_get_reg_value(0xa02));
	printk("[PMIC_DVT] [0x%x]=0x%x\n",0xa02,upmu_get_reg_value(0xa02));
	printk("[PMIC_DVT] [0x%x]=0x%x\n",0xa02,upmu_get_reg_value(0xa02));


	pmic_set_register_value(PMIC_RG_VTCXO28_ON_CTRL,1);

	printk("[PMIC_DVT] [0x%x]=0x%x\n",0xa02,upmu_get_reg_value(0xa02));
	printk("[PMIC_DVT] [0x%x]=0x%x\n",0xa02,upmu_get_reg_value(0xa02));
	printk("[PMIC_DVT] [0x%x]=0x%x\n",0xa02,upmu_get_reg_value(0xa02));

	pmic_set_register_value(PMIC_RG_VTCXO28_SRCLK_EN_SEL,0);

	printk("[PMIC_DVT] [0x%x]=0x%x\n",0xa02,upmu_get_reg_value(0xa02));
	printk("[PMIC_DVT] [0x%x]=0x%x\n",0xa02,upmu_get_reg_value(0xa02));
	printk("[PMIC_DVT] [0x%x]=0x%x\n",0xa02,upmu_get_reg_value(0xa02));

	setSrclkenMode(0);

	printk("[PMIC_DVT] [0x%x]=0x%x\n",0xa02,upmu_get_reg_value(0xa02));
	printk("[PMIC_DVT] [0x%x]=0x%x\n",0xa02,upmu_get_reg_value(0xa02));
	printk("[PMIC_DVT] [0x%x]=0x%x\n",0xa02,upmu_get_reg_value(0xa02));

	x=pmic_get_register_value(PMIC_DA_QI_VTCXO28_EN);

	printk("[PMIC_DVT] [0x%x]=0x%x\n",0x204,upmu_get_reg_value(0x204));
	printk("[PMIC_DVT] [0x%x]=0x%x\n",0x2d2,upmu_get_reg_value(0x2d2));
	printk("[PMIC_DVT] [0x%x]=0x%x\n",0x278,upmu_get_reg_value(0x278));
	printk("[PMIC_DVT] [0x%x]=0x%x\n",0x23c,upmu_get_reg_value(0x23c));
	printk("[PMIC_DVT] [0x%x]=0x%x\n",0xa02,upmu_get_reg_value(0xa02));

	printk("[PMIC_DVT] [0x%x]=0x%x\n",0xa02,upmu_get_reg_value(0xa02));
	printk("[PMIC_DVT] [0x%x]=0x%x\n",0xa02,upmu_get_reg_value(0xa02));
	printk("[PMIC_DVT] [0x%x]=0x%x\n",0xa02,upmu_get_reg_value(0xa02));

	if(pmic_get_register_value(PMIC_DA_QI_VTCXO28_EN)==0)
	{
		printk("[PMIC_DVT] VTCXO 1: 0 pass\n");
	}
	else
	{
		printk("[PMIC_DVT] VTCXO 1: 0 fail\n");
	}

	setSrclkenMode(1);
	y=pmic_get_register_value(PMIC_DA_QI_VTCXO28_EN);

	printk("[PMIC_DVT] [0x%x]=0x%x\n",0x204,upmu_get_reg_value(0x204));
	printk("[PMIC_DVT] [0x%x]=0x%x\n",0x2d2,upmu_get_reg_value(0x2d2));
	printk("[PMIC_DVT] [0x%x]=0x%x\n",0x278,upmu_get_reg_value(0x278));
	printk("[PMIC_DVT] [0x%x]=0x%x\n",0x23c,upmu_get_reg_value(0x23c));
	printk("[PMIC_DVT] [0x%x]=0x%x\n",0xa02,upmu_get_reg_value(0xa02));
	if(pmic_get_register_value(PMIC_DA_QI_VTCXO28_EN)==1)
	{
		printk("[PMIC_DVT] VTCXO 1: 1 pass\n");
	}
	else
	{
		printk("[PMIC_DVT] VTCXO 1: 1 fail\n");
	}


	printk("[PMIC_DVT] [0x%x]=0x%x\n",0xa02,upmu_get_reg_value(0xa02));
	printk("[PMIC_DVT] [0x%x]=0x%x\n",0xa02,upmu_get_reg_value(0xa02));
	printk("[PMIC_DVT] [0x%x]=0x%x\n",0xa02,upmu_get_reg_value(0xa02));

	printk("[PMIC_DVT] x:%d y:%d\n",x,y);

}

void pmic_ldo1_hw_on_control(void)
{
	int x,y;
	printk("[PMIC_DVT] 1 Ldo on/off control hw mode \n");

	printk("[PMIC_DVT] VA18 Ldo on/off control hw mode\n");
	pmic_set_register_value(PMIC_RG_VA18_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VA18_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VA18_EN)==1)
		{
			printk("[PMIC_DVT] VA18 pass\n");
		}
	}


	printk("[PMIC_DVT] VTCXO24 Ldo on/off control hw mode\n");
	pmic_set_register_value(PMIC_RG_VTCXO24_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VTCXO24_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VTCXO24_EN)==1)
		{
			printk("[PMIC_DVT] VTCXO24 pass\n");
		}
	}


	printk("[PMIC_DVT] VTCXO28 Ldo on/off control hw mode\n");
	pmic_set_register_value(PMIC_RG_VTCXO28_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VTCXO28_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VTCXO28_EN)==1)
		{
			printk("[PMIC_DVT] VTCXO 1 pass\n");
		}
	}


	printk("[PMIC_DVT] VCN28 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VCN28_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VCN28_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VCN28_EN)==1)
		{
			printk("[PMIC_DVT] VCN28 pass\n");
		}
	}


	printk("[PMIC_DVT] VCAMA Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VCAMA_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VCAMA_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VCAMA_EN)==1)
		{
			printk("[PMIC_DVT] VCAMA pass\n");
		}
	}

	printk("[PMIC_DVT] VUSB33 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VUSB33_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VUSB33_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VUSB33_EN)==1)
		{
			printk("[PMIC_DVT] VUSB33 pass\n");
		}
	}

	printk("[PMIC_DVT] VSIM1 WIFI Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VSIM1_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VSIM1_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VSIM1_EN)==1)
		{
			printk("[PMIC_DVT] VSIM1 WIFI pass\n");
		}
	}



	printk("[PMIC_DVT] VSIM2 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VSIM2_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VSIM2_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VSIM2_EN)==1)
		{
			printk("[PMIC_DVT] VSIM2 pass\n");
		}
	}


	printk("[PMIC_DVT] VEMC_3V3 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VEMC_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VEMC_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VEMC_EN)==1)
		{
			printk("[PMIC_DVT] VEMC_3V3 pass\n");
		}
	}


	printk("[PMIC_DVT] VMCH Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VMCH_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VMCH_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VMCH_EN)==1)
		{
			printk("[PMIC_DVT] VMCH pass\n");
		}
	}


	printk("[PMIC_DVT] VIO28 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VIO28_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VIO28_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VIO28_EN)==1)
		{
			printk("[PMIC_DVT] VIO28 pass\n");
		}
	}


	printk("[PMIC_DVT] VIBR Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VIBR_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VIBR_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VIBR_EN)==1)
		{
			printk("[PMIC_DVT] VIBR pass\n");
		}
	}


	printk("[PMIC_DVT] VCAMD Ldo on/off control sw mode\n");
//argus	pmic_set_register_value(PMIC_RG_VCAMD_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VCAMD_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VCAMD_EN)==1)
		{
			printk("[PMIC_DVT] VCAMD pass\n");
		}
	}

	printk("[PMIC_DVT] VRF18 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VRF18_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VRF18_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VRF18_EN)==1)
		{
			printk("[PMIC_DVT] VRF18 pass\n");
		}
	}


	printk("[PMIC_DVT] VIO18 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VIO18_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VIO18_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VIO18_EN)==1)
		{
			printk("[PMIC_DVT] VIO18 pass\n");
		}
	}


	printk("[PMIC_DVT] VCN18 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VCN18_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VCN18_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VCN18_EN)==1)
		{
			printk("[PMIC_DVT] VCN18 pass\n");
		}
	}

	printk("[PMIC_DVT] VCAMIO Ldo on/off control sw mode\n");
//ARGUS 	pmic_set_register_value(PMIC_RG_VCAMIO_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VCAMIO_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VCAMIO_EN)==1)
		{
			printk("[PMIC_DVT] VCAMIO pass\n");
		}
	}

	printk("[PMIC_DVT] VSRAM_PROC Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VSRAM_PROC_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VSRAM_PROC_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VSRAM_PROC_EN)==1)
		{
			printk("[PMIC_DVT] VSRAM_PROC pass\n");
		}
	}

	printk("[PMIC_DVT] VXO22 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VXO22_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VXO22_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VXO22_EN)==1)
		{
			printk("[PMIC_DVT] VXO22 pass\n");
		}
	}

	printk("[PMIC_DVT] VRF12 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VRF12_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VRF12_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VRF12_EN)==1)
		{
			printk("[PMIC_DVT] VRF12 pass\n");
		}
	}

	printk("[PMIC_DVT] VA10 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VA10_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VA10_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VA10_EN)==1)
		{
			printk("[PMIC_DVT] VA10 pass\n");
		}
	}

	printk("[PMIC_DVT] VDRAM Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VDRAM_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VDRAM_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VDRAM_EN)==1)
		{
			printk("[PMIC_DVT] VDRAM pass\n");
		}
	}

	printk("[PMIC_DVT] VMIPI Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VMIPI_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VMIPI_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VMIPI_EN)==1)
		{
			printk("[PMIC_DVT] VMIPI pass\n");
		}
	}

	printk("[PMIC_DVT] VGP3 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VGP3_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VGP3_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VGP3_EN)==1)
		{
			printk("[PMIC_DVT] VGP3 pass\n");
		}
	}

	printk("[PMIC_DVT] VBIF28 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VBIF28_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VBIF28_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VBIF28_EN)==1)
		{
			printk("[PMIC_DVT] VBIF28 pass\n");
		}
	}

	printk("[PMIC_DVT] VEFUSE Ldo on/off control sw mode\n");
//ARGUS	pmic_set_register_value(PMIC_RG_VEFUSE_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VEFUSE_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VEFUSE_EN)==1)
		{
			printk("[PMIC_DVT] VEFUSE pass\n");
		}
	}

	printk("[PMIC_DVT] VCN33 BT Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VCN33_ON_CTRL_BT,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VCN33_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VCN33_EN)==1)
		{
			printk("[PMIC_DVT] VCN33 BTpass\n");
		}
	}

	printk("[PMIC_DVT] VCN33 WIFI Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VCN33_ON_CTRL_WIFI,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VCN33_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VCN33_EN)==1)
		{
			printk("[PMIC_DVT] VCN33 WIFI pass\n");
		}
	}

	printk("[PMIC_DVT] VLDO28 Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VLDO28_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VLDO28_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VLDO28_EN)==1)
		{
			printk("[PMIC_DVT] VLDO28 pass\n");
		}
	}

	printk("[PMIC_DVT] VMC Ldo on/off control sw mode\n");
	pmic_set_register_value(PMIC_RG_VMC_ON_CTRL,1);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_QI_VMC_EN)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VMC_EN)==1)
		{
			printk("[PMIC_DVT] VMC pass\n");
		}
	}
}































void pmic_ldo2_vosel_test(int idx)
{
	kal_uint8 i;
	printk("[PMIC_DVT] 2 Ldo volsel test\n");


	if(idx==MT6351_POWER_LDO_VCAMA)
	{
		printk("[PMIC_DVT] VCAMA volsel test\n");
		pmic_set_register_value(PMIC_RG_VCAMA_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VCAMA_EN,1);

		for(i=0;i<8;i++)
		{
			pmic_set_register_value(PMIC_RG_VCAMA_VOSEL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VCN33_BT)
	{
		printk("[PMIC_DVT] VCN33 volsel test\n");
		pmic_set_register_value(PMIC_RG_VCN33_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VCN33_ON_CTRL_BT,0);
		pmic_set_register_value(PMIC_RG_VCN33_EN_BT,1);

		for(i=0;i<4;i++)
		{
			pmic_set_register_value(PMIC_RG_VCN33_VOSEL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VEFUSE)
	{
		printk("[PMIC_DVT] VEFUSE volsel test\n");
		pmic_set_register_value(PMIC_RG_VEFUSE_VOSEL,0);
//ARGUS		//pmic_set_register_value(PMIC_RG_VEFUSE_ON_CTRL,0);
		pmic_set_register_value(PMIC_RG_VEFUSE_EN,1);

		for(i=0;i<7;i++)
		{
			pmic_set_register_value(PMIC_RG_VEFUSE_VOSEL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VSIM1)
	{
		printk("[PMIC_DVT] VSIM1 volsel test\n");
		pmic_set_register_value(PMIC_RG_VSIM1_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VSIM1_ON_CTRL,0);
		pmic_set_register_value(PMIC_RG_VSIM1_EN,1);
		for(i=0;i<8;i++)
		{
			pmic_set_register_value(PMIC_RG_VSIM1_VOSEL,i);
			mdelay(300);
		}

	}
	else if(idx==MT6351_POWER_LDO_VSIM2)
	{
		printk("[PMIC_DVT] VSIM2 volsel test\n");
		pmic_set_register_value(PMIC_RG_VSIM2_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VSIM2_ON_CTRL,0);
		pmic_set_register_value(PMIC_RG_VSIM2_EN,1);

		for(i=0;i<8;i++)
		{
			pmic_set_register_value(PMIC_RG_VSIM2_VOSEL,i);
			mdelay(300);
		}

	}
	else if(idx==MT6351_POWER_LDO_VEMC)
	{
		printk("[PMIC_DVT] VEMC33 volsel test\n");
		pmic_set_register_value(PMIC_RG_VEMC_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VEMC_ON_CTRL,0);
		pmic_set_register_value(PMIC_RG_VEMC_EN,1);

		for(i=0;i<2;i++)
		{
			pmic_set_register_value(PMIC_RG_VEMC_VOSEL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VMCH)
	{
		printk("[PMIC_DVT] VMCH volsel test\n");
		pmic_set_register_value(PMIC_RG_VMCH_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VMCH_ON_CTRL,0);
		pmic_set_register_value(PMIC_RG_VMCH_EN,1);

		for(i=0;i<3;i++)
		{
			pmic_set_register_value(PMIC_RG_VMCH_VOSEL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VMC)
	{
		printk("[PMIC_DVT] VMC volsel test\n");
		pmic_set_register_value(PMIC_RG_VMC_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VMC_ON_CTRL,0);
		pmic_set_register_value(PMIC_RG_VMC_EN,1);

		for(i=0;i<8;i++)
		{
			pmic_set_register_value(PMIC_RG_VMC_VOSEL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VGP3)
	{
		printk("[PMIC_DVT] VGP3 volsel test\n");
		pmic_set_register_value(PMIC_RG_VGP3_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VGP3_ON_CTRL,0);
		pmic_set_register_value(PMIC_RG_VGP3_EN,1);

		for(i=0;i<8;i++)
		{
			pmic_set_register_value(PMIC_RG_VGP3_VOSEL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VIBR)
	{
		printk("[PMIC_DVT] VIBR volsel test\n");
		pmic_set_register_value(PMIC_RG_VIBR_EN,0);
		pmic_set_register_value(PMIC_RG_VIBR_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VIBR_EN,1);

		for(i=0;i<8;i++)
		{
			pmic_set_register_value(PMIC_RG_VIBR_VOSEL,i);
			mdelay(300);
		}

	}
	else if(idx==MT6351_POWER_LDO_VCAMD)
	{
		printk("[PMIC_DVT] VCAMD volsel test\n");
		pmic_set_register_value(PMIC_RG_VCAMD_VOSEL,0);
		//pmic_set_register_value(PMIC_RG_VCAMD_ON_CTRL,0);
		pmic_set_register_value(PMIC_RG_VCAMD_EN,1);

		for(i=0;i<8;i++)
		{
			pmic_set_register_value(PMIC_RG_VCAMD_VOSEL,i);
			mdelay(300);
		}

	}
	else if(idx==MT6351_POWER_LDO_VRF18)
	{
		printk("[PMIC_DVT] VRF18 volsel test\n");
		pmic_set_register_value(PMIC_RG_VRF18_EN,0);
		pmic_set_register_value(PMIC_RG_VRF18_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VRF18_EN,1);

		for(i=0;i<8;i++)
		{
			pmic_set_register_value(PMIC_RG_VRF18_VOSEL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VCAMIO)
	{
		printk("[PMIC_DVT] VCAM_IO volsel test\n");
		pmic_set_register_value(PMIC_RG_VCAMIO_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VCAMIO_EN,1);

		for(i=0;i<7;i++)
		{
			pmic_set_register_value(PMIC_RG_VCAMIO_VOSEL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VSRAM_PROC)
	{
		printk("[PMIC_DVT] VSRAM_PROC volsel test\n");
		pmic_set_register_value(PMIC_RG_VSRAM_PROC_EN,0);
		pmic_set_register_value(PMIC_BUCK_VSRAM_PROC_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VSRAM_PROC_EN,1);

		for(i=0;i<128;i++)
		{
			pmic_set_register_value(PMIC_BUCK_VSRAM_PROC_VOSEL,i);
			mdelay(50);
		}

	}
	else if(idx==MT6351_POWER_LDO_VMC)
	{
		printk("[PMIC_DVT] VM volsel test\n");
		pmic_set_register_value(PMIC_RG_VMC_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VMC_ON_CTRL,0);
		pmic_set_register_value(PMIC_RG_VMC_EN,1);

		for(i=0;i<3;i++)
		{
			pmic_set_register_value(PMIC_RG_VMC_VOSEL,i);
			mdelay(300);
		}

	}

}



void pmic_ldo3_cal_test(int idx)
{
	kal_uint8 i;
	printk("[PMIC_DVT] 3 Ldo cal test\n");

	if(idx==MT6351_POWER_LDO_VA18)
	{
		printk("[PMIC_DVT] VAUX18 cal test\n");
		//pmic_set_register_value(PMIC_RG_VAUX18_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VA18_EN,1);

		for(i=0;i<16;i++)
		{
			pmic_set_register_value(PMIC_RG_VA18_CAL,i);
			mdelay(300);
		}
	}
	if(idx==MT6351_POWER_LDO_VTCXO24)
	{
		printk("[PMIC_DVT] VTCXO0 cal test\n");
		//pmic_set_register_value(PMIC_RG_VTCXO_0_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VTCXO24_EN,1);

		for(i=0;i<16;i++)
		{
			pmic_set_register_value(PMIC_RG_VTCXO24_CAL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VTCXO28)
	{
		printk("[PMIC_DVT] VTCXO1 cal test\n");
		//pmic_set_register_value(PMIC_RG_VTCXO28_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VTCXO28_EN,1);

		for(i=0;i<16;i++)
		{
			pmic_set_register_value(PMIC_RG_VTCXO28_CAL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VCN28)
	{
		printk("[PMIC_DVT] VCN28 cal test\n");
		//pmic_set_register_value(PMIC_RG_VAUD28_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VCN28_EN,1);

		for(i=0;i<16;i++)
		{
			pmic_set_register_value(PMIC_RG_VCN28_CAL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VCAMA)
	{
		printk("[PMIC_DVT] VCAMA cal test\n");
		pmic_set_register_value(PMIC_RG_VCAMA_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VCAMA_EN,1);

		for(i=0;i<16;i++)
		{
			pmic_set_register_value(PMIC_RG_VCAMA_CAL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VUSB33)
	{
		printk("[PMIC_DVT] VUSB33 cal test\n");
		//pmic_set_register_value(PMIC_RG_VUSB33_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VUSB33_EN,1);

		for(i=0;i<16;i++)
		{
			pmic_set_register_value(PMIC_RG_VUSB33_CAL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VEFUSE)
	{
		printk("[PMIC_DVT] VEFUSE cal test\n");
		pmic_set_register_value(PMIC_RG_VEFUSE_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VEFUSE_EN,1);

		for(i=0;i<16;i++)
		{
			pmic_set_register_value(PMIC_RG_VEFUSE_CAL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VSIM1)
	{
		printk("[PMIC_DVT] VSIM1 cal test\n");
		pmic_set_register_value(PMIC_RG_VSIM1_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VSIM1_EN,1);

		for(i=0;i<16;i++)
		{
			pmic_set_register_value(PMIC_RG_VSIM1_CAL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VSIM2)
	{
		printk("[PMIC_DVT] VSIM2 cal test\n");
		pmic_set_register_value(PMIC_RG_VSIM2_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VSIM2_EN,1);

		for(i=0;i<16;i++)
		{
			pmic_set_register_value(PMIC_RG_VSIM2_CAL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VEMC)
	{
		printk("[PMIC_DVT] VEMC33 cal test\n");
		pmic_set_register_value(PMIC_RG_VEMC_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VEMC_EN,1);

		for(i=0;i<16;i++)
		{
			pmic_set_register_value(PMIC_RG_VEMC_CAL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VMCH)
	{
		printk("[PMIC_DVT] VMCH cal test\n");
		pmic_set_register_value(PMIC_RG_VMCH_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VMCH_EN,1);

		for(i=0;i<16;i++)
		{
			pmic_set_register_value(PMIC_RG_VMCH_CAL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VMC)
	{
		printk("[PMIC_DVT] VMC cal test\n");
		pmic_set_register_value(PMIC_RG_VMC_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VMC_EN,1);

		for(i=0;i<16;i++)
		{
			pmic_set_register_value(PMIC_RG_VMC_CAL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VCAMD)
	{
		printk("[PMIC_DVT] VCAMAF cal test\n");
		pmic_set_register_value(PMIC_RG_VCAMD_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VCAMD_EN,1);

		for(i=0;i<16;i++)
		{
			pmic_set_register_value(PMIC_RG_VCAMD_CAL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VIO28)
	{
		printk("[PMIC_DVT] VIO28 cal test\n");
		//pmic_set_register_value(PMIC_RG_VCAMA_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VIO28_EN,1);

		for(i=0;i<16;i++)
		{
			pmic_set_register_value(PMIC_RG_VIO28_CAL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VGP3)
	{
		printk("[PMIC_DVT] VGP3 cal test\n");
		pmic_set_register_value(PMIC_RG_VGP3_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VGP3_EN,1);

		for(i=0;i<16;i++)
		{
			pmic_set_register_value(PMIC_RG_VGP3_CAL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VIBR)
	{
		printk("[PMIC_DVT] VIBR cal test\n");
		pmic_set_register_value(PMIC_RG_VIBR_EN,0);
		pmic_set_register_value(PMIC_RG_VIBR_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VIBR_EN,1);

		for(i=0;i<16;i++)
		{
			pmic_set_register_value(PMIC_RG_VIBR_CAL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VCAMD)
	{
		printk("[PMIC_DVT] VCAMD cal test\n");
		pmic_set_register_value(PMIC_RG_VCAMD_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VCAMD_EN,1);

		for(i=0;i<16;i++)
		{
			pmic_set_register_value(PMIC_RG_VCAMD_CAL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VRF18)
	{
		printk("[PMIC_DVT] VRF18_0 cal test\n");
		//pmic_set_register_value(PMIC_RG_VRF18_0_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VRF18_EN,1);

		for(i=0;i<16;i++)
		{
			pmic_set_register_value(PMIC_RG_VRF18_CAL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VRF12)
	{
		printk("[PMIC_DVT] VRF18_1 cal test\n");
		pmic_set_register_value(PMIC_RG_VRF12_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VRF12_EN,1);

		for(i=0;i<16;i++)
		{
			pmic_set_register_value(PMIC_RG_VRF12_CAL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VIO18)
	{
		printk("[PMIC_DVT] VIO18 cal test\n");
		//pmic_set_register_value(PMIC_RG_VIO18_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VIO18_EN,1);

		for(i=0;i<16;i++)
		{
			pmic_set_register_value(PMIC_RG_VIO18_CAL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VCN18)
	{
		printk("[PMIC_DVT] VCN18 cal test\n");
		//pmic_set_register_value(PMIC_RG_VIO18_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VCN18_EN,1);

		for(i=0;i<16;i++)
		{
			pmic_set_register_value(PMIC_RG_VCN18_CAL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VCAMIO)
	{
		printk("[PMIC_DVT] VCAMIO cal test\n");
		pmic_set_register_value(PMIC_RG_VCAMIO_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VCAMIO_EN,1);

		for(i=0;i<16;i++)
		{
			pmic_set_register_value(PMIC_RG_VCAMIO_CAL,i);
			mdelay(300);
		}
	}
	else if(idx==MT6351_POWER_LDO_VMC)
	{
		printk("[PMIC_DVT] VCAMIO cal test\n");
		pmic_set_register_value(PMIC_RG_VMC_VOSEL,0);
		pmic_set_register_value(PMIC_RG_VMC_EN,1);

		for(i=0;i<16;i++)
		{
			pmic_set_register_value(PMIC_RG_VMC_CAL,i);
			mdelay(300);
		}
	}

}

void pmic_ldo4_lp_test(void)
{
	printk("[PMIC_DVT] 1 Ldo LP control \n");

	printk("[PMIC_DVT] VA18 Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VA18_AUXADC_PWDB_EN,1);
	pmic_set_register_value(PMIC_RG_VA18_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VA18_MODE_SET,0);
	if(pmic_get_register_value(PMIC_DA_QI_VA18_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VA18_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VA18_MODE)==1)
		{
			printk("[PMIC_DVT] VA18 Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VA18_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VA18_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VA18_MODE)==1)
		{
			printk("[PMIC_DVT] VA18 Ldo LP control hw mode pass\n");
		}
	}



	printk("[PMIC_DVT] VTCXO24 Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VTCXO24_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VTCXO24_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VTCXO24_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VTCXO24_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VTCXO24_MODE)==1)
		{
			printk("[PMIC_DVT] VTCXO24 Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VTCXO24_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VTCXO24_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VTCXO24_MODE)==1)
		{
			printk("[PMIC_DVT] VTCXO24 Ldo LP control hw mode pass\n");
		}
	}



	printk("[PMIC_DVT] VTCXO28 Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VTCXO28_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VTCXO28_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VTCXO28_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VTCXO28_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VTCXO28_MODE)==1)
		{
			printk("[PMIC_DVT] VTCXO28 Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VTCXO28_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VTCXO28_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VTCXO28_MODE)==1)
		{
			printk("[PMIC_DVT] VTCXO28 Ldo LP control hw mode pass\n");
		}
	}



	printk("[PMIC_DVT] VCN28 Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VCN28_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VCN28_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VCN28_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VCN28_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VCN28_MODE)==1)
		{
			printk("[PMIC_DVT] VCN28 Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VCN28_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VCN28_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VCN28_MODE)==1)
		{
			printk("[PMIC_DVT] VTCXO28 Ldo LP control hw mode pass\n");
		}
	}

	printk("[PMIC_DVT] VCAMA Ldo LP control sw mode\n");
//ARGUS	pmic_set_register_value(PMIC_RG_VCAMA_MODE_CTRL,0);
//ARGUS	pmic_set_register_value(PMIC_RG_VCAMA_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VCAMA_MODE)==0)
	{
//ARGUS		pmic_set_register_value(PMIC_RG_VCAMA_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VCAMA_MODE)==1)
		{
			printk("[PMIC_DVT] VCAMA Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
//ARGUS	pmic_set_register_value(PMIC_RG_VCAMA_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VCAMA_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VCAMA_MODE)==1)
		{
			printk("[PMIC_DVT] VTCXO28 Ldo LP control hw mode pass\n");
		}
	}

	printk("[PMIC_DVT] VUSB33 Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VUSB33_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VUSB33_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VUSB33_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VUSB33_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VUSB33_MODE)==1)
		{
			printk("[PMIC_DVT] VUSB33 Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VUSB33_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VUSB33_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VUSB33_MODE)==1)
		{
			printk("[PMIC_DVT] VUSB33 Ldo LP control hw mode pass\n");
		}
	}

	printk("[PMIC_DVT] VSIM1 Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VSIM1_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VSIM1_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VSIM1_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VSIM1_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VSIM1_MODE)==1)
		{
			printk("[PMIC_DVT] VSIM1 Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VSIM1_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VSIM1_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VSIM1_MODE)==1)
		{
			printk("[PMIC_DVT] VSIM1 Ldo LP control hw mode pass\n");
		}
	}



	printk("[PMIC_DVT] VSIM2 Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VSIM2_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VSIM2_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VSIM2_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VSIM2_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VSIM2_MODE)==1)
		{
			printk("[PMIC_DVT] VSIM2 Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VSIM1_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VSIM1_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VSIM1_MODE)==1)
		{
			printk("[PMIC_DVT] VSIM2 Ldo LP control hw mode pass\n");
		}
	}

	printk("[PMIC_DVT] VEMC_3V3 Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VEMC_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VEMC_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VEMC_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VEMC_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VEMC_MODE)==1)
		{
			printk("[PMIC_DVT] VEMC Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VEMC_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VEMC_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VEMC_MODE)==1)
		{
			printk("[PMIC_DVT] VEMC Ldo LP control hw mode pass\n");
		}
	}



	printk("[PMIC_DVT] VMCH Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VMCH_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VMCH_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VMCH_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VMCH_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VMCH_MODE)==1)
		{
			printk("[PMIC_DVT] VMCH Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VMCH_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VMCH_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VMCH_MODE)==1)
		{
			printk("[PMIC_DVT] VMCH Ldo LP control hw mode pass\n");
		}
	}

	printk("[PMIC_DVT] VIO18 Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VIO18_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VIO18_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VIO18_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VIO18_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VIO18_MODE)==1)
		{
			printk("[PMIC_DVT] VIO18 Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VIO18_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VIO18_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VIO18_MODE)==1)
		{
			printk("[PMIC_DVT] VIO18 Ldo LP control hw mode pass\n");
		}
	}

	printk("[PMIC_DVT] VIBR Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VIBR_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VIBR_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VIBR_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VIBR_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VIBR_MODE)==1)
		{
			printk("[PMIC_DVT] VIBR Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VIBR_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VIBR_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VIBR_MODE)==1)
		{
			printk("[PMIC_DVT] VIBR Ldo LP control hw mode pass\n");
		}
	}



	printk("[PMIC_DVT] VCAMD Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VCAMD_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VCAMD_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VCAMD_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VCAMD_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VCAMD_MODE)==1)
		{
			printk("[PMIC_DVT] VCAMD Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VCAMD_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VCAMD_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VCAMD_MODE)==1)
		{
			printk("[PMIC_DVT] VCAMD Ldo LP control hw mode pass\n");
		}
	}

	printk("[PMIC_DVT] VRF18 Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VRF18_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VRF18_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VRF18_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VRF18_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VRF18_MODE)==1)
		{
			printk("[PMIC_DVT] VRF18 Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VRF18_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VRF18_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VRF18_MODE)==1)
		{
			printk("[PMIC_DVT] VRF18 Ldo LP control hw mode pass\n");
		}
	}



	printk("[PMIC_DVT] VIO18 Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VIO18_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VIO18_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VIO18_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VIO18_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VIO18_MODE)==1)
		{
			printk("[PMIC_DVT] VIO18 Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VIO18_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VIO18_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VIO18_MODE)==1)
		{
			printk("[PMIC_DVT] VIO18 Ldo LP control hw mode pass\n");
		}
	}

	printk("[PMIC_DVT] VCN18 Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VCN18_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VCN18_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VCN18_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VCN18_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VCN18_MODE)==1)
		{
			printk("[PMIC_DVT] VCN18 Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VCN18_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VCN18_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VCN18_MODE)==1)
		{
			printk("[PMIC_DVT] VCN18 Ldo LP control hw mode pass\n");
		}
	}



	printk("[PMIC_DVT] VCAMIO Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VCAMIO_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VCAMIO_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VCAMIO_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VCAMIO_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VCAMIO_MODE)==1)
		{
			printk("[PMIC_DVT] VCAMIO Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VCAMIO_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VCAMIO_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VCAMIO_MODE)==1)
		{
			printk("[PMIC_DVT] VCAMIO Ldo LP control hw mode pass\n");
		}
	}

	printk("[PMIC_DVT] VSRAM_PROC Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VSRAM_PROC_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VSRAM_PROC_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VSRAM_PROC_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VSRAM_PROC_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VSRAM_PROC_MODE)==1)
		{
			printk("[PMIC_DVT] VSRAM_PROC Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VSRAM_PROC_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VSRAM_PROC_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VSRAM_PROC_MODE)==1)
		{
			printk("[PMIC_DVT] VSRAM_PROC Ldo LP control hw mode pass\n");
		}
	}

	printk("[PMIC_DVT] VXO22 Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VXO22_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VXO22_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VXO22_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VXO22_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VXO22_MODE)==1)
		{
			printk("[PMIC_DVT] VXO22 Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VXO22_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VXO22_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VXO22_MODE)==1)
		{
			printk("[PMIC_DVT] VXO22 Ldo LP control hw mode pass\n");
		}
	}

	printk("[PMIC_DVT] VRF12 Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VRF12_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VRF12_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VRF12_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VRF12_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VRF12_MODE)==1)
		{
			printk("[PMIC_DVT] VRF12 Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VRF12_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VRF12_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VRF12_MODE)==1)
		{
			printk("[PMIC_DVT] VRF12 Ldo LP control hw mode pass\n");
		}
	}

	printk("[PMIC_DVT] VA10 Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VA10_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VA10_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VA10_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VA10_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VA10_MODE)==1)
		{
			printk("[PMIC_DVT] VA10 Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VA10_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VA10_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VA10_MODE)==1)
		{
			printk("[PMIC_DVT] VA10 Ldo LP control hw mode pass\n");
		}
	}

	printk("[PMIC_DVT] VDRAM Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VDRAM_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VDRAM_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VDRAM_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VDRAM_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VDRAM_MODE)==1)
		{
			printk("[PMIC_DVT] VDRAM Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VDRAM_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VDRAM_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VDRAM_MODE)==1)
		{
			printk("[PMIC_DVT] VDRAM Ldo LP control hw mode pass\n");
		}
	}

	printk("[PMIC_DVT] VMIPI Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VMIPI_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VMIPI_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VMIPI_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VMIPI_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VMIPI_MODE)==1)
		{
			printk("[PMIC_DVT] VMIPI Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VMIPI_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VMIPI_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VMIPI_MODE)==1)
		{
			printk("[PMIC_DVT] VMIPI Ldo LP control hw mode pass\n");
		}
	}

	printk("[PMIC_DVT] VGP3 Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VGP3_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VGP3_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VGP3_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VGP3_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VGP3_MODE)==1)
		{
			printk("[PMIC_DVT] VGP3 Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VGP3_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VGP3_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VGP3_MODE)==1)
		{
			printk("[PMIC_DVT] VGP3 Ldo LP control hw mode pass\n");
		}
	}

	printk("[PMIC_DVT] VBIF28 Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VBIF28_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VBIF28_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VBIF28_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VBIF28_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VBIF28_MODE)==1)
		{
			printk("[PMIC_DVT] VBIF28 Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VBIF28_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VBIF28_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VBIF28_MODE)==1)
		{
			printk("[PMIC_DVT] VBIF28 Ldo LP control hw mode pass\n");
		}
	}

	printk("[PMIC_DVT] VEFUSE Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VEFUSE_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VEFUSE_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VEFUSE_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VEFUSE_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VEFUSE_MODE)==1)
		{
			printk("[PMIC_DVT] VEFUSE Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VEFUSE_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VEFUSE_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VEFUSE_MODE)==1)
		{
			printk("[PMIC_DVT] VEFUSE Ldo LP control hw mode pass\n");
		}
	}

	printk("[PMIC_DVT] VCN33 Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VCN33_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VCN33_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VCN33_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VCN33_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VCN33_MODE)==1)
		{
			printk("[PMIC_DVT] VCN33 Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VCN33_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VCN33_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VCN33_MODE)==1)
		{
			printk("[PMIC_DVT] VCN33 Ldo LP control hw mode pass\n");
		}
	}

	printk("[PMIC_DVT] VLDO28 Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VLDO28_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VLDO28_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VLDO28_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VLDO28_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VLDO28_MODE)==1)
		{
			printk("[PMIC_DVT] VLDO28 Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VLDO28_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VLDO28_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VLDO28_MODE)==1)
		{
			printk("[PMIC_DVT] VLDO28 Ldo LP control hw mode pass\n");
		}
	}

	printk("[PMIC_DVT] VMC Ldo LP control sw mode\n");
	pmic_set_register_value(PMIC_RG_VMC_MODE_CTRL,0);
	pmic_set_register_value(PMIC_RG_VMC_MODE_SET,0);

	if(pmic_get_register_value(PMIC_DA_QI_VMC_MODE)==0)
	{
		pmic_set_register_value(PMIC_RG_VMC_MODE_SET,1);
		if(pmic_get_register_value(PMIC_DA_QI_VMC_MODE)==1)
		{
			printk("[PMIC_DVT] VMC Ldo LP control sw mode pass\n");
		}
	}
	setSrclkenMode(0);
	pmic_set_register_value(PMIC_RG_VMC_MODE_SET,1);
	if(pmic_get_register_value(PMIC_DA_QI_VMC_MODE)==0)
	{
		setSrclkenMode(1);
		if(pmic_get_register_value(PMIC_DA_QI_VMC_MODE)==1)
		{
			printk("[PMIC_DVT] VMC Ldo LP control hw mode pass\n");
		}
	}

}

#if 0
void pmic_ldo5_fast_tran(void)
{
	printk("[PMIC_DVT] 5.FAST TRAN Test\n");
	//if(idx==PMIC_6328_VEMC33)
	{
		printk("[PMIC_DVT] VEMC33 FAST TRAN Test\n");

		pmic_set_register_value(PMIC_RG_VEMC_FAST_TRAN_EN,1);
		pmic_set_register_value(PMIC_RG_VEMC_SRCLK_FAST_TRAN_SEL,0);

		setSrclkenMode(0);
		if(pmic_get_register_value(PMIC_DA_QI_VEMC_FAST_TRAN_EN)==0)
		{
			setSrclkenMode(1);
			if(pmic_get_register_value(PMIC_DA_QI_VEMC_FAST_TRAN_EN)==1)
			{
				printk("[PMIC_DVT] VEMC33 FAST TRAN Test pass\n");
			}
		}
	}
	//else if(idx==PMIC_6328_VMCH)
	{
		printk("[PMIC_DVT] VMCH FAST TRAN Test\n");

		pmic_set_register_value(PMIC_RG_VMCH_FAST_TRAN_EN,1);
		pmic_set_register_value(PMIC_RG_VMCH_SRCLK_FAST_TRAN_SEL,0);

		setSrclkenMode(0);
		if(pmic_get_register_value(PMIC_DA_QI_VMCH_FAST_TRAN_EN)==0)
		{
			setSrclkenMode(1);
			if(pmic_get_register_value(PMIC_DA_QI_VMCH_FAST_TRAN_EN)==1)
			{
				printk("[PMIC_DVT] VMCH FAST TRAN Test pass\n");
			}
		}
	}
	//else if(idx==PMIC_6328_VGP3)
	{
		printk("[PMIC_DVT] VGP3 FAST TRAN Test\n");

		pmic_set_register_value(PMIC_RG_VGP3_FAST_TRAN_EN,1);
		pmic_set_register_value(PMIC_RG_VGP3_SRCLK_FAST_TRAN_SEL,0);

		setSrclkenMode(0);
		if(pmic_get_register_value(PMIC_DA_QI_VGP3_FAST_TRAN_EN)==0)
		{
			setSrclkenMode(1);
			if(pmic_get_register_value(PMIC_DA_QI_VGP3_FAST_TRAN_EN)==1)
			{
				printk("[PMIC_DVT] VGP3 FAST TRAN Test pass\n");
			}
		}
	}
	//else if(idx==PMIC_6328_VRF18_0)
	{
		printk("[PMIC_DVT] VRF18_0 FAST TRAN Test\n");

		pmic_set_register_value(PMIC_RG_VRF18_0_FAST_TRAN_EN,1);
		pmic_set_register_value(PMIC_RG_VRF18_0_SRCLK_FAST_TRAN_SEL,0);

		setSrclkenMode(0);
		if(pmic_get_register_value(PMIC_DA_QI_VRF18_0_FAST_TRAN_EN)==0)
		{
			setSrclkenMode(1);
			if(pmic_get_register_value(PMIC_DA_QI_VRF18_0_FAST_TRAN_EN)==1)
			{
				printk("[PMIC_DVT] VRF18_0 FAST TRAN Test pass\n");
			}
		}
	}
	//else if(idx==PMIC_6328_VRF18_1)
	{
		printk("[PMIC_DVT] VRF18_1 FAST TRAN Test\n");

		pmic_set_register_value(PMIC_RG_VRF18_1_FAST_TRAN_EN,1);
		pmic_set_register_value(PMIC_RG_VRF18_1_SRCLK_FAST_TRAN_SEL,0);

		setSrclkenMode(0);
		if(pmic_get_register_value(PMIC_DA_QI_VRF18_1_FAST_TRAN_EN)==0)
		{
			setSrclkenMode(1);
			if(pmic_get_register_value(PMIC_DA_QI_VRF18_1_FAST_TRAN_EN)==1)
			{
				printk("[PMIC_DVT] VRF18_1 FAST TRAN Test pass\n");
			}
		}
	}
	//else if(idx==PMIC_6328_VSRAM_PROC)
	{
		printk("[PMIC_DVT] VSRAM_PROC FAST TRAN Test\n");

		pmic_set_register_value(PMIC_RG_VSRAM_PROC_FAST_TRAN_EN,1);
		pmic_set_register_value(PMIC_RG_VSRAM_PROC_SRCLK_FAST_TRAN_SEL,0);

		setSrclkenMode(0);
		if(pmic_get_register_value(PMIC_DA_QI_VSRAM_PROC_FAST_TRAN_EN)==0)
		{
			setSrclkenMode(1);
			if(pmic_get_register_value(PMIC_DA_QI_VSRAM_PROC_FAST_TRAN_EN)==1)
			{
				printk("[PMIC_DVT] VSRAM_PROC FAST TRAN Test pass\n");
			}
		}
	}
	//else if(idx==PMIC_6328_VM)
	{
		printk("[PMIC_DVT] VM FAST TRAN Test\n");

		pmic_set_register_value(PMIC_RG_VM_FAST_TRAN_EN,1);
		pmic_set_register_value(PMIC_RG_VM_SRCLK_FAST_TRAN_SEL,0);

		setSrclkenMode(0);
		if(pmic_get_register_value(PMIC_DA_QI_VM_FAST_TRAN_EN)==0)
		{
			setSrclkenMode(1);
			if(pmic_get_register_value(PMIC_DA_QI_VM_FAST_TRAN_EN)==1)
			{
				printk("[PMIC_DVT] VM FAST TRAN Test pass\n");
			}
		}
	}


}

void pmic_vpa11(void)
{
	kal_uint8 i;

	printk("[PMIC_DVT] \npmic_vpa11\n");
	pmic_set_register_value(PMIC_VPA_VOSEL_CTRL,0);
	pmic_set_register_value(PMIC_VPA_EN_CTRL,0);
	pmic_set_register_value(PMIC_RG_VPA_EN,1);

	for(i=0;i<64;i++)
	{
		pmic_set_register_value(PMIC_VPA_VOSEL,i);

		printk("[PMIC_DVT] VPA_VOSEL:%d QI_VPA_VOSEL:%d QI_VPA_DLC:%d\n",i,
			pmic_get_register_value(PMIC_NI_VPA_VOSEL),
			pmic_get_register_value(PMIC_DA_QI_VPA_DLC));
	}

}

void pmic_vpa12(void)
{
	kal_uint8 i;
	kal_uint8 vpass=0;
	printk("[PMIC_DVT] \npmic_vpa12\n");

	for(i=0;i<4;i++)
	{
		pmic_set_register_value(PMIC_VPA_BURSTH,i);

		if(pmic_get_register_value(PMIC_DA_QI_VPA_BURSTH)!=i)
		{
			printk("[PMIC_DVT] VPA BURSTH test fail %d !=%d \n",i,pmic_get_register_value(PMIC_DA_QI_VPA_BURSTH));
			vpass=1;
		}

		printk("[PMIC_DVT] VPA BURSTH :VPA_BURSTH=%d    QI_VPA_BURSTH=%d \n",
			pmic_get_register_value(PMIC_VPA_BURSTH),
			pmic_get_register_value(PMIC_DA_QI_VPA_BURSTH));
	}

	for(i=0;i<4;i++)
	{
		pmic_set_register_value(PMIC_VPA_BURSTL,i);

		if(pmic_get_register_value(PMIC_DA_QI_VPA_BURSTL)!=i)
		{
			printk("[PMIC_DVT] VPA BURSTL test fail %d !=%d \n",i,pmic_get_register_value(PMIC_DA_QI_VPA_BURSTL));
			vpass=1;
		}
	}

	if(vpass==0)
	{
		printk("[PMIC_DVT] VPA BURSTH test pass \n");
	}


}
#endif
void pmic_reg(void)
{
	int i;
	for (i = 0;i < 0x0100;i++)
	{
		pmic_set_register_value(i, 0x5aa5);
		if (pmic_get_register_value(i) != 0x5aa5)
			printk("[PMIC_DVT] write 0x5aa5 [0x%x]= 0x%x Pass\n",pmu_flags_table[i].offset,pmic_get_register_value(i));
		else
			printk("[PMIC_DVT] write 0x5aa5 [0x%x]= 0x%x Fail\n",pmu_flags_table[i].offset,pmic_get_register_value(i));
	}

}

void pmic_buck11(void)
{

    /* Check on/off HW & SW mode control */
	printk("[PMIC_DVT] VCORE sw en test :\n");
	pmic_set_register_value(PMIC_BUCK_VCORE_EN_CTRL,0);
	pmic_set_register_value(PMIC_BUCK_VCORE_EN,1);

	printk("[PMIC_DVT] VCORE sw enable:en:%d \n",pmic_get_register_value(PMIC_DA_QI_VCORE_EN));
	if(pmic_get_register_value(PMIC_DA_QI_VCORE_EN)==1)
	{
		pmic_set_register_value(PMIC_BUCK_VCORE_EN,0);
		printk("[PMIC_DVT] VCORE sw disable:en:%d \n",pmic_get_register_value(PMIC_DA_QI_VCORE_EN));
		if(pmic_get_register_value(PMIC_DA_QI_VCORE_EN)==0)
		{
			printk("[PMIC_DVT] VCORE sw en test pass\n");
		}
	}

	pmic_set_register_value(PMIC_BUCK_VCORE_EN_CTRL,1);
	setSrclkenMode(1);
	printk("[PMIC_DVT] VCORE hw enable:en:%d \n",pmic_get_register_value(PMIC_DA_QI_VCORE_EN));
	if(pmic_get_register_value(PMIC_DA_QI_VCORE_EN)==1)
	{
		setSrclkenMode(0);
		printk("[PMIC_DVT] VCORE hw disable:en:%d \n",pmic_get_register_value(PMIC_DA_QI_VCORE_EN));
		if(pmic_get_register_value(PMIC_DA_QI_VCORE_EN)==0)
		{
			printk("[PMIC_DVT] VCORE hw en test pass\n");
		}
	}

	printk("[PMIC_DVT] VGPU sw en test :\n");
	pmic_set_register_value(PMIC_BUCK_VGPU_EN_CTRL,0);
	pmic_set_register_value(PMIC_BUCK_VGPU_EN,1);
	if(pmic_get_register_value(PMIC_DA_QI_VGPU_EN)==1)
	{
		pmic_set_register_value(PMIC_BUCK_VGPU_EN,0);
		if(pmic_get_register_value(PMIC_DA_QI_VGPU_EN)==0)
		{
			printk("[PMIC_DVT] VGPU sw en test pass\n");
		}
	}


	pmic_set_register_value(PMIC_BUCK_VGPU_EN_CTRL,1);
	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_QI_VGPU_EN)==1)
	{
		setSrclkenMode(0);
		if(pmic_get_register_value(PMIC_DA_QI_VGPU_EN)==0)
		{
			printk("[PMIC_DVT] VGPU hw en test pass\n");
		}
	}


	printk("[PMIC_DVT] VMODEM sw en test :\n");
	pmic_set_register_value(PMIC_BUCK_VMODEM_EN_CTRL,0);
	pmic_set_register_value(PMIC_BUCK_VMODEM_EN,1);
	if(pmic_get_register_value(PMIC_DA_QI_VMODEM_EN)==1)
	{
		pmic_set_register_value(PMIC_BUCK_VMODEM_EN,0);
		if(pmic_get_register_value(PMIC_DA_QI_VMODEM_EN)==0)
		{
			printk("[PMIC_DVT] VMODEM sw en test pass\n");
		}
	}


	pmic_set_register_value(PMIC_BUCK_VMODEM_EN_CTRL,1);
	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_QI_VMODEM_EN)==1)
	{
		setSrclkenMode(0);
		if(pmic_get_register_value(PMIC_DA_QI_VMODEM_EN)==0)
		{
			printk("[PMIC_DVT] VMODEM hw en test pass\n");
		}
	}

	printk("[PMIC_DVT] VMD1 sw en test :\n");
	pmic_set_register_value(PMIC_BUCK_VMD1_EN_CTRL,0);
	pmic_set_register_value(PMIC_BUCK_VMD1_EN,1);
	if(pmic_get_register_value(PMIC_DA_QI_VMD1_EN)==1)
	{
		pmic_set_register_value(PMIC_BUCK_VMD1_EN,0);
		if(pmic_get_register_value(PMIC_DA_QI_VMD1_EN)==0)
		{
			printk("[PMIC_DVT] VMD1 sw en test pass\n");
		}
	}


	pmic_set_register_value(PMIC_BUCK_VMD1_EN_CTRL,1);
	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_QI_VMD1_EN)==1)
	{
		setSrclkenMode(0);
		if(pmic_get_register_value(PMIC_DA_QI_VMD1_EN)==0)
		{
			printk("[PMIC_DVT] VMD1 hw en test pass\n");
		}
	}

	printk("[PMIC_DVT] VSRAM_PROC_MD sw en test :\n");
	pmic_set_register_value(PMIC_BUCK_VSRAM_MD_EN_CTRL,0);
	pmic_set_register_value(PMIC_BUCK_VSRAM_MD_EN,1);
	if(pmic_get_register_value(PMIC_DA_QI_VSRAM_MD_EN)==1)
	{
		pmic_set_register_value(PMIC_BUCK_VSRAM_MD_EN,0);
		if(pmic_get_register_value(PMIC_DA_QI_VSRAM_MD_EN)==0)
		{
			printk("[PMIC_DVT] VSRAM_MD sw en test pass\n");
		}
	}


	pmic_set_register_value(PMIC_BUCK_VSRAM_MD_EN_CTRL,1);
	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_QI_VSRAM_MD_EN)==1)
	{
		setSrclkenMode(0);
		if(pmic_get_register_value(PMIC_DA_QI_VSRAM_MD_EN)==0)
		{
			printk("[PMIC_DVT] VSRAM_MD hw en test pass\n");
		}
	}

	printk("[PMIC_DVT] VS1 sw en test :\n");
	pmic_set_register_value(PMIC_BUCK_VS1_EN_CTRL,0);
	pmic_set_register_value(PMIC_BUCK_VS1_EN,1);
	if(pmic_get_register_value(PMIC_DA_QI_VS1_EN)==1)
	{
		pmic_set_register_value(PMIC_BUCK_VS1_EN,0);
		if(pmic_get_register_value(PMIC_DA_QI_VS1_EN)==0)
		{
			printk("[PMIC_DVT] VS1 sw en test pass\n");
		}
	}


	pmic_set_register_value(PMIC_BUCK_VS1_EN_CTRL,1);
	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_QI_VS1_EN)==1)
	{
		setSrclkenMode(0);
		if(pmic_get_register_value(PMIC_DA_QI_VS1_EN)==0)
		{
			printk("[PMIC_DVT] VS1 hw en test pass\n");
		}
	}

	printk("[PMIC_DVT] VS2 sw en test :\n");
	pmic_set_register_value(PMIC_BUCK_VS2_EN_CTRL,0);
	pmic_set_register_value(PMIC_BUCK_VS2_EN,1);
	if(pmic_get_register_value(PMIC_DA_QI_VS2_EN)==1)
	{
		pmic_set_register_value(PMIC_BUCK_VS2_EN,0);
		if(pmic_get_register_value(PMIC_DA_QI_VS2_EN)==0)
		{
			printk("[PMIC_DVT] VS2 sw en test pass\n");
		}
	}


	pmic_set_register_value(PMIC_BUCK_VS2_EN_CTRL,1);
	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_QI_VS2_EN)==1)
	{
		setSrclkenMode(0);
		if(pmic_get_register_value(PMIC_DA_QI_VS2_EN)==0)
		{
			printk("[PMIC_DVT] VS2 hw en test pass\n");
		}
	}

	printk("[PMIC_DVT] VPA sw en test :\n");
	pmic_set_register_value(PMIC_BUCK_VPA_EN_CTRL,0);
	pmic_set_register_value(PMIC_BUCK_VPA_EN,1);
	if(pmic_get_register_value(PMIC_DA_QI_VPA_EN)==1)
	{
		pmic_set_register_value(PMIC_BUCK_VPA_EN,0);
		if(pmic_get_register_value(PMIC_DA_QI_VPA_EN)==0)
		{
			printk("[PMIC_DVT] VPA sw en test pass\n");
		}
	}


	pmic_set_register_value(PMIC_BUCK_VPA_EN_CTRL,1);
	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_QI_VPA_EN)==1)
	{
		setSrclkenMode(0);
		if(pmic_get_register_value(PMIC_DA_QI_VPA_EN)==0)
		{
			printk("[PMIC_DVT] VPA hw en test pass\n");
		}
	}


	printk("[PMIC_DVT] VSRAM_PROC sw en test :\n");
	pmic_set_register_value(PMIC_BUCK_VSRAM_PROC_EN_CTRL,0);
	pmic_set_register_value(PMIC_BUCK_VSRAM_PROC_EN,1);
	if(pmic_get_register_value(PMIC_DA_QI_VSRAM_PROC_EN)==1)
	{
		pmic_set_register_value(PMIC_BUCK_VSRAM_PROC_EN,0);
		if(pmic_get_register_value(PMIC_DA_QI_VSRAM_PROC_EN)==0)
		{
			printk("[PMIC_DVT] VSRAM_PROC sw en test pass\n");
		}
	}


	pmic_set_register_value(PMIC_BUCK_VSRAM_PROC_EN_CTRL,1);
	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_QI_VSRAM_PROC_EN)==1)
	{
		setSrclkenMode(0);
		if(pmic_get_register_value(PMIC_DA_QI_VSRAM_PROC_EN)==0)
		{
			printk("[PMIC_DVT] VSRAM_PROC hw en test pass\n");
		}
	}
}

void pmic_buck15plus17(void)
{
	kal_uint8 i;
	kal_uint8 value;

	printk("[PMIC_DVT] VCORE VOSEL test start\n");
	pmic_set_register_value(PMIC_BUCK_VCORE_VOSEL,0);
	pmic_set_register_value(PMIC_BUCK_VCORE_VOSEL_CTRL,0); //DEFAULT, LIMIT VOLTAGE
	/*
	Name         limit voltage
	VCORE        1.31V
	VGPU          1.31V
	VMODEM      1.31V
	VPA            3.4V
	VMD1          1.31V
	VSRAM_MD  1.31V
	VS1            2.2V
	VS2            1.5V
	*/

	for(i=0;i<128;i++)
	{
		pmic_set_register_value(PMIC_BUCK_VCORE_VOSEL,i);

		value=pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL_SYNC);
		if(i==value)
		{
			printk("[PMIC_DVT] VCORE VOSEL-SYNC %d %d Pass\n",i,pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL_SYNC));
			if (pmic_gray_code[i] == pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL))
				printk("[PMIC_DVT] VCORE Gray code %d is 0x%x Pass\n",pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL));
			else
				printk("[PMIC_DVT] VCORE Gray code %d is 0x%x Fail\n",pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL));
		}
		else
		{
			printk("[PMIC_DVT] VCORE VOSEL-SYNC %d %d %d %d %d Fail\n",i,value,
				pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL_SYNC));
		}


	}

	pmic_set_register_value(PMIC_BUCK_VCORE_VOSEL_CTRL,1);
	pmic_set_register_value(PMIC_BUCK_VCORE_VOSEL_SLEEP,10);
	pmic_set_register_value(PMIC_BUCK_VCORE_VOSEL_ON,50);

	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL_SYNC)!=10)
	{
		printk("[PMIC_DVT] VCORE VOSEL hw mode fail\n");
	}

	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL_SYNC)!=50)
	{
		printk("[PMIC_DVT] VCORE VOSEL hw mode fail\n");
	}
	printk("[PMIC_DVT] VCORE VOSEL test end. pass\n");

	printk("[PMIC_DVT] VGPU VOSEL test start\n");
	pmic_set_register_value(PMIC_BUCK_VGPU_VOSEL,0);
	pmic_set_register_value(PMIC_BUCK_VGPU_VOSEL_CTRL,0);

	for(i=0;i<128;i++)
	{
		pmic_set_register_value(PMIC_BUCK_VGPU_VOSEL,i);

		value=pmic_get_register_value(PMIC_DA_NI_VGPU_VOSEL_SYNC);
		if(i==value)
		{
			printk("[PMIC_DVT] VGPU VOSEL-SYNC %d %d Pass\n",i,pmic_get_register_value(PMIC_DA_NI_VGPU_VOSEL_SYNC));
			if (pmic_gray_code[i] == pmic_get_register_value(PMIC_DA_NI_VGPU_VOSEL))
				printk("[PMIC_DVT] VGPU Gray code %d is 0x%x Pass\n",pmic_get_register_value(PMIC_DA_NI_VGPU_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VGPU_VOSEL));
			else
				printk("[PMIC_DVT] VGPU Gray code %d is 0x%x Fail\n",pmic_get_register_value(PMIC_DA_NI_VGPU_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VGPU_VOSEL));

		}
		else
		{
			printk("[PMIC_DVT] VGPU VOSEL-SYNC %d %d %d %d %d Fail\n",i,value,
				pmic_get_register_value(PMIC_DA_NI_VGPU_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VGPU_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VGPU_VOSEL_SYNC));
		}


	}

	pmic_set_register_value(PMIC_BUCK_VGPU_VOSEL_CTRL,1);
	pmic_set_register_value(PMIC_BUCK_VGPU_VOSEL_SLEEP,10);
	pmic_set_register_value(PMIC_BUCK_VGPU_VOSEL_ON,50);

	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_NI_VGPU_VOSEL_SYNC)!=10)
	{
		printk("[PMIC_DVT] VGPU VOSEL hw mode fail\n");
	}

	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_NI_VGPU_VOSEL_SYNC)!=50)
	{
		printk("[PMIC_DVT] VGPU VOSEL hw mode fail\n");
	}
	printk("[PMIC_DVT] VGPU VOSEL test end. pass\n");

	printk("[PMIC_DVT] VMODEM VOSEL test start\n");
	pmic_set_register_value(PMIC_BUCK_VMODEM_VOSEL,0);
	pmic_set_register_value(PMIC_BUCK_VMODEM_VOSEL_CTRL,0);

	for(i=0;i<128;i++)
	{
		pmic_set_register_value(PMIC_BUCK_VMODEM_VOSEL,i);

		value=pmic_get_register_value(PMIC_DA_NI_VMODEM_VOSEL_SYNC);
		if(i==value)
		{
			printk("[PMIC_DVT] VMODEM VOSEL-SYNC %d %d Pass\n",i,pmic_get_register_value(PMIC_DA_NI_VMODEM_VOSEL_SYNC));
			if (pmic_gray_code[i] == pmic_get_register_value(PMIC_DA_NI_VMODEM_VOSEL))
				printk("[PMIC_DVT] VMODEM Gray code %d is 0x%x Pass\n",pmic_get_register_value(PMIC_DA_NI_VMODEM_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VMODEM_VOSEL));
			else
				printk("[PMIC_DVT] VMODEM Gray code %d is 0x%x Fail\n",pmic_get_register_value(PMIC_DA_NI_VMODEM_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VMODEM_VOSEL));

		}
		else
		{
			printk("[PMIC_DVT] VMODEM VOSEL-SYNC %d %d %d %d %d Fail\n",i,value,
				pmic_get_register_value(PMIC_DA_NI_VMODEM_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VMODEM_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VMODEM_VOSEL_SYNC));
		}


	}

	pmic_set_register_value(PMIC_BUCK_VMODEM_VOSEL_CTRL,1);
	pmic_set_register_value(PMIC_BUCK_VMODEM_VOSEL_SLEEP,10);
	pmic_set_register_value(PMIC_BUCK_VMODEM_VOSEL_ON,50);

	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_NI_VMODEM_VOSEL_SYNC)!=10)
	{
		printk("[PMIC_DVT] VMODEM VOSEL hw mode fail\n");
	}

	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_NI_VMODEM_VOSEL_SYNC)!=50)
	{
		printk("[PMIC_DVT] VMODEM VOSEL hw mode fail\n");
	}
	printk("[PMIC_DVT] VMODEM VOSEL test end. pass\n");

	printk("[PMIC_DVT] VMD1 VOSEL test start\n");
	pmic_set_register_value(PMIC_BUCK_VMD1_VOSEL,0);
	pmic_set_register_value(PMIC_BUCK_VMD1_VOSEL_CTRL,0);

	for(i=0;i<128;i++)
	{
		pmic_set_register_value(PMIC_BUCK_VMD1_VOSEL,i);

		value=pmic_get_register_value(PMIC_DA_NI_VMD1_VOSEL_SYNC);
		if(i==value)
		{
			printk("[PMIC_DVT] VMD1 VOSEL-SYNC %d %d Pass\n",i,pmic_get_register_value(PMIC_DA_NI_VMD1_VOSEL_SYNC));
			if (pmic_gray_code[i] == pmic_get_register_value(PMIC_DA_NI_VMD1_VOSEL))
				printk("[PMIC_DVT] VMD1 Gray code %d is 0x%x Pass\n",pmic_get_register_value(PMIC_DA_NI_VMD1_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VMD1_VOSEL));
			else
				printk("[PMIC_DVT] VMD1 Gray code %d is 0x%x Fail\n",pmic_get_register_value(PMIC_DA_NI_VMD1_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VMD1_VOSEL));

		}
		else
		{
			printk("[PMIC_DVT] VMD1 VOSEL-SYNC %d %d %d %d %d Fail\n",i,value,
				pmic_get_register_value(PMIC_DA_NI_VMD1_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VMD1_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VMD1_VOSEL_SYNC));
		}


	}

	pmic_set_register_value(PMIC_BUCK_VMD1_VOSEL_CTRL,1);
	pmic_set_register_value(PMIC_BUCK_VMD1_VOSEL_SLEEP,10);
	pmic_set_register_value(PMIC_BUCK_VMD1_VOSEL_ON,50);

	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_NI_VMD1_VOSEL_SYNC)!=10)
	{
		printk("[PMIC_DVT] VMD1 VOSEL hw mode fail\n");
	}

	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_NI_VMD1_VOSEL_SYNC)!=50)
	{
		printk("[PMIC_DVT] VMD1 VOSEL hw mode fail\n");
	}
	printk("[PMIC_DVT] VMD1 VOSEL test end. pass\n");

	printk("[PMIC_DVT] VSRAM_MD VOSEL test start\n");
	pmic_set_register_value(PMIC_BUCK_VSRAM_MD_VOSEL,0);
	pmic_set_register_value(PMIC_BUCK_VSRAM_MD_VOSEL_CTRL,0);

	for(i=0;i<128;i++)
	{
		pmic_set_register_value(PMIC_BUCK_VSRAM_MD_VOSEL,i);

		value=pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL_SYNC);
		if(i==value)
		{

			printk("[PMIC_DVT] VSRAM_MD VOSEL-SYNC %d %d Pass\n",i,pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL_SYNC));
			if (pmic_gray_code[i] == pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL))
				printk("[PMIC_DVT] VSRAM_MD Gray code %d is 0x%x Pass\n",pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL));
			else
				printk("[PMIC_DVT] VSRAM_MD Gray code %d is 0x%x Fail\n",pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL));

		}
		else
		{
			printk("[PMIC_DVT] VSRAM_MD VOSEL-SYNC %d %d %d %d %d Fail\n",i,value,
				pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL_SYNC));
		}


	}

	pmic_set_register_value(PMIC_BUCK_VSRAM_MD_VOSEL_CTRL,1);
	pmic_set_register_value(PMIC_BUCK_VSRAM_MD_VOSEL_SLEEP,10);
	pmic_set_register_value(PMIC_BUCK_VSRAM_MD_VOSEL_ON,50);

	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL_SYNC)!=10)
	{
		printk("[PMIC_DVT] VSRAM_MD VOSEL hw mode fail\n");
	}

	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL_SYNC)!=50)
	{
		printk("[PMIC_DVT] VSRAM_MD VOSEL hw mode fail\n");
	}
	printk("[PMIC_DVT] VSRAM_MD VOSEL test end. pass\n");

	printk("[PMIC_DVT] VS1 VOSEL test start\n");
	pmic_set_register_value(PMIC_BUCK_VS1_VOSEL,0);
	pmic_set_register_value(PMIC_BUCK_VS1_VOSEL_CTRL,0);

	for(i=0;i<128;i++)
	{
		pmic_set_register_value(PMIC_BUCK_VS1_VOSEL,i);

		value=pmic_get_register_value(PMIC_DA_NI_VS1_VOSEL_SYNC);
		if(i==value)
		{
			printk("[PMIC_DVT] VS1 VOSEL-SYNC %d %d Pass\n",i,pmic_get_register_value(PMIC_DA_NI_VS1_VOSEL_SYNC));
			if (pmic_gray_code[i] == pmic_get_register_value(PMIC_DA_NI_VS1_VOSEL))
				printk("[PMIC_DVT] VS1 Gray code %d is 0x%x Pass\n",pmic_get_register_value(PMIC_DA_NI_VS1_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VS1_VOSEL));
			else
				printk("[PMIC_DVT] VS1 Gray code %d is 0x%x Fail\n",pmic_get_register_value(PMIC_DA_NI_VS1_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VS1_VOSEL));

		}
		else
		{
			printk("[PMIC_DVT] VS1 VOSEL-SYNC %d %d %d %d %d Fail\n",i,value,
				pmic_get_register_value(PMIC_DA_NI_VS1_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VS1_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VS1_VOSEL_SYNC));
		}


	}

	pmic_set_register_value(PMIC_BUCK_VS1_VOSEL_CTRL,1);
	pmic_set_register_value(PMIC_BUCK_VS1_VOSEL_SLEEP,10);
	pmic_set_register_value(PMIC_BUCK_VS1_VOSEL_ON,50);

	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_NI_VS1_VOSEL_SYNC)!=10)
	{
		printk("[PMIC_DVT] VS1 VOSEL hw mode fail\n");
	}

	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_NI_VS1_VOSEL_SYNC)!=50)
	{
		printk("[PMIC_DVT] VS1 VOSEL hw mode fail\n");
	}
	printk("[PMIC_DVT] VS1 VOSEL test end. pass\n");

	printk("[PMIC_DVT] VS2 VOSEL test start\n");
	pmic_set_register_value(PMIC_BUCK_VS2_VOSEL,0);
	pmic_set_register_value(PMIC_BUCK_VS2_VOSEL_CTRL,0);

	for(i=0;i<128;i++)
	{
		pmic_set_register_value(PMIC_BUCK_VS2_VOSEL,i);

		value=pmic_get_register_value(PMIC_DA_NI_VS2_VOSEL_SYNC);
		if(i==value)
		{

			printk("[PMIC_DVT] VS2 VOSEL-SYNC %d %d Pass\n",i,pmic_get_register_value(PMIC_DA_NI_VS2_VOSEL_SYNC));
			if (pmic_gray_code[i] == pmic_get_register_value(PMIC_DA_NI_VS2_VOSEL))
				printk("[PMIC_DVT] VS2 Gray code %d is 0x%x Pass\n",pmic_get_register_value(PMIC_DA_NI_VS2_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VS2_VOSEL));
			else
				printk("[PMIC_DVT] VS2 Gray code %d is 0x%x Fail\n",pmic_get_register_value(PMIC_DA_NI_VS2_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VS2_VOSEL));

		}
		else
		{
			printk("[PMIC_DVT] VS2 VOSEL-SYNC %d %d %d %d %d Fail\n",i,value,
				pmic_get_register_value(PMIC_DA_NI_VS2_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VS2_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VS2_VOSEL_SYNC));
		}


	}

	pmic_set_register_value(PMIC_BUCK_VS2_VOSEL_CTRL,1);
	pmic_set_register_value(PMIC_BUCK_VS2_VOSEL_SLEEP,10);
	pmic_set_register_value(PMIC_BUCK_VS2_VOSEL_ON,50);

	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_NI_VS2_VOSEL_SYNC)!=10)
	{
		printk("[PMIC_DVT] VS2 VOSEL hw mode fail\n");
	}

	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_NI_VS2_VOSEL_SYNC)!=50)
	{
		printk("[PMIC_DVT] VS2 VOSEL hw mode fail\n");
	}
	printk("[PMIC_DVT] VS2 VOSEL test end. pass\n");

	printk("[PMIC_DVT] VPA VOSEL test start\n");
	pmic_set_register_value(PMIC_BUCK_VPA_VOSEL,0);
	pmic_set_register_value(PMIC_BUCK_VPA_VOSEL_CTRL,0);

	for(i=0;i<128;i++)
	{
		pmic_set_register_value(PMIC_BUCK_VPA_VOSEL,i);

		value=pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_SYNC);
		if(i==value)
		{
			printk("[PMIC_DVT] VPA VOSEL-SYNC %d %d Pass\n",i,pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_SYNC));
			if (pmic_gray_code[i] == pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_GRAY))
				printk("[PMIC_DVT] VPA Gray code %d is 0x%x Pass\n",pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_GRAY));
			else
				printk("[PMIC_DVT] VPA Gray code %d is 0x%x Fail\n",pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_GRAY));
		}
		else
		{
			printk("[PMIC_DVT] VPA VOSEL-SYNC %d %d %d %d %d Fail\n",i,value,
				pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_SYNC));
		}


	}

	pmic_set_register_value(PMIC_BUCK_VPA_VOSEL_CTRL,1);
	pmic_set_register_value(PMIC_BUCK_VPA_VOSEL_SLEEP,10);
	pmic_set_register_value(PMIC_BUCK_VPA_VOSEL_ON,50);

	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_SYNC)!=10)
	{
		printk("[PMIC_DVT] VPA VOSEL hw mode fail\n");
	}

	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_SYNC)!=50)
	{
		printk("[PMIC_DVT] VPA VOSEL hw mode fail\n");
	}
	printk("[PMIC_DVT] VPA VOSEL test end. pass\n");

	printk("[PMIC_DVT] VSRAM_PROC VOSEL test start\n");
	pmic_set_register_value(PMIC_BUCK_VSRAM_PROC_VOSEL,0);
	pmic_set_register_value(PMIC_BUCK_VSRAM_PROC_VOSEL_CTRL,0);

	for(i=0;i<128;i++)
	{
		pmic_set_register_value(PMIC_BUCK_VSRAM_PROC_VOSEL,i);

		value=pmic_get_register_value(PMIC_DA_NI_VSRAM_PROC_VOSEL_SYNC);
		if(i==value)
		{
			printk("[PMIC_DVT] VSRAM_PROC VOSEL-SYNC %d %d Pass\n",i,pmic_get_register_value(PMIC_DA_NI_VSRAM_PROC_VOSEL_SYNC));
			if (pmic_gray_code[i] == pmic_get_register_value(PMIC_DA_NI_VSRAM_PROC_VOSEL))
				printk("[PMIC_DVT] VSRAM_PROC Gray code %d is 0x%x Pass\n",pmic_get_register_value(PMIC_DA_NI_VSRAM_PROC_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VSRAM_PROC_VOSEL));
			else
				printk("[PMIC_DVT] VSRAM_PROC Gray code %d is 0x%x Fail\n",pmic_get_register_value(PMIC_DA_NI_VSRAM_PROC_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VSRAM_PROC_VOSEL));
		}
		else
		{
			printk("[PMIC_DVT] VSRAM_PROC VOSEL-SYNC %d %d %d %d %d Fail\n",i,value,
				pmic_get_register_value(PMIC_DA_NI_VSRAM_PROC_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VSRAM_PROC_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VSRAM_PROC_VOSEL_SYNC));
		}


	}

	pmic_set_register_value(PMIC_BUCK_VSRAM_PROC_VOSEL_CTRL,1);
	pmic_set_register_value(PMIC_BUCK_VSRAM_PROC_VOSEL_SLEEP,10);
	pmic_set_register_value(PMIC_BUCK_VSRAM_PROC_VOSEL_ON,50);

	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_NI_VSRAM_PROC_VOSEL_SYNC)!=10)
	{
		printk("[PMIC_DVT] VSRAM_PROC VOSEL hw mode fail\n");
	}

	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_NI_VSRAM_PROC_VOSEL_SYNC)!=50)
	{
		printk("[PMIC_DVT] VSRAM_PROC VOSEL hw mode fail\n");
	}
	printk("[PMIC_DVT] VSRAM_PROC VOSEL test end. pass\n");
}

void pmic_buck18(void)
{
	kal_uint8 i;
	kal_uint8 value;

	/* enable full code */
	pmic_set_register_value(PMIC_RG_SKIP_OTP_OUT,1);
	pmic_set_register_value(PMIC_RG_OTP_DOUT_368_383,1); // FULL VOLTAGE

	printk("[PMIC_DVT] VCORE VOSEL test start\n");
	pmic_set_register_value(PMIC_BUCK_VCORE_VOSEL,0);
	pmic_set_register_value(PMIC_BUCK_VCORE_VOSEL_CTRL,0);

	for(i=0;i<128;i++)
	{
		pmic_set_register_value(PMIC_BUCK_VCORE_VOSEL,i);

		value=pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL_SYNC);
		if(i==value)
		{
			printk("[PMIC_DVT] VCORE VOSEL-SYNC %d %d Pass\n",i,pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL_SYNC));
			if (pmic_gray_code[i] == pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL))
				printk("[PMIC_DVT] VCORE Gray code %d is 0x%x Pass\n",pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL));
			else
				printk("[PMIC_DVT] VCORE Gray code %d is 0x%x Fail\n",pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL));
		}
		else
		{
			printk("[PMIC_DVT] VCORE VOSEL-SYNC %d %d %d %d %d Fail\n",i,value,
				pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL_SYNC));
		}


	}

	pmic_set_register_value(PMIC_BUCK_VCORE_VOSEL_CTRL,1);
	pmic_set_register_value(PMIC_BUCK_VCORE_VOSEL_SLEEP,10);
	pmic_set_register_value(PMIC_BUCK_VCORE_VOSEL_ON,50);

	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL_SYNC)!=10)
	{
		printk("[PMIC_DVT] VCORE VOSEL hw mode fail\n");
	}

	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL_SYNC)!=50)
	{
		printk("[PMIC_DVT] VCORE VOSEL hw mode fail\n");
	}
	printk("[PMIC_DVT] VCORE VOSEL test end. pass\n");

	printk("[PMIC_DVT] VGPU VOSEL test start\n");
	pmic_set_register_value(PMIC_BUCK_VGPU_VOSEL,0);
	pmic_set_register_value(PMIC_BUCK_VGPU_VOSEL_CTRL,0);

	for(i=0;i<128;i++)
	{
		pmic_set_register_value(PMIC_BUCK_VGPU_VOSEL,i);

		value=pmic_get_register_value(PMIC_DA_NI_VGPU_VOSEL_SYNC);
		if(i==value)
		{
			printk("[PMIC_DVT] VGPU VOSEL-SYNC %d %d Pass\n",i,pmic_get_register_value(PMIC_DA_NI_VGPU_VOSEL_SYNC));
			if (pmic_gray_code[i] == pmic_get_register_value(PMIC_DA_NI_VGPU_VOSEL))
				printk("[PMIC_DVT] VGPU Gray code %d is 0x%x Pass\n",pmic_get_register_value(PMIC_DA_NI_VGPU_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VGPU_VOSEL));
			else
				printk("[PMIC_DVT] VGPU Gray code %d is 0x%x Fail\n",pmic_get_register_value(PMIC_DA_NI_VGPU_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VGPU_VOSEL));

		}
		else
		{
			printk("[PMIC_DVT] VGPU VOSEL-SYNC %d %d %d %d %d Fail\n",i,value,
				pmic_get_register_value(PMIC_DA_NI_VGPU_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VGPU_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VGPU_VOSEL_SYNC));
		}


	}

	pmic_set_register_value(PMIC_BUCK_VGPU_VOSEL_CTRL,1);
	pmic_set_register_value(PMIC_BUCK_VGPU_VOSEL_SLEEP,10);
	pmic_set_register_value(PMIC_BUCK_VGPU_VOSEL_ON,50);

	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_NI_VGPU_VOSEL_SYNC)!=10)
	{
		printk("[PMIC_DVT] VGPU VOSEL hw mode fail\n");
	}

	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_NI_VGPU_VOSEL_SYNC)!=50)
	{
		printk("[PMIC_DVT] VGPU VOSEL hw mode fail\n");
	}
	printk("[PMIC_DVT] VGPU VOSEL test end. pass\n");

	printk("[PMIC_DVT] VMODEM VOSEL test start\n");
	pmic_set_register_value(PMIC_BUCK_VMODEM_VOSEL,0);
	pmic_set_register_value(PMIC_BUCK_VMODEM_VOSEL_CTRL,0);

	for(i=0;i<128;i++)
	{
		pmic_set_register_value(PMIC_BUCK_VMODEM_VOSEL,i);

		value=pmic_get_register_value(PMIC_DA_NI_VMODEM_VOSEL_SYNC);
		if(i==value)
		{
			printk("[PMIC_DVT] VMODEM VOSEL-SYNC %d %d Pass\n",i,pmic_get_register_value(PMIC_DA_NI_VMODEM_VOSEL_SYNC));
			if (pmic_gray_code[i] == pmic_get_register_value(PMIC_DA_NI_VMODEM_VOSEL))
				printk("[PMIC_DVT] VMODEM Gray code %d is 0x%x Pass\n",pmic_get_register_value(PMIC_DA_NI_VMODEM_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VMODEM_VOSEL));
			else
				printk("[PMIC_DVT] VMODEM Gray code %d is 0x%x Fail\n",pmic_get_register_value(PMIC_DA_NI_VMODEM_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VMODEM_VOSEL));

		}
		else
		{
			printk("[PMIC_DVT] VMODEM VOSEL-SYNC %d %d %d %d %d Fail\n",i,value,
				pmic_get_register_value(PMIC_DA_NI_VMODEM_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VMODEM_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VMODEM_VOSEL_SYNC));
		}


	}

	pmic_set_register_value(PMIC_BUCK_VMODEM_VOSEL_CTRL,1);
	pmic_set_register_value(PMIC_BUCK_VMODEM_VOSEL_SLEEP,10);
	pmic_set_register_value(PMIC_BUCK_VMODEM_VOSEL_ON,50);

	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_NI_VMODEM_VOSEL_SYNC)!=10)
	{
		printk("[PMIC_DVT] VMODEM VOSEL hw mode fail\n");
	}

	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_NI_VMODEM_VOSEL_SYNC)!=50)
	{
		printk("[PMIC_DVT] VMODEM VOSEL hw mode fail\n");
	}
	printk("[PMIC_DVT] VMODEM VOSEL test end. pass\n");

	printk("[PMIC_DVT] VMD1 VOSEL test start\n");
	pmic_set_register_value(PMIC_BUCK_VMD1_VOSEL,0);
	pmic_set_register_value(PMIC_BUCK_VMD1_VOSEL_CTRL,0);

	for(i=0;i<128;i++)
	{
		pmic_set_register_value(PMIC_BUCK_VMD1_VOSEL,i);

		value=pmic_get_register_value(PMIC_DA_NI_VMD1_VOSEL_SYNC);
		if(i==value)
		{
			printk("[PMIC_DVT] VMD1 VOSEL-SYNC %d %d Pass\n",i,pmic_get_register_value(PMIC_DA_NI_VMD1_VOSEL_SYNC));
			if (pmic_gray_code[i] == pmic_get_register_value(PMIC_DA_NI_VMD1_VOSEL))
				printk("[PMIC_DVT] VMD1 Gray code %d is 0x%x Pass\n",pmic_get_register_value(PMIC_DA_NI_VMD1_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VMD1_VOSEL));
			else
				printk("[PMIC_DVT] VMD1 Gray code %d is 0x%x Fail\n",pmic_get_register_value(PMIC_DA_NI_VMD1_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VMD1_VOSEL));

		}
		else
		{
			printk("[PMIC_DVT] VMD1 VOSEL-SYNC %d %d %d %d %d Fail\n",i,value,
				pmic_get_register_value(PMIC_DA_NI_VMD1_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VMD1_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VMD1_VOSEL_SYNC));
		}


	}

	pmic_set_register_value(PMIC_BUCK_VMD1_VOSEL_CTRL,1);
	pmic_set_register_value(PMIC_BUCK_VMD1_VOSEL_SLEEP,10);
	pmic_set_register_value(PMIC_BUCK_VMD1_VOSEL_ON,50);

	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_NI_VMD1_VOSEL_SYNC)!=10)
	{
		printk("[PMIC_DVT] VMD1 VOSEL hw mode fail\n");
	}

	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_NI_VMD1_VOSEL_SYNC)!=50)
	{
		printk("[PMIC_DVT] VMD1 VOSEL hw mode fail\n");
	}
	printk("[PMIC_DVT] VMD1 VOSEL test end. pass\n");

	printk("[PMIC_DVT] VSRAM_MD VOSEL test start\n");
	pmic_set_register_value(PMIC_BUCK_VSRAM_MD_VOSEL,0);
	pmic_set_register_value(PMIC_BUCK_VSRAM_MD_VOSEL_CTRL,0);

	for(i=0;i<128;i++)
	{
		pmic_set_register_value(PMIC_BUCK_VSRAM_MD_VOSEL,i);

		value=pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL_SYNC);
		if(i==value)
		{

			printk("[PMIC_DVT] VSRAM_MD VOSEL-SYNC %d %d Pass\n",i,pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL_SYNC));
			if (pmic_gray_code[i] == pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL))
				printk("[PMIC_DVT] VSRAM_MD Gray code %d is 0x%x Pass\n",pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL));
			else
				printk("[PMIC_DVT] VSRAM_MD Gray code %d is 0x%x Fail\n",pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL));

		}
		else
		{
			printk("[PMIC_DVT] VSRAM_MD VOSEL-SYNC %d %d %d %d %d Fail\n",i,value,
				pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL_SYNC));
		}


	}

	pmic_set_register_value(PMIC_BUCK_VSRAM_MD_VOSEL_CTRL,1);
	pmic_set_register_value(PMIC_BUCK_VSRAM_MD_VOSEL_SLEEP,10);
	pmic_set_register_value(PMIC_BUCK_VSRAM_MD_VOSEL_ON,50);

	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL_SYNC)!=10)
	{
		printk("[PMIC_DVT] VSRAM_MD VOSEL hw mode fail\n");
	}

	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL_SYNC)!=50)
	{
		printk("[PMIC_DVT] VSRAM_MD VOSEL hw mode fail\n");
	}
	printk("[PMIC_DVT] VSRAM_MD VOSEL test end. pass\n");

	printk("[PMIC_DVT] VS1 VOSEL test start\n");
	pmic_set_register_value(PMIC_BUCK_VS1_VOSEL,0);
	pmic_set_register_value(PMIC_BUCK_VS1_VOSEL_CTRL,0);

	for(i=0;i<128;i++)
	{
		pmic_set_register_value(PMIC_BUCK_VS1_VOSEL,i);

		value=pmic_get_register_value(PMIC_DA_NI_VS1_VOSEL_SYNC);
		if(i==value)
		{
			printk("[PMIC_DVT] VS1 VOSEL-SYNC %d %d Pass\n",i,pmic_get_register_value(PMIC_DA_NI_VS1_VOSEL_SYNC));
			if (pmic_gray_code[i] == pmic_get_register_value(PMIC_DA_NI_VS1_VOSEL))
				printk("[PMIC_DVT] VS1 Gray code %d is 0x%x Pass\n",pmic_get_register_value(PMIC_DA_NI_VS1_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VS1_VOSEL));
			else
				printk("[PMIC_DVT] VS1 Gray code %d is 0x%x Fail\n",pmic_get_register_value(PMIC_DA_NI_VS1_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VS1_VOSEL));

		}
		else
		{
			printk("[PMIC_DVT] VS1 VOSEL-SYNC %d %d %d %d %d Fail\n",i,value,
				pmic_get_register_value(PMIC_DA_NI_VS1_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VS1_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VS1_VOSEL_SYNC));
		}


	}

	pmic_set_register_value(PMIC_BUCK_VS1_VOSEL_CTRL,1);
	pmic_set_register_value(PMIC_BUCK_VS1_VOSEL_SLEEP,10);
	pmic_set_register_value(PMIC_BUCK_VS1_VOSEL_ON,50);

	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_NI_VS1_VOSEL_SYNC)!=10)
	{
		printk("[PMIC_DVT] VS1 VOSEL hw mode fail\n");
	}

	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_NI_VS1_VOSEL_SYNC)!=50)
	{
		printk("[PMIC_DVT] VS1 VOSEL hw mode fail\n");
	}
	printk("[PMIC_DVT] VS1 VOSEL test end. pass\n");

	printk("[PMIC_DVT] VS2 VOSEL test start\n");
	pmic_set_register_value(PMIC_BUCK_VS2_VOSEL,0);
	pmic_set_register_value(PMIC_BUCK_VS2_VOSEL_CTRL,0);

	for(i=0;i<128;i++)
	{
		pmic_set_register_value(PMIC_BUCK_VS2_VOSEL,i);

		value=pmic_get_register_value(PMIC_DA_NI_VS2_VOSEL_SYNC);
		if(i==value)
		{

			printk("[PMIC_DVT] VS2 VOSEL-SYNC %d %d Pass\n",i,pmic_get_register_value(PMIC_DA_NI_VS2_VOSEL_SYNC));
			if (pmic_gray_code[i] == pmic_get_register_value(PMIC_DA_NI_VS2_VOSEL))
				printk("[PMIC_DVT] VS2 Gray code %d is 0x%x Pass\n",pmic_get_register_value(PMIC_DA_NI_VS2_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VS2_VOSEL));
			else
				printk("[PMIC_DVT] VS2 Gray code %d is 0x%x Fail\n",pmic_get_register_value(PMIC_DA_NI_VS2_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VS2_VOSEL));

		}
		else
		{
			printk("[PMIC_DVT] VS2 VOSEL-SYNC %d %d %d %d %d Fail\n",i,value,
				pmic_get_register_value(PMIC_DA_NI_VS2_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VS2_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VS2_VOSEL_SYNC));
		}


	}

	pmic_set_register_value(PMIC_BUCK_VS2_VOSEL_CTRL,1);
	pmic_set_register_value(PMIC_BUCK_VS2_VOSEL_SLEEP,10);
	pmic_set_register_value(PMIC_BUCK_VS2_VOSEL_ON,50);

	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_NI_VS2_VOSEL_SYNC)!=10)
	{
		printk("[PMIC_DVT] VS2 VOSEL hw mode fail\n");
	}

	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_NI_VS2_VOSEL_SYNC)!=50)
	{
		printk("[PMIC_DVT] VS2 VOSEL hw mode fail\n");
	}
	printk("[PMIC_DVT] VS2 VOSEL test end. pass\n");

	printk("[PMIC_DVT] VPA VOSEL test start\n");
	pmic_set_register_value(PMIC_BUCK_VPA_VOSEL,0);
	pmic_set_register_value(PMIC_BUCK_VPA_VOSEL_CTRL,0);

	for(i=0;i<128;i++)
	{
		pmic_set_register_value(PMIC_BUCK_VPA_VOSEL,i);

		value=pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_SYNC);
		if(i==value)
		{
			printk("[PMIC_DVT] VPA VOSEL-SYNC %d %d Pass\n",i,pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_SYNC));
			if (pmic_gray_code[i] == pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_GRAY))
				printk("[PMIC_DVT] VPA Gray code %d is 0x%x Pass\n",pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_GRAY));
			else
				printk("[PMIC_DVT] VPA Gray code %d is 0x%x Fail\n",pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_GRAY));
		}
		else
		{
			printk("[PMIC_DVT] VPA VOSEL-SYNC %d %d %d %d %d Fail\n",i,value,
				pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_SYNC));
		}


	}

	pmic_set_register_value(PMIC_BUCK_VPA_VOSEL_CTRL,1);
	pmic_set_register_value(PMIC_BUCK_VPA_VOSEL_SLEEP,10);
	pmic_set_register_value(PMIC_BUCK_VPA_VOSEL_ON,50);

	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_SYNC)!=10)
	{
		printk("[PMIC_DVT] VPA VOSEL hw mode fail\n");
	}

	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_SYNC)!=50)
	{
		printk("[PMIC_DVT] VPA VOSEL hw mode fail\n");
	}
	printk("[PMIC_DVT] VPA VOSEL test end. pass\n");

	printk("[PMIC_DVT] VSRAM_PROC VOSEL test start\n");
	pmic_set_register_value(PMIC_BUCK_VSRAM_PROC_VOSEL,0);
	pmic_set_register_value(PMIC_BUCK_VSRAM_PROC_VOSEL_CTRL,0);

	for(i=0;i<128;i++)
	{
		pmic_set_register_value(PMIC_BUCK_VSRAM_PROC_VOSEL,i);

		value=pmic_get_register_value(PMIC_DA_NI_VSRAM_PROC_VOSEL_SYNC);
		if(i==value)
		{
			printk("[PMIC_DVT] VSRAM_PROC VOSEL-SYNC %d %d Pass\n",i,pmic_get_register_value(PMIC_DA_NI_VSRAM_PROC_VOSEL_SYNC));
			if (pmic_gray_code[i] == pmic_get_register_value(PMIC_DA_NI_VSRAM_PROC_VOSEL))
				printk("[PMIC_DVT] VSRAM_PROC Gray code %d is 0x%x Pass\n",pmic_get_register_value(PMIC_DA_NI_VSRAM_PROC_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VSRAM_PROC_VOSEL));
			else
				printk("[PMIC_DVT] VSRAM_PROC Gray code %d is 0x%x Fail\n",pmic_get_register_value(PMIC_DA_NI_VSRAM_PROC_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VSRAM_PROC_VOSEL));
		}
		else
		{
			printk("[PMIC_DVT] VSRAM_PROC VOSEL-SYNC %d %d %d %d %d Fail\n",i,value,
				pmic_get_register_value(PMIC_DA_NI_VSRAM_PROC_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VSRAM_PROC_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VSRAM_PROC_VOSEL_SYNC));
		}


	}

	pmic_set_register_value(PMIC_BUCK_VSRAM_PROC_VOSEL_CTRL,1);
	pmic_set_register_value(PMIC_BUCK_VSRAM_PROC_VOSEL_SLEEP,10);
	pmic_set_register_value(PMIC_BUCK_VSRAM_PROC_VOSEL_ON,50);

	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_NI_VSRAM_PROC_VOSEL_SYNC)!=10)
	{
		printk("[PMIC_DVT] VSRAM_PROC VOSEL hw mode fail\n");
	}

	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_NI_VSRAM_PROC_VOSEL_SYNC)!=50)
	{
		printk("[PMIC_DVT] VSRAM_PROC VOSEL hw mode fail\n");
	}
	printk("[PMIC_DVT] VSRAM_PROC VOSEL test end. pass\n");
}

void pmic_buck_vcore01(void)
{
	printk("[PMIC_DVT] VCORE voice wake up test start\n");
	pmic_set_register_value(PMIC_BUCK_VCORE_VOSEL_CTRL,1);
	pmic_set_register_value(PMIC_RG_VOWEN_MODE,1); //SW MODE
	pmic_set_register_value(PMIC_BUCK_VCORE_VOSEL_SLEEP,10);

	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL_SYNC)!=10)
	{
		printk("[PMIC_DVT] VCORE VOSEL hw mode fail\n");
	}

	pmic_set_register_value(PMIC_RG_VOWEN_SW,1); //status = voice wakeup
	pmic_set_register_value(PMIC_BUCK_VCORE_VOSEL_AUD,50);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL_SYNC)!=50)
	{
		printk("[PMIC_DVT] VCORE VOSEL hw mode check VOSEL_AUD Fail %d %d %d\n", pmic_get_register_value(PMIC_BUCK_VCORE_DVFS_DONE),
														pmic_get_register_value(PMIC_BUCK_VCORE_VOSEL_AUD),
														pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL_SYNC));

	}
	else
	{
		printk("[PMIC_DVT] VCORE VOSEL hw mode check VOSEL_AUD Pass %d %d %d\n", pmic_get_register_value(PMIC_BUCK_VCORE_DVFS_DONE),
														pmic_get_register_value(PMIC_BUCK_VCORE_VOSEL_AUD),
														pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL_SYNC));
	}
	pmic_set_register_value(PMIC_BUCK_VCORE_VOSEL_ON,70);

	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL_SYNC)!=70)
	{
		printk("[PMIC_DVT] VCORE VOSEL hw mode check VOSEL_ON Fail %d %d %d\n", pmic_get_register_value(PMIC_BUCK_VCORE_DVFS_DONE),
														pmic_get_register_value(PMIC_BUCK_VCORE_VOSEL_ON),
														pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL_SYNC));

	}
	else
	{
		printk("[PMIC_DVT] VCORE VOSEL hw mode check VOSEL_ON Pass %d %d %d\n", pmic_get_register_value(PMIC_BUCK_VCORE_DVFS_DONE),
														pmic_get_register_value(PMIC_BUCK_VCORE_VOSEL_ON),
														pmic_get_register_value(PMIC_DA_NI_VCORE_VOSEL_SYNC));
	}
	printk("[PMIC_DVT] VCORE VOSEL test end. pass\n");



}

void pmic_buck_vsram_md01(void)
{
	printk("[PMIC_DVT] VCORE voice wake up test start\n");
	pmic_set_register_value(PMIC_BUCK_VSRAM_MD_VOSEL_CTRL,1);
	pmic_set_register_value(PMIC_RG_VOWEN_MODE,1); //SW MODE
	pmic_set_register_value(PMIC_BUCK_VSRAM_MD_VOSEL_SLEEP,10);

	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL_SYNC)!=10)
	{
		printk("[PMIC_DVT] VSRAM_MD VOSEL hw mode fail\n");
	}

	pmic_set_register_value(PMIC_RG_VOWEN_SW,1); //status = voice wakeup
	pmic_set_register_value(PMIC_BUCK_VSRAM_MD_VOSEL_AUD,50);
	setSrclkenMode(0);
	if(pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL_SYNC)!=50)
	{
		printk("[PMIC_DVT] VSRAM_MD VOSEL hw mode check VOSEL_AUD Fail %d %d %d\n", pmic_get_register_value(PMIC_BUCK_VSRAM_MD_DVFS_DONE),
														pmic_get_register_value(PMIC_BUCK_VSRAM_MD_VOSEL_AUD),
														pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL_SYNC));

	}
	else
	{
		printk("[PMIC_DVT] VSRAM_MD VOSEL hw mode check VOSEL_AUD Pass %d %d %d\n", pmic_get_register_value(PMIC_BUCK_VSRAM_MD_DVFS_DONE),
														pmic_get_register_value(PMIC_BUCK_VSRAM_MD_VOSEL_AUD),
														pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL_SYNC));
	}
	pmic_set_register_value(PMIC_BUCK_VSRAM_MD_VOSEL_ON,70);

	setSrclkenMode(1);
	if(pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL_SYNC)!=70)
	{
		printk("[PMIC_DVT] VSRAM_MD VOSEL hw mode check VOSEL_ON Fail %d %d %d\n", pmic_get_register_value(PMIC_BUCK_VSRAM_MD_DVFS_DONE),
														pmic_get_register_value(PMIC_BUCK_VSRAM_MD_VOSEL_ON),
														pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL_SYNC));

	}
	else
	{
		printk("[PMIC_DVT] VSRAM_MD VOSEL hw mode check VOSEL_ON Pass %d %d %d\n", pmic_get_register_value(PMIC_BUCK_VSRAM_MD_DVFS_DONE),
														pmic_get_register_value(PMIC_BUCK_VSRAM_MD_VOSEL_ON),
														pmic_get_register_value(PMIC_DA_NI_VSRAM_MD_VOSEL_SYNC));
	}
	printk("[PMIC_DVT] VSRAM_MD VOSEL test end. pass\n");



}



//vproc/vcore1/vsys22/vltetest
void pmic_buck_vpa01(void)
{
	kal_uint8 i;
	kal_uint8 value;
	kal_uint8 vpass=0;

	for(i=0;i<4;i++)
	{
		pmic_set_register_value(PMIC_BUCK_VPA_DLC,i);

		if(pmic_get_register_value(PMIC_DA_NI_VPA_DLC)!=i)
		{
			printk("[PMIC_DVT] VPA DLC test fail %d !=%d \n",i,pmic_get_register_value(PMIC_DA_NI_VPA_DLC));
			vpass=1;
		}
	}
	if(vpass==0)
	{
		printk("[PMIC_DVT] VPA DLC test pass \n");
	}

	printk("[PMIC_DVT] VPA VOSEL test start\n");
	pmic_set_register_value(PMIC_BUCK_VPA_VOSEL,0);
	pmic_set_register_value(PMIC_BUCK_VPA_VOSEL_CTRL,0);
	pmic_set_register_value(PMIC_BUCK_VPA_DLC_MAP_EN,1);

	printk("[PMIC_DVT] VPA DLC111:%d DLC011:%d DLC001:%d \n",
		pmic_get_register_value(PMIC_BUCK_VPA_VOSEL_DLC111),
		pmic_get_register_value(PMIC_BUCK_VPA_VOSEL_DLC011),
		pmic_get_register_value(PMIC_BUCK_VPA_VOSEL_DLC001));

	for(i=0;i<0x3F;i++)
	{
		pmic_set_register_value(PMIC_BUCK_VPA_VOSEL,i);

		value=pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_SYNC);
		if(i==value)
		{
			printk("[PMIC_DVT] VPA VOSEL-SYNC %d %d VPA_DLC:%d Pass\n",i,pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_SYNC),pmic_get_register_value(PMIC_DA_NI_VPA_DLC));
			if (pmic_gray_code[i] == pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_GRAY))
				printk("[PMIC_DVT] VPA Gray code %d is 0x%x Pass\n",pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_GRAY));
			else
				printk("[PMIC_DVT] VPA Gray code %d is 0x%x Fail\n",pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_SYNC), pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_GRAY));
		}
		else
		{
			printk("[PMIC_DVT] VPA VOSEL-SYNC %d %d %d %d %d Fail\n",i,value,
				pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_SYNC),
				pmic_get_register_value(PMIC_DA_NI_VPA_VOSEL_SYNC));
		}


	}



}

#ifdef BIF_DVT_ENABLE
int g_loop_out = 50;

void bif_check_read_data(int r_num, int *data)
{
    U32 ret=0;
    U32 reg_val=0;
    int i=0,flag_enum_offset=0;

	for (i = 0;i < r_num;i ++)
	{
	    //ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 15);
	    reg_val=pmic_get_register_value(PMIC_BIF_ERROR_0+flag_enum_offset);
		if (reg_val == 0)
	        printk("[PMIC_DVT][Pass] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
		else
			PMICDBG(0, "[PMIC_DVT][Fail] BIF_DATA_ERROR_0 (0) i=%d flag_enum_offset=%d : BIF_CON20[15]=0x%x\n", i, flag_enum_offset, reg_val);
//			printk("[PMIC_DVT][Fail] BIF_DATA_ERROR_0 (0) i=%d j=%d : BIF_CON20[15]=0x%x\n", i, j, reg_val);
	    //ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 8);
	    reg_val=pmic_get_register_value(PMIC_BIF_ACK_0+flag_enum_offset);
		if (reg_val == 0x1)
	    	printk("[PMIC_DVT][Pass] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
	    else
			PMICDBG(0, "[PMIC_DVT][Pass] BIF_ACK_0 (1) i=%d flag_enum_offset=%d  : BIF_CON20[8]=0x%x\n", i, flag_enum_offset, reg_val);
//			printk("[PMIC_DVT][Pass] BIF_ACK_0 (1) i=%d j=%d  : BIF_CON20[8]=0x%x\n", i, j, reg_val);
	    //ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0xFF, 0);
	    reg_val=pmic_get_register_value(PMIC_BIF_DATA_0+flag_enum_offset);
		if (reg_val == *(data+i))
	        printk("[PMIC_DVT][Pass] BIF_DATA_0 (0x%x) : BIF_CON20[7:0]=0x%x\n", *(data+i), reg_val);
		else
			PMICDBG(0, "[PMIC_DVT][Fail] BIF_DATA_0 (0x%x) i=%d flag_enum_offset=%d  : BIF_CON20[7:0]=0x%x\n", *(data+i), i, flag_enum_offset, reg_val);
//			printk("[PMIC_DVT][Fail] BIF_DATA_0 (0x%x) i=%d j=%d  : BIF_CON20[7:0]=0x%x\n", *(data+i), i, j, reg_val);
		if (i == 0)
		{
			nvmreg_15_8 = reg_val;
			screg_15_8 = reg_val;
			PMICDBG(0, "[PMIC_DVT][Pass] BIF_DATA_0 (%d) : nvmreg_15_8=0x%x\n", i, pcap_15_8);
		}
		if (i == 1)
		{
			nvmreg_7_0 = reg_val;
			screg_7_0 = reg_val;
			PMICDBG(0, "[PMIC_DVT][Pass] BIF_DATA_0 (%d) : nvmreg_7_0=0x%x\n", i, pcap_15_8);
		}
		if (i == 2)
		{
			nvmcap_15_8 = pcap_15_8 = reg_val;
			sccap_15_8 = reg_val;
			PMICDBG(0, "[PMIC_DVT][Pass] BIF_DATA_0 (%d) : nvmcap_15_8=0x%x pcap_15_8=0x%x\n", i, nvmcap_15_8, pcap_15_8);
			//printk("[PMIC_DVT][Pass] BIF_DATA_0 (%d) : pcap_15_8=0x%x\n", i, pcap_15_8);
		}
		if (i == 3)
		{
			nvmcap_7_0 = pcap_7_0 = reg_val;
			sccap_7_0 = reg_val;
			PMICDBG(0, "[PMIC_DVT][Pass] BIF_DATA_0 (%d) : nvmcap_15_8=0x%x pcap_15_8=0x%x\n", i, nvmcap_15_8, pcap_7_0);
			//printk("[PMIC_DVT][Pass] BIF_DATA_0 (%d) : pcap_15_8=0x%x\n", i, pcap_7_0);
		}
		flag_enum_offset+=3;
	}
}
void reset_bif_irq(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int loop_i=0;

    //ret=pmic_config_interface(MT6325_BIF_CON31, 0x1, 0x1, 1);
	ret = pmic_set_register_value(PMIC_BIF_IRQ_CLR, 1);
    printk("[PMIC_DVT]reset BIF_IRQ : BIF_IRQ=%x BIF_IRQ_CLR=%x\n", pmic_get_register_value(PMIC_BIF_IRQ), pmic_get_register_value(PMIC_BIF_IRQ_CLR));

    reg_val=0;
    do
    {
        //ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x1, 11);
		reg_val = pmic_get_register_value(PMIC_BIF_IRQ);
        if(loop_i++ > g_loop_out) break;
    }while(reg_val != 0);

	printk("[PMIC_DVT]polling BIF_IRQ : BIF_CON31[11]=0x%x %d\n", pmic_get_register_value(PMIC_BIF_IRQ), loop_i);
    //ret=pmic_config_interface(MT6325_BIF_CON31, 0x0, 0x1, 1);
	ret = pmic_set_register_value(PMIC_BIF_IRQ_CLR, 0);
	printk("[PMIC_DVT]PMIC_BIF_IRQ=%x BIF_IRQ_CLR=%x\n", pmic_get_register_value(PMIC_BIF_IRQ), pmic_get_register_value(PMIC_BIF_IRQ_CLR));
}

void set_bif_cmd(int bif_cmd[], int bif_cmd_len)
{
    int i=0;
    int con_index=0;
    U32 ret=0;

    for(i=0;i<bif_cmd_len;i++)
    {
        ret=pmic_config_interface(MT6351_BIF_CON0+con_index, bif_cmd[i], 0x07FF, 0);
        con_index += 0x2;
    	printk("[PMIC_DVT]BIF_COMMAND_%d Reg[0x%x]=0x%x\n", i,
        pmu_flags_table[PMIC_BIF_COMMAND_0+i].offset, pmic_get_register_value(PMIC_BIF_COMMAND_0 + i));
    }
}

void check_bat_lost(void)
{
    U32 ret=0;
    U32 reg_val=0;

    //ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x1, 13);
	reg_val = pmic_get_register_value(PMIC_BIF_BAT_LOST);
    printk("[PMIC_DVT][check_bat_lost] BIF_BAT_LOST : BIF_CON31[13]=0x%x - battery is detected(0)/battery is undetected(1)\n", reg_val);
}

void bif_debug(void)
{
    U32 ret=0;
    U32 qi_bif_tx_en=0;
    U32 qi_bif_tx_data=0;
    U32 qi_bif_rx_en=0;
    U32 qi_bif_rx_data=0;
    U32 bif_tx_state=0;
    U32 bif_flow_ctl_state=0;
    U32 bif_rx_state=0;
    U32 bif_data_num=0;
    U32 rgs_baton_hv=0;
    U32 repeat=10;
    int i=0;

    printk("[PMIC_DVT][bif_debug] qi_bif_tx_en, qi_bif_tx_data, qi_bif_rx_en, qi_bif_rx_data, bif_tx_state, bif_flow_ctl_state, bif_rx_state, bif_data_num, rgs_baton_hv\n");

    for(i=0;i<repeat;i++)
    {
        qi_bif_tx_en=pmic_get_register_value(PMIC_DA_QI_BIF_TX_EN);
        qi_bif_tx_data=pmic_get_register_value(PMIC_DA_QI_BIF_TX_DATA);
        qi_bif_rx_en=pmic_get_register_value(PMIC_DA_QI_BIF_RX_EN);
        qi_bif_rx_data=pmic_get_register_value(PMIC_DA_QI_BIF_TX_DATA);
        bif_tx_state=pmic_get_register_value(PMIC_BIF_TX_STATE);
        bif_flow_ctl_state=pmic_get_register_value(PMIC_BIF_FLOW_CTL_STATE);
        bif_rx_state=pmic_get_register_value(PMIC_BIF_RX_STATE);
        bif_data_num=pmic_get_register_value(PMIC_BIF_DATA_NUM);
        rgs_baton_hv=pmic_get_register_value(PMIC_RGS_BATON_HV);

        printk("[PMIC_DVT][bif_debug] %d,%d,%d,%d,%d,%d,%d,%d,%d\n",
            qi_bif_tx_en, qi_bif_tx_data, qi_bif_rx_en, qi_bif_rx_data, bif_tx_state, bif_flow_ctl_state, bif_rx_state, bif_data_num, rgs_baton_hv);
    }
}

void bif_polling_irq(void)
{
    U32 reg_val=0;
	int loop_i=0;
    do
    {
//        ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x1, 11);
		reg_val=pmic_get_register_value(PMIC_BIF_IRQ);

        if(loop_i++ > g_loop_out)
		{
			printk("[PMIC_DVT][Fail]PMIC_BIF_IRQ 0x%x %d\n", reg_val, loop_i);
			break;
        }
    }while(reg_val == 0);
	printk("0x%x %d\n", reg_val, loop_i);
}
void tc_bif_reset_slave(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[1]={0};
    int loop_i=0;

    //1. set command sequence
    printk("[PMIC_DVT][tc_bif_reset_slave] 1. set command sequence\n");
    bif_cmd[0]=0x0400;
    set_bif_cmd(bif_cmd,1);

    //2. parameter setting
    printk("[PMIC_DVT][tc_bif_reset_slave] 2. parameter setting\n");
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x1, 0xF, 12);
    ret = pmic_set_register_value(PMIC_BIF_TRASFER_NUM, 1);
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x0, 0x3, 8);
    ret = pmic_set_register_value(PMIC_BIF_COMMAND_TYPE, 0);

    //dump
    printk("[PMIC_DVT]Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_BIF_TRASFER_NUM].offset,pmic_get_register_value(PMIC_BIF_TRASFER_NUM),
        pmu_flags_table[PMIC_BIF_COMMAND_TYPE].offset,pmic_get_register_value(PMIC_BIF_COMMAND_TYPE));

    //3. trigger BIF module
    printk("[PMIC_DVT][tc_bif_reset_slave] 3. trigger BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    udelay(10);
    bif_debug();

	printk("[PMIC_DVT][tc_bif_reset_slave] 4. polling BIF_IRQ : BIF_CON31[11]=");
    bif_polling_irq();

    //disable BIF module
    printk("[PMIC_DVT][tc_bif_reset_slave] 5. disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
	ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);

    check_bat_lost();

    //reset BIF_IRQ
    reset_bif_irq();
}

void debug_dump_reg(void)
{
    U32 ret=0;
    U32 reg_val=0;

	reg_val=pmic_get_register_value(PMIC_BIF_BAT_LOST);
    printk("[PMIC_DVT]][Fail][tc_bif_100_step_2] BAT_LOST[13]=0x%x\n", reg_val);
	reg_val=pmic_get_register_value(PMIC_BIF_TIMEOUT);
    printk("[PMIC_DVT]][Fail][tc_bif_100_step_2] TIMEOUT[12]=0x%x\n", reg_val);
	reg_val=pmic_get_register_value(PMIC_BIF_TOTAL_VALID);
    printk("[PMIC_DVT]][Fail][tc_bif_100_step_2] TOTAL_VALID[14]=0x%x\n", reg_val);

    ret=pmic_read_interface(MT6351_BIF_CON18, &reg_val, 0xFFFF, 0);
    printk("[PMIC_DVT][debug_dump_reg] BIF_CON18[0x%x]=0x%x\n", MT6351_BIF_CON18, reg_val);

    ret=pmic_read_interface(MT6351_BIF_CON19, &reg_val, 0xFFFF, 0);
    printk("[PMIC_DVT][debug_dump_reg] BIF_CON19[0x%x]=0x%x\n", MT6351_BIF_CON19, reg_val);

    ret=pmic_read_interface(MT6351_BIF_CON20, &reg_val, 0xFFFF, 0);
    printk("[PMIC_DVT][debug_dump_reg] BIF_CON20[0x%x]=0x%x\n", MT6351_BIF_CON20, reg_val);

    ret=pmic_read_interface(MT6351_BIF_CON21, &reg_val, 0xFFFF, 0);
    printk("[PMIC_DVT][debug_dump_reg] BIF_CON21[0x%x]=0x%x\n", MT6351_BIF_CON21, reg_val);

    ret=pmic_read_interface(MT6351_BIF_CON22, &reg_val, 0xFFFF, 0);
    printk("[PMIC_DVT][debug_dump_reg] BIF_CON22[0x%x]=0x%x\n", MT6351_BIF_CON22, reg_val);

    ret=pmic_read_interface(MT6351_BIF_CON23, &reg_val, 0xFFFF, 0);
    printk("[PMIC_DVT][debug_dump_reg] BIF_CON23[0x%x]=0x%x\n", MT6351_BIF_CON23, reg_val);
}

#if 1 // tc_bif_100
void tc_bif_100_step_0(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int loop_i=0;

    printk("[PMIC_DVT][tc_bif_100_step_0]-----------------------------------\n");

    printk("[PMIC_DVT][tc_bif_100_step_0] 1. set power up regiser\n");
    ret=pmic_set_register_value(PMIC_BIF_POWER_UP, 1);

    printk("[PMIC_DVT][tc_bif_100_step_0] 2. trigger BIF module\n");
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    //dump
    printk("[PMIC_DVT]Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_BIF_POWER_UP].offset,pmic_get_register_value(PMIC_BIF_POWER_UP),
        pmu_flags_table[PMIC_BIF_TRASACT_TRIGGER].offset,pmic_get_register_value(PMIC_BIF_TRASACT_TRIGGER));

    udelay(10);
    bif_debug();

	printk("[PMIC_DVT][tc_bif_100_step_0] 3. polling BIF_IRQ : BIF_CON31[11]=");
	bif_polling_irq();

    printk("[PMIC_DVT][tc_bif_100_step_0] 4. disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);

    printk("[PMIC_DVT][tc_bif_100_step_0] 5. to disable power up mode\n");
    //ret=pmic_config_interface(MT6325_BIF_CON32, 0x0, 0x1, 15);
    ret=pmic_set_register_value(PMIC_BIF_POWER_UP, 0);

    //dump
    printk("[PMIC_DVT]TRASACT_TRIGGER[0x%x]=0x%x,POWER_UP[0x%x]=0x%x,BAT_LOST_SW[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_BIF_TRASACT_TRIGGER].offset,pmic_get_register_value(PMIC_BIF_TRASACT_TRIGGER),
        pmu_flags_table[PMIC_BIF_POWER_UP].offset,pmic_get_register_value(PMIC_BIF_POWER_UP),
        pmu_flags_table[PMIC_BIF_BAT_LOST_SW].offset,pmic_get_register_value(PMIC_BIF_BAT_LOST_SW)
        );

    check_bat_lost();

    reset_bif_irq();
}

void tc_bif_100_step_1(void)
{
    printk("[PMIC_DVT][tc_bif_100_step_1]-----------------------------------\n");

    tc_bif_reset_slave();
}

void tc_bif_100_step_2(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[3]={0,0,0};
    int loop_i=0;
	bool err = FALSE;

    printk("[PMIC_DVT][tc_bif_100_step_2]-----------------------------------\n");

    //1. set command sequence
    printk("[PMIC_DVT][tc_bif_100_step_2] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x0100;
    bif_cmd[2]=0x0300;
    set_bif_cmd(bif_cmd,3);

    //2. parameter setting
    printk("[PMIC_DVT][tc_bif_100_step_2] 2. parameter setting\n");
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x3, 0xF, 12);
    ret = pmic_set_register_value(PMIC_BIF_TRASFER_NUM, 3);
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x1, 0x3, 8);
    ret = pmic_set_register_value(PMIC_BIF_COMMAND_TYPE, 1);
    //ret=pmic_config_interface(MT6325_BIF_CON17, 0x1, 0xF, 12);
    ret = pmic_set_register_value(PMIC_BIF_READ_EXPECT_NUM, 1);

    //dump
    printk("[PMIC_DVT]Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_BIF_TRASFER_NUM].offset,pmic_get_register_value(PMIC_BIF_TRASFER_NUM),
        pmu_flags_table[PMIC_BIF_COMMAND_TYPE].offset,pmic_get_register_value(PMIC_BIF_COMMAND_TYPE),
        pmu_flags_table[PMIC_BIF_READ_EXPECT_NUM].offset,pmic_get_register_value(PMIC_BIF_READ_EXPECT_NUM));

    //3. trigger BIF module
    printk("[PMIC_DVT][tc_bif_100_step_2] 3. trigger BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    udelay(10);
    bif_debug();

	printk("[PMIC_DVT][tc_bif_100_step_0] 4. polling BIF_IRQ : BIF_CON31[11]=");
	bif_polling_irq();

    //disable BIF module
    printk("[PMIC_DVT][tc_bif_100_step_2] 5. disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);

    //data valid check
    printk("[PMIC_DVT][tc_bif_100_step_2] 6. data valid check\n");
    //ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x7, 12);
	reg_val=pmic_get_register_value(PMIC_BIF_TOTAL_VALID);
    printk("[PMIC_DVT][tc_bif_100_step_2] TOTAL_VALID[14]=0x%x\n", reg_val);

    if(reg_val == 0)
    {
        //read the number of read back data package
        printk("[PMIC_DVT][tc_bif_100_step_2] 7. read the number of read back data package\n");
        //ret=pmic_read_interface(MT6325_BIF_CON19, &reg_val, 0xF, 0);
        reg_val=pmic_get_register_value(PMIC_BIF_DATA_NUM);
		if (reg_val == 0x1)
	        printk("[PMIC_DVT][Pass][tc_bif_100_step_2] BIF_DATA_NUM (1) : BIF_CON19[3:0]=0x%x\n", reg_val);
		else
			printk("[PMIC_DVT][Fail][tc_bif_100_step_2] BIF_DATA_NUM (1) : BIF_CON19[3:0]=0x%x\n", reg_val);

        //read data back
        printk("[PMIC_DVT][tc_bif_100_step_2] 8. read data back\n");
        //ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 15);
        reg_val=pmic_get_register_value(PMIC_BIF_ERROR_0);
		if (reg_val == 0)
	        printk("[PMIC_DVT][Pass][tc_bif_100_step_2] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
		else
			printk("[PMIC_DVT][Fail][tc_bif_100_step_2] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        //ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 8);
        reg_val=pmic_get_register_value(PMIC_BIF_ACK_0);
		if (reg_val == 0x1)
        	printk("[PMIC_DVT][Pass][tc_bif_100_step_2] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
	    else
			printk("[PMIC_DVT][Pass][tc_bif_100_step_2] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
        //ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0xFF, 0);
        reg_val=pmic_get_register_value(PMIC_BIF_DATA_0);
		if (reg_val == 0x10)
	        printk("[PMIC_DVT][Pass][tc_bif_100_step_2] BIF_DATA_0 (10H) : BIF_CON20[7:0]=0x%x\n", reg_val);
		else
			printk("[PMIC_DVT][Fail][tc_bif_100_step_2] BIF_DATA_0 (10H) : BIF_CON20[7:0]=0x%x\n", reg_val);
    }
    else
    {
		reg_val=pmic_get_register_value(PMIC_BIF_BAT_LOST);
	    printk("[PMIC_DVT]][Fail][tc_bif_100_step_2] BAT_LOST[13]=0x%x\n", reg_val);
		reg_val=pmic_get_register_value(PMIC_BIF_TIMEOUT);
	    printk("[PMIC_DVT]][Fail][tc_bif_100_step_2] TIMEOUT[12]=0x%x\n", reg_val);
		reg_val=pmic_get_register_value(PMIC_BIF_TOTAL_VALID);
	    printk("[PMIC_DVT]][Fail][tc_bif_100_step_2] TOTAL_VALID[14]=0x%x\n", reg_val);

        debug_dump_reg();
    }

    //reset BIF_IRQ
    reset_bif_irq();
}
#endif

#if 1 // tc_bif_111

void tc_bif_step_common(u32 tc, int step, int data_w_num, int cmd_type, int data_r_num, int *bif_r_data)
{
    U32 ret=0;
    U32 reg_val=0;
    int loop_i=0;

    //2. parameter setting
    printk("[PMIC_DVT][tc_bif_%u_step_%d] 2. parameter setting\n", tc, step);
//    ret=pmic_config_interface(MT6325_BIF_CON15, 0x4, 0xF, 12);
//    ret=pmic_config_interface(MT6325_BIF_CON15, 0x1, 0x3, 8);
//    ret=pmic_config_interface(MT6325_BIF_CON17, 0x4, 0xF, 12);
    ret = pmic_set_register_value(PMIC_BIF_TRASFER_NUM, data_w_num);
    ret = pmic_set_register_value(PMIC_BIF_COMMAND_TYPE, cmd_type);
	if (data_r_num != 0)
	    ret = pmic_set_register_value(PMIC_BIF_READ_EXPECT_NUM, data_r_num);

    //dump
    printk("[PMIC_DVT]Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_BIF_TRASFER_NUM].offset,pmic_get_register_value(PMIC_BIF_TRASFER_NUM),
        pmu_flags_table[PMIC_BIF_COMMAND_TYPE].offset,pmic_get_register_value(PMIC_BIF_COMMAND_TYPE),
        pmu_flags_table[PMIC_BIF_READ_EXPECT_NUM].offset,pmic_get_register_value(PMIC_BIF_READ_EXPECT_NUM));


    //3. trigger BIF module
    printk("[PMIC_DVT][tc_bif_%u_step_%d] 3. trigger BIF module\n", tc, step);
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    udelay(10);
    bif_debug();

	printk("[PMIC_DVT][tc_bif_%u_step_%d] 4. polling BIF_IRQ : BIF_CON31[11]=",tc, step);
	bif_polling_irq();

    //4. disable BIF module
    printk("[PMIC_DVT][tc_bif_%u_step_%d] 4. disable BIF module\n", tc, step);
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);

	if(data_r_num == 0)
	{
		//no read data
		 check_bat_lost();
	}
	else{
	    //5. data valid check
	    printk("[PMIC_DVT][tc_bif_%u_step_%d] 5. data valid check\n", tc, step);
	    //ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x7, 12);
		reg_val=pmic_get_register_value(PMIC_BIF_TOTAL_VALID);
	    printk("[PMIC_DVT][tc_bif_%u_step_%d] TOTAL_VALID[14]=0x%x\n", tc, step, reg_val);

	    if(reg_val == 0)
	    {
	        //6. read the number of read back data package
	        printk("[PMIC_DVT][tc_bif_%u_step_%d] 6. read the number of read back data package\n", tc, step);
	        //ret=pmic_read_interface(MT6325_BIF_CON19, &reg_val, 0xF, 0);
	        reg_val=pmic_get_register_value(PMIC_BIF_DATA_NUM);
			if (reg_val == data_r_num)
		        printk("[PMIC_DVT][Pass][tc_bif_%u_step_%d] BIF_DATA_NUM (%d) : BIF_CON19[3:0]=0x%x\n", tc, step, data_r_num, reg_val);
			else
				PMICDBG(0, "[PMIC_DVT][Fail][tc_bif_%u_step_%d] BIF_DATA_NUM (%d) : BIF_CON19[3:0]=0x%x\n", tc, step, data_r_num, reg_val);

	        //read data back
	        printk("[PMIC_DVT][tc_bif_%u_step_%d] 8. read data back\n", tc, step);

			bif_check_read_data(data_r_num, bif_r_data);
	    }
	    else
	    {
	        debug_dump_reg();
	    }
	}

    //8. reset BIF_IRQ
    reset_bif_irq();
}

void tc_bif_111_step_1(void)
{
    int bif_cmd[4]={0,0,0,0};
	int bif_r_data[4]={0x01,0x01,0x0a,0x00};

    printk("[PMIC_DVT][tc_bif_111_step_1]-----------------------------------\n");

    //1. set command sequence
    printk("[PMIC_DVT][tc_bif_111_step_1] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x0424;
    bif_cmd[2]=0x0100;
    bif_cmd[3]=0x030A;
    set_bif_cmd(bif_cmd,0x4);

	tc_bif_step_common(111, 1, 0x4, 0x1, 0x4, bif_r_data);

#if 0
    //2. parameter setting
    printk("[PMIC_DVT][tc_bif_111_step_1] 2. parameter setting\n");
//    ret=pmic_config_interface(MT6325_BIF_CON15, 0x4, 0xF, 12);
//    ret=pmic_config_interface(MT6325_BIF_CON15, 0x1, 0x3, 8);
//    ret=pmic_config_interface(MT6325_BIF_CON17, 0x4, 0xF, 12);
    ret = pmic_set_register_value(PMIC_BIF_TRASFER_NUM, data_w_num);
    ret = pmic_set_register_value(PMIC_BIF_COMMAND_TYPE, 1);
    ret = pmic_set_register_value(PMIC_BIF_READ_EXPECT_NUM, data_r_num);

    //dump
    printk("[PMIC_DVT]Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_BIF_TRASFER_NUM].offset,pmic_get_register_value(PMIC_BIF_TRASFER_NUM),
        pmu_flags_table[PMIC_BIF_COMMAND_TYPE].offset,pmic_get_register_value(PMIC_BIF_COMMAND_TYPE),
        pmu_flags_table[PMIC_BIF_READ_EXPECT_NUM].offset,pmic_get_register_value(PMIC_BIF_READ_EXPECT_NUM));


    //3. trigger BIF module
    printk("[PMIC_DVT][tc_bif_111_step_1] 3. trigger BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    udelay(10);
    bif_debug();

	printk("[PMIC_DVT][tc_bif_100_step_0] 4. polling BIF_IRQ : BIF_CON31[11]=");
	bif_polling_irq();

    //4. disable BIF module
    printk("[PMIC_DVT][tc_bif_111_step_1] 4. disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);

    //5. data valid check
    printk("[PMIC_DVT][tc_bif_111_step_1] 5. data valid check\n");
    //ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x7, 12);
	reg_val=pmic_get_register_value(PMIC_BIF_TOTAL_VALID);
    printk("[PMIC_DVT][tc_bif_100_step_2] TOTAL_VALID[14]=0x%x\n", reg_val);

    if(reg_val == 0)
    {
        //6. read the number of read back data package
        printk("[PMIC_DVT][tc_bif_111_step_1] 6. read the number of read back data package\n");
        //ret=pmic_read_interface(MT6325_BIF_CON19, &reg_val, 0xF, 0);
        reg_val=pmic_get_register_value(PMIC_BIF_DATA_NUM);
		if (reg_val == data_r_num)
	        printk("[PMIC_DVT][Pass][tc_bif_100_step_2] BIF_DATA_NUM (%d) : BIF_CON19[3:0]=0x%x\n", data_r_num, reg_val);
		else
			PMICDBG(0, "[PMIC_DVT][Fail][tc_bif_100_step_2] BIF_DATA_NUM (%d) : BIF_CON19[3:0]=0x%x\n", data_r_num, reg_val);

        //read data back
        printk("[PMIC_DVT][tc_bif_100_step_2] 8. read data back\n");
		bif_check_read_data(data_r_num, bif_r_data);
#if 0
        //ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 15);
        reg_val=pmic_get_register_value(PMIC_BIF_ERROR_0);
		if (reg_val == 0)
	        printk("[PMIC_DVT][Pass][tc_bif_100_step_2] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
		else
			printk("[PMIC_DVT][Fail][tc_bif_100_step_2] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        //ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 8);
        reg_val=pmic_get_register_value(PMIC_BIF_ACK_0);
		if (reg_val == 0x1)
        	printk("[PMIC_DVT][Pass][tc_bif_100_step_2] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
	    else
			printk("[PMIC_DVT][Pass][tc_bif_100_step_2] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
        //ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0xFF, 0);
        reg_val=pmic_get_register_value(PMIC_BIF_DATA_0);
		if (reg_val == 0x0x)
	        printk("[PMIC_DVT][Pass][tc_bif_100_step_2] BIF_DATA_0 (10H) : BIF_CON20[7:0]=0x%x\n", reg_val);
		else
			printk("[PMIC_DVT][Fail][tc_bif_100_step_2] BIF_DATA_0 (10H) : BIF_CON20[7:0]=0x%x\n", reg_val);


        ret=pmic_read_interface(MT6325_BIF_CON21, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_111_step_1] BIF_DATA_ERROR_0 (0) : BIF_CON21[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON21, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_111_step_1] BIF_ACK_0 (1) : BIF_CON21[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON21, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_111_step_1] BIF_DATA_0 (01H) : BIF_CON21[7:0]=0x%x\n", reg_val);

        ret=pmic_read_interface(MT6325_BIF_CON22, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_111_step_1] BIF_DATA_ERROR_0 (0) : BIF_CON22[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON22, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_111_step_1] BIF_ACK_0 (1) : BIF_CON22[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON22, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_111_step_1] BIF_DATA_0 (pcap[15:8]=01H) : BIF_CON22[7:0]=0x%x\n", reg_val);
        pcap_15_8=reg_val;

        ret=pmic_read_interface(MT6325_BIF_CON23, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_111_step_1] BIF_DATA_ERROR_0 (0) : BIF_CON23[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON23, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_111_step_1] BIF_ACK_0 (1) : BIF_CON23[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON23, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_111_step_1] BIF_DATA_0 (pcap[7:0]=00H) : BIF_CON23[7:0]=0x%x\n", reg_val);
        pcap_7_0=reg_val;
#endif
    }
    else
    {
        debug_dump_reg();
    }

    //8. reset BIF_IRQ
    reset_bif_irq();
#endif
}

void tc_bif_111_step_2(void)
{
    int bif_cmd[4]={0,0,0,0};
	int bif_r_data[2]={0x0a,0x00};

    printk("[PMIC_DVT][tc_bif_111_step_2]-----------------------------------\n");

    //1. set command sequence
    printk("[PMIC_DVT][tc_bif_111_step_2] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x0422;
    bif_cmd[2]=(0x0100|pcap_15_8);
    bif_cmd[3]=(0x0300|pcap_7_0);
    set_bif_cmd(bif_cmd,4);


	tc_bif_step_common(111, 2, 0x4, 0x1, 0x2, bif_r_data);

#if 0
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[4]={0,0,0,0};
    int loop_i=0;


    //2. parameter setting
    printk("[PMIC_DVT][tc_bif_111_step_2] 2. parameter setting\n");
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x4, 0xF, 12);
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x1, 0x3, 8);
    //ret=pmic_config_interface(MT6325_BIF_CON17, 0x2, 0xF, 12);
    ret = pmic_set_register_value(PMIC_BIF_TRASFER_NUM, 4);
    ret = pmic_set_register_value(PMIC_BIF_COMMAND_TYPE, 1);
    ret = pmic_set_register_value(PMIC_BIF_READ_EXPECT_NUM, 2);
    //dump
    printk("[PMIC_DVT]Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_BIF_TRASFER_NUM].offset,pmic_get_register_value(PMIC_BIF_TRASFER_NUM),
        pmu_flags_table[PMIC_BIF_COMMAND_TYPE].offset,pmic_get_register_value(PMIC_BIF_COMMAND_TYPE),
        pmu_flags_table[PMIC_BIF_READ_EXPECT_NUM].offset,pmic_get_register_value(PMIC_BIF_READ_EXPECT_NUM));


    //3. trigger BIF module
    printk("[PMIC_DVT][tc_bif_111_step_2] 3. trigger BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    udelay(10);
    bif_debug();

	printk("[PMIC_DVT][tc_bif_100_step_0] 4. polling BIF_IRQ : BIF_CON31[11]=");
	bif_polling_irq();

    //4. disable BIF module
    printk("[PMIC_DVT][tc_bif_111_step_1] 4. disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);

    //5. data valid check
    printk("[PMIC_DVT][tc_bif_111_step_1] 5. data valid check\n");
    //ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x7, 12);
	reg_val=pmic_get_register_value(PMIC_BIF_TOTAL_VALID);
    printk("[PMIC_DVT][tc_bif_100_step_2] TOTAL_VALID[14]=0x%x\n", reg_val);



    if(reg_val == 0)
    {
        //6. read the number of read back data package
        printk("[PMIC_DVT][tc_bif_111_step_2] 6. read the number of read back data package\n");
        ret=pmic_read_interface(MT6325_BIF_CON19, &reg_val, 0xF, 0);
        printk("[PMIC_DVT][tc_bif_111_step_2] BIF_DATA_NUM (2) : BIF_CON19[3:0]=0x%x\n", reg_val);

        //7. read data back
        printk("[PMIC_DVT][tc_bif_111_step_2] 7. read data back\n");
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_111_step_2] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_111_step_2] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_111_step_2] BIF_DATA_0 (0A) : BIF_CON20[7:0]=0x%x\n", reg_val);
        pcreg_15_8=reg_val;

        ret=pmic_read_interface(MT6325_BIF_CON21, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_111_step_2] BIF_DATA_ERROR_0 (0) : BIF_CON21[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON21, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_111_step_2] BIF_ACK_0 (1) : BIF_CON21[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON21, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_111_step_2] BIF_DATA_0 (00) : BIF_CON21[7:0]=0x%x\n", reg_val);
        pcreg_7_0=reg_val;
    }
    else
    {
        printk("[PMIC_DVT][tc_bif_111_step_2] Fail : BIF_CON31[14:12]=0x%x\n", reg_val);

        debug_dump_reg();
    }

    //8. reset BIF_IRQ
    reset_bif_irq();
#endif
}


void tc_bif_111_step_3(void)
{
    int bif_cmd[4]={0,0,0,0};

	printk("[PMIC_DVT][tc_bif_111_step_3]-----------------------------------\n");

    //1. set command sequence
    printk("[PMIC_DVT][tc_bif_111_step_3] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=(0x0100|pcreg_15_8);
    bif_cmd[2]=(0x0200|pcreg_7_0);
    bif_cmd[3]=0x0000;
    set_bif_cmd(bif_cmd,4);


	tc_bif_step_common(111, 3, 0x4, 0x0, 0x0, NULL);

#if 0
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[4]={0,0,0,0};
    int loop_i=0;



    //2. parameter setting
    printk("[PMIC_DVT][tc_bif_111_step_3] 2. parameter setting\n");
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x4, 0xF, 12);
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x0, 0x3, 8);
    ret = pmic_set_register_value(PMIC_BIF_TRASFER_NUM, 4);
    ret = pmic_set_register_value(PMIC_BIF_COMMAND_TYPE, 0);
    //dump
    printk("[PMIC_DVT]Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_BIF_TRASFER_NUM].offset,pmic_get_register_value(PMIC_BIF_TRASFER_NUM),
        pmu_flags_table[PMIC_BIF_COMMAND_TYPE].offset,pmic_get_register_value(PMIC_BIF_COMMAND_TYPE),
        pmu_flags_table[PMIC_BIF_READ_EXPECT_NUM].offset,pmic_get_register_value(PMIC_BIF_READ_EXPECT_NUM));


    //3. trigger BIF module
    printk("[PMIC_DVT][tc_bif_111_step_3] 3. trigger BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    udelay(10);
    bif_debug();

	printk("[PMIC_DVT][tc_bif_100_step_0] 4. polling BIF_IRQ : BIF_CON31[11]=");
	bif_polling_irq();

    //4. disable BIF module
    printk("[PMIC_DVT][tc_bif_111_step_3] 4. disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);

    check_bat_lost();

    //5. reset BIF_IRQ
    reset_bif_irq();
#endif
}

void tc_bif_111_step_4(void)
{
    int bif_cmd[3]={0,0,0};
	int bif_r_data[1]={0x00};

    //1. set command sequence
    printk("[PMIC_DVT][tc_bif_111_step_4] 1. set command sequence\n");
    bif_cmd[0]=0x0600;
    bif_cmd[1]=(0x0100|pcreg_15_8);
    bif_cmd[2]=(0x0300|pcreg_7_0);
    set_bif_cmd(bif_cmd,3);

	tc_bif_step_common(111, 4, 0x3, 0x1, 0x1, bif_r_data);

#if 0
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[3]={0,0,0};
    int loop_i=0;

	  printk("[PMIC_DVT][tc_bif_111_step_4]-----------------------------------\n");



    //2. parameter setting
    printk("[PMIC_DVT][tc_bif_111_step_4] 2. parameter setting\n");
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x3, 0xF, 12);
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x1, 0x3, 8);
    //ret=pmic_config_interface(MT6325_BIF_CON17, 0x1, 0xF, 12);
    ret = pmic_set_register_value(PMIC_BIF_TRASFER_NUM, 3);
    ret = pmic_set_register_value(PMIC_BIF_COMMAND_TYPE, 1);
    ret = pmic_set_register_value(PMIC_BIF_READ_EXPECT_NUM, 1);
    //dump
    printk("[PMIC_DVT]Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_BIF_TRASFER_NUM].offset,pmic_get_register_value(PMIC_BIF_TRASFER_NUM),
        pmu_flags_table[PMIC_BIF_COMMAND_TYPE].offset,pmic_get_register_value(PMIC_BIF_COMMAND_TYPE),
        pmu_flags_table[PMIC_BIF_READ_EXPECT_NUM].offset,pmic_get_register_value(PMIC_BIF_READ_EXPECT_NUM));


    //3. trigger BIF module
    printk("[PMIC_DVT][tc_bif_111_step_4] 3. trigger BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    udelay(10);
    bif_debug();

	printk("[PMIC_DVT][tc_bif_100_step_0] 4. polling BIF_IRQ : BIF_CON31[11]=");
	bif_polling_irq();

    //4. disable BIF module
    printk("[PMIC_DVT][tc_bif_111_step_4] 4. disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);

    //5. data valid check
    printk("[PMIC_DVT][tc_bif_111_step_4] 5. data valid check\n");
    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x7, 12);
    printk("[PMIC_DVT][tc_bif_111_step_4] BIF_CON31[14:12]=0x%x\n", reg_val);

    if(reg_val == 0)
    {
        //6. read the number of read back data package
        printk("[PMIC_DVT][tc_bif_111_step_4] 6. read the number of read back data package\n");
        ret=pmic_read_interface(MT6325_BIF_CON19, &reg_val, 0xF, 0);
        printk("[PMIC_DVT][tc_bif_111_step_4] BIF_DATA_NUM (1) : BIF_CON19[3:0]=0x%x\n", reg_val);

        //7. read data back
        printk("[PMIC_DVT][tc_bif_111_step_4] 7. read data back\n");
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_111_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_111_step_4] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_111_step_4] BIF_DATA_0 (00H) : BIF_CON20[7:0]=0x%x\n", reg_val);
    }
    else
    {
        printk("[PMIC_DVT][tc_bif_111_step_4] Fail : BIF_CON31[14:12]=0x%x\n", reg_val);

        debug_dump_reg();
    }

    //8. reset BIF_IRQ
    reset_bif_irq();
#endif
}

void tc_bif_111_step_5(void)
{
    int bif_cmd[5]={0,0,0,0,0};
	int bif_r_data[1]={0x04};

    //1. set command sequence
	  printk("[PMIC_DVT][tc_bif_111_step_5]-----------------------------------\n");

    //1. set command sequence
    printk("[PMIC_DVT][tc_bif_111_step_5] 1. set command sequence\n");
    bif_cmd[0]=0x0600;
    bif_cmd[1]=(0x0100|pcreg_15_8);
    bif_cmd[2]=(0x0200|pcreg_7_0);
    bif_cmd[3]=0x0001;
    bif_cmd[4]=0x04C2;
    set_bif_cmd(bif_cmd,5);


	tc_bif_step_common(111, 5, 0x5, 0x1, 0x1, bif_r_data);

#if 0

    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[5]={0,0,0,0,0};
    int loop_i=0;


    //2. parameter setting
    printk("[PMIC_DVT][tc_bif_111_step_5] 2. parameter setting\n");
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x5, 0xF, 12);
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x1, 0x3, 8);
    //ret=pmic_config_interface(MT6325_BIF_CON17, 0x1, 0xF, 12);
    ret = pmic_set_register_value(PMIC_BIF_TRASFER_NUM, 5);
    ret = pmic_set_register_value(PMIC_BIF_COMMAND_TYPE, 1);
    ret = pmic_set_register_value(PMIC_BIF_READ_EXPECT_NUM, 1);
    //dump
    printk("[PMIC_DVT]Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_BIF_TRASFER_NUM].offset,pmic_get_register_value(PMIC_BIF_TRASFER_NUM),
        pmu_flags_table[PMIC_BIF_COMMAND_TYPE].offset,pmic_get_register_value(PMIC_BIF_COMMAND_TYPE),
        pmu_flags_table[PMIC_BIF_READ_EXPECT_NUM].offset,pmic_get_register_value(PMIC_BIF_READ_EXPECT_NUM));


    //3. trigger BIF module
    printk("[PMIC_DVT][tc_bif_111_step_5] 3. trigger BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    udelay(10);
    bif_debug();

	printk("[PMIC_DVT][tc_bif_100_step_0] 4. polling BIF_IRQ : BIF_CON31[11]=");
	bif_polling_irq();

    //4. disable BIF module
    printk("[PMIC_DVT][tc_bif_111_step_5] 4. disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);

    //5. data valid check
    printk("[PMIC_DVT][tc_bif_111_step_5] 5. data valid check\n");
    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x7, 12);
    printk("[PMIC_DVT][tc_bif_111_step_5] BIF_CON31[14:12]=0x%x\n", reg_val);

    if(reg_val == 0)
    {
        //6. read the number of read back data package
        printk("[PMIC_DVT][tc_bif_111_step_5] 6. read the number of read back data package\n");
        ret=pmic_read_interface(MT6325_BIF_CON19, &reg_val, 0xF, 0);
        printk("[PMIC_DVT][tc_bif_111_step_5] BIF_DATA_NUM (1) : BIF_CON19[3:0]=0x%x\n", reg_val);

        //7. read data back
        printk("[PMIC_DVT][tc_bif_111_step_5] 7. read data back\n");
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_111_step_5] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_111_step_5] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_111_step_5] BIF_DATA_0 (04h) : BIF_CON20[7:0]=0x%x\n", reg_val);
    }
    else
    {
        printk("[PMIC_DVT][tc_bif_111_step_5] Fail : BIF_CON31[14:12]=0x%x\n", reg_val);

        debug_dump_reg();
    }

    //8. reset BIF_IRQ
    reset_bif_irq();
#endif
}
#endif

#if 1 // tc_bif_112
//int nvmcap_15_8 = 0;
//int nvmcap_7_0 = 0;
//int nvmreg_15_8 = 0;
//int nvmreg_7_0 = 0;

void tc_bif_112_step_1(void)
{
    int bif_cmd[4]={0,0,0,0};
	int bif_r_data[4]={0x04, 0x01, 0x04, 0x00};

    //1. set command sequence
    printk("[PMIC_DVT][tc_bif_112_step_1] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x0424;
    bif_cmd[2]=0x0100;
    bif_cmd[3]=0x0316;
    set_bif_cmd(bif_cmd,4);

	tc_bif_step_common(112, 1, 0x4, 0x1, 0x4, bif_r_data);

#if 0
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[4]={0,0,0,0};
    int loop_i=0;


    //2. parameter setting
    printk("[PMIC_DVT][tc_bif_112_step_1] 2. parameter setting\n");
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x4, 0xF, 12);
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x1, 0x3, 8);
    //ret=pmic_config_interface(MT6325_BIF_CON17, 0x4, 0xF, 12);
    ret = pmic_set_register_value(PMIC_BIF_TRASFER_NUM, 4);
    ret = pmic_set_register_value(PMIC_BIF_COMMAND_TYPE, 1);
    ret = pmic_set_register_value(PMIC_BIF_READ_EXPECT_NUM, 4);
    //dump
    printk("[PMIC_DVT]Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_BIF_TRASFER_NUM].offset,pmic_get_register_value(PMIC_BIF_TRASFER_NUM),
        pmu_flags_table[PMIC_BIF_COMMAND_TYPE].offset,pmic_get_register_value(PMIC_BIF_COMMAND_TYPE),
        pmu_flags_table[PMIC_BIF_READ_EXPECT_NUM].offset,pmic_get_register_value(PMIC_BIF_READ_EXPECT_NUM));


    //3. trigger BIF module
    printk("[PMIC_DVT][tc_bif_112_step_1] 3. trigger BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    udelay(10);
    bif_debug();

	printk("[PMIC_DVT][tc_bif_100_step_0] 4. polling BIF_IRQ : BIF_CON31[11]=");
	bif_polling_irq();

    //4. disable BIF module
    printk("[PMIC_DVT][tc_bif_112_step_1] 4. disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);

    //5. data valid check
    printk("[PMIC_DVT][tc_bif_112_step_1] 5. data valid check\n");
    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x7, 12);
    printk("[PMIC_DVT][tc_bif_112_step_1] BIF_CON31[14:12]=0x%x\n", reg_val);

    if(reg_val == 0)
    {
        //6. read the number of read back data package
        printk("[PMIC_DVT][tc_bif_112_step_1] 6. read the number of read back data package\n");
        ret=pmic_read_interface(MT6325_BIF_CON19, &reg_val, 0xF, 0);
        printk("[PMIC_DVT][tc_bif_112_step_1] BIF_DATA_NUM (4) : BIF_CON19[3:0]=0x%x\n", reg_val);

        //7. read data back
        printk("[PMIC_DVT][tc_bif_112_step_1] 7. read data back\n");
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_112_step_1] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_112_step_1] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_112_step_1] BIF_DATA_0 (04H) : BIF_CON20[7:0]=0x%x\n", reg_val);

        ret=pmic_read_interface(MT6325_BIF_CON21, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_112_step_1] BIF_DATA_ERROR_0 (0) : BIF_CON21[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON21, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_112_step_1] BIF_ACK_0 (1) : BIF_CON21[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON21, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_112_step_1] BIF_DATA_0 (01H) : BIF_CON21[7:0]=0x%x\n", reg_val);

        ret=pmic_read_interface(MT6325_BIF_CON22, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_112_step_1] BIF_DATA_ERROR_0 (0) : BIF_CON22[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON22, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_112_step_1] BIF_ACK_0 (1) : BIF_CON22[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON22, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_112_step_1] BIF_DATA_0 (pcap[15:8]=04H) : BIF_CON22[7:0]=0x%x\n", reg_val);
        nvmcap_15_8=reg_val;

        ret=pmic_read_interface(MT6325_BIF_CON23, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_112_step_1] BIF_DATA_ERROR_0 (0) : BIF_CON23[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON23, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_112_step_1] BIF_ACK_0 (1) : BIF_CON23[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON23, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_112_step_1] BIF_DATA_0 (pcap[7:0]=00H) : BIF_CON23[7:0]=0x%x\n", reg_val);
        nvmcap_7_0=reg_val;
    }
    else
    {
        printk("[PMIC_DVT][tc_bif_112_step_1] Fail : BIF_CON31[14:12]=0x%x\n", reg_val);
    }

    //8. reset BIF_IRQ
    reset_bif_irq();
#endif
}

void tc_bif_112_step_2(void)
{
    int bif_cmd[4]={0,0,0,0};
	int bif_r_data[2]={0x0f, 0x00};

    //1. set command sequence
    printk("[PMIC_DVT][tc_bif_112_step_2] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x0422;
    bif_cmd[2]=(0x0100|nvmcap_15_8);
    bif_cmd[3]=(0x0300|nvmcap_7_0);
    set_bif_cmd(bif_cmd,4);

	tc_bif_step_common(112, 2, 0x4, 0x1, 0x2, bif_r_data);

#if 0

    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[4]={0,0,0,0};
    int loop_i=0;


    //2. parameter setting
    printk("[PMIC_DVT][tc_bif_112_step_2] 2. parameter setting\n");
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x4, 0xF, 12);
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x1, 0x3, 8);
    //ret=pmic_config_interface(MT6325_BIF_CON17, 0x2, 0xF, 12);
    ret = pmic_set_register_value(PMIC_BIF_TRASFER_NUM, 4);
    ret = pmic_set_register_value(PMIC_BIF_COMMAND_TYPE, 1);
    ret = pmic_set_register_value(PMIC_BIF_READ_EXPECT_NUM, 2);
    //dump
    printk("[PMIC_DVT]Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_BIF_TRASFER_NUM].offset,pmic_get_register_value(PMIC_BIF_TRASFER_NUM),
        pmu_flags_table[PMIC_BIF_COMMAND_TYPE].offset,pmic_get_register_value(PMIC_BIF_COMMAND_TYPE),
        pmu_flags_table[PMIC_BIF_READ_EXPECT_NUM].offset,pmic_get_register_value(PMIC_BIF_READ_EXPECT_NUM));


    //3. trigger BIF module
    printk("[PMIC_DVT][tc_bif_112_step_2] 3. trigger BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    udelay(10);
    bif_debug();

	printk("[PMIC_DVT][tc_bif_100_step_0] 4. polling BIF_IRQ : BIF_CON31[11]=");
	bif_polling_irq();

    //4. disable BIF module
    printk("[PMIC_DVT][tc_bif_112_step_2] 4. disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);

    //5. data valid check
    printk("[PMIC_DVT][tc_bif_112_step_2] 5. data valid check\n");
    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x7, 12);
    printk("[PMIC_DVT][tc_bif_112_step_2] BIF_CON31[14:12]=0x%x\n", reg_val);

    if(reg_val == 0)
    {
        //6. read the number of read back data package
        printk("[PMIC_DVT][tc_bif_112_step_2] 6. read the number of read back data package\n");
        ret=pmic_read_interface(MT6325_BIF_CON19, &reg_val, 0xF, 0);
        printk("[PMIC_DVT][tc_bif_112_step_2] BIF_DATA_NUM (2) : BIF_CON19[3:0]=0x%x\n", reg_val);

        //7. read data back
        printk("[PMIC_DVT][tc_bif_112_step_2] 7. read data back\n");
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_112_step_2] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_112_step_2] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_112_step_2] BIF_DATA_0 (nvmreg[15:8]=0FH) : BIF_CON20[7:0]=0x%x\n", reg_val);
        nvmreg_15_8=reg_val;

        ret=pmic_read_interface(MT6325_BIF_CON21, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_112_step_2] BIF_DATA_ERROR_0 (0) : BIF_CON21[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON21, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_112_step_2] BIF_ACK_0 (1) : BIF_CON21[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON21, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_112_step_2] BIF_DATA_0 (nvmreg[7:0]=00H) : BIF_CON21[7:0]=0x%x\n", reg_val);
        nvmreg_7_0=reg_val;
    }
    else
    {
        printk("[PMIC_DVT][tc_bif_112_step_2] Fail : BIF_CON31[14:12]=0x%x\n", reg_val);
    }

    //8. reset BIF_IRQ
    reset_bif_irq();
#endif
}

void tc_bif_112_step_3(void)
{
    int bif_cmd[11]={0,0,0,0,0,0,0,0,0,0,0};
	int bif_r_data[1]={0x0a};

    //1. set command sequence
    printk("[PMIC_DVT][tc_bif_112_step_3] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=(0x0100|nvmreg_15_8);
    bif_cmd[2]=(0x0200|nvmcap_7_0) + 0x09;
    bif_cmd[3]=0x0000;
    bif_cmd[4]=0x0001;
    bif_cmd[5]=0x0002;
    bif_cmd[6]=0x0003;
    bif_cmd[7]=0x0004;
    bif_cmd[8]=0x0005;
    bif_cmd[9]=0x0006;
    bif_cmd[10]=0x04C2;
    set_bif_cmd(bif_cmd,11);

	tc_bif_step_common(112, 3, 0xb, 0x1, 0x1, bif_r_data);


#if 0
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[11]={0,0,0,0,0,0,0,0,0,0,0};
    int loop_i=0;

	  printk("[PMIC_DVT][tc_bif_112_step_3]-----------------------------------\n");


    //2. parameter setting
    printk("[PMIC_DVT][tc_bif_112_step_3] 2. parameter setting\n");
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0xB, 0xF, 12);
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x1, 0x3, 8);
    //ret=pmic_config_interface(MT6325_BIF_CON17, 0x1, 0xF, 12);
    ret = pmic_set_register_value(PMIC_BIF_TRASFER_NUM, 0xb);
    ret = pmic_set_register_value(PMIC_BIF_COMMAND_TYPE, 1);
    ret = pmic_set_register_value(PMIC_BIF_READ_EXPECT_NUM, 1);
    //dump
    printk("[PMIC_DVT]Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_BIF_TRASFER_NUM].offset,pmic_get_register_value(PMIC_BIF_TRASFER_NUM),
        pmu_flags_table[PMIC_BIF_COMMAND_TYPE].offset,pmic_get_register_value(PMIC_BIF_COMMAND_TYPE),
        pmu_flags_table[PMIC_BIF_READ_EXPECT_NUM].offset,pmic_get_register_value(PMIC_BIF_READ_EXPECT_NUM));


    //3. trigger BIF module
    printk("[PMIC_DVT][tc_bif_112_step_3] 3. trigger BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    udelay(10);
    bif_debug();

	printk("[PMIC_DVT][tc_bif_100_step_0] 4. polling BIF_IRQ : BIF_CON31[11]=");
	bif_polling_irq();

    //4. disable BIF module
    printk("[PMIC_DVT][tc_bif_112_step_3] 4. disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);

    //5. data valid check
    printk("[PMIC_DVT][tc_bif_112_step_3] 5. data valid check\n");
    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x7, 12);
    printk("[PMIC_DVT][tc_bif_112_step_3] BIF_CON31[14:12]=0x%x\n", reg_val);

    if(reg_val == 0)
    {
        //6. read the number of read back data package
        printk("[PMIC_DVT][tc_bif_112_step_3] 6. read the number of read back data package\n");
        ret=pmic_read_interface(MT6325_BIF_CON19, &reg_val, 0xF, 0);
        printk("[PMIC_DVT][tc_bif_112_step_3] BIF_DATA_NUM (1) : BIF_CON19[3:0]=0x%x\n", reg_val);

        //7. read data back
        printk("[PMIC_DVT][tc_bif_112_step_3] 7. read data back\n");
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_112_step_3] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_112_step_3] BIF_ACK_0 (0/1) : BIF_CON20[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_112_step_3] BIF_DATA_0 (0Ah) : BIF_CON20[7:0]=0x%x\n", reg_val);
    }
    else
    {
        printk("[PMIC_DVT][tc_bif_112_step_3] Fail : BIF_CON31[14:12]=0x%x\n", reg_val);
    }

    //8. reset BIF_IRQ
    reset_bif_irq();
#endif
}

void tc_bif_112_step_4(void)
{
    int bif_cmd[4]={0,0,0,0};
	int bif_r_data[7]={0x00,0x01,0x02,0x03,0x04,0x05,0x06};

    //1. set command sequence
    printk("[PMIC_DVT][tc_bif_112_step_4] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x0427;
    bif_cmd[2]=(0x0100|nvmreg_15_8);
    bif_cmd[3]=(0x0300|nvmcap_7_0) + 0x09;
    set_bif_cmd(bif_cmd,4);


	tc_bif_step_common(112, 4, 0x4, 0x1, 0x7, bif_r_data);

#if 0
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[4]={0,0,0,0};
    int loop_i=0;

	  printk("[PMIC_DVT][tc_bif_112_step_4]-----------------------------------\n");


    //2. parameter setting
    printk("[PMIC_DVT][tc_bif_112_step_4] 2. parameter setting\n");
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x4, 0xF, 12);
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x1, 0x3, 8);
    //ret=pmic_config_interface(MT6325_BIF_CON17, 0x7, 0xF, 12);
    ret = pmic_set_register_value(PMIC_BIF_TRASFER_NUM, 4);
    ret = pmic_set_register_value(PMIC_BIF_COMMAND_TYPE, 1);
    ret = pmic_set_register_value(PMIC_BIF_READ_EXPECT_NUM, 7);
    //dump
    printk("[PMIC_DVT]Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_BIF_TRASFER_NUM].offset,pmic_get_register_value(PMIC_BIF_TRASFER_NUM),
        pmu_flags_table[PMIC_BIF_COMMAND_TYPE].offset,pmic_get_register_value(PMIC_BIF_COMMAND_TYPE),
        pmu_flags_table[PMIC_BIF_READ_EXPECT_NUM].offset,pmic_get_register_value(PMIC_BIF_READ_EXPECT_NUM));


    //3. trigger BIF module
    printk("[PMIC_DVT][tc_bif_112_step_4] 3. trigger BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    udelay(10);
    bif_debug();

	printk("[PMIC_DVT][tc_bif_100_step_0] 4. polling BIF_IRQ : BIF_CON31[11]=");
	bif_polling_irq();

    //4. disable BIF module
    printk("[PMIC_DVT][tc_bif_112_step_4] 4. disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);

    //5. data valid check
    printk("[PMIC_DVT][tc_bif_112_step_4] 5. data valid check\n");
    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x7, 12);
    printk("[PMIC_DVT][tc_bif_112_step_4] BIF_CON31[14:12]=0x%x\n", reg_val);

    if(reg_val == 0)
    {
        //6. read the number of read back data package
        printk("[PMIC_DVT][tc_bif_112_step_4] 6. read the number of read back data package\n");
        ret=pmic_read_interface(MT6325_BIF_CON19, &reg_val, 0xF, 0);
        printk("[PMIC_DVT][tc_bif_112_step_4] BIF_DATA_NUM (7) : BIF_CON19[3:0]=0x%x\n", reg_val);

        //7. read data back
        printk("[PMIC_DVT][tc_bif_112_step_4] 7. read data back\n");
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_112_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_112_step_4] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_112_step_4] BIF_DATA_0 (00h) : BIF_CON20[7:0]=0x%x\n", reg_val);

        ret=pmic_read_interface(MT6325_BIF_CON21, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_112_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON21[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON21, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_112_step_4] BIF_ACK_0 (1) : BIF_CON21[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON21, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_112_step_4] BIF_DATA_0 (01h) : BIF_CON21[7:0]=0x%x\n", reg_val);

        ret=pmic_read_interface(MT6325_BIF_CON22, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_112_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON22[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON22, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_112_step_4] BIF_ACK_0 (1) : BIF_CON22[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON22, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_112_step_4] BIF_DATA_0 (02h) : BIF_CON22[7:0]=0x%x\n", reg_val);

        ret=pmic_read_interface(MT6325_BIF_CON23, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_112_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON23[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON23, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_112_step_4] BIF_ACK_0 (1) : BIF_CON23[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON23, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_112_step_4] BIF_DATA_0 (03h) : BIF_CON23[7:0]=0x%x\n", reg_val);

        ret=pmic_read_interface(MT6325_BIF_CON24, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_112_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON24[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON24, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_112_step_4] BIF_ACK_0 (1) : BIF_CON24[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON24, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_112_step_4] BIF_DATA_0 (04h) : BIF_CON24[7:0]=0x%x\n", reg_val);

        ret=pmic_read_interface(MT6325_BIF_CON25, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_112_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON25[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON25, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_112_step_4] BIF_ACK_0 (1) : BIF_CON25[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON25, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_112_step_4] BIF_DATA_0 (05h) : BIF_CON25[7:0]=0x%x\n", reg_val);

        ret=pmic_read_interface(MT6325_BIF_CON26, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_112_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON26[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON26, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_112_step_4] BIF_ACK_0 (1) : BIF_CON26[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON26, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_112_step_4] BIF_DATA_0 (06h) : BIF_CON26[7:0]=0x%x\n", reg_val);
    }
    else
    {
        printk("[PMIC_DVT][tc_bif_112_step_4] Fail : BIF_CON31[14:12]=0x%x\n", reg_val);
    }

    //8. reset BIF_IRQ
    reset_bif_irq();
#endif
}

#endif

#if 1 // tc_bif_113
//int sccap_15_8 = 0;
//int sccap_7_0 = 0;
//int screg_15_8 = 0;
//int screg_7_0 = 0;

void tc_bif_113_step_2(void)
{
    int bif_cmd[4]={0,0,0,0};
	int bif_r_data[4]={0x02,0x01,0x02,0x00};

    //1. set command sequence
    printk("[PMIC_DVT][tc_bif_113_step_2] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x0424;
    bif_cmd[2]=0x0100;
    bif_cmd[3]=0x030E;
    set_bif_cmd(bif_cmd,4);


	tc_bif_step_common(113, 2, 0x4, 0x1, 0x4, bif_r_data);

#if 0
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[4]={0,0,0,0};
    int loop_i=0;

	  printk("[PMIC_DVT][tc_bif_113_step_2]-----------------------------------\n");



    //2. parameter setting
    printk("[PMIC_DVT][tc_bif_113_step_2] 2. parameter setting\n");
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x4, 0xF, 12);
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x1, 0x3, 8);
    //ret=pmic_config_interface(MT6325_BIF_CON17, 0x4, 0xF, 12);
    ret = pmic_set_register_value(PMIC_BIF_TRASFER_NUM, 4);
    ret = pmic_set_register_value(PMIC_BIF_COMMAND_TYPE, 1);
    ret = pmic_set_register_value(PMIC_BIF_READ_EXPECT_NUM, 4);
    //dump
    printk("[PMIC_DVT]Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_BIF_TRASFER_NUM].offset,pmic_get_register_value(PMIC_BIF_TRASFER_NUM),
        pmu_flags_table[PMIC_BIF_COMMAND_TYPE].offset,pmic_get_register_value(PMIC_BIF_COMMAND_TYPE),
        pmu_flags_table[PMIC_BIF_READ_EXPECT_NUM].offset,pmic_get_register_value(PMIC_BIF_READ_EXPECT_NUM));


    //3. trigger BIF module
    printk("[PMIC_DVT][tc_bif_113_step_2] 3. trigger BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    udelay(10);
    bif_debug();

	printk("[PMIC_DVT][tc_bif_100_step_0] 4. polling BIF_IRQ : BIF_CON31[11]=");
	bif_polling_irq();

    //4. disable BIF module
    printk("[PMIC_DVT][tc_bif_113_step_2] 4. disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);

    //5. data valid check
    printk("[PMIC_DVT][tc_bif_113_step_2] 5. data valid check\n");
    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x7, 12);
    printk("[PMIC_DVT][tc_bif_113_step_2] BIF_CON31[14:12]=0x%x\n", reg_val);

    if(reg_val == 0)
    {
        //6. read the number of read back data package
        printk("[PMIC_DVT][tc_bif_113_step_2] 6. read the number of read back data package\n");
        ret=pmic_read_interface(MT6325_BIF_CON19, &reg_val, 0xF, 0);
        printk("[PMIC_DVT][tc_bif_113_step_2] BIF_DATA_NUM (4) : BIF_CON19[3:0]=0x%x\n", reg_val);

        //7. read data back
        printk("[PMIC_DVT][tc_bif_113_step_2] 7. read data back\n");
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_113_step_2] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_113_step_2] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_113_step_2] BIF_DATA_0 (02H) : BIF_CON20[7:0]=0x%x\n", reg_val);

        ret=pmic_read_interface(MT6325_BIF_CON21, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_113_step_2] BIF_DATA_ERROR_0 (0) : BIF_CON21[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON21, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_113_step_2] BIF_ACK_0 (1) : BIF_CON21[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON21, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_113_step_2] BIF_DATA_0 (01H) : BIF_CON21[7:0]=0x%x\n", reg_val);

        ret=pmic_read_interface(MT6325_BIF_CON22, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_113_step_2] BIF_DATA_ERROR_0 (0) : BIF_CON22[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON22, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_113_step_2] BIF_ACK_0 (1) : BIF_CON22[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON22, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_113_step_2] BIF_DATA_0 (sccap[15:8]=02H) : BIF_CON22[7:0]=0x%x\n", reg_val);
        sccap_15_8=reg_val;

        ret=pmic_read_interface(MT6325_BIF_CON23, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_113_step_2] BIF_DATA_ERROR_0 (0) : BIF_CON23[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON23, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_113_step_2] BIF_ACK_0 (1) : BIF_CON23[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON23, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_113_step_2] BIF_DATA_0 (sccap[7:0]=00H) : BIF_CON23[7:0]=0x%x\n", reg_val);
        sccap_7_0=reg_val;
    }
    else
    {
        printk("[PMIC_DVT][tc_bif_113_step_2] Fail : BIF_CON31[14:12]=0x%x\n", reg_val);
    }

    //8. reset BIF_IRQ
    reset_bif_irq();
#endif
}

void tc_bif_113_step_3(void)
{
    int bif_cmd[4]={0,0,0,0};
	int bif_r_data[2]={0x0c,0x00};


    //1. set command sequence
    printk("[PMIC_DVT][tc_bif_113_step_3] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x0422;
    bif_cmd[2]=(0x0100|sccap_15_8);
    bif_cmd[3]=(0x0300|sccap_7_0);
    set_bif_cmd(bif_cmd,4);


	tc_bif_step_common(113, 3, 0x4, 0x1, 0x2, bif_r_data);

#if 0
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[4]={0,0,0,0};
    int loop_i=0;

	  printk("[PMIC_DVT][tc_bif_113_step_3]-----------------------------------\n");


    //2. parameter setting
    printk("[PMIC_DVT][tc_bif_113_step_3] 2. parameter setting\n");
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x4, 0xF, 12);
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x1, 0x3, 8);
    //ret=pmic_config_interface(MT6325_BIF_CON17, 0x2, 0xF, 12);
    ret = pmic_set_register_value(PMIC_BIF_TRASFER_NUM, 4);
    ret = pmic_set_register_value(PMIC_BIF_COMMAND_TYPE, 1);
    ret = pmic_set_register_value(PMIC_BIF_READ_EXPECT_NUM, 2);
    //dump
    printk("[PMIC_DVT]Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_BIF_TRASFER_NUM].offset,pmic_get_register_value(PMIC_BIF_TRASFER_NUM),
        pmu_flags_table[PMIC_BIF_COMMAND_TYPE].offset,pmic_get_register_value(PMIC_BIF_COMMAND_TYPE),
        pmu_flags_table[PMIC_BIF_READ_EXPECT_NUM].offset,pmic_get_register_value(PMIC_BIF_READ_EXPECT_NUM));

    //3. trigger BIF module
    printk("[PMIC_DVT][tc_bif_113_step_3] 3. trigger BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    udelay(10);
    bif_debug();

	printk("[PMIC_DVT][tc_bif_100_step_0] 4. polling BIF_IRQ : BIF_CON31[11]=");
	bif_polling_irq();

    //4. disable BIF module
    printk("[PMIC_DVT][tc_bif_113_step_3] 4. disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);

    //5. data valid check
    printk("[PMIC_DVT][tc_bif_113_step_3] 5. data valid check\n");
    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x7, 12);
    printk("[PMIC_DVT][tc_bif_113_step_3] BIF_CON31[14:12]=0x%x\n", reg_val);

    if(reg_val == 0)
    {
        //6. read the number of read back data package
        printk("[PMIC_DVT][tc_bif_113_step_3] 6. read the number of read back data package\n");
        ret=pmic_read_interface(MT6325_BIF_CON19, &reg_val, 0xF, 0);
        printk("[PMIC_DVT][tc_bif_113_step_3] BIF_DATA_NUM (2) : BIF_CON19[3:0]=0x%x\n", reg_val);

        //7. read data back
        printk("[PMIC_DVT][tc_bif_113_step_3] 7. read data back\n");
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_113_step_3] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_113_step_3] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_113_step_3] BIF_DATA_0 (screg[15:8]=0CH) : BIF_CON20[7:0]=0x%x\n", reg_val);
        screg_15_8=reg_val;

        ret=pmic_read_interface(MT6325_BIF_CON21, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_113_step_3] BIF_DATA_ERROR_0 (0) : BIF_CON21[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON21, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_113_step_3] BIF_ACK_0 (1) : BIF_CON21[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON21, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_113_step_3] BIF_DATA_0 (screg[7:0]=00H) : BIF_CON21[7:0]=0x%x\n", reg_val);
        screg_7_0=reg_val;
    }
    else
    {
        printk("[PMIC_DVT][tc_bif_113_step_3] Fail : BIF_CON31[14:12]=0x%x\n", reg_val);
    }

    //8. reset BIF_IRQ
    reset_bif_irq();
#endif
}

void tc_bif_113_step_4(void)
{
    int bif_cmd[4]={0,0,0,0};


    //1. set command sequence
    printk("[PMIC_DVT][tc_bif_113_step_4] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=(0x0100|screg_15_8);
    bif_cmd[2]=(0x0200|screg_7_0);
    bif_cmd[3]=0x00FF;
    set_bif_cmd(bif_cmd,4);


	tc_bif_step_common(113, 3, 0x4, 0x0, 0x0, NULL);

#if 0
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[4]={0,0,0,0};
    int loop_i=0;

	  printk("[PMIC_DVT][tc_bif_113_step_4]-----------------------------------\n");

    //1. set command sequence
    printk("[PMIC_DVT][tc_bif_113_step_4] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=(0x0100|screg_15_8);
    bif_cmd[2]=(0x0200|screg_7_0);
    bif_cmd[3]=0x00FF;
    set_bif_cmd(bif_cmd,4);

    //2. parameter setting
    printk("[PMIC_DVT][tc_bif_113_step_4] 2. parameter setting\n");
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x4, 0xF, 12);
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x0, 0x3, 8);
    //ret=pmic_config_interface(MT6325_BIF_CON17, 0x0, 0xF, 12);
    ret = pmic_set_register_value(PMIC_BIF_TRASFER_NUM, 4);
    ret = pmic_set_register_value(PMIC_BIF_COMMAND_TYPE, 0);
    //dump
    printk("[PMIC_DVT]Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_BIF_TRASFER_NUM].offset,pmic_get_register_value(PMIC_BIF_TRASFER_NUM),
        pmu_flags_table[PMIC_BIF_COMMAND_TYPE].offset,pmic_get_register_value(PMIC_BIF_COMMAND_TYPE),
        pmu_flags_table[PMIC_BIF_READ_EXPECT_NUM].offset,pmic_get_register_value(PMIC_BIF_READ_EXPECT_NUM));

    //3. trigger BIF module
    printk("[PMIC_DVT][tc_bif_113_step_4] 3. trigger BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    udelay(10);
    bif_debug();

	printk("[PMIC_DVT][tc_bif_100_step_0] 4. polling BIF_IRQ : BIF_CON31[11]=");
	bif_polling_irq();

    //4. disable BIF module
    printk("[PMIC_DVT][tc_bif_113_step_4] 4. disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);

    check_bat_lost();

    //5. reset BIF_IRQ
    reset_bif_irq();
#endif
}

//void tc_bif_113_step_5(void)
void tc_bif_113_step_5_positive(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[1]={0};
    int loop_i=0;

	  printk("[PMIC_DVT][tc_bif_113_step_5]-----------------------------------\n");

    //1. set command sequence
    printk("[PMIC_DVT][tc_bif_113_step_5] 1. set command sequence\n");
    bif_cmd[0]=0x0411;
    set_bif_cmd(bif_cmd,1);

    //2. parameter setting
    printk("[PMIC_DVT][tc_bif_113_step_5] 2. parameter setting\n");
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x1, 0xF, 12);
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x2, 0x3, 8);
    ret = pmic_set_register_value(PMIC_BIF_TRASFER_NUM, 1);
    ret = pmic_set_register_value(PMIC_BIF_COMMAND_TYPE, 2);

    //dump
    printk("[PMIC_DVT]Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_BIF_TRASFER_NUM].offset,pmic_get_register_value(PMIC_BIF_TRASFER_NUM),
        pmu_flags_table[PMIC_BIF_COMMAND_TYPE].offset,pmic_get_register_value(PMIC_BIF_COMMAND_TYPE),
        pmu_flags_table[PMIC_BIF_READ_EXPECT_NUM].offset,pmic_get_register_value(PMIC_BIF_READ_EXPECT_NUM));

    //3. trigger BIF module
    printk("[PMIC_DVT][tc_bif_113_step_5] 3. trigger BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TIMEOUT_SET, 0x7F);
	ret=pmic_set_register_value(PMIC_BIF_RX_DATA_SW, 1);
	ret=pmic_set_register_value(PMIC_BIF_TEST_MODE4, 1);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    mdelay(10);
    bif_debug();
    ret=pmic_set_register_value(PMIC_BIF_RX_DATA_SW, 0);
	printk("[PMIC_DVT][tc_bif_100_step_0] 4. polling BIF_IRQ : BIF_CON31[11]=");
	bif_polling_irq();

    //4. disable BIF module
    printk("[PMIC_DVT][tc_bif_113_step_5] 4. disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);
	ret=pmic_set_register_value(PMIC_BIF_TEST_MODE4, 0);

    //5. data valid check
    printk("[PMIC_DVT][tc_bif_113_step_5] 5. data valid check\n");
	reg_val=pmic_get_register_value(PMIC_BIF_BAT_LOST);
	if (reg_val == 0)
	    printk("[PMIC_DVT]][Fail][tc_bif_113_step_5] BAT_LOST[13]=0x%x\n", reg_val);
	else
		PMICDBG(0, "[PMIC_DVT]][Fail][tc_bif_113_step_5] BAT_LOST[13]=0x%x\n", reg_val);
	reg_val=pmic_get_register_value(PMIC_BIF_TIMEOUT);
	if (reg_val == 0)
	    printk("[PMIC_DVT]][Fail][tc_bif_113_step_5] TIMEOUT[12]=0x%x\n", reg_val);
	else
		PMICDBG(0, "[PMIC_DVT]][Fail][tc_bif_113_step_5] TIMEOUT[12]=0x%x\n", reg_val);
	reg_val=pmic_get_register_value(PMIC_BIF_RESPONSE);
	if (reg_val == 1)
	    printk("[PMIC_DVT]][Fail][tc_bif_113_step_5] BIF_RESPONSE[12]=0x%x\n", reg_val);
	else
		PMICDBG(0, "[PMIC_DVT]][Fail][tc_bif_113_step_5] BIF_RESPONSE[12]=0x%x\n", reg_val);

//    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x1, 13);
//    printk("[PMIC_DVT][tc_bif_113_step_5] BIF_BAT_LOST : BIF_CON31[13]=0x%x - battery is detected(0)/battery is undetected(1)\n", reg_val);
//    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x1, 12);
//    printk("[PMIC_DVT][tc_bif_113_step_5] BIF_TIMEOUT : BIF_CON31[12]=0x%x - positive(0)/negative(1)\n", reg_val);
//    ret=pmic_read_interface(MT6325_BIF_CON19, &reg_val, 0x1, 12);
//    printk("[PMIC_DVT][tc_bif_113_step_5] BIF_RESPONSE : BIF_CON19[12]=0x%x - positive(1)/negative(0)\n", reg_val);

    //6. reset BIF_IRQ
    reset_bif_irq();
}

#if 0
void tc_bif_113_step_5_positive_old(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[1]={0};
    int loop_i=0;

	printk("[PMIC_DVT][tc_bif_113_step_5_positive]-----------------------------------\n");

    //1. set command sequence
    printk("[PMIC_DVT][tc_bif_113_step_5_positive] 1. set command sequence\n");
    bif_cmd[0]=0x0411;
    set_bif_cmd(bif_cmd,1);

    //2. parameter setting
    printk("[PMIC_DVT][tc_bif_113_step_5_positive] 2. parameter setting\n");
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x1, 0xF, 12);
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x2, 0x3, 8);
    ret = pmic_set_register_value(PMIC_BIF_TRASFER_NUM,1);
    ret = pmic_set_register_value(PMIC_BIF_COMMAND_TYPE, 2);

    //new
    ret=pmic_config_interface(MT6325_BIF_CON15,0x7F, 0x7F,0);

    //only E2
    ret=pmic_config_interface(MT6325_BIF_CON37,0x1FF, 0x1FF,0);

    ret=pmic_config_interface(MT6325_BIF_CON30, 0x1, 0x1, 12);
    ret=pmic_config_interface(MT6325_BIF_CON30, 0x1, 0x1, 4);

    //dump
    printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        MT6325_BIF_CON15,upmu_get_reg_value(MT6325_BIF_CON15),
        MT6325_BIF_CON17,upmu_get_reg_value(MT6325_BIF_CON17),
        MT6325_BIF_CON30,upmu_get_reg_value(MT6325_BIF_CON30),
        MT6325_BIF_CON37,upmu_get_reg_value(MT6325_BIF_CON37)
        );

    //3. trigger BIF module
    printk("[PMIC_DVT][tc_bif_113_step_5_positive] 5. trigger BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    udelay(10);
    bif_debug();

    //new
    mdelay(10);
    ret=pmic_config_interface(MT6325_BIF_CON30, 0x0, 0x1, 12);
    //dump
    printk(ANDROID_LOG_INFO, "Power/PMIC", "BIF_RX_DATA_SW=0 : Reg[0x%x]=0x%x\n",
        MT6325_BIF_CON30,upmu_get_reg_value(MT6325_BIF_CON30)
        );

	printk("[PMIC_DVT][tc_bif_100_step_0] 4. polling BIF_IRQ : BIF_CON31[11]=");
	bif_polling_irq();

    //disable BIF module
    printk("[PMIC_DVT][tc_bif_113_step_5_positive] disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);

    printk("[PMIC_DVT][tc_bif_113_step_5_positive] disable SW control\n");
    ret=pmic_config_interface(MT6325_BIF_CON30, 0x0, 0x1, 4);

    //data valid check
    printk("[PMIC_DVT][tc_bif_113_step_5_positive] 8. data valid check\n");
    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x1, 13);
    printk("[PMIC_DVT][tc_bif_113_step_5_positive] BIF_BAT_LOST : BIF_CON31[13]=0x%x - battery is detected(0)/battery is undetected(1)\n", reg_val);
    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x1, 12);
    printk("[PMIC_DVT][tc_bif_113_step_5_positive] BIF_TIMEOUT : BIF_CON31[12]=0x%x - positive(0)/negative(1)\n", reg_val);
    ret=pmic_read_interface(MT6325_BIF_CON19, &reg_val, 0x1, 12);
    printk("[PMIC_DVT][tc_bif_113_step_5_positive] BIF_RESPONSE : BIF_CON19[12]=0x%x - positive(1)/negative(0)\n", reg_val);

    //6. reset BIF_IRQ
    reset_bif_irq();
}
#endif
void tc_bif_113_step_5_negative(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[1]={0};
    int loop_i=0;

	  printk("[PMIC_DVT][tc_bif_113_step_5_negative]-----------------------------------\n");

    //1. set command sequence
    printk("[PMIC_DVT][tc_bif_113_step_5_negative] 1. set command sequence\n");
    bif_cmd[0]=0x0411;
    set_bif_cmd(bif_cmd,1);

    //2. parameter setting
    printk("[PMIC_DVT][tc_bif_113_step_5_negative] 2. parameter setting\n");
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x1, 0xF, 12);
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x2, 0x3, 8);
    ret = pmic_set_register_value(PMIC_BIF_TRASFER_NUM, 1);
    ret = pmic_set_register_value(PMIC_BIF_COMMAND_TYPE, 2);
    //dump
    printk("[PMIC_DVT]Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_BIF_TRASFER_NUM].offset,pmic_get_register_value(PMIC_BIF_TRASFER_NUM),
        pmu_flags_table[PMIC_BIF_COMMAND_TYPE].offset,pmic_get_register_value(PMIC_BIF_COMMAND_TYPE),
        pmu_flags_table[PMIC_BIF_READ_EXPECT_NUM].offset,pmic_get_register_value(PMIC_BIF_READ_EXPECT_NUM));


    //3. trigger BIF module
    printk("[PMIC_DVT][tc_bif_113_step_5_negative] 3. trigger BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    udelay(10);
    bif_debug();

	printk("[PMIC_DVT][tc_bif_100_step_0] 4. polling BIF_IRQ : BIF_CON31[11]=");
	bif_polling_irq();

    //4. disable BIF module
    printk("[PMIC_DVT][tc_bif_113_step_5_negative] 4. disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);

    //5. data valid check
    printk("[PMIC_DVT][tc_bif_113_step_5_negative] 5. data valid check\n");
	reg_val=pmic_get_register_value(PMIC_BIF_BAT_LOST);
	if (reg_val == 0)
	    printk("[PMIC_DVT]][Fail][tc_bif_113_step_5_negative] BAT_LOST[13]=0x%x\n", reg_val);
	else
		PMICDBG(0, "[PMIC_DVT]][Fail][tc_bif_113_step_5_negative] BAT_LOST[13]=0x%x\n", reg_val);
	reg_val=pmic_get_register_value(PMIC_BIF_TIMEOUT);
	if (reg_val == 1)
	    printk("[PMIC_DVT]][Fail][tc_bif_113_step_5_negative] TIMEOUT[12]=0x%x\n", reg_val);
	else
		PMICDBG(0, "[PMIC_DVT]][Fail][tc_bif_113_step_5_negative] TIMEOUT[12]=0x%x\n", reg_val);
	reg_val=pmic_get_register_value(PMIC_BIF_RESPONSE);
	if (reg_val == 0)
	    printk("[PMIC_DVT]][Fail][tc_bif_113_step_5_negative] BIF_RESPONSE[12]=0x%x\n", reg_val);
	else
		PMICDBG(0, "[PMIC_DVT]][Fail][tc_bif_113_step_5_negative] BIF_RESPONSE[12]=0x%x\n", reg_val);


    //5. data valid check
//    printk("[PMIC_DVT][tc_bif_113_step_5_negative] 5. data valid check\n");
//    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x1, 13);
//    printk("[PMIC_DVT][tc_bif_113_step_5_negative] BIF_BAT_LOST : BIF_CON31[13]=0x%x - battery is detected(0)/battery is undetected(1)\n", reg_val);
//    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x1, 12);
//    printk("[PMIC_DVT][tc_bif_113_step_5_negative] BIF_TIMEOUT : BIF_CON31[12]=0x%x - positive(0)/negative(1)\n", reg_val);
//    ret=pmic_read_interface(MT6325_BIF_CON19, &reg_val, 0x1, 12);
//    printk("[PMIC_DVT][tc_bif_113_step_5_negative] BIF_RESPONSE : BIF_CON19[12]=0x%x - positive(1)/negative(0)\n", reg_val);

    //6. reset BIF_IRQ
    reset_bif_irq();
}
#endif

#if 1 // tc_bif_114
void tc_bif_114_step_1_positive(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[1]={0};
    int loop_i=0;

	  printk("[PMIC_DVT][tc_bif_114_step_1_positive]-----------------------------------\n");

    //1. set command sequence
    printk("[PMIC_DVT][tc_bif_114_step_1_positive] 1. set command sequence\n");
    bif_cmd[0]=0x0410;
    set_bif_cmd(bif_cmd,1);

    //2. parameter setting
    printk("[PMIC_DVT][tc_bif_114_step_1_positive] 2. parameter setting\n");
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x1, 0xF, 12);
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x2, 0x3, 8);
    ret = pmic_set_register_value(PMIC_BIF_TRASFER_NUM, 1);
    ret = pmic_set_register_value(PMIC_BIF_COMMAND_TYPE, 2);

    //dump
    printk("[PMIC_DVT]Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_BIF_TRASFER_NUM].offset,pmic_get_register_value(PMIC_BIF_TRASFER_NUM),
        pmu_flags_table[PMIC_BIF_COMMAND_TYPE].offset,pmic_get_register_value(PMIC_BIF_COMMAND_TYPE),
        pmu_flags_table[PMIC_BIF_READ_EXPECT_NUM].offset,pmic_get_register_value(PMIC_BIF_READ_EXPECT_NUM));


    //3. trigger BIF module
    printk("[PMIC_DVT][tc_bif_114_step_1_positive] 3. trigger BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    //----------------------------------------------------------------
    udelay(10);
    bif_debug();
    mdelay(1000);
    printk("[PMIC_DVT][tc_bif_114_step_1_positive] wait 1s and then\n");

    //new
//    ret=pmic_config_interface(MT6325_BIF_CON30, 0x0, 0x1, 12);
//    ret=pmic_config_interface(MT6325_BIF_CON30, 0x1, 0x1, 4);
    //dump
//    printk(ANDROID_LOG_INFO, "Power/PMIC", "after set, Reg[0x%x]=0x%x\n",
//        MT6325_BIF_CON30,upmu_get_reg_value(MT6325_BIF_CON30)
//        );

	ret=pmic_set_register_value(PMIC_BIF_RX_DATA_SW, 0);
	ret=pmic_set_register_value(PMIC_BIF_TEST_MODE4, 1);

	printk("[PMIC_DVT][tc_bif_100_step_0] 4. polling BIF_IRQ : BIF_CON31[11]=");
	bif_polling_irq();

    //5. disable BIF module
    printk("[PMIC_DVT][tc_bif_114_step_1_positive] 5. disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);


    printk("[PMIC_DVT][tc_bif_114_step_1_positive] disable SW control\n");
//    ret=pmic_config_interface(MT6325_BIF_CON30, 0x0, 0x1, 4);
	ret=pmic_set_register_value(PMIC_BIF_TEST_MODE4, 0);

    //6. data valid check
    printk("[PMIC_DVT][tc_bif_113_step_5] 5. data valid check\n");
	reg_val=pmic_get_register_value(PMIC_BIF_BAT_LOST);
	if (reg_val == 0)
	    printk("[PMIC_DVT]][Fail][tc_bif_113_step_5] BAT_LOST[13]=0x%x\n", reg_val);
	else
		PMICDBG(0, "[PMIC_DVT]][Fail][tc_bif_113_step_5] BAT_LOST[13]=0x%x\n", reg_val);
	reg_val=pmic_get_register_value(PMIC_BIF_TIMEOUT);
	if (reg_val == 0)
	    printk("[PMIC_DVT]][Fail][tc_bif_113_step_5] TIMEOUT[12]=0x%x\n", reg_val);
	else
		PMICDBG(0, "[PMIC_DVT]][Fail][tc_bif_113_step_5] TIMEOUT[12]=0x%x\n", reg_val);
	reg_val=pmic_get_register_value(PMIC_BIF_RESPONSE);
	if (reg_val == 1)
	    printk("[PMIC_DVT]][Fail][tc_bif_113_step_5] BIF_RESPONSE[12]=0x%x\n", reg_val);
	else
		PMICDBG(0, "[PMIC_DVT]][Fail][tc_bif_113_step_5] BIF_RESPONSE[12]=0x%x\n", reg_val);


    //6. data valid check
//    printk("[PMIC_DVT][tc_bif_114_step_1_positive] 6. data valid check\n");
//    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x1, 13);
//    printk("[PMIC_DVT][tc_bif_114_step_1_positive] BIF_BAT_LOST : BIF_CON31[13]=0x%x - battery is detected(0)/battery is undetected(1)\n", reg_val);
//    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x1, 12);
//    printk("[PMIC_DVT][tc_bif_114_step_1_positive] BIF_TIMEOUT : BIF_CON31[12]=0x%x - positive(0)/negative(1)\n", reg_val);
//    ret=pmic_read_interface(MT6325_BIF_CON19, &reg_val, 0x1, 12);
//    printk("[PMIC_DVT][tc_bif_114_step_1_positive] BIF_RESPONSE : BIF_CON19[12]=0x%x - positive(1)/negative(0)\n", reg_val);

    //6. reset BIF_IRQ
    reset_bif_irq();
}

void tc_bif_114_step_1_negative(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[1]={0};
    int loop_i=0;

	  printk("[PMIC_DVT][tc_bif_114_step_1_negative]-----------------------------------\n");

    //1. set command sequence
    printk("[PMIC_DVT][tc_bif_114_step_1_negative] 1. set command sequence\n");
    bif_cmd[0]=0x0410;
    set_bif_cmd(bif_cmd,1);

    //2. parameter setting
    printk("[PMIC_DVT][tc_bif_114_step_1_negative] 2. parameter setting\n");
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x1, 0xF, 12);
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x2, 0x3, 8);
    ret = pmic_set_register_value(PMIC_BIF_TRASFER_NUM, 1);
    ret = pmic_set_register_value(PMIC_BIF_COMMAND_TYPE, 2);
    //dump
    printk("[PMIC_DVT]Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_BIF_TRASFER_NUM].offset,pmic_get_register_value(PMIC_BIF_TRASFER_NUM),
        pmu_flags_table[PMIC_BIF_COMMAND_TYPE].offset,pmic_get_register_value(PMIC_BIF_COMMAND_TYPE),
        pmu_flags_table[PMIC_BIF_READ_EXPECT_NUM].offset,pmic_get_register_value(PMIC_BIF_READ_EXPECT_NUM));


    //3. trigger BIF module
    printk("[PMIC_DVT][tc_bif_114_step_1_negative] 3. trigger BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    //----------------------------------------------------------------
    udelay(10);
    bif_debug();
    mdelay(1000);
    printk("[PMIC_DVT][tc_bif_114_step_1_negative] wait 1s and then\n");

    //4. BIF_BACK_NORMAL = 1
    printk("[PMIC_DVT][tc_bif_114_step_1_negative] 4. BIF_BACK_NORMAL = 1\n");
//    ret=pmic_config_interface(MT6325_BIF_CON31, 0x1, 0x1, 0);
//    printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x\n",
//        MT6325_BIF_CON31,upmu_get_reg_value(MT6325_BIF_CON31)
//        );
	ret=pmic_set_register_value(PMIC_BIF_BACK_NORMAL, 1);

	printk("[PMIC_DVT][tc_bif_100_step_0] 4. polling BIF_IRQ : BIF_CON31[11]=");
	bif_polling_irq();

    //5. disable BIF module
    printk("[PMIC_DVT][tc_bif_114_step_1_negative] 5. disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);

    printk("[PMIC_DVT][tc_bif_114_step_1_negative] clear rg_back_normal\n");
    //ret=pmic_config_interface(MT6325_BIF_CON31, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_BACK_NORMAL, 0);

    //6. data valid check
    printk("[PMIC_DVT][tc_bif_113_step_5_negative] 5. data valid check\n");
	reg_val=pmic_get_register_value(PMIC_BIF_BAT_LOST);
	if (reg_val == 0)
	    printk("[PMIC_DVT]][Fail][tc_bif_113_step_5_negative] BAT_LOST[13]=0x%x\n", reg_val);
	else
		PMICDBG(0, "[PMIC_DVT]][Fail][tc_bif_113_step_5_negative] BAT_LOST[13]=0x%x\n", reg_val);
	reg_val=pmic_get_register_value(PMIC_BIF_TIMEOUT);
	if (reg_val == 1)
	    printk("[PMIC_DVT]][Fail][tc_bif_113_step_5_negative] TIMEOUT[12]=0x%x\n", reg_val);
	else
		PMICDBG(0, "[PMIC_DVT]][Fail][tc_bif_113_step_5_negative] TIMEOUT[12]=0x%x\n", reg_val);
	reg_val=pmic_get_register_value(PMIC_BIF_RESPONSE);
	if (reg_val == 0)
	    printk("[PMIC_DVT]][Fail][tc_bif_113_step_5_negative] BIF_RESPONSE[12]=0x%x\n", reg_val);
	else
		PMICDBG(0, "[PMIC_DVT]][Fail][tc_bif_113_step_5_negative] BIF_RESPONSE[12]=0x%x\n", reg_val);

    //6. data valid check
//    printk("[PMIC_DVT][tc_bif_114_step_1_negative] 6. data valid check\n");
//    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x1, 13);
//    printk("[PMIC_DVT][tc_bif_114_step_1_negative] BIF_BAT_LOST : BIF_CON31[13]=0x%x - battery is detected(0)/battery is undetected(1)\n", reg_val);
//    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x1, 12);
//    printk("[PMIC_DVT][tc_bif_114_step_1_negative] BIF_TIMEOUT : BIF_CON31[12]=0x%x - positive(0)/negative(1)\n", reg_val);
//    ret=pmic_read_interface(MT6325_BIF_CON19, &reg_val, 0x1, 12);
//    printk("[PMIC_DVT][tc_bif_114_step_1_negative] BIF_RESPONSE : BIF_CON19[12]=0x%x - positive(1)/negative(0)\n", reg_val);

    //6. reset BIF_IRQ
    reset_bif_irq();
}

#endif

#if 1 // tc_bif_115
void tc_bif_115_step1(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[11]={0,0,0,0,0,0,0,0,0,0,0};

	printk("[PMIC_DVT][tc_bif_115_step1]-----------------------------------\n");

    printk("[PMIC_DVT][tc_bif_115_step1] 1. sw control and switch to sw mode\n");
    //ret=pmic_config_interface(MT6325_BIF_CON30, 0x1, 0x1, 11);
    ret=pmic_set_register_value(PMIC_BIF_BAT_LOST_SW, 1);
    //ret=pmic_config_interface(MT6325_BIF_CON30, 0x1, 0x1, 6);
    ret=pmic_set_register_value(PMIC_BIF_TEST_MODE6, 1);
    //dump
    printk("[PMIC_DVT]Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,\n",
        pmu_flags_table[PMIC_BIF_BAT_LOST_SW].offset,pmic_get_register_value(PMIC_BIF_BAT_LOST_SW),
        pmu_flags_table[PMIC_BIF_TEST_MODE6].offset,pmic_get_register_value(PMIC_BIF_TEST_MODE6));


    //set command sequence
    printk("[PMIC_DVT][tc_bif_115_step1] 2. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x010F;
    bif_cmd[2]=0x0209;
    bif_cmd[3]=0x0000;
    bif_cmd[4]=0x0001;
    bif_cmd[5]=0x0002;
    bif_cmd[6]=0x0003;
    bif_cmd[7]=0x0004;
    bif_cmd[8]=0x0005;
    bif_cmd[9]=0x0006;
    bif_cmd[10]=0x04C2;
    set_bif_cmd(bif_cmd,11);

    //parameter setting
    printk("[PMIC_DVT][tc_bif_115_step1] 3. parameter setting\n");
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0xB, 0xF, 12);
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x0, 0x3, 8);
    //ret=pmic_config_interface(MT6325_BIF_CON17, 0x1, 0xF, 12);
    ret = pmic_set_register_value(PMIC_BIF_TRASFER_NUM, 0xb);
    ret = pmic_set_register_value(PMIC_BIF_COMMAND_TYPE, 0);
    ret = pmic_set_register_value(PMIC_BIF_READ_EXPECT_NUM, 1);
    //dump
//    printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
//        MT6325_BIF_CON15,upmu_get_reg_value(MT6325_BIF_CON15),
//        MT6325_BIF_CON17,upmu_get_reg_value(MT6325_BIF_CON17)
//        );

    //3. trigger BIF module
    printk("[PMIC_DVT][tc_bif_115_step1] 4. trigger BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    udelay(10);
    bif_debug();
    mdelay(1000);

    printk("[PMIC_DVT][tc_bif_115_step1] After 1s\n");

    reg_val=0;
    //while(reg_val == 0)
    {
        //ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x1, 11);
        reg_val=pmic_get_register_value(PMIC_BIF_IRQ);
		if(reg_val == 0)
        	printk("[PMIC_DVT][Pass][tc_bif_115_step1] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);
		else
			PMICDBG(0,"[PMIC_DVT][Fail][tc_bif_115_step1] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);
    }

    //disable BIF module
    printk("[PMIC_DVT][tc_bif_115_step1] disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);

    //reset BIF_IRQ
    reset_bif_irq();
}
#endif

#if 1 // tc_bif_116
void tc_bif_116_step_1(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[11]={0,0,0,0,0,0,0,0,0,0,0};
    int loop_i=0;

	printk("[PMIC_DVT][tc_bif_116_step_1]-----------------------------------\n");

    //1. set command sequence
    printk("[PMIC_DVT][tc_bif_116_step_1] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x010F;
    bif_cmd[2]=0x0209;
    bif_cmd[3]=0x0000;
    bif_cmd[4]=0x0001;
    bif_cmd[5]=0x0002;
    bif_cmd[6]=0x0003;
    bif_cmd[7]=0x0004;
    bif_cmd[8]=0x0005;
    bif_cmd[9]=0x0006;
    bif_cmd[10]=0x04C2;
    set_bif_cmd(bif_cmd,11);

    //2. parameter setting
    printk("[PMIC_DVT][tc_bif_116_step_1] 2. parameter setting\n");
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0xB, 0xF, 12);
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x0, 0x3, 8);
    //ret=pmic_config_interface(MT6325_BIF_CON17, 0x1, 0xF, 12);
    ret = pmic_set_register_value(PMIC_BIF_TRASFER_NUM, 0xb);
    ret = pmic_set_register_value(PMIC_BIF_COMMAND_TYPE, 0);
    ret = pmic_set_register_value(PMIC_BIF_READ_EXPECT_NUM, 1);
    //dump
    printk("[PMIC_DVT]Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_BIF_TRASFER_NUM].offset,pmic_get_register_value(PMIC_BIF_TRASFER_NUM),
        pmu_flags_table[PMIC_BIF_COMMAND_TYPE].offset,pmic_get_register_value(PMIC_BIF_COMMAND_TYPE),
        pmu_flags_table[PMIC_BIF_READ_EXPECT_NUM].offset,pmic_get_register_value(PMIC_BIF_READ_EXPECT_NUM));


    //3. trigger BIF module
    printk("[PMIC_DVT][tc_bif_116_step_1] 3. trigger BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    udelay(10);
    bif_debug();

    mdelay(10);
    printk("[PMIC_DVT][tc_bif_116_step_1] wait 10ms and then\n");

    //ret=pmic_config_interface(MT6325_BIF_CON30, 0x1, 0x1, 11);
    ret=pmic_set_register_value(PMIC_BIF_BAT_LOST_SW, 1);
    //ret=pmic_config_interface(MT6325_BIF_CON30, 0x1, 0x1, 6);
    ret=pmic_set_register_value(PMIC_BIF_TEST_MODE6, 1);

    //dump
//    printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x\n",
//        MT6325_BIF_CON30,upmu_get_reg_value(MT6325_BIF_CON30)
//        );

	printk("[PMIC_DVT][tc_bif_116_step_1] polling BIF_IRQ : BIF_CON31[11]=");
	bif_polling_irq();

    printk("[PMIC_DVT][tc_bif_116_step_1] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

    //ret=pmic_config_interface(MT6325_BIF_CON30, 0x0, 0x1, 6);
    ret=pmic_set_register_value(PMIC_BIF_TEST_MODE6, 0);
    //dump
//    printk(ANDROID_LOG_INFO, "Power/PMIC", "disable sw control : Reg[0x%x]=0x%x\n",
//        MT6325_BIF_CON30,upmu_get_reg_value(MT6325_BIF_CON30)
//        );

    //disable BIF module
    printk("[PMIC_DVT][tc_bif_116_step_1] disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);

	//data valid check
    printk("[PMIC_DVT][tc_bif_116_step_1] 5. data valid check\n");
	reg_val=pmic_get_register_value(PMIC_BIF_BAT_LOST);
	if (reg_val == 1)
	    printk("[PMIC_DVT]][Fail][tc_bif_116_step_1] BAT_LOST[13]=0x%x\n", reg_val);
	else
		PMICDBG(0, "[PMIC_DVT]][Fail][tc_bif_116_step_1] BAT_LOST[13]=0x%x\n", reg_val);

    //data valid check
//    printk("[PMIC_DVT][tc_bif_116_step_1] data valid check\n");
//    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x1, 12);
//    printk("[PMIC_DVT][tc_bif_116_step_1] BIF_CON31[12]=0x%x, need 0\n", reg_val);
//    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x1, 13);
//    printk("[PMIC_DVT][tc_bif_116_step_1] BIF_CON31[13]=0x%x, need 1\n", reg_val);
//    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x1, 14);
//    printk("[PMIC_DVT][tc_bif_116_step_1] BIF_CON31[14]=0x%x, need 0\n", reg_val);

    //reset BIF_IRQ
    reset_bif_irq();
}

void tc_bif_116_step_2(void)
{
    int bif_cmd[11]={0,0,0,0,0,0,0,0,0,0,0};
	int bif_r_data[1]={0x0a};


    //1. set command sequence
    printk("[PMIC_DVT][tc_bif_116_step_2] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x010F;
    bif_cmd[2]=0x0209;
    bif_cmd[3]=0x0000;
    bif_cmd[4]=0x0001;
    bif_cmd[5]=0x0002;
    bif_cmd[6]=0x0003;
    bif_cmd[7]=0x0004;
    bif_cmd[8]=0x0005;
    bif_cmd[9]=0x0006;
    bif_cmd[10]=0x04C2;
    set_bif_cmd(bif_cmd,11);


	tc_bif_step_common(116, 2, 0xb, 0x0, 0x1, bif_r_data);

#if 0
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[11]={0,0,0,0,0,0,0,0,0,0,0};
    int loop_i=0;

	printk("[PMIC_DVT][tc_bif_116_step_2]-----------------------------------\n");



    //2. parameter setting
    printk("[PMIC_DVT][tc_bif_116_step_2] 2. parameter setting\n");
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0xB, 0xF, 12);
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x0, 0x3, 8);
    //ret=pmic_config_interface(MT6325_BIF_CON17, 0x1, 0xF, 12);
    ret = pmic_set_register_value(PMIC_BIF_TRASFER_NUM, 0xb);
    ret = pmic_set_register_value(PMIC_BIF_COMMAND_TYPE, 0);
    ret = pmic_set_register_value(PMIC_BIF_READ_EXPECT_NUM, 1);
    //dump
    printk("[PMIC_DVT]Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_BIF_TRASFER_NUM].offset,pmic_get_register_value(PMIC_BIF_TRASFER_NUM),
        pmu_flags_table[PMIC_BIF_COMMAND_TYPE].offset,pmic_get_register_value(PMIC_BIF_COMMAND_TYPE),
        pmu_flags_table[PMIC_BIF_READ_EXPECT_NUM].offset,pmic_get_register_value(PMIC_BIF_READ_EXPECT_NUM));


    //3. trigger BIF module
    printk("[PMIC_DVT][tc_bif_116_step_2] 3. trigger BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    udelay(10);
    bif_debug();

	printk("[PMIC_DVT][PMIC_DVT][tc_bif_116_step_2] polling BIF_IRQ : BIF_CON31[11]=");
	bif_polling_irq();

    printk("[PMIC_DVT][tc_bif_116_step_2] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

    //disable BIF module
    printk("[PMIC_DVT][tc_bif_116_step_2] disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);

    //data valid check
    printk("[PMIC_DVT][tc_bif_116_step_2] data valid check\n");
    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x7, 12);
    printk("[PMIC_DVT][tc_bif_116_step_2] BIF_CON31[14:12]=0x%x\n", reg_val);

    if(reg_val == 0)
    {
        //6. read the number of read back data package
        printk("[PMIC_DVT][tc_bif_116_step_2] read the number of read back data package\n");
        ret=pmic_read_interface(MT6325_BIF_CON19, &reg_val, 0xF, 0);
        printk("[PMIC_DVT][tc_bif_116_step_2] BIF_DATA_NUM (1) : BIF_CON19[3:0]=0x%x\n", reg_val);

        //7. read data back
        printk("[PMIC_DVT][tc_bif_116_step_2] read data back\n");
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_116_step_2] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_116_step_2] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_116_step_2] BIF_DATA_0 (0AH) : BIF_CON20[7:0]=0x%x\n", reg_val);
    }
    else
    {
        printk("[PMIC_DVT][tc_bif_116_step_2] Fail : BIF_CON31[14:12]=0x%x\n", reg_val);
    }

    //reset BIF_IRQ
    reset_bif_irq();
#endif
}

void tc_bif_116_step_3(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[4]={0,0,0,0};
    int loop_i=0;

	printk("[PMIC_DVT][tc_bif_116_step_3]-----------------------------------\n");

    //1. set command sequence
    printk("[PMIC_DVT][tc_bif_116_step_3] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x0427;
    bif_cmd[2]=0x010F;
    bif_cmd[3]=0x0309;
    set_bif_cmd(bif_cmd,4);

    //2. parameter setting
    printk("[PMIC_DVT][tc_bif_116_step_3] 2. parameter setting\n");
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x4, 0xF, 12);
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x1, 0x3, 8);
    //ret=pmic_config_interface(MT6325_BIF_CON17, 0x7, 0xF, 12);
    ret = pmic_set_register_value(PMIC_BIF_TRASFER_NUM, 4);
    ret = pmic_set_register_value(PMIC_BIF_COMMAND_TYPE, 1);
    ret = pmic_set_register_value(PMIC_BIF_READ_EXPECT_NUM, 7);
    //dump
    printk("[PMIC_DVT]Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_BIF_TRASFER_NUM].offset,pmic_get_register_value(PMIC_BIF_TRASFER_NUM),
        pmu_flags_table[PMIC_BIF_COMMAND_TYPE].offset,pmic_get_register_value(PMIC_BIF_COMMAND_TYPE),
        pmu_flags_table[PMIC_BIF_READ_EXPECT_NUM].offset,pmic_get_register_value(PMIC_BIF_READ_EXPECT_NUM));


    //3. trigger BIF module
    printk("[PMIC_DVT][tc_bif_116_step_3] 3. trigger BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    udelay(10);
    bif_debug();

    mdelay(10);
    printk("[PMIC_DVT][tc_bif_116_step_3] wait 10ms and then\n");

    //ret=pmic_config_interface(MT6325_BIF_CON30, 0x1, 0x1, 11);
    ret=pmic_set_register_value(PMIC_BIF_BAT_LOST_SW, 1);
    //ret=pmic_config_interface(MT6325_BIF_CON30, 0x1, 0x1, 6);
    ret=pmic_set_register_value(PMIC_BIF_TEST_MODE6, 1);

    //dump
//    printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x\n",
//        MT6325_BIF_CON30,upmu_get_reg_value(MT6325_BIF_CON30)
//        );

	printk("[PMIC_DVT][tc_bif_116_step_3] polling BIF_IRQ : BIF_CON31[11]=");
	bif_polling_irq();

    printk("[PMIC_DVT][tc_bif_116_step_3] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

    //ret=pmic_config_interface(MT6325_BIF_CON30, 0x0, 0x1, 6);
	ret=pmic_set_register_value(PMIC_BIF_TEST_MODE6, 0);
    //dump
//    printk(ANDROID_LOG_INFO, "Power/PMIC", "disable sw control : Reg[0x%x]=0x%x\n",
//        MT6325_BIF_CON30,upmu_get_reg_value(MT6325_BIF_CON30)
//        );

    //disable BIF module
    printk("[PMIC_DVT][tc_bif_116_step_3] disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);

    //data valid check
    printk("[PMIC_DVT][tc_bif_116_step_1] 5. data valid check\n");
	reg_val=pmic_get_register_value(PMIC_BIF_BAT_LOST);
	if (reg_val == 1)
	    printk("[PMIC_DVT]][Fail][tc_bif_116_step_1] BAT_LOST[13]=0x%x\n", reg_val);
	else
		PMICDBG(0, "[PMIC_DVT]][Fail][tc_bif_116_step_1] BAT_LOST[13]=0x%x\n", reg_val);


//    printk("[PMIC_DVT][tc_bif_116_step_3] data valid check\n");
//    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x1, 12);
//    printk("[PMIC_DVT][tc_bif_116_step_3] BIF_CON31[12]=0x%x, need 0\n", reg_val);
//    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x1, 13);
//    printk("[PMIC_DVT][tc_bif_116_step_3] BIF_CON31[13]=0x%x, need 1\n", reg_val);
//    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x1, 14);
//    printk("[PMIC_DVT][tc_bif_116_step_3] BIF_CON31[14]=0x%x, need 0\n", reg_val);

    //reset BIF_IRQ
    reset_bif_irq();
}

void tc_bif_116_step_4(void)
{
   int bif_cmd[4]={0,0,0,0};
	int bif_r_data[7]={0x00,0x01,0x02,0x03,0x04,0x05,0x06};


    //1. set command sequence
    printk("[PMIC_DVT][tc_bif_116_step_4] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x0427;
    bif_cmd[2]=0x010F;
    bif_cmd[3]=0x0309;
    set_bif_cmd(bif_cmd,4);


	tc_bif_step_common(116, 4, 0x4, 0x1, 0x7, bif_r_data);

#if 0
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[4]={0,0,0,0};
    int loop_i=0;

	printk("[PMIC_DVT][tc_bif_116_step_4]-----------------------------------\n");

    //1. set command sequence
    printk("[PMIC_DVT][tc_bif_116_step_4] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x0427;
    bif_cmd[2]=0x010F;
    bif_cmd[3]=0x0309;
    set_bif_cmd(bif_cmd,4);

    //2. parameter setting
    printk("[PMIC_DVT][tc_bif_116_step_4] 2. parameter setting\n");
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x4, 0xF, 12);
    //ret=pmic_config_interface(MT6325_BIF_CON15, 0x1, 0x3, 8);
    //ret=pmic_config_interface(MT6325_BIF_CON17, 0x7, 0xF, 12);
    ret = pmic_set_register_value(PMIC_BIF_TRASFER_NUM, 4);
    ret = pmic_set_register_value(PMIC_BIF_COMMAND_TYPE, 1);
    ret = pmic_set_register_value(PMIC_BIF_READ_EXPECT_NUM, 7);
    //dump
    printk("[PMIC_DVT]Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_BIF_TRASFER_NUM].offset,pmic_get_register_value(PMIC_BIF_TRASFER_NUM),
        pmu_flags_table[PMIC_BIF_COMMAND_TYPE].offset,pmic_get_register_value(PMIC_BIF_COMMAND_TYPE),
        pmu_flags_table[PMIC_BIF_READ_EXPECT_NUM].offset,pmic_get_register_value(PMIC_BIF_READ_EXPECT_NUM));


    //3. trigger BIF module
    printk("[PMIC_DVT][tc_bif_116_step_4] 3. trigger BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    udelay(10);
    bif_debug();

	printk("[PMIC_DVT][tc_bif_116_step_4] polling BIF_IRQ : BIF_CON31[11]=");
	bif_polling_irq();

    printk("[PMIC_DVT][tc_bif_116_step_4] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

    //disable BIF module
    printk("[PMIC_DVT][tc_bif_116_step_4] disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);

    //data valid check
    printk("[PMIC_DVT][tc_bif_116_step_4] data valid check\n");
    ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x7, 12);
    printk("[PMIC_DVT][tc_bif_116_step_4] BIF_CON31[14:12]=0x%x\n", reg_val);

    if(reg_val == 0)
    {
        //6. read the number of read back data package
        printk("[PMIC_DVT][tc_bif_116_step_4] 6. read the number of read back data package\n");
        ret=pmic_read_interface(MT6325_BIF_CON19, &reg_val, 0xF, 0);
        printk("[PMIC_DVT][tc_bif_116_step_4] BIF_DATA_NUM (7) : BIF_CON19[3:0]=0x%x\n", reg_val);

        //7. read data back
        printk("[PMIC_DVT][tc_bif_116_step_4] 7. read data back\n");
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_116_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_116_step_4] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON20, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_116_step_4] BIF_DATA_0 (00h) : BIF_CON20[7:0]=0x%x\n", reg_val);

        ret=pmic_read_interface(MT6325_BIF_CON21, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_116_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON21[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON21, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_116_step_4] BIF_ACK_0 (1) : BIF_CON21[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON21, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_116_step_4] BIF_DATA_0 (01h) : BIF_CON21[7:0]=0x%x\n", reg_val);

        ret=pmic_read_interface(MT6325_BIF_CON22, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_116_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON22[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON22, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_116_step_4] BIF_ACK_0 (1) : BIF_CON22[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON22, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_116_step_4] BIF_DATA_0 (02h) : BIF_CON22[7:0]=0x%x\n", reg_val);

        ret=pmic_read_interface(MT6325_BIF_CON23, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_116_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON23[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON23, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_116_step_4] BIF_ACK_0 (1) : BIF_CON23[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON23, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_116_step_4] BIF_DATA_0 (03h) : BIF_CON23[7:0]=0x%x\n", reg_val);

        ret=pmic_read_interface(MT6325_BIF_CON24, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_116_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON24[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON24, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_116_step_4] BIF_ACK_0 (1) : BIF_CON24[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON24, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_116_step_4] BIF_DATA_0 (04h) : BIF_CON24[7:0]=0x%x\n", reg_val);

        ret=pmic_read_interface(MT6325_BIF_CON25, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_116_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON25[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON25, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_116_step_4] BIF_ACK_0 (1) : BIF_CON25[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON25, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_116_step_4] BIF_DATA_0 (05h) : BIF_CON25[7:0]=0x%x\n", reg_val);

        ret=pmic_read_interface(MT6325_BIF_CON26, &reg_val, 0x1, 15);
        printk("[PMIC_DVT][tc_bif_116_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON26[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON26, &reg_val, 0x1, 8);
        printk("[PMIC_DVT][tc_bif_116_step_4] BIF_ACK_0 (1) : BIF_CON26[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6325_BIF_CON26, &reg_val, 0xFF, 0);
        printk("[PMIC_DVT][tc_bif_116_step_4] BIF_DATA_0 (06h) : BIF_CON26[7:0]=0x%x\n", reg_val);
    }
    else
    {
        printk("[PMIC_DVT][tc_bif_116_step_4] Fail : BIF_CON31[14:12]=0x%x\n", reg_val);
    }

    //reset BIF_IRQ
    reset_bif_irq();
#endif
}
#endif

#if 0
void tc_bif_1008_step_0(void)
{
    U32 ret=0;


    printk("[PMIC_DVT][tc_bif_1008_step_0]-----------------------------------\n");

    printk("[PMIC_DVT][tc_bif_1008_step_0] 1. set power up regiser\n");
    ret=pmic_config_interface(MT6325_BIF_CON32, 0x1, 0x1, 15);

    printk("[PMIC_DVT][tc_bif_1008_step_0] 2. trigger BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x1, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 1);

    //dump
    printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        MT6325_BIF_CON32,upmu_get_reg_value(MT6325_BIF_CON32),
        MT6325_BIF_CON18,upmu_get_reg_value(MT6325_BIF_CON18)
        );

    udelay(10);
    bif_debug();

    #if 0
    reg_val=0;
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6325_BIF_CON31, &reg_val, 0x1, 11);
        printk("[PMIC_DVT][tc_bif_1008_step_0] 3. polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);
    }
    #else
    printk("[PMIC_DVT][tc_bif_1008_step_0] wait EINT\n");
    #endif
}

void tc_bif_1008_step_1(void)
{
    U32 ret=0;


    printk("[PMIC_DVT][tc_bif_1008_step_1]-----------------------------------\n");

    printk("[PMIC_DVT][tc_bif_1008_step_1] 4. disable BIF module\n");
    //ret=pmic_config_interface(MT6325_BIF_CON18, 0x0, 0x1, 0);
    ret=pmic_set_register_value(PMIC_BIF_TRASACT_TRIGGER, 0);

    printk("[PMIC_DVT][tc_bif_1008_step_1] 5. to disable power up mode\n");
    ret=pmic_config_interface(MT6325_BIF_CON32, 0x0, 0x1, 15);

    //dump
    printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        MT6325_BIF_CON32,upmu_get_reg_value(MT6325_BIF_CON32),
        MT6325_BIF_CON18,upmu_get_reg_value(MT6325_BIF_CON18)
        );

    check_bat_lost();

    reset_bif_irq();
}

#endif

void bif_init(void)
{
    //On PMU EVB need :
    //cd /sys/devices/platform/mt-pmic/
    //echo 001E 0000 > pmic_access
    //echo 000E FFFF > pmic_access
    //echo 8C20 0000 > pmic_access
    //echo 8C12 FFFF > pmic_access

    //(1)	BIF top interrupt enable
	pmic_set_register_value(PMIC_INT_CON0_CLR, 0xFFFF);
	pmic_set_register_value(PMIC_INT_CON0_CLR, 0xFFFF);
	pmic_set_register_value(PMIC_INT_CON0_CLR, 0xFFFF);

    //enable BIF interrupt
	pmic_set_register_value(PMIC_INT_CON0_SET, 0x4000);

    //enable BIF clock
    pmic_set_register_value(PMIC_TOP_CKPDN_CON2_CLR, 0x0070);

    //enable HT protection
    pmic_set_register_value(PMIC_RG_BATON_HT_EN, 1);

    //turn on LDO
//    mt6325_upmu_set_rg_vbif28_en(1);
	pmic_set_register_value(PMIC_RG_VBIF28_ON_CTRL,0);
	pmic_set_register_value(PMIC_RG_VBIF28_EN,1);
	mdelay(50);

    //20140523, Waverly
//    pmic_config_interface(0x0ABE,0x8017,0xFFFF,0);
	pmic_set_register_value(PMIC_BIF_RX_DEG_EN,0x8000);
	pmic_set_register_value(PMIC_BIF_RX_DEG_WND,0x17);

    //20140528, Waverly
//argus    pmic_config_interface(0x0F00,0x0005,0xFFFF,0);
	pmic_set_register_value(PMIC_RG_BATON_EN,0x1);
	pmic_set_register_value(PMIC_BATON_TDET_EN,0x1);
//argus    pmic_config_interface(0x0ABA,0x500F,0xFFFF,0);
	pmic_set_register_value(PMIC_RG_BATON_HT_EN_DLY_TIME,0x1);

    printk("[PMIC_DVT][bif_init] done. [0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,\n",
        pmu_flags_table[PMIC_RG_BATON_HT_EN].offset, pmic_get_register_value(PMIC_RG_BATON_HT_EN),
        pmu_flags_table[PMIC_RG_VBIF28_ON_CTRL].offset, pmic_get_register_value(PMIC_RG_VBIF28_ON_CTRL),
        pmu_flags_table[PMIC_RG_VBIF28_EN].offset, pmic_get_register_value(PMIC_RG_VBIF28_EN)
        );
}

void tc_bif_100(void)
{
    printk("[PMIC_DVT][tc_bif_100] run read/write function\n");

    bif_init();

    tc_bif_100_step_0();
    mdelay(10);
    tc_bif_100_step_1();
    tc_bif_100_step_2();
}

void tc_bif_111(void)
{
    printk("[PMIC_DVT][tc_bif_111] run read/write function\n");

    tc_bif_reset_slave();
    mdelay(1);

    tc_bif_111_step_1();//read 4 data
    mdelay(500);
    tc_bif_111_step_1();//read 4 data
    mdelay(500);
    tc_bif_111_step_1();//read 4 data
    mdelay(500);
    tc_bif_111_step_1();//read 4 data
    mdelay(500);
    tc_bif_111_step_1();//read 4 data
    mdelay(500);

    tc_bif_111_step_2();//read 2 data
    mdelay(500);
    tc_bif_111_step_2();//read 2 data
    mdelay(500);
    tc_bif_111_step_2();//read 2 data
    mdelay(500);
    tc_bif_111_step_2();//read 2 data
    mdelay(500);
    tc_bif_111_step_2();//read 2 data
    mdelay(500);

    tc_bif_111_step_3();//write
    tc_bif_111_step_4();//check write is ok or not
    tc_bif_111_step_5();//change back to the original device address with TQ
}

void tc_bif_112(void)
{
    printk("[PMIC_DVT][tc_bif_112] run burst write/read function\n");

    tc_bif_reset_slave();
    mdelay(1);

    tc_bif_112_step_1(); //burst read 4 data
    mdelay(500);
    tc_bif_112_step_1(); //burst read 4 data
    mdelay(500);
    tc_bif_112_step_1(); //burst read 4 data
    mdelay(500);
    tc_bif_112_step_1(); //burst read 4 data
    mdelay(500);
    tc_bif_112_step_1(); //burst read 4 data
    mdelay(500);

    tc_bif_112_step_2(); //burst read 2 data
    mdelay(500);
    tc_bif_112_step_2(); //burst read 2 data
    mdelay(500);
    tc_bif_112_step_2(); //burst read 2 data
    mdelay(500);
    tc_bif_112_step_2(); //burst read 2 data
    mdelay(500);
    tc_bif_112_step_2(); //burst read 2 data
    mdelay(500);

    tc_bif_112_step_3(); //burst write 8 date

    tc_bif_112_step_4(); //burst read 8 date
    mdelay(500);
    tc_bif_112_step_4(); //burst read 8 date
    mdelay(500);
    tc_bif_112_step_4(); //burst read 8 date
    mdelay(500);
    tc_bif_112_step_4(); //burst read 8 date
    mdelay(500);
    tc_bif_112_step_4(); //burst read 8 date
    mdelay(500);
}

void tc_bif_113(void)
{
    printk("[PMIC_DVT][tc_bif_113] run bus command (ISTS)\n");

	pmic_set_register_value(PMIC_RG_BIF_X4_CK_DIVSEL, 0x4);
//    pmic_config_interface(MT6325_TOP_CKSEL_CON0, 0x4, MT6325_PMIC_RG_BIF_X4_CK_DIVSEL_MASK, MT6325_PMIC_RG_BIF_X4_CK_DIVSEL_SHIFT);
    printk("[PMIC_DVT][PMIC_RG_BIF_X4_CK_DIVSEL] set done. Reg[0x%x]=0x%x\n",
        pmu_flags_table[PMIC_RG_BIF_X4_CK_DIVSEL].offset, pmic_get_register_value(PMIC_RG_BIF_X4_CK_DIVSEL)
        );

    pmic_config_interface(MT6351_BIF_CON15,0x7F,0x7F, 0);
    printk("[PMIC_DVT][RG_BIF_X4_CK_DIVSEL] Reg[0x%x]=0x%x\n",
            MT6351_BIF_CON15, upmu_get_reg_value(MT6351_BIF_CON15)
            );

    tc_bif_reset_slave();
    mdelay(1);
    mdelay(500);

    tc_bif_113_step_2(); //burst read 4 data
    mdelay(500);
    tc_bif_113_step_2(); //burst read 4 data
    mdelay(500);
    tc_bif_113_step_2(); //burst read 4 data
    mdelay(500);
    tc_bif_113_step_2(); //burst read 4 data
    mdelay(500);
    tc_bif_113_step_2(); //burst read 4 data
    mdelay(500);

    tc_bif_113_step_3(); //burst read 2 data
    mdelay(500);
    tc_bif_113_step_3(); //burst read 2 data
    mdelay(500);
    tc_bif_113_step_3(); //burst read 2 data
    mdelay(500);
    tc_bif_113_step_3(); //burst read 2 data
    mdelay(500);
    tc_bif_113_step_3(); //burst read 2 data
    mdelay(500);

    tc_bif_113_step_4(); //write enable irq

    //tc_bif_113_step_5(); //write bus command ISTS
    mdelay(2000);
    tc_bif_113_step_5_negative(); //write bus command ISTS : negative response
    mdelay(1000);
    tc_bif_113_step_5_positive(); //write bus command ISTS : positive response
}

void tc_bif_114(void)
{
    printk("[PMIC_DVT][tc_bif_114] run interrupt mode (EINT)\n");

    tc_bif_114_step_1_positive();
    tc_bif_114_step_1_negative();
}

void tc_bif_115(void)
{
    printk("[PMIC_DVT][tc_bif_115] battery lost\n");

    tc_bif_115_step1();
}

void tc_bif_116(void)
{
    printk("[PMIC_DVT][tc_bif_116] battery lost test : E1 option\n");

    tc_bif_reset_slave();
    mdelay(1);
    tc_bif_116_step_1();
    tc_bif_116_step_2();
    tc_bif_116_step_3();
    tc_bif_116_step_4();
}

void tc_bif_117(void)
{
#if 0
    printk("[PMIC_DVT][tc_bif_1007] BIF EINT test\n");

    bif_init();

    tc_bif_100_step_0();
#endif
}

void tc_bif_118(void)
{
#if 0
    printk("[PMIC_DVT][tc_bif_1008] BIF EINT test\n");

    bif_init();

    tc_bif_1008_step_0();
#endif
}

#endif

extern void pmic_regulator_test(void);

extern const PMU_FLAG_TABLE_ENTRY pmu_flags_table[];

void pmic_getid(void)
{
	printk("[PMIC_DVT] PMIC SWCID=0x%x  \n", pmic_get_register_value(PMIC_SWCID));
	printk("[PMIC_DVT] PMIC HWCID=0x%x  \n", pmic_get_register_value(PMIC_HWCID));

}

extern void dump_ldo_status_read_debug(void);


extern void mt6351_dump_register(void);

PMU_FLAGS_LIST_ENUM pmuflag;

void pmic_dvt_entry(int test_id)
{
    printk("[PMIC_DVT] [pmic_dvt_entry] start : test_id=%d\n", test_id);


/*
	CH0: BATSNS
	CH1: ISENSE
	CH2: VCDT
	CH3: BAT TEMP
	CH4: PMIC TEMP
	CH5:
	CH6:
	CH7: TSX
*/


    switch (test_id)
    {
		//ADC
		case 50:PMIC_IMM_GetOneChannelValue(0,0,0);break;
		case 51:PMIC_IMM_GetOneChannelValue(1,0,0);break;
		case 52:PMIC_IMM_GetOneChannelValue(2,0,0);break;
		case 53:PMIC_IMM_GetOneChannelValue(3,0,0);break;
		case 54:PMIC_IMM_GetOneChannelValue(4,0,0);break;
		case 55:PMIC_IMM_GetOneChannelValue(5,0,0);break;
		case 56:PMIC_IMM_GetOneChannelValue(7,0,0);break;



        case 61: pmic_auxadc2_trim111(0); break;
        case 62: pmic_auxadc2_trim111(1); break;
        case 63: pmic_auxadc2_trim111(2); break;
        case 64: pmic_auxadc2_trim111(3); break;
        case 65: pmic_auxadc2_trim111(7); break;

        case 0: pmic_auxadc1_basic111(); break;

        case 2: pmic_auxadc3_prio111(); break;
        case 3: pmic_auxadc3_prio112(); break;
        case 4: pmic_auxadc3_prio113(); break;
        case 5: pmic_auxadc3_prio114(); break;
        case 6: pmic_auxadc3_prio115(); break;


        case 7: pmic_lbat111(); break;
        case 8: pmic_lbat112(); break;
        case 9: pmic_lbat113(); break;
        case 10: pmic_lbat114(); break;

        case 11: pmic_lbat2111(); break;
        case 12: pmic_lbat2112(); break;
        case 13: pmic_lbat2113(); break;
        case 14: pmic_lbat2114(); break;

        case 15: pmic_thr111(); break;
        case 16: pmic_thr112(); break;
        case 17: pmic_thr113(); break;
        case 18: pmic_thr114(); break;

        case 19: pmic_acc111(); break;

        case 20: pmic_IMP111(); break;
        case 21: pmic_IMP112(); break;

        case 22: pmic_cck11(); break;
        case 23: pmic_cck12(); break;
        case 24: pmic_cck13(); break;
        case 25: pmic_cck14(); break;
        case 26: pmic_cck15(); break;
		case 27: pmic_cck16(); break;

		//efuse
		case 100: pmic_efuse11(); break;
		case 101: pmic_efuse21(); break;

        // BIF
        case 110: tc_bif_100(); break;
        case 111: tc_bif_111(); break;
        case 112: tc_bif_112(); break;
        case 113: tc_bif_113(); break;
        case 114: tc_bif_114(); break;
        case 115: tc_bif_115(); break;
        case 116: tc_bif_116(); break;
        //case 117: tc_bif_117(); break; //EINT
        //case 118: tc_bif_118(); break; //EINT

		//ldo
		/* TC 1.1~1.30 LDO HW/SW on control test */
		case 200: pmic_ldo1_sw_on_control(); break;
		case 201: pmic_ldo1_hw_on_control(); break;
		/* TC 4.1~1.30 normal/lp mode control test */
		case 202: pmic_ldo4_lp_test(); break;
		//case 203: pmic_ldo5_fast_tran(); break;

		case 204: pmic_vtcxo_1();break;
		case 205: pmic_ldo_vio18();break;


		case 211: pmic_ldo2_vosel_test(MT6351_POWER_LDO_VCAMA); break;
		case 212: pmic_ldo2_vosel_test(MT6351_POWER_LDO_VCN33_BT); break;
		case 213: pmic_ldo2_vosel_test(MT6351_POWER_LDO_VEFUSE); break;
		case 214: pmic_ldo2_vosel_test(MT6351_POWER_LDO_VSIM1); break;
		case 215: pmic_ldo2_vosel_test(MT6351_POWER_LDO_VSIM2); break;
		case 216: pmic_ldo2_vosel_test(MT6351_POWER_LDO_VEMC); break;
		case 217: pmic_ldo2_vosel_test(MT6351_POWER_LDO_VMCH); break;
		case 218: pmic_ldo2_vosel_test(MT6351_POWER_LDO_VMC); break;
		case 220: pmic_ldo2_vosel_test(MT6351_POWER_LDO_VGP3); break;
		case 221: pmic_ldo2_vosel_test(MT6351_POWER_LDO_VIBR); break;
		case 222: pmic_ldo2_vosel_test(MT6351_POWER_LDO_VCAMD); break;
		case 223: pmic_ldo2_vosel_test(MT6351_POWER_LDO_VRF18); break;
		case 224: pmic_ldo2_vosel_test(MT6351_POWER_LDO_VCAMIO); break;
		case 225: pmic_ldo2_vosel_test(MT6351_POWER_LDO_VSRAM_PROC); break;


		case 251: pmic_ldo3_cal_test(MT6351_POWER_LDO_VA18); break;
		case 252: pmic_ldo3_cal_test(MT6351_POWER_LDO_VTCXO24); break;
		case 253: pmic_ldo3_cal_test(MT6351_POWER_LDO_VTCXO28); break;
		case 254: pmic_ldo3_cal_test(MT6351_POWER_LDO_VCN28); break;
		case 255: pmic_ldo3_cal_test(MT6351_POWER_LDO_VCN28); break;
		case 256: pmic_ldo3_cal_test(MT6351_POWER_LDO_VCAMA); break;
		case 257: pmic_ldo3_cal_test(MT6351_POWER_LDO_VCN33_BT); break;
		case 258: pmic_ldo3_cal_test(MT6351_POWER_LDO_VUSB33); break;
		case 259: pmic_ldo3_cal_test(MT6351_POWER_LDO_VEFUSE); break;
		case 260: pmic_ldo3_cal_test(MT6351_POWER_LDO_VSIM1); break;
		case 261: pmic_ldo3_cal_test(MT6351_POWER_LDO_VSIM2); break;
		case 262: pmic_ldo3_cal_test(MT6351_POWER_LDO_VEMC); break;
		case 263: pmic_ldo3_cal_test(MT6351_POWER_LDO_VMCH); break;
		case 264: pmic_ldo3_cal_test(MT6351_POWER_LDO_VMC); break;
		case 265: pmic_ldo3_cal_test(MT6351_POWER_LDO_VCAMA); break;
		case 266: pmic_ldo3_cal_test(MT6351_POWER_LDO_VIO28); break;
		case 267: pmic_ldo3_cal_test(MT6351_POWER_LDO_VGP3); break;
		case 268: pmic_ldo3_cal_test(MT6351_POWER_LDO_VIBR); break;
		case 269: pmic_ldo3_cal_test(MT6351_POWER_LDO_VCAMD); break;
		case 270: pmic_ldo3_cal_test(MT6351_POWER_LDO_VRF18); break;
		case 271: pmic_ldo3_cal_test(MT6351_POWER_LDO_VRF12); break;
		case 272: pmic_ldo3_cal_test(MT6351_POWER_LDO_VIO18); break;
		case 273: pmic_ldo3_cal_test(MT6351_POWER_LDO_VCN18); break;
		case 274: pmic_ldo3_cal_test(MT6351_POWER_LDO_VCAMIO); break;
		case 280: pmic_ldo1_dummy_load_control(); break;


		//VPA argus
		//case 300: pmic_vpa11(); break;
		//case 301: pmic_vpa12(); break;

		//BUCK
		/* TC 1.1 Check on/off HW & SW mode control */
		case 400: pmic_buck11(); break;
		/* TC 1.5 Check SW & HW mode of voltage control between sleep mode and normal mode
		TC 1.7 Conversion of Binary to gray code for VOSEL
		voltage limitation */
		case 401: pmic_buck15plus17(); break;
		/* TC 1.8 voltage limitation, enable full code */
		case 402: pmic_buck18(); break;
		/* DVT general TC VPA:0.1 Check QI_VPA_VOSEL and VPA_DLC mapping */
		case 403: pmic_buck_vpa01(); break;
		/* DVT general TC VCORE:0.1 voice wake up BUCK_VCORE_VOSEL_AUD */
		case 404: pmic_buck_vcore01(); break;
		/* DVT general TC VSRAM_MD:0.1 voice wake up BUCK_VSRAM_MD_VOSEL_AUD */
		case 405: pmic_buck_vsram_md01(); break;
		//case 405: pmic_buck211(); break;


		//reg
		case 410: pmic_reg();break;


		//regulator
//argus		case 500: pmic_regulator_test(); break;
		case 501: dump_ldo_status_read_debug(); break;


		case 600: pmic_getid(); break;
		case 601: mt6351_dump_register(); break;
		case 602:
		printk("[PMIC_DVT] flag=%d val=0x%x\n",pmuflag,pmic_get_register_value(pmuflag));
		break;

        default:
            printk("[PMIC_DVT] [pmic_dvt_entry] test_id=%d\n", test_id);
            break;
    }

    printk("[PMIC_DVT] [pmic_dvt_entry] end\n\n");
}

