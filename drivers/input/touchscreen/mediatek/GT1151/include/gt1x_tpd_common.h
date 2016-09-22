/* drivers/input/touchscreen/gt1x_tpd_custom.h
 *
 * 2010 - 2014 Goodix Technology.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Version: 1.0
 * Revision Record:
 *      V1.0:  first release. 2014/09/28.
 */

#ifndef GT1X_TPD_COMMON_H__
#define GT1X_TPD_COMMON_H__

extern void gt1x_auto_update_done(void);
extern int mt_eint_set_deint(int eint_num, int irq_num);
extern int mt_eint_clr_deint(int eint_num);
extern int mt_get_gpio_mode_base(unsigned long pin);
extern int mt_get_gpio_pull_select_base(unsigned long pin);
extern int mt_get_gpio_in_base(unsigned long pin);
extern int mt_get_gpio_out_base(unsigned long pin);
extern int mt_get_gpio_pull_enable_base(unsigned long pin);
extern int mt_get_gpio_dir_base(unsigned long pin);
extern int mt_get_gpio_ies_base(unsigned long pin);
extern int mt_get_gpio_smt_base(unsigned long pin);
extern void mt_eint_dump_status(unsigned int eint);

#endif /* GT1X_TPD_COMMON_H__ */
