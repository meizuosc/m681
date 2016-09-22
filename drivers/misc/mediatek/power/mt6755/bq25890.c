#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>

#include <cust_acc.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <linux/hwmsen_helper.h>
#include <linux/xlog.h>


#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

#include "bq25890.h"
#include "cust_charging.h"
#include <mach/charging.h>

#if defined(CONFIG_MTK_FPGA)
#else
#ifdef CONFIG_OF
#else
#include <cust_i2c.h>
#endif
#endif

/**********************************************************
  *
  *   [I2C Slave Setting]
  *
  *********************************************************/

#ifdef CONFIG_OF
#else

#define bq25890_SLAVE_ADDR_WRITE   0xD6
#define bq25890_SLAVE_ADDR_Read    0xD7

#ifdef I2C_SWITHING_CHARGER_CHANNEL
#define bq25890_BUSNUM I2C_SWITHING_CHARGER_CHANNEL
#else
#define bq25890_BUSNUM 0
#endif

#endif
static struct i2c_client *new_client;
static const struct i2c_device_id bq25890_i2c_id[] = { {"bq25890", 0}, {} };

kal_bool chargin_hw_init_done = KAL_FALSE;
static int bq25890_driver_probe(struct i2c_client *client, const struct i2c_device_id *id);


/**********************************************************
  *
  *   [Global Variable]
  *
  *********************************************************/
kal_uint8 bq25890_reg[bq25890_REG_NUM] = { 0 };

static DEFINE_MUTEX(bq25890_i2c_access);

int g_bq25890_hw_exist = 0;

/**********************************************************
  *
  *   [I2C Function For Read/Write bq25890]
  *
  *********************************************************/
kal_uint32 bq25890_read_byte(kal_uint8 cmd, kal_uint8 *returnData)
{
	char cmd_buf[1] = { 0x00 };
	char readData = 0;
	int ret = 0;

	mutex_lock(&bq25890_i2c_access);

	/* new_client->addr = ((new_client->addr) & I2C_MASK_FLAG) | I2C_WR_FLAG; */
	new_client->ext_flag =
	    ((new_client->ext_flag) & I2C_MASK_FLAG) | I2C_WR_FLAG | I2C_DIRECTION_FLAG;

	cmd_buf[0] = cmd;
	ret = i2c_master_send(new_client, &cmd_buf[0], (1 << 8 | 1));
	if (ret < 0) {
		/* new_client->addr = new_client->addr & I2C_MASK_FLAG; */
		new_client->ext_flag = 0;
		mutex_unlock(&bq25890_i2c_access);

		return 0;
	}

	readData = cmd_buf[0];
	*returnData = readData;

	/* new_client->addr = new_client->addr & I2C_MASK_FLAG; */
	new_client->ext_flag = 0;
	mutex_unlock(&bq25890_i2c_access);

	return 1;
}

kal_uint32 bq25890_write_byte(kal_uint8 cmd, kal_uint8 writeData)
{
	char write_data[2] = { 0 };
	int ret = 0;

	mutex_lock(&bq25890_i2c_access);

	write_data[0] = cmd;
	write_data[1] = writeData;

	new_client->ext_flag = ((new_client->ext_flag) & I2C_MASK_FLAG) | I2C_DIRECTION_FLAG;

	ret = i2c_master_send(new_client, write_data, 2);
	if (ret < 0) {
		new_client->ext_flag = 0;
		mutex_unlock(&bq25890_i2c_access);
		return 0;
	}

	new_client->ext_flag = 0;
	mutex_unlock(&bq25890_i2c_access);
	return 1;
}

/**********************************************************
  *
  *   [Read / Write Function]
  *
  *********************************************************/
kal_uint32 bq25890_read_interface(kal_uint8 RegNum, kal_uint8 *val, kal_uint8 MASK,
				  kal_uint8 SHIFT)
{
	kal_uint8 bq25890_reg = 0;
	kal_uint32 ret = 0;

	ret = bq25890_read_byte(RegNum, &bq25890_reg);

	battery_log(BAT_LOG_FULL, "[bq25890_read_interface] Reg[%x]=0x%x\n", RegNum, bq25890_reg);

	bq25890_reg &= (MASK << SHIFT);
	*val = (bq25890_reg >> SHIFT);

	battery_log(BAT_LOG_FULL, "[bq25890_read_interface] val=0x%x\n", *val);

	return ret;
}

kal_uint32 bq25890_config_interface(kal_uint8 RegNum, kal_uint8 val, kal_uint8 MASK,
				    kal_uint8 SHIFT)
{
	kal_uint8 bq25890_reg = 0;
	kal_uint32 ret = 0;

	ret = bq25890_read_byte(RegNum, &bq25890_reg);
	battery_log(BAT_LOG_FULL, "[bq25890_config_interface] Reg[%x]=0x%x\n", RegNum, bq25890_reg);

	bq25890_reg &= ~(MASK << SHIFT);
	bq25890_reg |= (val << SHIFT);

	ret = bq25890_write_byte(RegNum, bq25890_reg);
	battery_log(BAT_LOG_FULL, "[bq25890_config_interface] write Reg[%x]=0x%x\n", RegNum,
		    bq25890_reg);

	/* Check */
	/* bq25890_read_byte(RegNum, &bq25890_reg); */
	/* printk("[bq25890_config_interface] Check Reg[%x]=0x%x\n", RegNum, bq25890_reg); */

	return ret;
}

/* write one register directly */
kal_uint32 bq25890_reg_config_interface(kal_uint8 RegNum, kal_uint8 val)
{
	kal_uint32 ret = 0;

	ret = bq25890_write_byte(RegNum, val);

	return ret;
}

/**********************************************************
  *
  *   [Internal Function]
  *
  *********************************************************/
/* CON0---------------------------------------------------- */

void bq25890_set_en_hiz(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq25890_config_interface((kal_uint8) (bq25890_CON0),
				       (kal_uint8) (val),
				       (kal_uint8) (CON0_EN_HIZ_MASK),
				       (kal_uint8) (CON0_EN_HIZ_SHIFT)
	    );
}

void bq25890_set_en_ilim(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq25890_config_interface((kal_uint8) (bq25890_CON0),
				       (kal_uint8) (val),
				       (kal_uint8) (CON0_EN_ILIM_MASK),
				       (kal_uint8) (CON0_EN_ILIM_SHIFT)
	    );
}

void bq25890_set_iinlim(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq25890_config_interface((kal_uint8) (bq25890_CON0),
				       (val),
				       (kal_uint8) (CON0_IINLIM_MASK),
				       (kal_uint8) (CON0_IINLIM_SHIFT)
	    );
}

kal_uint32 bq25890_get_iinlim(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq25890_read_interface((kal_uint8) (bq25890_CON0),
				     (&val),
				     (kal_uint8) (CON0_IINLIM_MASK), (kal_uint8) (CON0_IINLIM_SHIFT)
	    );
	return val;
}



/* CON1---------------------------------------------------- */

void bq25890_ADC_start(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq25890_config_interface((kal_uint8) (bq25890_CON2),
				       (kal_uint8) (val),
				       (kal_uint8) (CON2_CONV_START_MASK),
				       (kal_uint8) (CON2_CONV_START_SHIFT)
	    );
}

void bq25890_set_ADC_rate(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq25890_config_interface((kal_uint8) (bq25890_CON2),
				       (kal_uint8) (val),
				       (kal_uint8) (CON2_CONV_RATE_MASK),
				       (kal_uint8) (CON2_CONV_RATE_SHIFT)
	    );
}

void bq25890_set_vindpm_os(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq25890_config_interface((kal_uint8) (bq25890_CON1),
				       (kal_uint8) (val),
				       (kal_uint8) (CON1_VINDPM_OS_MASK),
				       (kal_uint8) (CON1_VINDPM_OS_SHIFT)
	    );
}

/* CON2---------------------------------------------------- */

void bq25890_set_ico_en_start(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq25890_config_interface((kal_uint8) (bq25890_CON2),
				       (kal_uint8) (val),
				       (kal_uint8) (CON2_ICO_EN_MASK),
				       (kal_uint8) (CON2_ICO_EN_RATE_SHIFT)
	    );
}



/* CON3---------------------------------------------------- */

void bq25890_wd_reset(kal_uint32 val)
{
	kal_uint32 ret = 0;


	ret = bq25890_config_interface((kal_uint8) (bq25890_CON3),
				       (val),
				       (kal_uint8) (CON3_WD_MASK), (kal_uint8) (CON3_WD_SHIFT)
	    );

}

void bq25890_otg_en(kal_uint32 val)
{
	kal_uint32 ret = 0;


	ret = bq25890_config_interface((kal_uint8) (bq25890_CON3),
				       (val),
				       (kal_uint8) (CON3_OTG_CONFIG_MASK),
				       (kal_uint8) (CON3_OTG_CONFIG_SHIFT)
	    );

}

void bq25890_chg_en(kal_uint32 val)
{
	kal_uint32 ret = 0;


	ret = bq25890_config_interface((kal_uint8) (bq25890_CON3),
				       (val),
				       (kal_uint8) (CON3_CHG_CONFIG_MASK),
				       (kal_uint8) (CON3_CHG_CONFIG_SHIFT)
	    );

}

kal_uint32 bq25890_get_chg_en(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq25890_read_interface((kal_uint8) (bq25890_CON3),
				     (&val),
				     (kal_uint8) (CON3_CHG_CONFIG_MASK),
				     (kal_uint8) (CON3_CHG_CONFIG_SHIFT)
	    );
	return val;
}


void bq25890_set_sys_min(kal_uint32 val)
{
	kal_uint32 ret = 0;


	ret = bq25890_config_interface((kal_uint8) (bq25890_CON3),
				       (val),
				       (kal_uint8) (CON3_SYS_V_LIMIT_MASK),
				       (kal_uint8) (CON3_SYS_V_LIMIT_SHIFT)
	    );

}

/* CON4---------------------------------------------------- */

void bq25890_en_pumpx(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq25890_config_interface((kal_uint8) (bq25890_CON4),
				       (kal_uint8) (val),
				       (kal_uint8) (CON4_EN_PUMPX_MASK),
				       (kal_uint8) (CON4_EN_PUMPX_SHIFT)
	    );
}


void bq25890_set_ichg(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq25890_config_interface((kal_uint8) (bq25890_CON4),
				       (kal_uint8) (val),
				       (kal_uint8) (CON4_ICHG_MASK), (kal_uint8) (CON4_ICHG_SHIFT)
	    );
}

kal_uint32 bq25890_get_reg_ichg(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq25890_read_interface((kal_uint8) (bq25890_CON4),
				     (&val),
				     (kal_uint8) (CON4_ICHG_MASK), (kal_uint8) (CON4_ICHG_SHIFT)
	    );
	return val;
}

/* CON5---------------------------------------------------- */

void bq25890_set_iprechg(kal_uint32 val)
{
	kal_uint32 ret = 0;


	ret = bq25890_config_interface((kal_uint8) (bq25890_CON5),
				       (val),
				       (kal_uint8) (CON5_IPRECHG_MASK),
				       (kal_uint8) (CON5_IPRECHG_SHIFT)
	    );

}

void bq25890_set_iterml(kal_uint32 val)
{
	kal_uint32 ret = 0;


	ret = bq25890_config_interface((kal_uint8) (bq25890_CON5),
				       (val),
				       (kal_uint8) (CON5_ITERM_MASK), (kal_uint8) (CON5_ITERM_SHIFT)
	    );

}



/* CON6---------------------------------------------------- */

void bq25890_set_vreg(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq25890_config_interface((kal_uint8) (bq25890_CON6),
				       (kal_uint8) (val),
				       (kal_uint8) (CON6_2XTMR_EN_MASK),
				       (kal_uint8) (CON6_2XTMR_EN_SHIFT)
	    );
}

void bq25890_set_batlowv(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq25890_config_interface((kal_uint8) (bq25890_CON6),
				       (kal_uint8) (val),
				       (kal_uint8) (CON6_BATLOWV_MASK),
				       (kal_uint8) (CON6_BATLOWV_SHIFT)
	    );
}

void bq25890_set_vrechg(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq25890_config_interface((kal_uint8) (bq25890_CON6),
				       (kal_uint8) (val),
				       (kal_uint8) (CON6_VRECHG_MASK),
				       (kal_uint8) (CON6_VRECHG_SHIFT)
	    );
}

/* CON7---------------------------------------------------- */


void bq25890_en_term_chg(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq25890_config_interface((kal_uint8) (bq25890_CON7),
				       (kal_uint8) (val),
				       (kal_uint8) (CON7_EN_TERM_CHG_MASK),
				       (kal_uint8) (CON7_EN_TERM_CHG_SHIFT)
	    );
}

void bq25890_en_state_dis(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq25890_config_interface((kal_uint8) (bq25890_CON7),
				       (kal_uint8) (val),
				       (kal_uint8) (CON7_STAT_DIS_MASK),
				       (kal_uint8) (CON7_STAT_DIS_SHIFT)
	    );
}

void bq25890_set_wd_timer(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq25890_config_interface((kal_uint8) (bq25890_CON7),
				       (kal_uint8) (val),
				       (kal_uint8) (CON7_WTG_TIM_SET_MASK),
				       (kal_uint8) (CON7_WTG_TIM_SET_SHIFT)
	    );
}

void bq25890_en_chg_timer(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq25890_config_interface((kal_uint8) (bq25890_CON7),
				       (kal_uint8) (val),
				       (kal_uint8) (CON7_EN_TIMER_MASK),
				       (kal_uint8) (CON7_EN_TIMER_SHIFT)
	    );
}

void bq25890_set_chg_timer(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq25890_config_interface((kal_uint8) (bq25890_CON7),
				       (kal_uint8) (val),
				       (kal_uint8) (CON7_SET_CHG_TIM_MASK),
				       (kal_uint8) (CON7_SET_CHG_TIM_SHIFT)
	    );
}

/* CON8--------------------------------------------------- */
void bq25890_set_thermal_regulation(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq25890_config_interface((kal_uint8) (bq25890_CON8),
				       (kal_uint8) (val),
				       (kal_uint8) (CON8_TREG_MASK), (kal_uint8) (CON8_TREG_SHIFT)
	    );
}

void bq25890_set_VBAT_clamp(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq25890_config_interface((kal_uint8) (bq25890_CON8),
				       (kal_uint8) (val),
				       (kal_uint8) (CON8_VCLAMP_MASK),
				       (kal_uint8) (CON8_VCLAMP_SHIFT)
	    );
}

void bq25890_set_VBAT_IR_compensation(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq25890_config_interface((kal_uint8) (bq25890_CON8),
				       (kal_uint8) (val),
				       (kal_uint8) (CON8_BAT_COMP_MASK),
				       (kal_uint8) (CON8_BAT_COMP_SHIFT)
	    );
}

/* CON9---------------------------------------------------- */
void bq25890_pumpx_up(kal_uint32 val)
{
	kal_uint32 ret = 0;

	bq25890_en_pumpx(1);
	if (val == 1) {
		ret = bq25890_config_interface((kal_uint8) (bq25890_CON9),
					       (kal_uint8) (1),
					       (kal_uint8) (CON9_PUMPX_UP),
					       (kal_uint8) (CON9_PUMPX_UP_SHIFT)
		    );
	} else {
		ret = bq25890_config_interface((kal_uint8) (bq25890_CON9),
					       (kal_uint8) (1),
					       (kal_uint8) (CON9_PUMPX_DN),
					       (kal_uint8) (CON9_PUMPX_DN_SHIFT)
		    );
	}
/* Input current limit = 800 mA, changes after port detection*/
	bq25890_set_iinlim(0x14);
/* CC mode current = 2048 mA*/
	bq25890_set_ichg(0x20);
	msleep(3000);
}

/* CONA---------------------------------------------------- */
void bq25890_set_boost_ilim(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq25890_config_interface((kal_uint8) (bq25890_CONA),
				       (kal_uint8) (val),
				       (kal_uint8) (CONA_BOOST_ILIM_MASK),
				       (kal_uint8) (CONA_BOOST_ILIM_SHIFT)
	    );
}

void bq25890_set_boost_vlim(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq25890_config_interface((kal_uint8) (bq25890_CONA),
				       (kal_uint8) (val),
				       (kal_uint8) (CONA_BOOST_VLIM_MASK),
				       (kal_uint8) (CONA_BOOST_VLIM_SHIFT)
	    );
}

/* CONB---------------------------------------------------- */


kal_uint32 bq25890_get_vbus_state(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq25890_read_interface((kal_uint8) (bq25890_CONB),
				     (&val),
				     (kal_uint8) (CONB_VBUS_STAT_MASK),
				     (kal_uint8) (CONB_VBUS_STAT_SHIFT)
	    );
	return val;
}


kal_uint32 bq25890_get_chrg_state(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq25890_read_interface((kal_uint8) (bq25890_CONB),
				     (&val),
				     (kal_uint8) (CONB_CHRG_STAT_MASK),
				     (kal_uint8) (CONB_CHRG_STAT_SHIFT)
	    );
	return val;
}

kal_uint32 bq25890_get_pg_state(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq25890_read_interface((kal_uint8) (bq25890_CONB),
				     (&val),
				     (kal_uint8) (CONB_PG_STAT_MASK),
				     (kal_uint8) (CONB_PG_STAT_SHIFT)
	    );
	return val;
}



kal_uint32 bq25890_get_sdp_state(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq25890_read_interface((kal_uint8) (bq25890_CONB),
				     (&val),
				     (kal_uint8) (CONB_SDP_STAT_MASK),
				     (kal_uint8) (CONB_SDP_STAT_SHIFT)
	    );
	return val;
}

kal_uint32 bq25890_get_vsys_state(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq25890_read_interface((kal_uint8) (bq25890_CONB),
				     (&val),
				     (kal_uint8) (CONB_VSYS_STAT_MASK),
				     (kal_uint8) (CONB_VSYS_STAT_SHIFT)
	    );
	return val;
}

/* CON0C---------------------------------------------------- */
kal_uint32 bq25890_get_wdt_state(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq25890_read_interface((kal_uint8) (bq25890_CONC),
				     (&val),
				     (kal_uint8) (CONB_WATG_STAT_MASK),
				     (kal_uint8) (CONB_WATG_STAT_SHIFT)
	    );
	return val;
}

kal_uint32 bq25890_get_boost_state(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq25890_read_interface((kal_uint8) (bq25890_CONC),
				     (&val),
				     (kal_uint8) (CONB_BOOST_STAT_MASK),
				     (kal_uint8) (CONB_BOOST_STAT_SHIFT)
	    );
	return val;
}

kal_uint32 bq25890_get_chrg_fault_state(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq25890_read_interface((kal_uint8) (bq25890_CONC),
				     (&val),
				     (kal_uint8) (CONC_CHRG_FAULT_MASK),
				     (kal_uint8) (CONC_CHRG_FAULT_SHIFT)
	    );
	return val;
}

kal_uint32 bq25890_get_bat_state(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq25890_read_interface((kal_uint8) (bq25890_CONC),
				     (&val),
				     (kal_uint8) (CONB_BAT_STAT_MASK),
				     (kal_uint8) (CONB_BAT_STAT_SHIFT)
	    );
	return val;
}


/* COND */
void bq25890_set_FORCE_VINDPM(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq25890_config_interface((kal_uint8) (bq25890_COND),
				       (kal_uint8) (val),
				       (kal_uint8) (COND_FORCE_VINDPM_MASK),
				       (kal_uint8) (COND_FORCE_VINDPM_SHIFT)
	    );
}

void bq25890_set_VINDPM(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq25890_config_interface((kal_uint8) (bq25890_COND),
				       (kal_uint8) (val),
				       (kal_uint8) (COND_VINDPM_MASK),
				       (kal_uint8) (COND_VINDPM_SHIFT)
	    );
}

/* CONDE */
kal_uint32 bq25890_get_vbat(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq25890_read_interface((kal_uint8) (bq25890_CONE),
				     (&val),
				     (kal_uint8) (CONE_VBAT_MASK), (kal_uint8) (CONE_VBAT_SHIFT)
	    );
	return val;
}

/* CON11 */
kal_uint32 bq25890_get_vbus(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq25890_read_interface((kal_uint8) (bq25890_CON11),
				     (&val),
				     (kal_uint8) (CON11_VBUS_MASK), (kal_uint8) (CON11_VBUS_SHIFT)
	    );
	return val;
}

/* CON12 */
kal_uint32 bq25890_get_ichg(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq25890_read_interface((kal_uint8) (bq25890_CON12),
				     (&val),
				     (kal_uint8) (CONB_ICHG_STAT_MASK),
				     (kal_uint8) (CONB_ICHG_STAT_SHIFT)
	    );
	return val;
}



/* CON13 /// */


kal_uint32 bq25890_get_idpm_state(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq25890_read_interface((kal_uint8) (bq25890_CON13),
				     (&val),
				     (kal_uint8) (CON13_IDPM_STAT_MASK),
				     (kal_uint8) (CON13_IDPM_STAT_SHIFT)
	    );
	return val;
}

kal_uint32 bq25890_get_vdpm_state(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq25890_read_interface((kal_uint8) (bq25890_CON13),
				     (&val),
				     (kal_uint8) (CON13_VDPM_STAT_MASK),
				     (kal_uint8) (CON13_VDPM_STAT_SHIFT)
	    );
	return val;
}




/**********************************************************
  *
  *   [Internal Function]
  *
  *********************************************************/
void bq25890_hw_component_detect(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq25890_read_interface(0x03, &val, 0xFF, 0x0);

	if (val == 0)
		g_bq25890_hw_exist = 0;
	else
		g_bq25890_hw_exist = 1;

	pr_debug("[bq25890_hw_component_detect] exist=%d, Reg[0x03]=0x%x\n",
		 g_bq25890_hw_exist, val);
}

int is_bq25890_exist(void)
{
	pr_debug("[is_bq25890_exist] g_bq25890_hw_exist=%d\n", g_bq25890_hw_exist);

	return g_bq25890_hw_exist;
}

void bq25890_dump_register(void)
{
	kal_uint8 i = 0;
	kal_uint8 ichg = 0;
	kal_uint8 ichg_reg = 0;
	kal_uint8 iinlim = 0;
	kal_uint8 vbat = 0;
	kal_uint8 chrg_state = 0;
	kal_uint8 chr_en = 0;
	kal_uint8 vbus = 0;
	kal_uint8 vdpm = 0;
	kal_uint8 fault = 0;

	bq25890_ADC_start(1);
	for (i = 0; i < bq25890_REG_NUM; i++) {
		bq25890_read_byte(i, &bq25890_reg[i]);
		battery_log(BAT_LOG_FULL, "[bq25890 reg@][0x%x]=0x%x ", i, bq25890_reg[i]);
	}
	bq25890_ADC_start(1);
	iinlim = bq25890_get_iinlim();
	chrg_state = bq25890_get_chrg_state();
	chr_en = bq25890_get_chg_en();
	ichg_reg = bq25890_get_reg_ichg();
	ichg = bq25890_get_ichg();
	vbat = bq25890_get_vbat();
	vbus = bq25890_get_vbus();
	vdpm = bq25890_get_vdpm_state();
	fault = bq25890_get_chrg_fault_state();
	battery_log(BAT_LOG_CRTI,
		    "[PE+]BQ25896 Ichg_reg=%d mA, Iinlin=%d mA, Vbus=%d mV, err=%d",
		     ichg_reg * 64, iinlim * 50 + 100, vbus * 100 + 2600, fault);
	battery_log(BAT_LOG_CRTI, "[PE+]BQ25896 Ichg=%d mA, Vbat =%d mV, ChrStat=%d, CHGEN=%d, VDPM=%d\n",
		    ichg * 50, vbat * 20 + 2304, chrg_state, chr_en, vdpm);

}

void bq25890_hw_init(void)
{
	/*battery_log(BAT_LOG_CRTI, "[bq25890_hw_init] After HW init\n");*/
	bq25890_dump_register();
}

static int bq25890_driver_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;

	battery_log(BAT_LOG_CRTI, "[bq25890_driver_probe]\n");

	new_client = client;

	/* --------------------- */
	bq25890_hw_component_detect();
	bq25890_dump_register();
	/* bq25890_hw_init(); //move to charging_hw_xxx.c */
	chargin_hw_init_done = KAL_TRUE;

	return 0;

}

/**********************************************************
  *
  *   [platform_driver API]
  *
  *********************************************************/
kal_uint8 g_reg_value_bq25890 = 0;
static ssize_t show_bq25890_access(struct device *dev, struct device_attribute *attr, char *buf)
{
	battery_log(BAT_LOG_CRTI, "[show_bq25890_access] 0x%x\n", g_reg_value_bq25890);
	return sprintf(buf, "%u\n", g_reg_value_bq25890);
}

static ssize_t store_bq25890_access(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL;
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	battery_log(BAT_LOG_CRTI, "[store_bq25890_access]\n");

	if (buf != NULL && size != 0) {
		battery_log(BAT_LOG_CRTI, "[store_bq25890_access] buf is %s and size is %zu\n", buf,
			    size);
		reg_address = simple_strtoul(buf, &pvalue, 16);
		/*ret = kstrtoul(buf, 16, reg_address); *//* This must be a null terminated string */
		if (size > 3) {
			reg_value = simple_strtoul((pvalue + 1), NULL, 16);
			/*ret = kstrtoul(buf + 3, 16, reg_value); */
			battery_log(BAT_LOG_CRTI,
				    "[store_bq25890_access] write bq25890 reg 0x%x with value 0x%x !\n",
				    reg_address, reg_value);
			ret = bq25890_config_interface(reg_address, reg_value, 0xFF, 0x0);
		} else {
			ret = bq25890_read_interface(reg_address, &g_reg_value_bq25890, 0xFF, 0x0);
			battery_log(BAT_LOG_CRTI,
				    "[store_bq25890_access] read bq25890 reg 0x%x with value 0x%x !\n",
				    reg_address, g_reg_value_bq25890);
			battery_log(BAT_LOG_CRTI,
				    "[store_bq25890_access] Please use \"cat bq25890_access\" to get value\r\n");
		}
	}
	return size;
}

static DEVICE_ATTR(bq25890_access, 0664, show_bq25890_access, store_bq25890_access);	/* 664 */

static int bq25890_user_space_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	battery_log(BAT_LOG_CRTI, "******** bq25890_user_space_probe!! ********\n");

	ret_device_file = device_create_file(&(dev->dev), &dev_attr_bq25890_access);

	return 0;
}

struct platform_device bq25890_user_space_device = {
	.name = "bq25890-user",
	.id = -1,
};

static struct platform_driver bq25890_user_space_driver = {
	.probe = bq25890_user_space_probe,
	.driver = {
		   .name = "bq25890-user",
		   },
};

#ifdef CONFIG_OF
static const struct of_device_id bq25890_of_match[] = {
	{.compatible = "mediatek,SWITHING_CHARGER"},
	{},
};
#else
static struct i2c_board_info i2c_bq25890 __initdata = {
	I2C_BOARD_INFO("bq25890", (bq25890_SLAVE_ADDR_WRITE >> 1))
};
#endif

static struct i2c_driver bq25890_driver = {
	.driver = {
		   .name = "bq25890",
#ifdef CONFIG_OF
		   .of_match_table = bq25890_of_match,
#endif
		   },
	.probe = bq25890_driver_probe,
	.id_table = bq25890_i2c_id,
};

static int __init bq25890_init(void)
{
	int ret = 0;

	/* i2c registeration using DTS instead of boardinfo*/
#ifdef CONFIG_OF
	battery_log(BAT_LOG_CRTI, "[bq25890_init] init start with i2c DTS");
#else
	battery_log(BAT_LOG_CRTI, "[bq25890_init] init start. ch=%d\n", bq25890_BUSNUM);
	i2c_register_board_info(bq25890_BUSNUM, &i2c_bq25890, 1);
#endif
	if (i2c_add_driver(&bq25890_driver) != 0) {
		battery_log(BAT_LOG_CRTI,
			    "[bq25890_init] failed to register bq25890 i2c driver.\n");
	} else {
		battery_log(BAT_LOG_CRTI,
			    "[bq25890_init] Success to register bq25890 i2c driver.\n");
	}

	/* bq25890 user space access interface */
	ret = platform_device_register(&bq25890_user_space_device);
	if (ret) {
		battery_log(BAT_LOG_CRTI, "****[bq25890_init] Unable to device register(%d)\n",
			    ret);
		return ret;
	}
	ret = platform_driver_register(&bq25890_user_space_driver);
	if (ret) {
		battery_log(BAT_LOG_CRTI, "****[bq25890_init] Unable to register driver (%d)\n",
			    ret);
		return ret;
	}

	return 0;
}

static void __exit bq25890_exit(void)
{
	i2c_del_driver(&bq25890_driver);
}
module_init(bq25890_init);
module_exit(bq25890_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C bq25890 Driver");
MODULE_AUTHOR("will cai <will.cai@mediatek.com>");
