#define KL_LOG KL_PMAP
#include <sys/klog.h>
#include <sys/pmap.h>
#include <sys/_pmap.h>
#include <aarch64/armreg.h>

/*
 * This table describes which access bits need to be set in page table entry
 * for successful memory translation by MMU. Other configurations causes memory
 * fault - see aarch64/trap.c.
 *
 * +--------------+----+------+----+----+
 * |    access    | AF | USER | RO | XN |
 * +==============+====+======+====+====+
 * | user read    | 1  | 1    | *  | *  |
 * +--------------+----+------+----+----+
 * | user write   | 1  | 1    | 0  | *  |
 * +--------------+----+------+----+----+
 * | user exec    | 1  | 1    | *  | 0  |
 * +--------------+----+------+----+----+
 * | kernel read  | 1  | *    | *  | *  |
 * +--------------+----+------+----+----+
 * | kernel write | 1  | *    | 0  | *  |
 * +--------------+----+------+----+----+
 * | kernel exec  | 1  | *    | *  | 0  |
 * +--------------+----+------+----+----+
 */

static const pte_t pte_common = L3_PAGE | ATTR_SH_IS;
static const pte_t pte_noexec = ATTR_XN | ATTR_SW_NOEXEC;

static const pte_t vm_prot_map[] = {
  [VM_PROT_NONE] = pte_noexec | pte_common,
  [VM_PROT_READ] =
    ATTR_AP_RO | ATTR_SW_READ | ATTR_AF | pte_noexec | pte_common,
  [VM_PROT_WRITE] =
    ATTR_AP_RW | ATTR_SW_WRITE | ATTR_AF | pte_noexec | pte_common,
  [VM_PROT_READ | VM_PROT_WRITE] = ATTR_AP_RW | ATTR_SW_READ | ATTR_SW_WRITE |
                                   ATTR_AF | pte_noexec | pte_common,
  [VM_PROT_EXEC] = ATTR_AF | pte_common,
  [VM_PROT_READ | VM_PROT_EXEC] =
    ATTR_AP_RO | ATTR_SW_READ | ATTR_AF | pte_common,
  [VM_PROT_WRITE | VM_PROT_EXEC] =
    ATTR_AP_RW | ATTR_SW_WRITE | ATTR_AF | pte_common,
  [VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXEC] =
    ATTR_AP_RW | ATTR_SW_READ | ATTR_SW_WRITE | ATTR_AF | pte_common,
};

/*
 * Page directory.
 */

pde_t pde_make(int lvl, paddr_t pa) {
  if (lvl == 0)
    return pa | L0_TABLE;
  if (lvl == 1)
    return pa | L1_TABLE;
  return pa | L2_TABLE;
}

/*
 * Page table.
 */

pte_t pte_make(paddr_t pa, vm_prot_t prot, unsigned flags) {
  pte_t pte = pa | vm_prot_map[prot];
  if (!(flags & _PMAP_KERNEL)) {
    pte |= ATTR_AP_USER;
    pte &= ~ATTR_AF;
  }
  if (flags & PMAP_NOCACHE)
    return pte | ATTR_IDX(ATTR_NORMAL_MEM_NC);
  if (flags & PMAP_WRITE_THROUGH)
    return pte | ATTR_IDX(ATTR_NORMAL_MEM_WT);
  return pte | ATTR_IDX(ATTR_NORMAL_MEM_WB);
}

pte_t pte_protect(pte_t pte, vm_prot_t prot) {
  return vm_prot_map[prot] |
         (pte & ~(ATTR_AP_MASK | ATTR_XN | ATTR_SW_FLAGS | ATTR_AF)) |
         (pte & ATTR_AP_USER); /* This flag shouldn't be cleaned */
}

/*
 * Physical map management.
 */

void pmap_md_activate(pmap_t *umap) {
  uint64_t tcr = READ_SPECIALREG(TCR_EL1);

  if (umap == NULL) {
    WRITE_SPECIALREG(TCR_EL1, tcr | TCR_EPD0);
  } else {
    uint64_t ttbr0 = ((uint64_t)umap->asid << ASID_SHIFT) | umap->pde;
    WRITE_SPECIALREG(TTBR0_EL1, ttbr0);
    WRITE_SPECIALREG(TCR_EL1, tcr & ~TCR_EPD0);
  }
}

void pmap_md_bootstrap(pde_t *pd __unused) {
  dmap_paddr_base = 0;
  dmap_paddr_end = DMAP_SIZE;
}

/*
 * Direct map.
 */

void *phys_to_dmap(paddr_t addr) {
  assert((addr >= dmap_paddr_base) && (addr < dmap_paddr_end));
  return (void *)(addr - dmap_paddr_base) + DMAP_BASE;
}
