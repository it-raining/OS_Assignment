// #ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Memory management unit mm/mm.c
 */

#include "../include/mm.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * init_pte - Initialize PTE entry
 */
int init_pte(uint32_t *pte,
             int pre,    // present
             int fpn,    // FPN
             int drt,    // dirty
             int swp,    // swap
             int swptyp, // swap type
             int swpoff) // swap offset
{
  if (pre != 0) {
    if (swp == 0) { // Non swap ~ page online
      if (fpn == 0)
        return -1; // Invalid setting

      /* Valid setting with FPN */
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
    } else { // page swapped
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
      SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
    }
  }

  return 0;
}

/*
 * pte_set_swap - Set PTE entry for swapped page
 * @pte    : target page table entry (PTE)
 * @swptyp : swap type
 * @swpoff : swap offset
 */
/*
   cập nhật một Page Table Entry (PTE) để đánh dấu trang là đã bị swap
   và lưu trữ thông tin về loại swap và vị trí (offset) trong vùng swap.
*/
int pte_set_swap(uint32_t *pte, int swptyp, int swpoff) {
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
  SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);

  return 0;
}

/*
 * pte_set_swap - Set PTE entry for on-line page
 * @pte   : target page table entry (PTE)
 * @fpn   : frame page number (FPN)
 */
int pte_set_fpn(uint32_t *pte, int fpn) {
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);

  return 0;
}

/*
 * vmap_page_range - map a range of page at aligned address
 */
int vmap_page_range(struct pcb_t *caller, int addr, int pgnum,
                    struct framephy_struct *frames,
                    struct vm_rg_struct *ret_rg) {
  // uint32_t * pte = malloc(sizeof(uint32_t));
  struct framephy_struct *fpit = malloc(sizeof(struct framephy_struct));
  // int  fpn;
  int pgit = 0;
  int pgn = PAGING_PGN(addr);

  /* TODO: update the rg_end and rg_start of ret_rg */
  ret_rg->rg_start = addr;
  ret_rg->rg_end = addr + (pgnum * PAGING_PAGESZ);
  ret_rg->vmaid = 0;

  /* TODO map range of frame to address space
   *      in page table pgd in caller->mm
   */
  for (; pgit < pgnum; pgit++) {
    if (fpit == NULL) {
      return -1; // Error: insufficient frames
    }
    uint32_t *pte = &caller->mm->pgd[pgn + pgit];
    pte_set_fpn(pte, fpit->fpn);
    fpit = fpit->fp_next;
    enlist_pgn_node(&caller->mm->fifo_pgn, pgn + pgit);
  }
  return 0;
}

/*
 * alloc_pages_range - allocate req_pgnum of frame in ram
 * @caller    : caller
 * @req_pgnum : request page num
 * @frm_lst   : frame list
 */
/*
    Chức năng: Cấp phát req_pgnum khung trang cho một tiến trình
              req_num: số khung trang cần cấp phát
              frm_lst: danh sách các khung trang đã cấp phát
*/
int alloc_pages_range(struct pcb_t *caller, int req_pgnum,
                      struct framephy_struct **frm_lst) {
  int pgit, fpn;
  struct framephy_struct *newfp_str;
  if (req_pgnum > (caller->mram->maxsz /
                   PAGING_PAGESZ)) { // maxsz: kích thước tối đa của bộ nhớ
    return -1;
  }

  for (pgit = 0; pgit < req_pgnum; pgit++) {
    if (MEMPHY_get_freefp(caller->mram, &fpn) == 0) { // Cấp pahst từ bộ nhớ RAM
      newfp_str =
          (struct framephy_struct *)malloc(sizeof(struct framephy_struct));

      newfp_str->fpn = fpn;
      newfp_str->owner = caller->mm;

      // Thêm khung trang vào danh sách các khung đã cấp phát
      if (*frm_lst == NULL) {
        *frm_lst = newfp_str;
      } else {
        newfp_str->fp_next = *frm_lst;
        *frm_lst = newfp_str;
      }
    } else { // Nếu không có khung trong RAM, tiến hành swap vào vùng MEMSWP
      /*Thông tin trang sẽ bị swap*/
      int victim_fpn; // số khung trang vật lý
      int victim_pgn; // Số trang
      int victim_pte; // Page Table Entry (thông tin về trang trong bảng trang
                      // của tiến trình)
      int swpfpn =
          -1; // số khung trong vùng swap mà sẽ nhận dữ liệu từ victim_page

      // Tìm victim_page để swap ra vùng swap
      if (find_victim_page(caller->mm, &victim_pgn) < 0) {
        return -3000;
      }

      // Lấy PTE và FPN của trang nạn nhân
      victim_pte = caller->mm->pgd[victim_pgn];
      victim_fpn = PAGING_FPN(victim_pte);

      // Cấp phát khung trang mới cho swap
      newfp_str =
          (struct framephy_struct *)malloc(sizeof(struct framephy_struct));

      newfp_str->fpn = victim_fpn;
      newfp_str->owner = caller->mm;

      // Thêm khung trang vào danh sách các khung đã cấp phát
      if (*frm_lst == NULL) {
        *frm_lst = newfp_str;
      } else {
        newfp_str->fp_next = *frm_lst;
        *frm_lst = newfp_str;
      }

      // Tiến hành swap trang từ RAM sang vùng swap
      uint32_t pte = caller->mm->pgd[victim_pgn];
      if (MEMPHY_get_freefp(caller->active_mswp, &swpfpn) ==
          0) { // Nếu có khung trống trong swap
        //__swap_cp_page(): sao chép dữ liệu từ khung trang trong RAM
        //(victim_fpn) sang khung trang trong vùng swap (swpfpn)
        __swap_cp_page(caller->mram, victim_fpn, caller->active_mswp, swpfpn);
        pte_set_swap(&pte, 0,
                     swpfpn); // lưu trữ thông tin của khung swap vào PTE
        caller->mm->pgd[victim_pgn] =
            pte; // thay thế PTE cũ với PTE mới đã được cập nhật thông tin swap
      } else { // Nếu không có khung trống trong ram + swap
        /*Out of memeory*/
        return -3000;
      }
    }
  }

  return 0;
}

/*
 * vm_map_ram - do the mapping all vm are to ram storage device
 * @caller    : caller
 * @astart    : vm area start
 * @aend      : vm area end
 * @mapstart  : start mapping point
 * @incpgnum  : number of mapped page
 * @ret_rg    : returned region
 */
int vm_map_ram(struct pcb_t *caller, int astart, int aend, int mapstart,
               int incpgnum, struct vm_rg_struct *ret_rg) {
  struct framephy_struct *frm_lst = NULL;
  int ret_alloc;

  /*@bksysnet: author provides a feasible solution of getting frames
   *FATAL logic in here, wrong behaviour if we have not enough page
   *i.e. we request 1000 frames meanwhile our RAM has size of 3 frames
   *Don't try to perform that case in this simple work, it will result
   *in endless procedure of swap-off to get frame and we have not provide
   *duplicate control mechanism, keep it simple
   */
  ret_alloc = alloc_pages_range(caller, incpgnum, &frm_lst);

  if (ret_alloc < 0 && ret_alloc != -3000)
    return -1;

  /* Out of memory */
  if (ret_alloc == -3000) {
#ifdef MMDBG
    printf("OOM: vm_map_ram out of memory \n");
#endif
    return -1;
  }

  /* it leaves the case of memory is enough but half in ram, half in swap
   * do the swaping all to swapper to get the all in ram */
  vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg);

  return 0;
}

/* Swap copy content page from source frame to destination frame
 * @mpsrc  : source memphy
 * @srcfpn : source physical page number (FPN)
 * @mpdst  : destination memphy
 * @dstfpn : destination physical page number (FPN)
 **/
int __swap_cp_page(struct memphy_struct *mpsrc, int srcfpn,
                   struct memphy_struct *mpdst, int dstfpn) {
  int cellidx;
  int addrsrc, addrdst;
  for (cellidx = 0; cellidx < PAGING_PAGESZ; cellidx++) {
    addrsrc = srcfpn * PAGING_PAGESZ + cellidx;
    addrdst = dstfpn * PAGING_PAGESZ + cellidx;

    BYTE data;
    MEMPHY_read(mpsrc, addrsrc, &data);
    MEMPHY_write(mpdst, addrdst, data);
  }

  return 0;
}

/*
 *Initialize a empty Memory Management instance
 * @mm:     self mm
 * @caller: mm owner
 */
int init_mm(struct mm_struct *mm, struct pcb_t *caller) {
  /*Khởi tạo hai vùng nhớ*/
  struct vm_area_struct *vma0 = malloc(sizeof(struct vm_area_struct));
  struct vm_area_struct *vma1 = malloc(sizeof(struct vm_area_struct));
  /*Khởi tạo bảng trang*/
  mm->pgd = malloc(PAGING_MAX_PGN * sizeof(uint32_t));

  if (!vma0 || !mm->pgd || !vma1)
    return -1;

  for (int i = 0; i < PAGING_MAX_PGN; i++) {
    mm->pgd[i] = 0;
  }

  /* By default the owner comes with at least one vma for DATA */
  vma0->vm_id = 0;
  vma0->vm_start = 0;
  vma0->vm_end = vma0->vm_start;
  vma0->sbrk = vma0->vm_start;
  /*Khởi tạo và thêm vùng nhớ tự  dođầu vào VMA0*/
  struct vm_rg_struct *first_rg = init_vm_rg(vma0->vm_start, vma0->vm_end, 0);
  enlist_vm_rg_node(&vma0->vm_freerg_list, first_rg);

  /*Thiết lập cho heap-VMA1*/
  vma1->vm_id = 1;
#ifdef MM_PAGING_HEAP_GODOWN
  vma1->vm_start = caller->vmemsz;
  vma1->vm_end = vma1->vm_start;
  vma1->sbrk = vma1->vm_start;
#endif
  /*Khởi tạo và thêm vùng nhớ tự  dođầu vào VMA1*/
  struct vm_rg_struct *heap_rg = init_vm_rg(vma1->vm_start, vma1->vm_end, 1);
  enlist_vm_rg_node(&vma1->vm_freerg_list, heap_rg);

  /*Liên kết VMA1 và VMA0*/
  vma0->vm_next = vma1;
  vma1->vm_next = NULL;

  /* Point vma owner backward */
  vma0->vm_mm = mm;
  vma1->vm_mm = mm;

  /* TODO: update mmap */
  mm->mmap = vma0;

  /* Thiết lập danh sách trang trống */
  mm->fifo_pgn = NULL;
  return 0;
}

struct vm_rg_struct *init_vm_rg(int rg_start, int rg_end, int vmaid) {
  struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));

  rgnode->rg_start = rg_start;
  rgnode->rg_end = rg_end;
  rgnode->vmaid = vmaid;
  rgnode->rg_next = NULL;

  return rgnode;
}

int enlist_vm_rg_node(struct vm_rg_struct **rglist,
                      struct vm_rg_struct *rgnode) {
  rgnode->rg_next = *rglist;
  *rglist = rgnode;

  return 0;
}

int enlist_pgn_node(struct pgn_t **plist, int pgn) {
  struct pgn_t *pnode = malloc(sizeof(struct pgn_t));

  pnode->pgn = pgn;
  pnode->pg_next = *plist;
  *plist = pnode;

  return 0;
}

int print_list_fp(struct framephy_struct *ifp) {
  struct framephy_struct *fp = ifp;

  printf("print_list_fp: ");
  if (fp == NULL) {
    printf("NULL list\n");
    return -1;
  }
  printf("\n");
  while (fp != NULL) {
    printf("fp[%d]\n", fp->fpn);
    fp = fp->fp_next;
  }
  printf("\n");
  return 0;
}

int print_list_rg(struct vm_rg_struct *irg) {
  struct vm_rg_struct *rg = irg;

  printf("print_list_rg: ");
  if (rg == NULL) {
    printf("NULL list\n");
    return -1;
  }
  printf("\n");
  while (rg != NULL) {
    printf("rg[%ld->%ld<at>vma=%d]\n", rg->rg_start, rg->rg_end, rg->vmaid);
    rg = rg->rg_next;
  }
  printf("\n");
  return 0;
}

int print_list_vma(struct vm_area_struct *ivma) {
  struct vm_area_struct *vma = ivma;

  printf("print_list_vma: ");
  if (vma == NULL) {
    printf("NULL list\n");
    return -1;
  }
  printf("\n");
  while (vma != NULL) {
    printf("va[%ld->%ld]\n", vma->vm_start, vma->vm_end);
    vma = vma->vm_next;
  }
  printf("\n");
  return 0;
}

int print_list_pgn(struct pgn_t *ip) {
  printf("print_list_pgn: ");
  if (ip == NULL) {
    printf("NULL list\n");
    return -1;
  }
  printf("\n");
  while (ip != NULL) {
    printf("va[%d]-\n", ip->pgn);
    ip = ip->pg_next;
  }
  printf("n");
  return 0;
}

int print_pgtbl(struct pcb_t *caller, uint32_t start, uint32_t end) {
  int pgn_start, pgn_end;
  int pgit;

  if (end == -1) {
    pgn_start = 0;
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, 0);
    end = cur_vma->vm_end;
  }
  pgn_start = PAGING_PGN(start);
  pgn_end = PAGING_PGN(end);

  printf("print_pgtbl: %d - %d\n", start, end);
  if (caller == NULL) {
    printf("NULL caller\n");
    return -1;
  }
  printf("\n");

  for (pgit = pgn_start; pgit < pgn_end; pgit++) {
    printf("%08ld: %08x\n", pgit * sizeof(uint32_t), caller->mm->pgd[pgit]);
  }

  return 0;
}

// #endif
