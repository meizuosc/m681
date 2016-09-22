#ifndef __MT_MMC_H__
#define __MT_MMC_H__

#ifdef CONFIG_MTK_MMC
extern void msdc_clk_status(int *status);
#else
void msdc_clk_status(int *status)
{
	*status = 0;
}
#endif

#endif /* __MT_MMC_H__ */

