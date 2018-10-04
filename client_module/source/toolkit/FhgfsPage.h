#ifndef FHGFSPAGE_H_
#define FHGFSPAGE_H_

#include <linux/fs.h>


struct FhgfsPage;
typedef struct FhgfsPage FhgfsPage;


static inline void FhgfsPage_unmapUnlockReleasePage(struct page* page);
static inline void FhgfsPage_unmapUnlockReleaseFhgfsPage(FhgfsPage* this);

static inline loff_t FhgfsPage_getFileOffset(FhgfsPage* this);
static inline void FhgfsPage_zeroPage(FhgfsPage* this);
static inline pgoff_t FhgfsPage_getPageIndex(FhgfsPage* this);


struct FhgfsPage
{
      struct page* page;
      void *data; // real page data, obtained by kmap(page)
      int length; // size of *used* data of this page
};


/**
 * Unmap, unlock and release a (kernel) page
 */
void FhgfsPage_unmapUnlockReleasePage(struct page* page)
{
   kunmap(page);

   unlock_page(page);

   put_page(page);
}


/**
 * Unmap, unlock and release an fhgfs page
 */
void FhgfsPage_unmapUnlockReleaseFhgfsPage(FhgfsPage* this)
{
   FhgfsPage_unmapUnlockReleasePage(this->page);
}


/**
 * Get the offset (within a file) of the page
 */
loff_t FhgfsPage_getFileOffset(FhgfsPage* this)
{
   return page_offset(this->page); // offset within the file
}

/**
 * Zero the data of a page
 */
void FhgfsPage_zeroPage(FhgfsPage* this)
{
   // zero_user_segment() would be optimal, but not available in older kernels
   // zero_user_segment(page, zeroOffset, BEEGFS_PAGE_SIZE);
   // BUT we can use our kmapped Fhgfs_Page directly for zeroing

   memset(this->data, 0, PAGE_SIZE);
}

/**
 * Return the page (in file) index of a page
 */
pgoff_t FhgfsPage_getPageIndex(FhgfsPage* this)
{
   return this->page->index;
}

#endif // FHGFSPAGE_H_
