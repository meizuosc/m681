#include <linux/kconfig.h>
#include <linux/memblock.h>
#include <linux/mrdump.h>
#include <linux/reboot.h>
#include <asm/memory.h>
#include <asm/page.h>
#include <linux/io.h>
#include <mach/wd_api.h>

/* change to DTS
    #define MRDUMP_CB_ADDR (PHYS_OFFSET + 0x1F00000)
    #define MRDUMP_CB_SIZE 0x2000
*/
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/mod_devicetable.h>
static phys_addr_t mrdump_base;
static phys_addr_t mrdump_size;

uint64_t aee_get_mrdump_base(void)
{
	return (uint64_t) mrdump_base;
}

uint64_t aee_get_mrdump_size(void)
{
	return (uint64_t) mrdump_size;
}

static void mrdump_hw_enable(bool enabled)
{
#ifdef CONFIG_HAVE_DDR_RESERVE_MODE
	struct wd_api *wd_api = NULL;
	get_wd_api(&wd_api);
	if (wd_api)
		wd_api->wd_dram_reserved_mode(enabled);
#endif
}

static void mrdump_reboot(void)
{
	int res;
	struct wd_api *wd_api = NULL;

	res = get_wd_api(&wd_api);
	if (res) {
		pr_alert("arch_reset, get wd api error %d\n", res);
		while (1) {
			cpu_relax();
		}
	} else {
		wd_api->wd_sw_reset(0);
	}
}

const struct mrdump_platform mrdump_v2_platform = {
	.hw_enable = mrdump_hw_enable,
	.reboot = mrdump_reboot
};


static __init void mrdump_plat_register(void)
{
	pr_alert("register mrdump_plat\n");
	mrdump_platform_init(NULL, &mrdump_v2_platform);
}
device_initcall(mrdump_plat_register);
