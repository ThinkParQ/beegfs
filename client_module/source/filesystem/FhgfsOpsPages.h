/*
 * fhgfs page cache methods
 *
 */

#ifndef FHGFSOPSPAGES_H_
#define FHGFSOPSPAGES_H_

#include <toolkit/FhgfsPage.h>


#define BEEGFS_MAX_PAGE_LIST_SIZE (65535) // Allow a maximum page list size and therefore right now
                                         // also IO size of 65536 pages, so 262144 MiB with 4K pages
                                         // NOTE: *MUST* be larger than INITIAL_FIND_PAGES


extern bool FhgfsOpsPages_initPageListVecCache(void);
extern void FhgfsOpsPages_destroyPageListVecCache(void);

extern int FhgfsOps_readpagesVec(struct file* file, struct address_space* mapping,
   struct list_head* page_list, unsigned num_pages);
extern int FhgfsOpsPages_readpageSync(struct file* file, struct page* page);
#ifdef KERNEL_HAS_READ_FOLIO
extern int FhgfsOps_read_folio(struct file *file, struct folio *folio);
#else
extern int FhgfsOpsPages_readpage(struct file *file, struct page *page);
#endif

#ifdef KERNEL_HAS_FOLIO
extern void FhgfsOpsPages_readahead(struct readahead_control *ractl);
#else
extern int FhgfsOpsPages_readpages(struct file* file, struct address_space* mapping,
   struct list_head* pageList, unsigned numPages);
#endif

extern int FhgfsOpsPages_writepage(struct page *page, struct writeback_control *wbc);
extern int FhgfsOpsPages_writepages(struct address_space* mapping, struct writeback_control* wbc);

static inline void FhgfsOpsPages_incInodeFileSizeOnPagedRead(struct inode* inode, loff_t offset,
   ssize_t readRes);
extern void __FhgfsOpsPages_incInodeFileSizeOnPagedRead(struct inode* inode, loff_t offset,
   size_t readRes);

extern bool FhgfsOpsPages_isShortRead(struct inode* inode, pgoff_t pageIndex,
   bool needInodeRefresh);
extern void FhgfsOpsPages_endReadPage(Logger* log, struct inode* inode,
   struct FhgfsPage* fhgfsPage, int readRes);
extern void FhgfsOpsPages_endWritePage(struct page* page, int writeRes, struct inode* inode);

extern int FhgfsOpsPages_writeBackPage(struct inode *inode, struct page *page);


/**
 * If the meta server has told us for some reason a wrong file-size (i_size) the caller would
 * wrongly discard data beyond i_size. So we are going to correct i_size here.
 *
 * Note: This is the fast inline version, which should almost always only read i_size and then
 *       return. So mostly no need to increase the stack size.
 * */
void FhgfsOpsPages_incInodeFileSizeOnPagedRead(struct inode* inode, loff_t offset, ssize_t readRes)
{
   loff_t i_size = i_size_read(inode);

   if (unlikely(readRes <= 0) )
      return;

   if (unlikely(readRes && (offset + (loff_t)readRes > i_size) ) )
      __FhgfsOpsPages_incInodeFileSizeOnPagedRead(inode, offset, readRes);
}

#endif /* FHGFSOPSPAGES_H_ */
