#include <linux/kconfig.h>
#include <linux/memblock.h>
#include <linux/mrdump.h>
#include <linux/reboot.h>
#include <asm/memory.h>
#include <asm/page.h>
#include <linux/io.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <asm/sections.h>
#include <asm/setup.h>
#include <asm/sizes.h>

#ifdef CONFIG_OF_RESERVED_MEM
reservedmem_of_init_fn free_reserve_memory_mrdump_fn(struct reserved_mem *rmem,
						unsigned long node,
						const char *uname)
{
	pr_alert("%s, name: %s, uname: %s, base: 0x%llx, size: 0x%llx\n",
		 __func__, rmem->name, uname,
		 (unsigned long long)rmem->base,
		 (unsigned long long)rmem->size);
	if (memblock_is_region_reserved(rmem->base, rmem->size) && rmem->base > __pa(_end)) {
		pr_alert
	    ("mrdump disabled , return reserve memory base = %x size=%x\n",
			(unsigned int)rmem->base, (unsigned int)rmem->size);

		if (memblock_free(rmem->base, rmem->size)) {
			pr_alert
			("mrdump memblock_free failed on memory base 0x%x size=%x\n",
			(unsigned int)rmem->base, (unsigned int)rmem->size);
		}
	} else {
		pr_alert
		("mrdump disabled and DTS reserved failed on memory base = %x size=%x\n",
			(unsigned int)rmem->base, (unsigned int)rmem->size);
	}
	return 0;
}

RESERVEDMEM_OF_DECLARE(mrdump_reserved_memory, "mediatek,mrdump-V2",
		       free_reserve_memory_mrdump_fn);
#endif
