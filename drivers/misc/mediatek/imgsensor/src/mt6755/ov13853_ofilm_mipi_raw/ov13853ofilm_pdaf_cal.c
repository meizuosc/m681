#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/types.h>


#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"


#include "ov13853ofilmmipiraw_Sensor.h"
/****************************Modify Following Strings for Debug****************************/
#define PFX "OV13853_OFilm_camera_pdaf"
/****************************   Modify end    *******************************************/

#define LOG_INF(format, args...)	pr_debug(PFX "[%s] " format, __FUNCTION__, ##args)

/*
struct otp_pdaf_struct {
unsigned char pdaf_flag; //0x075E: bit[7]--0:empty; 0x55:Valid
unsigned char data[1372]; //0x075F~0x0CBA
unsigned char pdaf_checksum;//checksum of pd, SUM(0x075E~0x0CBA)%255+1
};
*/
//kernel-3.18/drivers\misc/mediatek/cam_cal/src/legacy/mt6755/gt24c64a_eeprom/gt24c64a.c
extern bool selective_read_byte_ofilm(u32 addr, BYTE* data,u16 i2c_id);
extern int selective_read_region_ofilm(u32 addr, BYTE* data,u16 i2c_id,u32 size);


#define GT24C64A_DEVICE_ID 0xA0

#define PDAF_DATA_SIZE 1372	 //496+876
#define PDAF_ADDR 0x75E	     //first byte: flag
#define PDAF_OFFSET 1	       //pdaf data offset
#define PDAF_FLAG 0x55	     //valid flag
#define PDAF_CRC_ADDR 0xCBB  //checksum addr


static unsigned char pddata[PDAF_DATA_SIZE];
static int first_read_pdflag = 1;


bool read_ofilm_otp_pdaf_data(kal_uint16 addr, BYTE* data, kal_uint32 size)
{
	kal_uint8 pdaf_flag = 0, read_sum = 0, check_sum = 0;
	kal_uint32 get_size = 0, sum = 0;
	int i;

	LOG_INF("read_ofilm_otp_pdaf_data enter! addr: 0x%x, data: %p, size: %d\n",addr,data,size);

	if(1==first_read_pdflag){

		first_read_pdflag=0;
		memset(pddata,0x0,sizeof(pddata));

		selective_read_byte_ofilm(PDAF_ADDR, &pdaf_flag, GT24C64A_DEVICE_ID);
		if(PDAF_FLAG != pdaf_flag)
		{
			LOG_INF("read_ofilm_otp_pdaf_data fail! pdaf flag: %d\n",pdaf_flag);
			first_read_pdflag = 1;
			return false;
		}

		get_size = selective_read_region_ofilm((PDAF_ADDR+PDAF_OFFSET), data, GT24C64A_DEVICE_ID, PDAF_DATA_SIZE);
		if(PDAF_DATA_SIZE != get_size)
		{
			LOG_INF("read_ofilm_otp_pdaf_data fail! read size: %d\n",get_size);
			first_read_pdflag = 1;
			return false;
		}

		selective_read_byte_ofilm(PDAF_CRC_ADDR, &read_sum, GT24C64A_DEVICE_ID);
		for(i = 0; i < PDAF_DATA_SIZE; i++) {
			sum += data[i];
			pddata[i]=data[i];
		}
		sum += pdaf_flag;  //add for OFilm OTP spec!!!

		check_sum = sum % 255 + 1;
		if(read_sum != check_sum)
		{
			LOG_INF("read_ofilm_otp_pdaf_data fail! read_sum: %d, check_sum: %d\n",read_sum,check_sum);
			first_read_pdflag = 1;
			return false;
		}
		else
		{
			LOG_INF("read_ofilm_otp_pdaf_data ok! data check ok.\n");
		}
	}else{
		for(i = 0; i < PDAF_DATA_SIZE; i++) {
			data[i]=pddata[i];
		}
	}

	return true;
}

