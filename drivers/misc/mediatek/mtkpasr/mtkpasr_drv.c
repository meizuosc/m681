/*
 * MTKPASR SW Module
 */

#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/buffer_head.h>
#include <linux/device.h>
#include <linux/genhd.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/lzo.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/swap.h>
#include <linux/syscore_ops.h>
#include <linux/suspend.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#else
#include <linux/fb.h>
#endif
#include <linux/migrate.h>
#include <linux/mm_inline.h>
#include "mm/internal.h"
#include "mtkpasr_drv.h"

/* #define NO_UART_CONSOLE */
#define MTKPASR_STATISTICS
#define MTKPASR_FAST_PATH

/* MTKPASR Information */
static struct zs_pool *mtkpasr_mem_pool;
static struct table *mtkpasr_table;
static u64 mtkpasr_disksize;		/* bytes */
static u32 mtkpasr_total_slots;
static u32 mtkpasr_free_slots;

/* Rank & Bank Information */
static struct mtkpasr_bank *mtkpasr_banks;
static struct mtkpasr_rank *mtkpasr_ranks;
static int num_banks;
static int num_ranks;
static unsigned long pages_per_bank;
static unsigned long mtkpasr_start_pfn;
static unsigned long mtkpasr_end_pfn;
static unsigned long mtkpasr_total_pfns;

/* Strategy control for PASR SW operation - MAFL */
static unsigned int mtkpasr_ops_invariant;
static unsigned int prev_mafl_count;
static unsigned int before_mafl_count;

/*
 * Collected list for recycled page blocks - It will be moved to mafl or buddy allocator if necessary.
 */
static LIST_HEAD(collected_list);
static unsigned int collected_count;

/* For no-PASR-imposed banks */
static struct nopasr_bank *nopasr_banks;
static int num_nopasr_banks;

/* For page migration operation */
static LIST_HEAD(fromlist);
static LIST_HEAD(tolist);
static int fromlist_count;
static int tolist_count;
static unsigned long mtkpasr_migration_end;
static unsigned long mtkpasr_last_scan;
static int mtkpasr_admit_order;

/* Switch : Enabled by default */
int mtkpasr_enable = 1;
unsigned long mtkpasr_enable_sr = 1;

/* Receive PM notifier flag */
static bool pm_in_hibernation;

/*-- MTKPASR INTERNAL-USED PARAMETERS --*/

/* Map of in-use pages: For a bank with 128MB, we need 32 pages. */
static void *src_pgmap;
/* Maps for pages in external compression */
static unsigned long *extcomp;
/* With the size equal to src_pgmap */
static unsigned long *sorted_for_extcomp;
/* MTKPASR state */
static enum mtkpasr_phase mtkpasr_status = MTKPASR_OFF;
/* Atomic variable to indicate MTKPASR slot */
static atomic_t sloti;

/* Debug filter */
#ifdef CONFIG_MT_ENG_BUILD
int mtkpasr_debug_level = 3;
#else
int mtkpasr_debug_level = 1;
#endif

/* Globals */
struct mtkpasr *mtkpasr_device;

#define MTKPASR_EXHAUSTED	(low_wmark_pages(MTKPASR_ZONE) + pageblock_nr_pages)
/* Show mem banks */
int mtkpasr_show_banks(char *buf)
{
	int i, j, len = 0, tmp;

	if (mtkpasr_device->init_done == 0)
		return sprintf(buf, "MTKPASR is not initialized!\n");

	/* Overview */
	tmp = sprintf(buf,
		"num_banks[%d] num_ranks[%d] mtkpasr_start_pfn[%ld] mtkpasr_end_pfn[%ld] mtkpasr_total_pfns[%ld]\n",
			num_banks, num_ranks, mtkpasr_start_pfn, mtkpasr_end_pfn, mtkpasr_total_pfns);
	buf += tmp;
	len += tmp;

	/* Show ranks & banks */
	for (i = 0; i < num_ranks; i++) {
		tmp = sprintf(buf, "Rank[%d] - start_bank[%d] end_bank[%d]\n", i, mtkpasr_ranks[i].start_bank,
				mtkpasr_ranks[i].end_bank);
		buf += tmp;
		len += tmp;
		for (j = mtkpasr_ranks[i].start_bank; j <= mtkpasr_ranks[i].end_bank; j++) {
			tmp = sprintf(buf, "  Bank[%d] - start_pfn[0x%lx] end_pfn[0x%lx] inmafl[%d] segment[%d]\n",
					j, mtkpasr_banks[j].start_pfn, mtkpasr_banks[j].end_pfn-1,
					mtkpasr_banks[j].inmafl, mtkpasr_banks[j].segment);
			buf += tmp;
			len += tmp;
		}
	}

	/* Show remaining banks */
	for (i = 0; i < num_banks; i++) {
		if (mtkpasr_banks[i].rank == NULL) {
			tmp = sprintf(buf, "Bank[%d] - start_pfn[0x%lx] end_pfn[0x%lx] inmafl[%d] segment[%d]\n",
					i, mtkpasr_banks[i].start_pfn, mtkpasr_banks[i].end_pfn-1,
					mtkpasr_banks[i].inmafl, mtkpasr_banks[i].segment);
			buf += tmp;
			len += tmp;
		}
	}

	/* Others */
	tmp = sprintf(buf, "Exhausted level[%ld]\n", (unsigned long)MTKPASR_EXHAUSTED);
	buf += tmp;
	len += tmp;

#ifdef NO_UART_CONSOLE
	memcpy(buf, mtkpasr_log_buf, 1024);
	len += 1024;
#endif

	return len;
}

static void mtkpasr_free_page(struct mtkpasr *mtkpasr, size_t index)
{
	unsigned long handle = mtkpasr_table[index].handle;

	if (unlikely(!handle)) {
		/*
		 * No memory is allocated for zero filled pages.
		 * Simply clear zero page flag.
		 */
		return;
	}

	zs_free(mtkpasr_mem_pool, handle);

	/* Reset related fields to make it consistent ! */
	mtkpasr_table[index].handle = 0;
	mtkpasr_table[index].size = 0;
	mtkpasr_table[index].obj = NULL;
}

/* 0x5D5D5D5D */
static void handle_zero_page(struct page *page)
{
	void *user_mem;

	user_mem = kmap_atomic(page);
	memset(user_mem, 0x5D, PAGE_SIZE);
	kunmap_atomic(user_mem);

	flush_dcache_page(page);
}

static int mtkpasr_read(struct mtkpasr *mtkpasr, u32 index, struct page *page)
{
	int ret;
	size_t clen;
	unsigned char *user_mem, *cmem;

	/* !! We encapsulate the page into mtkpasr_table !! */
	page = mtkpasr_table[index].obj;
	if (page == NULL) {
		mtkpasr_err("\n\n\n\nNull Page!\n\n\n\n");
		return 0;
	}

	/* Requested page is not present in compressed area */
	if (unlikely(!mtkpasr_table[index].handle)) {
		mtkpasr_err("Not present : page pfn[%ld]\n", page_to_pfn(page));
		handle_zero_page(page);
		return 0;
	}

	user_mem = kmap_atomic(page);
	clen = PAGE_SIZE;
	cmem = zs_map_object(mtkpasr_mem_pool, mtkpasr_table[index].handle, ZS_MM_RO);

	if (mtkpasr_table[index].size == PAGE_SIZE) {
		memcpy(user_mem, cmem, PAGE_SIZE);
		ret = LZO_E_OK;
	} else {
		ret = lzo1x_decompress_safe(cmem, mtkpasr_table[index].size, user_mem, &clen);
	}

	zs_unmap_object(mtkpasr_mem_pool, mtkpasr_table[index].handle);

	kunmap_atomic(user_mem);

	/* Should NEVER happen. Return bio error if it does. */
	if (unlikely(ret != LZO_E_OK)) {
		mtkpasr_err("Decompression failed! err=%d, page=%u, pfn=%lu\n", ret, index, page_to_pfn(page));
		/* Should be zero! */
		/* return 0; */	/* to free this slot */
	}

	/* Can't use it because maybe some pages w/o actual mapping
	flush_dcache_page(page); */

	/* Free this object */
	mtkpasr_free_page(mtkpasr, index);

	return 0;
}

static int mtkpasr_write(struct mtkpasr *mtkpasr, u32 index, struct page *page)
{
	int ret;
	size_t clen;
	unsigned long handle;
	unsigned char *user_mem, *cmem, *src;

	src = mtkpasr->compress_buffer;

	/*
	 * System overwrites unused sectors. Free memory associated
	 * with this sector now.
	 */

	user_mem = kmap_atomic(page);

	ret = lzo1x_1_compress(user_mem, PAGE_SIZE, src, &clen, mtkpasr->compress_workmem);

	kunmap_atomic(user_mem);

	if (unlikely(ret != LZO_E_OK)) {
		mtkpasr_err("Compression failed! err=%d\n", ret);
		ret = -EIO;
		goto out;
	}

	if (unlikely(clen > max_cmpr_size)) {
		src = NULL;
		clen = PAGE_SIZE;
	}

	handle = zs_malloc(mtkpasr_mem_pool, clen);
	if (!handle) {
		mtkpasr_err("Error allocating memory for compressed page: %u, size=%zu\n", index, clen);
		ret = -ENOMEM;
		goto out;
	}
	cmem = zs_map_object(mtkpasr_mem_pool, handle, ZS_MM_WO);

	if (clen == PAGE_SIZE)
		src = kmap_atomic(page);
	memcpy(cmem, src, clen);
	if (clen == PAGE_SIZE)
		kunmap_atomic(src);

	zs_unmap_object(mtkpasr_mem_pool, handle);

	/* Update global MTKPASR table */
	mtkpasr_table[index].handle = handle;
	mtkpasr_table[index].size = clen;
	mtkpasr_table[index].obj = page;

	return 0;

out:
	return ret;
}

/* This is the main entry for active memory compression for PASR */
/*
 * return 0 means success
 */
int mtkpasr_forward_rw(struct mtkpasr *mtkpasr, u32 index, struct page *page, int rw)
{
	int ret = -ENOMEM;

	/* Sanity check */
	BUG_ON(extcomp == NULL);

	if (rw == READ) {
		ret = mtkpasr_read(mtkpasr, index, page);
		mtkpasr_free_slots = (!!ret) ? mtkpasr_free_slots : (mtkpasr_free_slots + 1);
	} else {
		/* No free slot! */
		if (mtkpasr_free_slots == 0) {
			mtkpasr_log("No free slots!\n");
			return ret;
		}
		ret = mtkpasr_write(mtkpasr, index, page);
		mtkpasr_free_slots = (!!ret) ? mtkpasr_free_slots : (mtkpasr_free_slots - 1);
	}

	return ret;
}
EXPORT_SYMBOL(mtkpasr_forward_rw);

/* Acquire the number of free slots */
int mtkpasr_acquire_frees(void)
{
	return mtkpasr_free_slots;
}
EXPORT_SYMBOL(mtkpasr_acquire_frees);

/* Acquire the number of total slots */
int mtkpasr_acquire_total(void)
{
	return mtkpasr_total_slots;
}
EXPORT_SYMBOL(mtkpasr_acquire_total);

/* This is a recovery step for invalid PASR status */
void mtkpasr_reset_slots(void)
{
	size_t index;

	/* Free all pages that are still in this mtkpasr device */
	for (index = 0; index < mtkpasr_total_slots; index++) {
		unsigned long handle = mtkpasr_table[index].handle;

		if (!handle)
			continue;

		zs_free(mtkpasr_mem_pool, handle);

		/* Reset related fields to make it consistent ! */
		mtkpasr_table[index].handle = 0;
		mtkpasr_table[index].size = 0;
		mtkpasr_table[index].obj = NULL;

		/* Add it */
		mtkpasr_free_slots++;
	}

#ifdef CONFIG_MTKPASR_DEBUG
	if (mtkpasr_free_slots != mtkpasr_total_slots)
		BUG();
#endif
}

/*******************************/
/* MTKPASR Core Implementation */
/*******************************/

/* Return whether current system has enough free memory to save it from congestion */
#define SAFE_ORDER	(THREAD_SIZE_ORDER + 1)
static struct zone *nz;
static unsigned long safe_level;
static int pasr_check_free_safe(void)
{
	unsigned long free = zone_page_state(nz, NR_FREE_PAGES);

	/* Subtract order:0 and order:1 from total free number */
	free = free - MTKPASR_ZONE->free_area[0].nr_free - (MTKPASR_ZONE->free_area[1].nr_free << 1);

	/* Judgement */
	if (free > safe_level)
		return 0;

	return -1;
}

/* Strategy control - MAFL */
static unsigned long mafl_total_count;
unsigned int mtkpasr_show_collected(void)
{
	return collected_count;
}
unsigned long mtkpasr_show_page_reserved(void)
{
	return mafl_total_count + (collected_count << (MAX_ORDER - 1));
}
bool mtkpasr_no_phaseone_ops(void)
{
	int safe_mtkpasr_pfns;

	safe_mtkpasr_pfns = mtkpasr_total_pfns >> 1;
	return (prev_mafl_count == mafl_total_count) || (mafl_total_count > safe_mtkpasr_pfns);
}
bool mtkpasr_no_ops(void)
{
	return (mafl_total_count == mtkpasr_total_pfns) || (mtkpasr_ops_invariant > MAX_OPS_INVARIANT);
}

/* Reset state to MTKPASR_OFF */
void mtkpasr_reset_state(void)
{
	mtkpasr_reset_slots();
	mtkpasr_status = MTKPASR_OFF;
}

/* Enabling SR or Turning off DPD. It should be called after syscore_resume
 * Actually, it only does reset on the status of all ranks & banks!
 *
 * Return - MTKPASR_WRONG_STATE (Should bypass it & go to next state)
 *	    MTKPASR_SUCCESS
 */
enum mtkpasr_phase mtkpasr_enablingSR(void)
{
	enum mtkpasr_phase result = MTKPASR_SUCCESS;
	int check_dpd = 0;
	int i, j;

	/* Sanity Check */
	if (mtkpasr_status != MTKPASR_ON && mtkpasr_status != MTKPASR_DPD_ON) {
		mtkpasr_err("Error Current State [%d]!\n", mtkpasr_status);
		return MTKPASR_WRONG_STATE;

	} else {
		check_dpd = (mtkpasr_status == MTKPASR_DPD_ON) ? 1 : 0;
		/* Go to MTKPASR_ENABLINGSR state */
		mtkpasr_status = MTKPASR_ENABLINGSR;
	}

	/* From which state */
	if (check_dpd) {
		for (i = 0; i < num_ranks; i++) {
			if (mtkpasr_ranks[i].inused == MTKPASR_DPDON) {
				/* Clear rank */
				mtkpasr_ranks[i].inused = 0;
				/* Clear all related banks */
				for (j = mtkpasr_ranks[i].start_bank; j <= mtkpasr_ranks[i].end_bank; j++)
					mtkpasr_banks[j].inused = 0;
				mtkpasr_info("Call DPDOFF API!\n");
			}
		}
	}

	/* Check all banks */
	for (i = 0; i < num_banks; i++) {
		if (mtkpasr_banks[i].inused == MTKPASR_SROFF) {
			/* Clear bank */
			mtkpasr_banks[i].inused = 0;
			mtkpasr_info("Call SPM SR/ON API on bank[%d]!\n", i);
		} else {
			mtkpasr_info("Bank[%d] free[%d]!\n", i, mtkpasr_banks[i].inused);
		}
	}

	/* Go to MTKPASR_EXITING state if success(Always being success!) */
	if (result == MTKPASR_SUCCESS)
		mtkpasr_status = MTKPASR_EXITING;

	return result;
}

/* Decompress all immediate data. It should be called right after mtkpasr_enablingSR
 *
 * Return - MTKPASR_WRONG_STATE
 *          MTKPASR_SUCCESS
 *          MTKPASR_FAIL (Some fatal error!)
 */
enum mtkpasr_phase mtkpasr_exiting(void)
{
	enum mtkpasr_phase result = MTKPASR_SUCCESS;
	int current_index;
	int ret = 0;
	struct mtkpasr *mtkpasr;
	int should_flush_cache = 0;
#ifdef CONFIG_MTKPASR_DEBUG
	int decompressed = 0;
#endif

	mtkpasr_info("\n");

	/* Sanity Check */
	if (mtkpasr_status != MTKPASR_EXITING) {
		mtkpasr_err("Error Current State [%d]!\n", mtkpasr_status);
		/*
		 * Failed to exit PASR!! - This will cause some user processes died unexpectedly!
		 * We don't do anything here because it is harmless to kernel.
		 */
		return MTKPASR_WRONG_STATE;
	}

	/* Main thread is here */
	mtkpasr = &mtkpasr_device[0];

	/* Do decompression */
	current_index = atomic_dec_return(&sloti);
	should_flush_cache = current_index;
	while (current_index >= 0) {
		ret = mtkpasr_forward_rw(mtkpasr, current_index, NULL, READ);
		/* Unsuccessful decompression */
		if (unlikely(ret))
			break;
#ifdef CONFIG_MTKPASR_DEBUG
		++decompressed;
#endif
		/* Next */
		current_index = atomic_dec_return(&sloti);
	}

	/* Check decompression result */
	if (ret) {
		mtkpasr_err("Failed Decompression!\n");
		/*
		 * Failed to exit PASR!! - This will cause some user processes died unexpectedly!
		 * We don't do anything here because it is harmless to kernel.
		 */
		result = MTKPASR_FAIL;
	}

	/* Go to MTKPASR_OFF state if success */
	if (result == MTKPASR_SUCCESS)
		mtkpasr_status = MTKPASR_OFF;

	/* Check whether we should flush cache */
	if (should_flush_cache >= 0)
		flush_cache_all();

#ifdef CONFIG_MTKPASR_DEBUG
	mtkpasr_info("Decompressed pages [%d]\n", decompressed);
#endif
	return result;
}

/* If something error happens at MTKPASR_ENTERING/MTKPASR_DISABLINGSR, then call it */
void mtkpasr_restoring(void)
{
	mtkpasr_info("\n");

	/* Sanity Check */
	if (mtkpasr_status != MTKPASR_ENTERING && mtkpasr_status != MTKPASR_DISABLINGSR) {
		mtkpasr_err("Error Current State [%d]!\n", mtkpasr_status);
		return;

	} else {
		/* Go to MTKPASR_RESTORING state */
		mtkpasr_status = MTKPASR_RESTORING;
	}

	/*
	 * No matter which status it reaches, we only need to do is to reset all slots here!
	 * (Data stored is not corrupted!)
	 */
	mtkpasr_reset_slots();

	/* Go to MTKPASR_OFF state */
	mtkpasr_status = MTKPASR_OFF;

	mtkpasr_info("(END)\n");
}

/*
 * Check whether current page is compressed
 * return 1 means found
 *        0       not found
 */
static int check_if_compressed(long start, long end, int pfn)
{
#ifndef CONFIG_64BIT
	long mid;
	int found = 0;

	/* Needed! */
	end = end - 1;

	/* Start to search */
	while (start <= end) {
		mid = (start + end) >> 1;
		if (pfn == extcomp[mid]) {
			found = 1;
			break;
		} else if (pfn > extcomp[mid]) {
			start = mid + 1;
		} else {
			end = mid - 1;
		}
	}

	return found;
#else
	return 0;
#endif
}

/* Return the number of inuse pages */
static u32 check_inused(unsigned long start, unsigned long end, long comp_start, long comp_end)
{
	int inused = 0;
	struct page *page;

	for (; start < end; start++) {
		page = pfn_to_page(start);
		if (PAGE_INUSED(page)) {
			if (check_if_compressed(comp_start, comp_end, start) == 0)
				++inused;
		} else {
			start += ((1 << PAGE_ORDER(page)) - 1);
		}
	}

	return inused;
}

/* Compute bank inused! */
static void compute_bank_inused(int all)
{
	int i;

	/* Fast path - MAFL */
	if (mtkpasr_no_ops()) {
		for (i = 0; i < num_banks; i++) {
			if (mtkpasr_banks[i].inmafl == mtkpasr_banks[i].valid_pages) {
				mtkpasr_banks[i].inused = 0;
			} else {
				/* Rough estimation */
				mtkpasr_banks[i].inused = mtkpasr_banks[i].valid_pages - mtkpasr_banks[i].inmafl;
			}
		}
		goto fast_path;
	}

	/*
	 * Drain pcp LRU lists to free some "unused" pages!
	 * (During page migration, there may be some OLD pages be in pcp pagevec! To free them!)
	 * To call lru_add_drain();
	 *
	 * Drain pcp free lists to free some hot/cold pages into buddy!
	 * To call drain_local_pages(NULL);
	 */
	MTKPASR_FLUSH();

	/* Scan banks - MAFL */
	for (i = 0; i < num_banks; i++) {
		mtkpasr_banks[i].inused = check_inused(mtkpasr_banks[i].start_pfn + mtkpasr_banks[i].inmafl,
				mtkpasr_banks[i].end_pfn, mtkpasr_banks[i].comp_start, mtkpasr_banks[i].comp_end);
	}

fast_path:
	/* Should we compute no-PASR-imposed banks? */
	if (all != 0) {
		/* Excluding 1st nopasr_banks (Kernel resides here.)*/
		for (i = 1; i < num_nopasr_banks; i++)
			nopasr_banks[i].inused = check_inused(nopasr_banks[i].start_pfn, nopasr_banks[i].end_pfn, 0, 0);
	}
}

#define MARK_PAGE_COLLECTED(p)		set_page_count(p, 1)
#define UNMARK_PAGE_COLLECTED(p)	set_page_count(p, 0)
#define PAGE_COLLECTED(p)		(page_count(p) == 1)
#define PAGE_NOT_COLLECTED(p)		(page_count(p) == 0)

/* Remove free page blocks from buddy */
static void remove_page_from_buddy(int bank)
{
	struct page *spage, *epage;
	struct zone *z;
	unsigned long flags;
	unsigned int order;
	int free_count;
	int *inmafl;

	inmafl = &mtkpasr_banks[bank].inmafl;
	spage = pfn_to_page(mtkpasr_banks[bank].start_pfn);
	epage = pfn_to_page(mtkpasr_banks[bank].end_pfn);
	z = page_zone(spage);

	/* Lock this zone */
	spin_lock_irqsave(&z->lock, flags);

	/* Check whether remaining pages are in buddy */
	spage += *inmafl;
	while (spage < epage) {
		if (PAGE_INUSED(spage))
			spage++;
		else {
			order = PAGE_ORDER(spage);
			free_count = 1 << order;
			/* Only collect the pageblocks with order of "MAX_ORDER - 1" */
			if (PAGE_NOT_COLLECTED(spage) && (order == (MAX_ORDER - 1))) {
				/* Delete it from buddy */
				list_del(&spage->lru);
				z->free_area[order].nr_free--;
				/* No removal on page block's order - rmv_PAGE_ORDER(spage); */
				__mod_zone_page_state(z, NR_FREE_PAGES, -(free_count));
				/* Add to collected_list */
				list_add_tail(&spage->lru, &collected_list);
				collected_count++;
				/* Mark page as collected */
				MARK_PAGE_COLLECTED(spage);
			}
			spage += free_count;
		}
	}

	/* UnLock this zone */
	spin_unlock_irqrestore(&z->lock, flags);

	mtkpasr_log("bank[%d] collected_count[%u] ", bank, collected_count);
}

/* Test whether it can be removed from buddy temporarily - MAFL */
static void remove_bank_from_buddy(int bank)
{
	int has_extcomp = mtkpasr_banks[bank].comp_start - mtkpasr_banks[bank].comp_end;
	struct page *spage, *epage;
	struct zone *z;
	unsigned long flags;
	unsigned int order;
	int free_count;
	struct list_head *mafl;
	int *inmafl;

	/* mafl is full */
	inmafl = &mtkpasr_banks[bank].inmafl;
	if (*inmafl == mtkpasr_banks[bank].valid_pages)
		return;

	/* This bank can't be removed! Don't consider banks with external compression. */
	if (has_extcomp != 0)
		return;

	spage = pfn_to_page(mtkpasr_banks[bank].start_pfn);
	epage = pfn_to_page(mtkpasr_banks[bank].end_pfn);
	z = page_zone(spage);

	/* Lock this zone */
	spin_lock_irqsave(&z->lock, flags);

	/* Check whether remaining pages are in buddy */
	spage += *inmafl;
	while (spage < epage) {
		/* Not in buddy, exit (TBD, "just exit" is not a good way to gather a free bank) */
		if (PAGE_INUSED(spage)) {
			spin_unlock_irqrestore(&z->lock, flags);
			return;
		}
		/* Check next page block */
		free_count = 1 << PAGE_ORDER(spage);
		spage += free_count;
	}

	/* Remove it from buddy to bank's mafl */
	mafl = &mtkpasr_banks[bank].mafl;
	spage = pfn_to_page(mtkpasr_banks[bank].start_pfn) + *inmafl;
	while (spage < epage) {
		/* Delete it from buddy */
		list_del(&spage->lru);
		order = PAGE_ORDER(spage);
		/* Update kernel buddy information when the page is not collected */
		if (PAGE_COLLECTED(spage)) {
			UNMARK_PAGE_COLLECTED(spage);
			collected_count--;
		} else {
			z->free_area[order].nr_free--;
			/* No removal on page block's order - rmv_PAGE_ORDER(spage); */
			__mod_zone_page_state(z, NR_FREE_PAGES, -(1UL << order));
		}
		/* Add it to mafl */
		list_add_tail(&spage->lru, mafl);
		/* Check next page block */
		free_count = 1 << order;
		spage += free_count;
		/* Update statistics */
		*inmafl += free_count;
		mafl_total_count += free_count;
	}

	/* UnLock this zone */
	spin_unlock_irqrestore(&z->lock, flags);

#ifdef CONFIG_MTKPASR_DEBUG
	if (mtkpasr_banks[bank].inmafl != mtkpasr_banks[bank].valid_pages)
		BUG();
#endif
}

static bool mtkpasr_no_exhausted(int request_order)
{
	long free, exhausted_level = MTKPASR_EXHAUSTED;
	int order;

	free = zone_page_state(MTKPASR_ZONE, NR_FREE_PAGES);
	if (request_order > 0) {
		for (order = 0; order < request_order; ++order)
			free -= MTKPASR_ZONE->free_area[order].nr_free << order;
		exhausted_level >>= order;
	}

	return free >= exhausted_level;
}

/* Early path to release mtkpasr reserved pages */
void try_to_release_mtkpasr_page(int request_order)
{
	int current_bank = 0;
	struct list_head *mafl = NULL;
	struct page *page;
	struct zone *z;
	unsigned long flags;
	unsigned int order;
	int free_count;

	/* If we force whole rank off, don't shrink MTKPASR pages */
	if (mtkpasr_force_rankoff())
		return;

	/* We are in MTKPASR stage! */
	if (unlikely(current->flags & PF_MTKPASR))
		return;

	z = MTKPASR_ZONE;
	spin_lock_irqsave(&z->lock, flags);
	/* Try to get one page block(back to buddy allocator) from collected_list */
	if (!list_empty(&collected_list)) {
		page = list_entry(collected_list.next, struct page, lru);
		list_del(&page->lru);
		UNMARK_PAGE_COLLECTED(page);
		collected_count--;
		order = MAX_ORDER - 1;
		/* Add to tail!! */
		list_add_tail(&page->lru, &z->free_area[order].free_list[MIGRATE_MTKPASR]);
		__mod_zone_page_state(z, NR_FREE_PAGES, 1UL << order);
		z->free_area[order].nr_free++;

		spin_unlock_irqrestore(&z->lock, flags);
		return;
	}
	spin_unlock_irqrestore(&z->lock, flags);

	/* Check whether it is empty */
	if (mafl_total_count <= 0)
		return;

	/* Test whether mtkpasr is under suitable level */
	if (mtkpasr_no_exhausted(request_order))
		return;

	/* Try to release one page block */
	while (current_bank < num_banks) {
		mafl = &mtkpasr_banks[current_bank].mafl;
		if (!list_empty(mafl))
			break;
		++current_bank;
		mafl = NULL;
	}

	/* Avoid uninitialized */
	if (mafl == NULL)
		return;

	/* Lock this zone - suppose a bank doesn't cross zone (TBD) */
	z = page_zone(pfn_to_page(mtkpasr_banks[current_bank].start_pfn));
	spin_lock_irqsave(&z->lock, flags);

	/* It may be empty here due to another release operation! */
	if (list_empty(mafl)) {
		spin_unlock_irqrestore(&z->lock, flags);
		return;
	}

	/* Put the last page block back - (Should be paired with remove_bank_from_buddy) */
	page = list_entry(mafl->prev, struct page, lru);
	list_del(&page->lru);
	order = PAGE_ORDER(page);

	/* Add to tail!! */
	list_add_tail(&page->lru, &z->free_area[order].free_list[MIGRATE_MTKPASR]);
	__mod_zone_page_state(z, NR_FREE_PAGES, 1UL << order);
	z->free_area[order].nr_free++;

	/* Update statistics */
	free_count = 1 << order;
	mtkpasr_banks[current_bank].inmafl -= free_count;
	mafl_total_count -= free_count;

	/* UnLock this zone */
	spin_unlock_irqrestore(&z->lock, flags);

	/* Sanity check */
	if (mtkpasr_banks[current_bank].inmafl < 0)
		mtkpasr_info("BUG: Negative inmafl in bank[%d] Remaining MAFL [%ld]!\n",
				current_bank, mafl_total_count);
}

/* Shrinking mtkpasr_banks[bank]'s mafl totally */
static void shrink_mafl_all(int bank)
{
	struct list_head *mafl = NULL;
	int *inmafl;
	struct page *page, *next;
	struct zone *z;
	unsigned long flags;
	unsigned int order;
#ifdef CONFIG_MTKPASR_DEBUG
	int free_count = 0;
#endif

	/* Sanity check */
	if (bank >= num_banks || bank < 0)
		return;

	mafl = &mtkpasr_banks[bank].mafl;
	if (list_empty(mafl))
		return;

	z = page_zone(pfn_to_page(mtkpasr_banks[bank].start_pfn));

	/* Lock this zone */
	spin_lock_irqsave(&z->lock, flags);

	/* Current number in mafl */
	if (list_empty(mafl)) {
		spin_unlock_irqrestore(&z->lock, flags);
		return;
	}
	inmafl = &mtkpasr_banks[bank].inmafl;

	/* Put them back */
	list_for_each_entry_safe(page, next, mafl, lru) {
		list_del(&page->lru);
		order = PAGE_ORDER(page);
		/* Add to tail!! */
		list_add_tail(&page->lru, &z->free_area[order].free_list[MIGRATE_MTKPASR]);
		__mod_zone_page_state(z, NR_FREE_PAGES, 1UL << order);
		z->free_area[order].nr_free++;
		/* Set page buddy */
#ifdef CONFIG_MTKPASR_DEBUG
		free_count += (1 << order);
#endif
	}

#ifdef CONFIG_MTKPASR_DEBUG
	/* Test whether they are equal */
	if (free_count != *inmafl) {
		mtkpasr_err("\n\n BANK[%d] free_count[%d] inmafl[%d]\n\n\n", bank, free_count, *inmafl);
		BUG();
	}
#endif

	/* Update statistics */
	mafl_total_count -= *inmafl;
	*inmafl = 0;

	/* UnLock this zone */
	spin_unlock_irqrestore(&z->lock, flags);
}

static unsigned long __putback_free_pages(struct list_head *freelist)
{
	struct page *page, *next;
	unsigned long count = 0;

	list_for_each_entry_safe(page, next, freelist, lru) {
		list_del(&page->lru);
		__free_page(page);
		count++;
	}

	return count;
}

/* Release pages from freelist */
static void putback_free_pages(void)
{
	/* Clear tolist */
	if (tolist_count != 0) {
		if (__putback_free_pages(&tolist) != tolist_count)
			mtkpasr_err("Should be the same!\n");
		tolist_count = 0;
	}
}

int mtkpasr_isolate_page(struct page *page)
{
	struct zone *zone = page_zone(page);
	struct lruvec *lruvec;
	unsigned long flags;
	isolate_mode_t mode = ISOLATE_ASYNC_MIGRATE|ISOLATE_UNEVICTABLE;

	/* Lock this zone - USE trylock version! */
	if (!spin_trylock_irqsave(&zone->lru_lock, flags)) {
		mtkpasr_info("\n[%s][%d] Failed to lock this zone!\n\n", __func__, __LINE__);
		return -EAGAIN;
	}

	/* Try to isolate this page */
	if (__isolate_lru_page(page, mode) != 0) {
		spin_unlock_irqrestore(&zone->lru_lock, flags);
		return -EACCES;
	}

	/* Successfully isolated */
	lruvec = mem_cgroup_page_lruvec(page, zone);
	del_page_from_lru_list(page, lruvec, page_lru(page));

	/* Unlock this zone */
	spin_unlock_irqrestore(&zone->lru_lock, flags);

	/* Should we count isolated? - TODO */

	return 0;
}

static int collect_free_pages_for_compacting_banks(struct mtkpasr_bank_cc *bcc);
static struct page *compacting_alloc(struct page *migratepage, unsigned long data, int **result);

/* Collect free pages */
static bool collect_free_pages(struct mtkpasr_bank_cc *bccp, int *bi)
{
	int i = *bi;
	bool ret = false;

	while (collect_free_pages_for_compacting_banks(bccp) < fromlist_count) {
		if (bccp->to_cursor == 0) {
			if (++i == num_banks)
				goto leave;
			bccp->to_bank = i;
			bccp->to_cursor = mtkpasr_banks[i].start_pfn;
		} else {
			mtkpasr_err("Failed to collect free pages!\n");
			BUG();
		}
	}

	/* Successful leave */
	ret = true;

leave:
	/* Update bi(bank_index) */
	*bi = i;
	return ret;
}

/* Release all collected page blocks to buddy allocator */
static void release_collected_pages(void)
{
	struct zone *z;
	unsigned long flags;
	struct page *page, *next;
	unsigned int order = MAX_ORDER - 1;

	z = MTKPASR_ZONE;
	spin_lock_irqsave(&z->lock, flags);
	list_for_each_entry_safe(page, next, &collected_list, lru) {
		list_del(&page->lru);
		UNMARK_PAGE_COLLECTED(page);
		collected_count--;
		/* Add to tail!! */
		list_add_tail(&page->lru, &z->free_area[order].free_list[MIGRATE_MTKPASR]);
		__mod_zone_page_state(z, NR_FREE_PAGES, 1UL << order);
		z->free_area[order].nr_free++;
	}
	spin_unlock_irqrestore(&z->lock, flags);

	BUG_ON(collected_count != 0);
	mtkpasr_log("Collected_count[%u]\n", collected_count);
}

/* Shrink all mtkpasr memory */
void shrink_mtkpasr_all(void)
{
	int i;
	struct page *page, *end_page;
	int to_be_migrated = 1;
	struct mtkpasr_bank_cc bank_cc;

	/* No valid PASR range */
	if (num_banks <= 0)
		return;

	/* Release all collected page blocks */
	release_collected_pages();

	/* Go through all banks */
	for (i = 0; i < num_banks; i++)
		shrink_mafl_all(i);

	/* Move all LRU pages from normal to high */
	bank_cc.to_bank = i = 0;
	bank_cc.to_cursor = mtkpasr_banks[i].start_pfn;
	page = pfn_to_page(mtkpasr_start_pfn);
	end_page = pfn_to_page(mtkpasr_migration_end);
	while (end_page < page) {
		/* To isolate */
		if (PAGE_INUSED(end_page)) {
			if (!mtkpasr_isolate_page(end_page)) {
				list_add(&end_page->lru, &fromlist);
				++fromlist_count;
				++to_be_migrated;
			}
			/* To migrate */
			if ((to_be_migrated % MTKPASR_CHECK_MIGRATE) == 0) {
				/* Try to collect free pages */
				if (!collect_free_pages(&bank_cc, &i))
					goto done;
				/* Start migration */
				if (MIGRATE_PAGES(&fromlist, compacting_alloc, 0) != 0)
					putback_lru_pages(&fromlist);
				fromlist_count = 0;
			}
		} else {
			end_page += ((1 << PAGE_ORDER(end_page)) - 1);
		}
		/* Next one */
		end_page++;
	}

done:
	/* Clear fromlist */
	if (fromlist_count != 0) {
		putback_lru_pages(&fromlist);
		fromlist_count = 0;
	}

	/* Clear tolist */
	putback_free_pages();
}

void shrink_mtkpasr_late_resume(void)
{
	/* Check whether it is an early resume (No MTKPASR is triggered) */
	if (!is_mtkpasr_triggered())
		return;

	/* Reset ops invariant */
	mtkpasr_ops_invariant = 0;

	/* Clear triggered */
	clear_mtkpasr_triggered();
}

/*
 * Scan Bank Information & disable its SR or DPD full rank if possible. It should be called after syscore_suspend
 * - sr , indicate which segment will be SR-offed.
 *   dpd, indicate which package will be dpd.
 *
 * Return - MTKPASR_WRONG_STATE
 *          MTKPASR_GET_WAKEUP
 *	    MTKPASR_SUCCESS
 */
enum mtkpasr_phase mtkpasr_disablingSR(u32 *sr, u32 *dpd)
{
	enum mtkpasr_phase result = MTKPASR_SUCCESS;
	int i, j;
	int enter_dpd = 0;
	u32 banksr = 0x0;	/* From SPM's specification, 0 means SR-on, 1 means SR-off */
	bool keep_ops = true;

	/* Reset SR */
	*sr = banksr;

	/* Sanity Check */
	if (mtkpasr_status != MTKPASR_DISABLINGSR) {
		mtkpasr_err("Error Current State [%d]!\n", mtkpasr_status);
		return MTKPASR_WRONG_STATE;
	}

	/* Any incoming wakeup sources? */
	if (CHECK_PENDING_WAKEUP) {
		mtkpasr_log("Pending Wakeup Sources!\n");
		return MTKPASR_GET_WAKEUP;
	}

	/* Scan banks */
	compute_bank_inused(0);

	for (i = 0; i < num_ranks; i++) {
		mtkpasr_ranks[i].inused = 0;
		for (j = mtkpasr_ranks[i].start_bank; j <= mtkpasr_ranks[i].end_bank; j++) {
			if (mtkpasr_banks[j].inused != 0)
				++mtkpasr_ranks[i].inused;
		}
	}

	/* Check whether a full rank is cleared */
	for (i = 0; i < num_ranks; i++) {
		if (mtkpasr_ranks[i].inused == 0) {
			mtkpasr_info("DPD!\n");
			/* Set MTKPASR_DPDON */
			mtkpasr_ranks[i].inused = MTKPASR_DPDON;
			/* Set all related banks as MTKPASR_RDPDON */
			for (j = mtkpasr_ranks[i].start_bank; j <= mtkpasr_ranks[i].end_bank; j++) {
				/* Test whether it can be removed from buddy temporarily - MAFL */
				remove_bank_from_buddy(j);
				/* DPD mode */
				mtkpasr_banks[j].inused = MTKPASR_RDPDON;
				/* Disable its SR */
				banksr = banksr | (0x1 << (mtkpasr_banks[j].segment /*& MTKPASR_SEGMENT_CH0*/));
			}
			enter_dpd = 1;
		}
	}

	/* Check whether "other" banks are cleared */
	for (i = 0; i < num_banks; i++) {
		if (mtkpasr_banks[i].inused == 0) {
			/* Set MTKPASR_SROFF */
			mtkpasr_banks[i].inused = MTKPASR_SROFF;
			/* Disable its SR */
			banksr = banksr | (0x1 << (mtkpasr_banks[i].segment /*& MTKPASR_SEGMENT_CH0*/));
			/* Test whether it can be removed from buddy temporarily - MAFL */
			remove_bank_from_buddy(i);
			mtkpasr_info("SPM SR/OFF[%d]!\n", i);
		} else {
			/* Collect remaining free page blocks */
			if (mtkpasr_banks[i].inused <= MTKPASR_INUSED)
				remove_page_from_buddy(i);

			mtkpasr_log("Bank[%d] %s[%d]!\n", i,
					(mtkpasr_banks[i].inused == MTKPASR_RDPDON) ? "RDPDON":"inused",
					mtkpasr_banks[i].inused);
		}

		/* To check whether we should do aggressive PASR SW in the future(no external compression) */
		if (keep_ops) {
			if (mtkpasr_banks[i].comp_pos != 0)
				keep_ops = false;
		}
	}

	/* Go to MTKPASR_ON state if success */
	if (result == MTKPASR_SUCCESS) {
		mtkpasr_status = enter_dpd ? MTKPASR_DPD_ON : MTKPASR_ON;
		*sr = banksr;
	}

	/* Update strategy control - MAFL */
	if (before_mafl_count == mafl_total_count) {	/* Ops-invariant */
		/* It hints not to do ops */
		if (!keep_ops)
			mtkpasr_ops_invariant = KEEP_NO_OPS;
		/* Check whether it is hard to apply PASR :( */
		if (mtkpasr_ops_invariant != KEEP_NO_OPS) {
			++mtkpasr_ops_invariant;
			if (mtkpasr_ops_invariant > MAX_NO_OPS_INVARIANT)
				mtkpasr_ops_invariant = MAX_OPS_INVARIANT;
		}
	} else {
		mtkpasr_ops_invariant = 0;
	}
	prev_mafl_count = mafl_total_count;

	mtkpasr_log("Ops_invariant[%u] result [%s] mtkpasr_status [%s]\n", mtkpasr_ops_invariant,
			(result == MTKPASR_SUCCESS) ? "MTKPASR_SUCCESS" : "MTKPASR_FAIL",
			(enter_dpd == 1) ? "MTKPASR_DPD_ON" : "MTKPASR_ON");

	return result;
}

static struct page *compacting_alloc(struct page *migratepage, unsigned long data, int **result)
{
	struct page *page = NULL;

	if (!list_empty(&tolist)) {
		page = list_entry(tolist.next, struct page, lru);
		list_del(&page->lru);
		--tolist_count;
	}

	return page;
}

/* Collect free pages - Is it too long?? */
static int collect_free_pages_for_compacting_banks(struct mtkpasr_bank_cc *bcc)
{
	struct page *page, *end_page;
	struct zone *z;
	unsigned long flags;

	/* Sanity check - 20131126 */
	if (bcc->to_cursor == 0)
		return tolist_count;

	/* We have enough free pages */
	if (tolist_count >= fromlist_count)
		return tolist_count;

	/* To gather free pages */
	page = pfn_to_page(bcc->to_cursor);
	z = page_zone(page);
	end_page = pfn_to_page(mtkpasr_banks[bcc->to_bank].end_pfn);
	while (page < end_page) {
		int checked;

		/* Lock this zone */
		spin_lock_irqsave(&z->lock, flags);
		/* Find free pages */
		if (PAGE_INUSED(page)) {
			spin_unlock_irqrestore(&z->lock, flags);
			++page;
			continue;
		}

		checked = pasr_find_free_page(page, &tolist);
		/* Unlock this zone */
		spin_unlock_irqrestore(&z->lock, flags);

		/* Update checked */
		tolist_count += checked;
		page += checked;

		/* Update to_cursor & inused */
		bcc->to_cursor += (page - pfn_to_page(bcc->to_cursor));
		BANK_INUSED(bcc->to_bank) += checked;

		/* Enough free pages? */
		if (tolist_count >= fromlist_count)
			break;

		/* Is to bank full? */
		if (BANK_INUSED(bcc->to_bank) == mtkpasr_banks[bcc->to_bank].valid_pages) {
			bcc->to_cursor = 0;
			break;
		}
	}

	if (page == end_page) {
		mtkpasr_info("\"To\" bank[%d] is full!\n", bcc->to_bank);
		bcc->to_cursor = 0;
	}

	return tolist_count;
}

/* Do isolation for bank compaction */
static int compacting_isolation(unsigned long *fc, unsigned long ec, int from)
{
	int ret = 0, file_isolated = 0, anon_isolated = 0;
	unsigned long spfn = *fc, epfn = *fc + MTKPASR_CHECK_MIGRATE;
	struct zone *zone;
	struct page *page;

	/* Set boundary */
	if (epfn > ec)
		epfn = ec;

	/* Scan inuse pages */
	while (spfn < epfn) {
		page = pfn_to_page(spfn);
		if (PAGE_INUSED(page)) {
			/* Check whether this page is compressed */
			if (check_if_compressed(mtkpasr_banks[from].comp_start, mtkpasr_banks[from].comp_end, spfn)) {
				++spfn;
				continue;
			}
			/* To isolate it */
			if (!mtkpasr_isolate_page(page)) {
				list_add(&page->lru, &fromlist);
				++fromlist_count;
				++ret;
				--BANK_INUSED(from);
				/* Accumulate isolated */
				ACCUMULATE_ISOLATED();
			} else {
				/* This bank can't be cleared! */
				mtkpasr_log("(BC) Bank[%d] can't be cleared!\n", from);
				ret = -1;
				break;
			}
		} else {
			spfn += ((1 << PAGE_ORDER(page)) - 1);
		}
		/* Update scan base */
		++spfn;
	}

	/* Update cursor */
	page = pfn_to_page(*fc);
	zone = page_zone(page);
	*fc = spfn;

	/* Update isolated */
	UPDATE_ISOLATED();

	return ret;
}

/*
 * Migrate pages from "from" to "to"
 * =0, success
 * >0, success but no new free bank generated
 *     (Above two conditions will update cursors.)
 * <0, fail(only due to CHECK_PENDING_WAKEUP)
 */
static int compacting_banks(int from, int to, unsigned long *from_cursor, unsigned long *to_cursor)
{
	unsigned long fc = *from_cursor;
	unsigned long ec = mtkpasr_banks[from].end_pfn;
	int to_be_migrated = 0;
	int ret = 0;
	struct mtkpasr_bank_cc bank_cc = {
		.to_bank = to,
		.to_cursor = *to_cursor,
	};

	/* Any incoming wakeup sources? */
	if (CHECK_PENDING_WAKEUP) {
		mtkpasr_log("Pending Wakeup Sources!\n");
		return -EBUSY;
	}

	/* Do reset */
	if (list_empty(&fromlist))
		fromlist_count = 0;

	if (list_empty(&tolist))
		tolist_count = 0;

	/* Migrate MTKPASR_CHECK_MIGRATE pages per batch */
	while (fc < ec) {
		/* Any incoming wakeup sources? */
		if (CHECK_PENDING_WAKEUP) {
			mtkpasr_log("Pending Wakeup Sources!\n");
			ret = -EBUSY;
			break;
		}
		/* Isolation */
		to_be_migrated = compacting_isolation(&fc, ec, from);
		if (to_be_migrated == -1) {
			ret = -1;
			break;
		}
		/* To migrate */
		if (!list_empty(&fromlist)) {
			if (collect_free_pages_for_compacting_banks(&bank_cc) >= fromlist_count) {
				if (MIGRATE_PAGES(&fromlist, compacting_alloc, 0) != 0) {
					mtkpasr_log("(AC) Bank[%d] can't be cleared!\n", from);
					ret = -1;
					goto next;
				}
				/* Migration is done for this batch */
				fromlist_count = 0;
			} else {
				ret = 1;
				goto next;
			}
		}
		/* Is from bank empty (Earlier leaving condition than (fc == ec)) */
		if (BANK_INUSED(from) == 0) {
			fc = 0;
			if (!list_empty(&fromlist)) {
				ret = 1;
			} else {
				/* A new free bank is generated! */
				ret = 0;
			}
			break;
		}
	}

	/* From bank is scanned completely (Should always be false!) */
	if (fc == ec) {
		mtkpasr_err("Should always be false!\n");
		fc = 0;
		if (!list_empty(&fromlist)) {
			ret = 1;
		} else {
			/* A new free bank is generated! */
			ret = 0;
		}
	}

	/* Complete remaining compacting */
	if (ret > 0 && !list_empty(&fromlist)) {
		if (collect_free_pages_for_compacting_banks(&bank_cc) >= fromlist_count) {
			if (MIGRATE_PAGES(&fromlist, compacting_alloc, 0) != 0) {
				mtkpasr_log("(AC) Bank[%d] can't be cleared!\n", from);
				ret = -1;
				goto next;
			}
			/* Migration is done for this batch */
			fromlist_count = 0;
		} else {
			ret = 1;
			goto next;
		}
	}

next:

	/* Should we put all pages in fromlist back */
	if (ret == -1) {
		/* We should put all pages from fromlist back */
		putback_lru_pages(&fromlist);
		fromlist_count = 0;
		/* We can't clear this bank. Go to the next one! */
		fc = 0;
		ret = 1;
	}

	/* Update cursors */
	*from_cursor = fc;
	*to_cursor = bank_cc.to_cursor;

	return ret;
}

/* Main entry of compacting banks (No compaction on the last one(modem...)) */
static enum mtkpasr_phase mtkpasr_compact_banks(int toget)
{
	int to_be_sorted = num_banks;
	int dsort_banks[to_be_sorted];
	int i, j, tmp, ret;
	unsigned long from_cursor = 0, to_cursor = 0;
	enum mtkpasr_phase result = MTKPASR_SUCCESS;

	/* Any incoming wakeup sources? */
	if (CHECK_PENDING_WAKEUP) {
		mtkpasr_log("Pending Wakeup Sources!\n");
		return MTKPASR_GET_WAKEUP;
	}

	/* Release all collected page blocks */
	release_collected_pages();

	/* Initialization */
	for (i = 0; i < to_be_sorted; ++i)
		dsort_banks[i] = i;

	/* Sorting banks */
	for (i = to_be_sorted; i > 1; --i) {
		for (j = 0; j < i-1; ++j) {
			/* By rank (descending) */
			if (BANK_RANK(dsort_banks[j]) < BANK_RANK(dsort_banks[j+1]))
				continue;
			/* By inused (descending) */
			if (BANK_INUSED(dsort_banks[j]) < BANK_INUSED(dsort_banks[j+1])) {
				tmp = dsort_banks[j];
				dsort_banks[j] = dsort_banks[j+1];
				dsort_banks[j+1] = tmp;
			}
		}
	}

#ifdef CONFIG_MTKPASR_DEBUG
	for (i = 0; i < to_be_sorted; ++i)
		mtkpasr_info("[%d] - (%d) - inused(%d) - rank(%p)\n", i, dsort_banks[i],
				BANK_INUSED(dsort_banks[i]), BANK_RANK(dsort_banks[i]));
#endif

	/* Go through banks */
	i = to_be_sorted - 1;	/* from-bank */
	j = 0;			/* to-bank */
	while (i > j) {
		/* Whether from-bank is empty */
		if (BANK_INUSED(dsort_banks[i]) == 0) {
			--i;
			continue;
		}
		/* Whether to-bank is full */
		if (BANK_INUSED(dsort_banks[j]) == mtkpasr_banks[dsort_banks[j]].valid_pages) {
			++j;
			continue;
		}
		/* Set compacting position if needed */
		if (!from_cursor) {
			from_cursor = mtkpasr_banks[dsort_banks[i]].start_pfn;
			/* Set scan offset for compacting banks - MAFL */
			from_cursor += mtkpasr_banks[dsort_banks[i]].inmafl;
		}
		if (!to_cursor) {
			/* Shrinking (remaining) mafl totally - MAFL */
			shrink_mafl_all(dsort_banks[j]);
			/* Set the destination */
			to_cursor = mtkpasr_banks[dsort_banks[j]].start_pfn;
		}
		/* Start compaction on banks */
		ret = compacting_banks(dsort_banks[i], dsort_banks[j], &from_cursor, &to_cursor);
		if (ret >= 0) {
			if (!from_cursor)
				--i;
			if (!to_cursor)
				++j;
			if (!ret)
				--toget;
			else
				continue;
		} else {
			/* Error occurred! */
			mtkpasr_err("Error occurred during the compacting on banks!\n");
			if (ret == -EBUSY) {
				result = MTKPASR_GET_WAKEUP;
			} else {
				result = MTKPASR_FAIL;
#ifdef CONFIG_MTKPASR_DEBUG
				BUG();
#endif
			}
			break;
		}
		/* Should we stop the compaction! */
		if (!toget)
			break;
	}

	/* Putback & release pages if needed */
	putback_lru_pages(&fromlist);
	fromlist_count = 0;
	putback_free_pages();

	return result;
}


/* Apply compaction on banks */
static enum mtkpasr_phase mtkpasr_compact(void)
{
	int i;
	int no_compact = 0;
	int total_free = 0;
	int free_banks;

	/* Scan banks for inused */
	compute_bank_inused(0);

	/* Any incoming wakeup sources? */
	if (CHECK_PENDING_WAKEUP) {
		mtkpasr_log("Pending Wakeup Sources!\n");
		return MTKPASR_GET_WAKEUP;
	}

	/* Check whether we should do compaction on banks */
	for (i = 0; i < num_banks; ++i) {
		total_free += (mtkpasr_banks[i].valid_pages - mtkpasr_banks[i].inused);
		if (mtkpasr_banks[i].inused == 0)
			++no_compact;
	}

	/* How many free banks we could get */
	free_banks = total_free / pages_per_bank;
	if (no_compact >= free_banks) {
		mtkpasr_info("No need to do compaction on banks!\n");
		/* No need to do compaction on banks */
		return MTKPASR_SUCCESS;
	}

	/* Actual compaction on banks */
	return mtkpasr_compact_banks(free_banks - no_compact);
}

/*
 * Return the number of pages which need to be processed & do reset.
 *
 * src_map	- store the pages' pfns which need to be compressed.
 * start	- Start PFN of current scanned bank
 * end		- End PFN of current scanned bank
 * sorted	- Buffer for recording which bank(by offset) is externally compressed.
 */
static int pasr_scan_memory(void *src_map, unsigned long start, unsigned long end, unsigned long *sorted)
{
	struct page *page, *epage;
	unsigned long *pgmap = (unsigned long *)src_map;
	int need_compressed = 0;

	/* We don't need to go through following loop because there is no page to be processed. */
	if (start == end)
		return need_compressed;

	/* Initialize start/end */
	page = pfn_to_page(start);
	epage = pfn_to_page(end);

	/* Start to scan inuse pages */
	do {
		if (PAGE_INUSED(page)) {
			*pgmap++ = (unsigned long)page;
			++need_compressed;
		} else {
			page += ((1 << PAGE_ORDER(page)) - 1);
		}
	} while (++page < epage);

	/* Sanity check */
	if (page != epage)
		mtkpasr_err("\n\n\nVery bad: inconsistent page scanning!\n\n\n\n\n");

	/* Clear sorted (we only need to clear (end-start) entries.)*/
	if (sorted != NULL)
		memset(sorted, 0, (end-start)*sizeof(unsigned long));

	mtkpasr_info("@@@ start_pfn[0x%lx] end_pfn[0x%lx] - to process[%d] @@@\n", start, end, need_compressed);

	return need_compressed;
}

/* Reset the free range for migration */
static void reset_free_range(void)
{
	mtkpasr_last_scan = mtkpasr_start_pfn - pageblock_nr_pages;
}

/*
 * Helper function for page migration - To avoid fragmentation through mtkpasr_admit_order
 * (Running under IRQ-disabled environment)
 */
static struct page *mtkpasr_alloc_noirq(struct page *migratepage, unsigned long data, int **result)
{
	struct page *page = NULL;

	/* Just disable IRQ */
	local_irq_disable();

retry:
	/* Isolate free pages if necessary */
	if (!list_empty(&tolist)) {
		page = list_entry(tolist.next, struct page, lru);
		list_del(&page->lru);
		--tolist_count;
	} else {
		unsigned long start_pfn, end_pfn;

		/* No admission on page allocation */
		if (unlikely(mtkpasr_admit_order < 0))
			return NULL;

		/* Check whether mtkpasr_last_scan meets the end, need to update allocation status to avoid HWT */
		if (mtkpasr_last_scan < mtkpasr_migration_end) {
			mtkpasr_last_scan = mtkpasr_start_pfn - pageblock_nr_pages;
			mtkpasr_admit_order = -1;
			return NULL;
		}

		/* To collect free pages */
		page = pfn_to_page(mtkpasr_last_scan);
		end_pfn = mtkpasr_last_scan + pageblock_nr_pages;
		for (start_pfn = mtkpasr_last_scan; start_pfn < end_pfn; start_pfn++, page++) {
			int order, checked, checked_1;

			/* Is it a valid pfn */
			if (!pfn_valid_within(start_pfn))
				continue;

			/* Is it in buddy */
			if (PAGE_INUSED(page))
				continue;

			/* Is it ok */
			order = PAGE_ORDER(page);
			checked_1 = (1 << order) - 1;
			if (order > mtkpasr_admit_order)
				goto next_pageblock;

			checked = pasr_find_free_page(page, &tolist);
			tolist_count += checked;

			/* Sanity check */
			BUG_ON(checked != checked_1 + 1);
next_pageblock:
			/* Align start_pfn to the end of this page */
			start_pfn += checked_1;

			/* Align page to the end of this page block */
			page += checked_1;
		}
		/* Update mtkpasr_last_scan*/
		mtkpasr_last_scan -= pageblock_nr_pages;

		/* There may be some free pages, retry it */
		goto retry;
	}

	return page;
}

#ifdef MTKPASR_STATISTICS
/* Just for recording the state of PASR execution */
struct pasrstat {
	int reclaimed;
	int migrated;
	int compressed;
	int nop;
};
#endif

#define ADD_TO_COMP()			\
do {					\
	*comp = (unsigned long)page;	\
	comp++;				\
	nr_to_comp++;			\
} while (0)

/*
 * Try to clean up the page range indicated in pgmap - (Assume all pages in pgmap are in the same zone)
 *
 * It composes of the following 3 steps,
 * 1. Isolation
 * 2. Drop clean page (reclaim)
 * 3. Page migration
 *
 * If there are pages can't be moved out from this range, we will put it into the "comp".
 *
 * Return the number of pages in "comp".
 */
static int clean_up_page_range(unsigned long *pgmap, unsigned long nr_pages, unsigned long *comp, void *todo)
{
	struct zone *zone;
	struct page *page;
	struct lruvec *lruvec;
	unsigned long nr_reclaimed, nr_to_comp = 0;
	int nr_ret = 0, i, file_isolated = 0, anon_isolated = 0;
	LIST_HEAD(checklist);	/* Store isolated LRU pages */
#ifdef MTKPASR_STATISTICS
	int migrated = 0, nop = 0;
	struct pasrstat *stat = (struct pasrstat *)todo;
#endif

	/* Sanity check */
	if (nr_pages == 0)
		return 0;

	/* Just disable IRQ */
	local_irq_disable();

	/* Start processing */
	zone = page_zone((struct page *)pgmap[0]);
	for (i = 0; i < nr_pages; i++) {
		page = (struct page *)pgmap[i];

		/* Try to isolate */
		if (__isolate_lru_page(page, ISOLATE_ASYNC_MIGRATE|ISOLATE_UNEVICTABLE) != 0) {
			if (comp != NULL)
				ADD_TO_COMP();
#ifdef MTKPASR_STATISTICS
			else
				nop++;
#endif
			continue;
		}

		/* Remove it from lru list */
		lruvec = mem_cgroup_page_lruvec(page, zone);
		del_page_from_lru_list(page, lruvec, page_lru(page));
		list_add(&page->lru, &checklist);
#ifdef MTKPASR_STATISTICS
		migrated++;
#endif
		/* Accumulate isolated */
		ACCUMULATE_ISOLATED();
	}

	/* Update isolated */
	UPDATE_ISOLATED();

	/* Drop clean page */
	nr_reclaimed = reclaim_clean_pages_from_list(zone, &checklist);

	/* Page migration */
	nr_ret = MIGRATE_PAGES(&checklist, mtkpasr_alloc_noirq, 0);

	/* Put back remaining pages(original LRUs) to their original LRUs */
	if (nr_ret != 0)
		putback_lru_pages(&checklist);

	/* Update STATISTICS */
#ifdef MTKPASR_STATISTICS
	stat->reclaimed += nr_reclaimed;
	stat->migrated += (migrated - nr_reclaimed - nr_ret);
	stat->nop += (nr_ret + nop);
#endif

	return nr_to_comp;
}

/* Compute maximum safe order for page allocation */
static int pasr_compute_safe_order(void)
{
	struct zone *z = MTKPASR_ZONE;
	int order;
	unsigned long watermark = low_wmark_pages(z);
	/* long free_pages = zone_page_state(z, NR_FREE_PAGES); */

	/* Start from order:1 to make system more robust */
	for (order = 1; order < MAX_ORDER; ++order) {
		if (!zone_watermark_ok(z, order, (watermark + (1 << order)), 0, 0))
			return order - 2;
	}

	return MAX_ORDER - 1;
}

/*
 * Drop, Compress, Migration, Compaction
 *
 * Return - MTKPASR_WRONG_STATE
 *          MTKPASR_GET_WAKEUP
 *	    MTKPASR_SUCCESS
 */
static enum mtkpasr_phase mtkpasr_entering(void)
{
#define SAFE_CONDITION_CHECK() {										\
		/* Is there any incoming ITRs from wake-up sources? If yes, then abort it. */			\
		if (CHECK_PENDING_WAKEUP) {									\
			mtkpasr_log("Pending Wakeup Sources!\n");						\
			ret = -EBUSY;										\
			break;											\
		}												\
		/* Check whether current system is safe! */							\
		if (unlikely(pasr_check_free_safe())) {								\
			mtkpasr_log("Unsafe System Status!\n");							\
			mtkpasr_admit_order = -1;								\
			ret = -1;										\
			break;											\
		}												\
		/* Check whether mtkpasr_admit_order is not negative! */					\
		if (mtkpasr_admit_order < 0) {									\
			mtkpasr_log("mtkpasr_admit_order is negative!\n");					\
			ret = -1;										\
			break;											\
		}												\
	}

	struct mtkpasr *mtkpasr;
	unsigned long bank_start_pfn, bank_end_pfn, *start;
	int ret = 0, current_bank, to_process, to_compress = 0;
	enum mtkpasr_phase result = MTKPASR_SUCCESS;
#ifdef MTKPASR_STATISTICS
	struct pasrstat stat = {0, 0, 0, 0};
#endif

	/* Sanity Check */
	if (mtkpasr_status != MTKPASR_OFF) {
		mtkpasr_err("Error Current State [%d]!\n", mtkpasr_status);
		return MTKPASR_WRONG_STATE;

	} else {
		/* Go to MTKPASR_ENTERING state */
		mtkpasr_status = MTKPASR_ENTERING;
	}

	/* Is any incoming wakeup sources */
	if (CHECK_PENDING_WAKEUP) {
		mtkpasr_log("Pending Wakeup Sources!\n");
		return MTKPASR_GET_WAKEUP;
	}

	/* Set for verification of ops-invariant */
	before_mafl_count = mafl_total_count;

	/* Transition-invariant */
	if (prev_mafl_count == mafl_total_count) {
		if (mtkpasr_no_ops()) {
			/* Go to the next state */
			mtkpasr_status = MTKPASR_DISABLINGSR;
			goto fast_path;
		}
	} else {
		/* Transition is variant. Clear ops-invariant to proceed it.*/
		mtkpasr_ops_invariant = 0;
	}

	/* Do we have extcomp */
	if (extcomp != NULL) {
		for (current_bank = 0; current_bank < num_banks; ++current_bank)
			mtkpasr_banks[current_bank].comp_pos = 0;
	}

	/* Preliminary work before actual PASR SW operation */
	MTKPASR_FLUSH();

	/* Indicate starting bank */
	current_bank = num_banks - 1;

	/* Check whether current system is safe! */
	if (pasr_check_free_safe()) {
		mtkpasr_log("Unsafe System Status!\n");
		mtkpasr_admit_order = -1;
		goto no_safe;
	}

	/* Current admit order */
	mtkpasr_admit_order = pasr_compute_safe_order();
	mtkpasr_info("mtkpasr_admit_order is [%d]\n", mtkpasr_admit_order);

	/* Main thread */
	mtkpasr = &mtkpasr_device[0];

	/* Reset migrate page range */
	reset_free_range();

	/* Reset "CROSS-OPS" variables: extcomp position index, extcomp start & end positions */
	atomic_set(&sloti, -1);

next_bank:
	/* Drop, Migration Phase */
#ifdef MTKPASR_STATISTICS
	memset(&stat, 0, sizeof(struct pasrstat));
#endif

	/* Set start pos at extcomp */
	if (extcomp != NULL)
		mtkpasr_banks[current_bank].comp_start = (s16)to_compress;

	/* Scan MTKPASR-imposed range */
	bank_start_pfn = mtkpasr_banks[current_bank].start_pfn + mtkpasr_banks[current_bank].inmafl;
	bank_end_pfn = mtkpasr_banks[current_bank].end_pfn;
	to_process = pasr_scan_memory(src_pgmap, bank_start_pfn, bank_end_pfn, sorted_for_extcomp);
	start = src_pgmap;
	mtkpasr_info("BANK[%d] - to_process[%d]\n", current_bank, to_process);
	while (to_process > 0) {
		SAFE_CONDITION_CHECK();
#ifdef MTKPASR_STATISTICS
		to_compress += clean_up_page_range(start,
				(to_process >= MTKPASR_CHECK_MIGRATE) ? MTKPASR_CHECK_MIGRATE : to_process,
				extcomp, &stat);
#else
		to_compress += clean_up_page_range(start,
				(to_process >= MTKPASR_CHECK_MIGRATE) ? MTKPASR_CHECK_MIGRATE : to_process,
				extcomp, 0);
#endif
		to_process -= MTKPASR_CHECK_MIGRATE;
		start += MTKPASR_CHECK_MIGRATE;
	}

	/* Check current execution result: pending wakeup source! */
	if (ret == -EBUSY) {
		mtkpasr_log("Failed MTKPASR due to pending wakeup source!\n");
		result = MTKPASR_GET_WAKEUP;
		goto fast_path;
	}

	/* Set end pos at extcomp */
	if (extcomp != NULL)
		mtkpasr_banks[current_bank].comp_end = (s16)to_compress;

	mtkpasr_info(" to_compress[%d]\n", to_compress);

#ifdef MTKPASR_STATISTICS
	mtkpasr_info(" reclaimed[%d] migrated[%d] nop[%d]\n", stat.reclaimed, stat.migrated, stat.nop);
#endif

	/* Process the next bank */
	if (--current_bank >= 0)
		goto next_bank;

	/* Compression Phase - TODO */
	if (!ret && extcomp != NULL)
		mtkpasr_info(" Compression Progress\n");

no_safe:
	/* Go to MTKPASR_DISABLINGSR state if success */
	if (result == MTKPASR_SUCCESS) {
		/*
		 * Migration to non-PASR range may not be complete, so we should compact banks for PASR.
		 * (Implies that there may be some movable pages)
		 */
		if (mtkpasr_admit_order < 0)
			result = mtkpasr_compact();
		/* Go to the next state */
		if (result == MTKPASR_SUCCESS)
			mtkpasr_status = MTKPASR_DISABLINGSR;
		/* This should be called no matter whether it is a successful PASR. IMPORTANT! */
		flush_cache_all();
	}

fast_path:

	/* Putback remaining free pages */
	putback_free_pages();

	mtkpasr_log("result [%s]\n\n",
			(result == MTKPASR_SUCCESS) ? "MTKPASR_SUCCESS" :
			((result == MTKPASR_FAIL) ? "MTKPASR_FAIL" : "MTKPASR_GET_WAKEUP"));

	return result;
}

/******************************/
/* PASR Driver Initialization */
/******************************/

/* Reset */
void __mtkpasr_reset_device(struct mtkpasr *mtkpasr)
{
	mtkpasr->init_done = 0;

	/* Free various per-device buffers */
	if (mtkpasr->compress_workmem != NULL) {
		kfree(mtkpasr->compress_workmem);
		mtkpasr->compress_workmem = NULL;
	}
	if (mtkpasr->compress_buffer != NULL) {
		free_pages((unsigned long)mtkpasr->compress_buffer, 1);
		mtkpasr->compress_buffer = NULL;
	}
}

void mtkpasr_reset_device(struct mtkpasr *mtkpasr)
{
	down_write(&mtkpasr->init_lock);
	__mtkpasr_reset_device(mtkpasr);
	up_write(&mtkpasr->init_lock);
}

void mtkpasr_reset_global(void)
{
	mtkpasr_reset_slots();

	/* Free table */
	kfree(mtkpasr_table);
	mtkpasr_table = NULL;
	mtkpasr_disksize = 0;

	/* Destroy pool */
	zs_destroy_pool(mtkpasr_mem_pool);
	mtkpasr_mem_pool = NULL;
}

int mtkpasr_init_device(struct mtkpasr *mtkpasr)
{
	int ret;

	down_write(&mtkpasr->init_lock);

	if (mtkpasr->init_done) {
		up_write(&mtkpasr->init_lock);
		return 0;
	}

	/* Create working buffer only when we need to do external compression */
	if (extcomp != NULL) {
		mtkpasr->compress_workmem = kzalloc(LZO1X_MEM_COMPRESS, GFP_KERNEL);
		if (!mtkpasr->compress_workmem) {
			/* pr_err("Error allocating compressor working memory!\n"); */
			ret = -ENOMEM;
			goto fail;
		}

		mtkpasr->compress_buffer = (void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO, 1);
		if (!mtkpasr->compress_buffer) {
			/* pr_err("Error allocating compressor buffer space\n"); */
			ret = -ENOMEM;
			goto fail;
		}
	}

	mtkpasr->init_done = 1;
	up_write(&mtkpasr->init_lock);

	pr_debug("Initialization done!\n");
	return 0;

fail:
	__mtkpasr_reset_device(mtkpasr);
	up_write(&mtkpasr->init_lock);
	pr_err("Initialization failed: err=%d\n", ret);
	return ret;
}

/* Adjust 1st rank to exclude Normal zone! */
static void __init mtkpasr_construct_bankrank(void)
{
	int bank, rank, hwrank, prev;
	unsigned long spfn, epfn;

	/******************************/
	/* PASR default imposed range */
	/******************************/

	/* Reset total pfns */
	mtkpasr_total_pfns = 0;

	/* Setup PASR/DPD bank/rank information */
	for (bank = 0, rank = -1, prev = -1; bank < num_banks; ++bank) {
		/* Construct basic bank/rank information */
		hwrank = query_bank_information(bank, &spfn, &epfn, true);
		mtkpasr_banks[bank].start_pfn = spfn;
		mtkpasr_banks[bank].end_pfn = epfn;
		mtkpasr_banks[bank].inused = 0;
		mtkpasr_banks[bank].segment = pasr_bank_to_segment(spfn, epfn);
		mtkpasr_banks[bank].comp_pos = 0;
		if (hwrank >= 0) {
			if (prev != hwrank) {
				++rank;
				/* Update rank */
				mtkpasr_ranks[rank].start_bank = (u16)bank;
				mtkpasr_ranks[rank].hw_rank = hwrank;
				mtkpasr_ranks[rank].inused = 0;
			}
			mtkpasr_banks[bank].rank = &mtkpasr_ranks[rank];
			mtkpasr_ranks[rank].end_bank = (u16)bank;		/* Update rank */
			prev = hwrank;
		} else {
			mtkpasr_banks[bank].rank = NULL;			/* No related rank! */
		}

		/* Mark it As Free by removing pages from buddy allocator to its List - MAFL */
		INIT_LIST_HEAD(&mtkpasr_banks[bank].mafl);
		mtkpasr_banks[bank].inmafl = 0;

		/* SIMPLE adjustment on banks to exclude invalid PFNs */
		spfn = mtkpasr_banks[bank].start_pfn;
		epfn = mtkpasr_banks[bank].end_pfn;

		/* Find out the 1st valid PFN */
		while (spfn < epfn) {
			if (pfn_valid(spfn)) {
				mtkpasr_banks[bank].start_pfn = spfn++;
				break;
			}
			++spfn;
		}

		/* From spfn, to find out the 1st invalid PFN */
		while (spfn < epfn) {
			if (!pfn_valid(spfn)) {
				mtkpasr_banks[bank].end_pfn = spfn;
				break;
			}
			++spfn;
		}

		/* Update valid_pages */
		mtkpasr_banks[bank].valid_pages = mtkpasr_banks[bank].end_pfn - mtkpasr_banks[bank].start_pfn;

		/* To fix mtkpasr_total_pfns(only contain valid pages) */
		mtkpasr_total_pfns += mtkpasr_banks[bank].valid_pages;

		mtkpasr_info("Bank[%d] - start_pfn[0x%lx] end_pfn[0x%lx] valid_pages[%u] rank[%p]\n",
				bank, mtkpasr_banks[bank].start_pfn, mtkpasr_banks[bank].end_pfn,
				mtkpasr_banks[bank].valid_pages, mtkpasr_banks[bank].rank);
	}

	/* To compute average pages per bank */
	pages_per_bank = mtkpasr_total_pfns / num_banks;

	/**************************/
	/* PASR non-imposed range */
	/**************************/

	/* Try to remove some pages from buddy to enhance the PASR performance - MAFL */
	compute_bank_inused(0);

	/* Reserve all first */
	for (bank = 0; bank < num_banks; bank++) {
		if (mtkpasr_banks[bank].inused == 0) {
			remove_bank_from_buddy(bank);
			mtkpasr_info("(+)bank[%d]\n", bank);
		} else {
			mtkpasr_info("(-)bank[%d] inused[%u]\n", bank, mtkpasr_banks[bank].inused);
		}
	}

	prev_mafl_count = mafl_total_count;

	/* Sanity check - confirm all pages in MTKPASR banks are MIGRATE_MTKPASR */
	rank = 0;
	for (bank = 0; bank < num_banks; bank++) {
		spfn = mtkpasr_banks[bank].start_pfn;
		epfn = mtkpasr_banks[bank].end_pfn;
		for (; spfn < epfn; spfn += pageblock_nr_pages)
			if (!is_migrate_mtkpasr(get_pageblock_migratetype(pfn_to_page(spfn))))
				rank++;
	}

	if (rank != 0)
		mtkpasr_info("\n\n\n[%s][%d]: There is non-MIGRATE_MTKPASR page in MTKPASR range!!!\n\n\n",
				__func__, __LINE__);

	mtkpasr_info("Non-MIGRATE_MTKPASR pages in MTKPASR range [%d]\n", rank);
}

#ifdef CONFIG_PM
static int mtkpasr_pm_event(struct notifier_block *notifier, unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	/* Going to hibernate */
	case PM_HIBERNATION_PREPARE:
		/* MTKPASR off */
		pm_in_hibernation = true;
		return NOTIFY_DONE;
	/* Hibernation finished */
	case PM_POST_HIBERNATION:
		/* MTKPASR on */
		pm_in_hibernation = false;
		return NOTIFY_DONE;
	/* Going to suspend the system */
	case PM_SUSPEND_PREPARE:
		mtkpasr_info("PM_SUSPEND_PREPARE\n");
		mtkpasr_phaseone_ops_internal();
		return NOTIFY_DONE;
	/* Suspend finished
	case PM_POST_SUSPEND:
		mtkpasr_info("PM_POST_SUSPEND\n");
		shrink_mtkpasr_late_resume();
		return NOTIFY_DONE;
	*/
	}
	return NOTIFY_OK;
}

static struct notifier_block mtkpasr_pm_notifier_block = {
	.notifier_call = mtkpasr_pm_event,
	.priority = 0,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
/* Early suspend/resume callbacks & descriptor */
static void mtkpasr_early_suspend(struct early_suspend *h)
{
	mtkpasr_info("\n");
}

static void mtkpasr_late_resume(struct early_suspend *h)
{
	mtkpasr_info("\n");
	shrink_mtkpasr_late_resume();
}

static struct early_suspend mtkpasr_early_suspend_desc = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1,
	.suspend = mtkpasr_early_suspend,
	.resume = mtkpasr_late_resume,
};
#else /* !CONFIG_HAS_EARLYSUSPEND */
/* FB event notifier */
static int mtkpasr_fb_event(struct notifier_block *notifier, unsigned long event, void *data)
{
	struct fb_event *fb_event = data;
	int *blank = fb_event->data;
	int new_status = *blank ? 1 : 0;

	switch (event) {
	case FB_EVENT_BLANK:
		if (new_status == 0) {
			mtkpasr_info("FB_EVENT_BLANK: UNBLANK!\n");
			shrink_mtkpasr_late_resume();
		} else
			mtkpasr_info("FB_EVENT_BLANK: BLANK!\n");

		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block mtkpasr_fb_notifier_block = {
	.notifier_call = mtkpasr_fb_event,
	.priority = 0,
};
#endif

static int mtkpasr_syscore_suspend(void)
{
	enum mtkpasr_phase result;
	int ret = 0;
	int irq_disabled = 0;		/* MTKPASR_FLUSH -> drain_all_pages -> on_each_cpu_mask will enable local irq */

	if (!mtkpasr_enable)
		return 0;

	/* If system is currently in hibernation, just return. */
	if (pm_in_hibernation == true) {
		mtkpasr_log("In hibernation!\n");
		return 0;
	}

	/* Setup SPM wakeup event firstly */
	spm_set_wakeup_src_check();

	/* Check whether we are in irq-disabled environment */
	if (irqs_disabled())
		irq_disabled = 1;

	/* Count for every trigger */
	++mtkpasr_triggered;

	/* It will go to MTKPASR stage */
	current->flags |= PF_MTKPASR | PF_SWAPWRITE;

	/* RAM-to-RAM compression - State change: MTKPASR_OFF -> MTKPASR_ENTERING -> MTKPASR_DISABLINGSR */
	result = mtkpasr_entering();

	/* It will leave MTKPASR stage */
	current->flags &= ~(PF_MTKPASR | PF_SWAPWRITE);

	/* Any pending wakeup source? */
	if (result == MTKPASR_GET_WAKEUP) {
		mtkpasr_restoring();
		mtkpasr_err("PM: Failed to enter MTKPASR\n");
		++failed_mtkpasr;
		ret = -1;
	} else if (result == MTKPASR_WRONG_STATE) {
		mtkpasr_reset_state();
		mtkpasr_err("Wrong state!\n");
		++failed_mtkpasr;
	}

	/* Recover it to irq-disabled environment if needed */
	if (irq_disabled == 1) {
		if (!irqs_disabled()) {
			mtkpasr_log("IRQ is enabled! To disable it here!\n");
			arch_suspend_disable_irqs();
		}
	}

	return ret;
}

static void mtkpasr_syscore_resume(void)
{
	enum mtkpasr_phase result;

	if (!mtkpasr_enable)
		return;

	/* If system is currently in hibernation, just return. */
	if (pm_in_hibernation == true) {
		mtkpasr_log("In hibernation!\n");
		return;
	}

	/* RAM-to-RAM decompression - State change: MTKPASR_EXITING -> MTKPASR_OFF */
	result = mtkpasr_exiting();

	if (result == MTKPASR_WRONG_STATE) {
		mtkpasr_reset_state();
		mtkpasr_err("Wrong state!\n");
	} else if (result == MTKPASR_FAIL) {
		mtkpasr_err("\n\n\n Some Fatal Error!\n\n\n");
	}
}

static struct syscore_ops mtkpasr_syscore_ops = {
	.suspend	= mtkpasr_syscore_suspend,
	.resume		= mtkpasr_syscore_resume,
};

static int __init mtkpasr_init_ops(void)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	/* Register early suspend/resume desc */
	register_early_suspend(&mtkpasr_early_suspend_desc);
#else
	/* Register FB notifier */
	if (fb_register_client(&mtkpasr_fb_notifier_block) != 0)
		goto out;
#endif

	/* Register PM notifier */
	if (!register_pm_notifier(&mtkpasr_pm_notifier_block))
		register_syscore_ops(&mtkpasr_syscore_ops);
	else
		mtkpasr_err("Failed to register pm notifier block\n");

#ifndef CONFIG_HAS_EARLYSUSPEND
out:
#endif
	return 0;
}
#endif

/* mtkpasr initcall */
static int __init mtkpasr_init(void)
{
	int ret;

	/* MTKPASR table of slot information for external compression (MTKPASR_MAX_EXTCOMP) */
	mtkpasr_total_slots = MTKPASR_MAX_EXTCOMP + 1;	/* Leave a slot for buffering */
	mtkpasr_free_slots = mtkpasr_total_slots;
	mtkpasr_disksize = mtkpasr_total_slots << PAGE_SHIFT;
	mtkpasr_table = kcalloc(mtkpasr_total_slots, sizeof(*mtkpasr_table), GFP_KERNEL);
	if (!mtkpasr_table) {
		mtkpasr_err("Error allocating mtkpasr address table\n");
		ret = -ENOMEM;
		goto fail;
	}

	/* Create MTKPASR mempool. */
	mtkpasr_mem_pool = ZS_CREATE_POOL("mtkpasr", GFP_ATOMIC|__GFP_HIGHMEM);
	if (!mtkpasr_mem_pool) {
		mtkpasr_err("Error creating memory pool\n");
		ret = -ENOMEM;
		goto no_mem_pool;
	}

	/* We have only 1 mtkpasr device. */
	mtkpasr_device = kzalloc(sizeof(struct mtkpasr), GFP_KERNEL);
	if (!mtkpasr_device) {
		/* mtkpasr_err("Failed to create mtkpasr_device\n"); */
		ret = -ENOMEM;
		goto out;
	}

	/* Construct memory rank & bank information */
	num_banks = compute_valid_pasr_range(&mtkpasr_start_pfn, &mtkpasr_end_pfn, &num_ranks);
	if (num_banks < 0) {
		mtkpasr_err("No valid PASR range!\n");
		ret = -EINVAL;
		goto free_devices;
	}

	/* To allocate memory for src_pgmap if needed (corresponding to one bank size) */
	if (src_pgmap == NULL) {
		src_pgmap = (void *)__get_free_pages(GFP_KERNEL, get_order(pasrbank_pfns * sizeof(unsigned long)));
		if (src_pgmap == NULL) {
			mtkpasr_err("Failed to allocate (order:%d)(%ld) memory!\n",
					get_order(pasrbank_pfns * sizeof(unsigned long)), pasrbank_pfns);
			ret = -ENOMEM;
			goto free_devices;
		}
	}

	/* To allocate memory for keeping external compression information */
	if (extcomp == NULL && !!MTKPASR_MAX_EXTCOMP) {
		extcomp = (unsigned long *)__get_free_pages(GFP_KERNEL,
				get_order(MTKPASR_MAX_EXTCOMP * sizeof(unsigned long)));
		if (extcomp == NULL) {
			mtkpasr_err("Failed to allocate memory for extcomp!\n");
			ret = -ENOMEM;
			goto no_memory;
		}
		sorted_for_extcomp = (unsigned long *)__get_free_pages(GFP_KERNEL,
				get_order(pasrbank_pfns * sizeof(unsigned long)));
		if (sorted_for_extcomp == NULL) {
			free_pages((unsigned long)extcomp, get_order(MTKPASR_MAX_EXTCOMP * sizeof(unsigned long)));
			mtkpasr_err("Failed to allocate memory for extcomp!\n");
			ret = -ENOMEM;
			goto no_memory;
		}
	}

	/* Basic initialization */
	init_rwsem(&mtkpasr_device->init_lock);
	spin_lock_init(&mtkpasr_device->stat64_lock);
	mtkpasr_device->init_done = 0;

	/* Create working buffers */
	ret = mtkpasr_init_device(mtkpasr_device);
	if (ret < 0) {
		mtkpasr_err("Failed to initialize mtkpasr device\n");
		goto reset_devices;
	}

	/* Create SYSFS interface */
	ret = sysfs_create_group(power_kobj, &mtkpasr_attr_group);
	if (ret < 0) {
		mtkpasr_err("Error creating sysfs group\n");
		goto reset_devices;
	}

	/* mtkpasr_total_pfns = mtkpasr_end_pfn - mtkpasr_start_pfn; */
	mtkpasr_banks = kcalloc(num_banks, sizeof(struct mtkpasr_bank), GFP_KERNEL);
	if (!mtkpasr_banks) {
		/* mtkpasr_err("Error allocating mtkpasr banks information!\n"); */
		ret = -ENOMEM;
		goto free_banks_ranks;
	}
	mtkpasr_ranks = kcalloc(num_ranks, sizeof(struct mtkpasr_rank), GFP_KERNEL);
	if (!mtkpasr_ranks) {
		/* mtkpasr_err("Error allocating mtkpasr ranks information!\n"); */
		ret = -ENOMEM;
		goto free_banks_ranks;
	}

	mtkpasr_construct_bankrank();

	/* Indicate migration end */
	mtkpasr_migration_end = NODE_DATA(0)->node_start_pfn + pasrbank_pfns;

	mtkpasr_info("num_banks[%d] num_ranks[%d] mtkpasr_start_pfn[%ld] mtkpasr_end_pfn[%ld] mtkpasr_total_pfns[%ld]\n",
			num_banks, num_ranks, mtkpasr_start_pfn, mtkpasr_end_pfn, mtkpasr_total_pfns);

#ifdef CONFIG_PM
	/* Register syscore_ops */
	mtkpasr_init_ops();
#endif

	/* Setup others */
	nz = &NODE_DATA(0)->node_zones[ZONE_NORMAL];
	safe_level = low_wmark_pages(nz);
#ifdef CONFIG_HIGHMEM
	safe_level += nz->lowmem_reserve[ZONE_HIGHMEM];
#endif
	safe_level = safe_level >> 2;

	return 0;

free_banks_ranks:
	if (mtkpasr_banks != NULL) {
		kfree(mtkpasr_banks);
		mtkpasr_banks = NULL;
	}
	if (mtkpasr_ranks != NULL) {
		kfree(mtkpasr_ranks);
		mtkpasr_ranks = NULL;
	}
	sysfs_remove_group(power_kobj, &mtkpasr_attr_group);

reset_devices:
	mtkpasr_reset_device(mtkpasr_device);
	if (extcomp != NULL) {
		free_pages((unsigned long)extcomp, get_order(MTKPASR_MAX_EXTCOMP * sizeof(unsigned long)));
		free_pages((unsigned long)sorted_for_extcomp, get_order(pasrbank_pfns * sizeof(unsigned long)));
	}

no_memory:
	free_pages((unsigned long)src_pgmap, get_order(pasrbank_pfns * sizeof(unsigned long)));

free_devices:
	kfree(mtkpasr_device);

out:
	zs_destroy_pool(mtkpasr_mem_pool);

no_mem_pool:
	kfree(mtkpasr_table);
	mtkpasr_table = NULL;
	mtkpasr_disksize = 0;

fail:
	/* Disable MTKPASR */
	mtkpasr_enable = 0;
	mtkpasr_enable_sr = 0;
	num_banks = 0;

	return ret;
}

static void __exit mtkpasr_exit(void)
{
	sysfs_remove_group(power_kobj, &mtkpasr_attr_group);
	if (mtkpasr_device->init_done)
		mtkpasr_reset_device(mtkpasr_device);

	mtkpasr_reset_global();

	kfree(mtkpasr_device);

	pr_debug("Cleanup done!\n");
}
device_initcall_sync(mtkpasr_init);
module_exit(mtkpasr_exit);

MODULE_AUTHOR("MTK");
MODULE_DESCRIPTION("MTK proprietary PASR driver");
