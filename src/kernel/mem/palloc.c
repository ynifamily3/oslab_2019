#include <mem/palloc.h>
#include <bitmap.h>
#include <type.h>
#include <round.h>
#include <mem/mm.h>
#include <synch.h>
#include <device/console.h>
#include <mem/paging.h>
#include <mem/hashing.h>

/* Page allocator.  Hands out memory in page-size (or
   page-multiple) chunks.  
   */

/* kernel heap page struct */
struct khpage
{
	uint16_t page_type;
	uint16_t nalloc; // 몇 페이지가 연속으로 할당 하는가
	uint32_t used_bit[4];
	struct khpage *next; // 다음 커널 힙 페이지
};

/* free list */
struct freelist
{
	struct khpage *list;
	int nfree;
};

static struct khpage *khpage_list;
static struct freelist freelist;
static uint32_t page_alloc_index;

/* Initializes the page allocator. */
// 페이지 할당자 초기화
void init_palloc(void)
{
	/* Calculate the space needed for the khpage list */
	// 커널 힙 페이지 리스트에 필요한 공간을 계산한다.
	// 커널 힙 페이지 1024개이고, 한 개의 커널 힙 페이지는
	// 32bit * 6의 크기(=24바이트)로 구성되어 있음. (= 24 킬로바이트)
	size_t bm_size = sizeof(struct khpage) * 1024; // 24 * 1024

	/* khpage list alloc */
	// 커널 소스코드 바로 뒤에 커널페이지 리스트를 준비한다. (0x  10 00 00 = 1 Mbyte 바로 뒤에)
	khpage_list = (struct khpage *)(KERNEL_ADDR);

	/* initialize */
	memset((void *)khpage_list, 0, bm_size); // 0x10 00 00 ~ 0x 10 60 00
	// printk("-3- capacity test : %x\n", KERNEL_ADDR + bm_size); // 0x10 60 00
	page_alloc_index = 0; // 마지막을 가리키는 할당 포인터인 것 같다.
	// printk("---->>>> addr of freelist : 0x%x / 0x%x\n", &freelist, freelist);
	// 0x 1c84c / 0x0
	freelist.list = NULL; // 자유로운 리스트 없다고 초기화
	freelist.nfree = 0; // 자유로운 리스트 없다고 초기화
}

/* Obtains and returns a group of PAGE_CNT contiguous free pages.
   */
  // 연속된 페이지 cnt개수 만큼의 자유 페이지를 얻고 리턴한다.
  // 가상주소를 리턴한다.
uint32_t *
palloc_get_multiple(size_t page_cnt)
{
	void *pages = NULL; // 리턴할 페이지 주소
	struct khpage *khpage = freelist.list; // 자유 페이지 블록의 시작점을 봄
	struct khpage *prepage = freelist.list; // 자유 페이지 블록의 시작점을 봄
	size_t page_idx; // 페이지 인덱스

	if (page_cnt == 0) // cnt가 0이면 NULL을 리턴한다.
		return NULL;

	while (khpage != NULL) // 리스트의 끝점에 닿지 않는 이상 무한 반복한다. (혹은 자유 리스트가 없으면 실행되지 않는다.)
	{
		if (khpage->nalloc == page_cnt) // 자유 페이지에 있는 nalloc만큼의 양이 정확하게 존재 시
		{
			// 페이지의 인덱스를 이런 공식으로 구할 수 있음. 페이지는 도대체 어디에 있는가.
			// ( 엘레먼트 위치 - 초기 위치 ) / 구조체 크기
			page_idx = ((uint32_t)khpage - (uint32_t)khpage_list) / sizeof(struct khpage);
			pages = (void *)(VKERNEL_HEAP_START + page_idx * PAGE_SIZE);

			if (prepage == khpage)
			{ // 처음에 hit 한 경우
				freelist.list = khpage->next;
				freelist.nfree--;
				break;
			}
			else
			{ // 그 외에 hit 한 경우
				prepage->next = khpage->next;
				freelist.nfree--;
				break;
			}
		}
		prepage = khpage;
		khpage = khpage->next; // Linked list의 다음 원소를 살펴 봄.
	}

	// 아직도 할당 받지 못 했으면, (자유 페이지가 없는 경우)
	if (pages == NULL)
	{
		//printk("um meoo~~\n");
		// 끝 점에 있는 (초기 값 : 0) 페이지를 나누어 준다. 끝 점 주소로 점프.
		pages = (void *)(VKERNEL_HEAP_START + page_alloc_index * PAGE_SIZE); 
		page_idx = page_alloc_index; // 이건 왜 있는거야?
		page_alloc_index += page_cnt;
	}


	if (pages != NULL)
	{
		memset(pages, 0, PAGE_SIZE * page_cnt); // 할당 받은 페이지의 모든 것을 0으로 채움.
		// 이 부분에서 인터럽트같은게 걸려서 간헐적으로 의도치 않은 주소 점핑 있음.

		// 처음 두 개 (페이지 디렉토리, 페이지 테이블 제외하고 해쉬에서 관리하도록 함) (아마?)
		//printk("first? : 0x%08x\n", pages);
		if ((uint32_t)pages != VKERNEL_HEAP_START && (uint32_t)pages != VKERNEL_HEAP_START + PAGE_SIZE) {
			insert_hash_table((uint32_t)pages); // 해쉬 테이블에 넣기.
		}
	}
	return (uint32_t *)pages; // 할당받은 페이지의 가상 주소를 리턴해 준다.
}

/* Obtains a single free page and returns its address.
   */
uint32_t *
palloc_get_page(void)
{
	return palloc_get_multiple(1);
}

/* Frees the PAGE_CNT pages starting at PAGES. */
void palloc_free_multiple(void *pages, size_t page_cnt)
{
	struct khpage *khpage = freelist.list;
	size_t page_idx = (((uint32_t)pages - VKERNEL_HEAP_START) / PAGE_SIZE);
	// printk("page idx test : %u\n", page_idx); // 2 4 5 (첫 시도의 one_page 1 2 two_pages)
	if (pages == NULL || page_cnt == 0)
		return;

	// 애초에 프리 리스트에 암것도 없으면
	if (khpage == NULL)
	{
		freelist.list = khpage_list + page_idx; // 커널힙페이지 리스트 + 오프셋 (포인터덧셈)
		// printk("first free : 0x%x\n", freelist.list);
		freelist.list->nalloc = page_cnt; // nalloc 을 카운트 수만큼 할당.
		freelist.list->next = NULL;
	}
	else
	{

		while (khpage->next != NULL)
		{ // 다음에 뭔가 있으면 계속 전진한다.
			khpage = khpage->next;
		}

		khpage->next = khpage_list + page_idx; // LL의 마지막에 덧붙인다.

		// printk("free... : 0x%x\n", khpage->next);

		khpage->next->nalloc = page_cnt; //
		khpage->next->next = NULL;
	}

	freelist.nfree++; // 프리 된 개수를 늘림 (LL개수)
	// printk("after nfree : %d\n", freelist.nfree);
	delete_hash_table((uint32_t)pages);
}

/* Frees the page at PAGE. */
void palloc_free_page(void *page)
{
	palloc_free_multiple(page, 1);
}

void palloc_pf_test(void)
{
	printk("------------------------------------\n");
	uint32_t *one_page1 = palloc_get_page(); // 한 페이지를 받아옴 hash value inserted
	// 여기서 음메 두번 울림 그래서 다음 페이지가.... 한 칸 건너 뛰었나 봄.
	// printk("--- > one page allocated : 0x%x / real : 0x%x\n", one_page1, VH_TO_RH(one_page1));
	// printk("content of page : %u\n", *one_page1);
	uint32_t *one_page2 = palloc_get_page(); // 한 페이지를 받아옴 hash value inserted
	// printk("--- > one page allocated : 0x%x / real : 0x%x\n", one_page2, VH_TO_RH(one_page2));
	uint32_t *two_page1 = palloc_get_multiple(2); // 두 페이지를 받아옴 // hash value inserted
	// printk("--- > two page allocated : 0x%x / real : 0x%x\n", two_page1, VH_TO_RH(two_page1));
	uint32_t *three_page;
	printk("one_page1 = %x\n", one_page1); // 한 페이지의 가상 주소
	printk("one_page2 = %x\n", one_page2); // 한 페이지의 가상 주소
	printk("two_page1 = %x\n", two_page1); // 두 페이지의 가상 주소

	printk("=----------------------------------=\n");
	// printk("free all page attempt\n");
	palloc_free_page(one_page1); // 릴리즈
	palloc_free_page(one_page2); // 릴리즈
	palloc_free_multiple(two_page1, 2); // 릴리즈
	// printk("All release finished\n");
	// test next next...
	// printk("firstFree - nalloc : %d / next : 0x%x\n", freelist.list->nalloc, freelist.list->next);
	// printk("secondFree - nalloc : %d / next : 0x%x\n", freelist.list->next->nalloc, freelist.list->next->next);
	// printk("thirdFree - nalloc : %d / next : 0x%x\n", freelist.list->next->next->nalloc, freelist.list->next->next->next);

	one_page1 = palloc_get_page(); // re allocate 
	two_page1 = palloc_get_multiple(2); // re allocate
	one_page2 = palloc_get_page(); // re allocate
	// 결과적으론 아까와 똑같은 곳으로 들어가게 된다.

	printk("one_page1 = %x\n", one_page1);
	printk("one_page2 = %x\n", one_page2);
	printk("two_page1 = %x\n", two_page1);

	printk("=----------------------------------=\n");
	//printk("3 page free x\n");
	palloc_free_multiple(one_page2, 3); // three page free (violation??)
	// 결국 이거를 하면 one_page2 부분 부터 two_page1 말단까지 싹 다 운영체제에 반납됨
	// two_page1 부분을 침범하게 된다.
	// 지금 상황에선 free 된건 없으므로
	//	freelist에 기록을 한다. (freelist 첫번째꺼.)
	// printk("firstFree - nalloc : %d / next : 0x%x\n", freelist.list->nalloc, freelist.list->next);
	// printk(">>>>>>>>>>>><<<<<<<<<<<<,\n");
	one_page2 = palloc_get_page(); // 한 페이지를 얻어옴. (프리 리스트에 없으니  끝에 할당 될듯, page_alloc_index는 오를 듯)
	// printk("one_page2 real address : 0x%x\n", VH_TO_RH(one_page2)); // 0x207000
	three_page = palloc_get_multiple(3); // 세 페이지를 얻어옴. (얘는 적중해서 0x204000 이거 가져올듯)
	// printk("three page real address : 0x%x\n", VH_TO_RH(three_page));

	printk("one_page1 = %x\n", one_page1); // 실주소는 0x202000
	printk("one_page2 = %x\n", one_page2); // 실주소는 0x207000
	printk("three_page = %x\n", three_page); // 실주소는 0x204000
	
	// printk("real one_page1 = %x\n", VH_TO_RH(one_page1));
	// printk("real one_page2 = %x\n", VH_TO_RH(one_page2));
	// printk("real three_page = %x\n", VH_TO_RH(three_page));
	// printk("<********************************>\n");
	// printk("before final free : Freelist.nfree? : %d\n:", freelist.nfree); => 0
	// 할당 되어 있는 것들 해제 테스트
	palloc_free_page(one_page1); // 실주소 0x202000 해제
	palloc_free_page(three_page); // 실주소 0x204000 해제 (1/3split 하여 각각 해제하는 것을 묘사한 듯.)
	three_page = (uint32_t *)((uint32_t)three_page + 0x1000); // 기존 three page + 4k (two_pages 있던 곳) => 0x205000
	// printk("where afet calc ? : 0x%x\n", VH_TO_RH(three_page)); 0x205000
	palloc_free_page(three_page); // 실주소 0x205000 해제 ( 2/3)
	// printk("freed REAL : 0x%x\n", VH_TO_RH(three_page));
	three_page = (uint32_t *)((uint32_t)three_page + 0x1000); // 0x206000
	palloc_free_page(three_page); // 실주소 0x206000 해제 (3/3)
	// printk("freed REAL : 0x%x\n", VH_TO_RH(three_page));
	palloc_free_page(one_page2); // 실주소 0x207000 해제
	// printk("freed REAL : 0x%x\n", VH_TO_RH(one_page2));
	// assume 전부 해제됨
	// printk("after final free : Freelist.nfree? : %d\n", freelist.nfree); // 5
	/*int i;
	struct khpage *cur = freelist.list;
	for (i = 0; i < freelist.nfree; i++) {
		uint32_t page_idx = ((uint32_t)cur - (uint32_t)khpage_list) / sizeof(struct khpage);
		// 2 4 5 6 7
		void *whereReal = (void *)(VKERNEL_HEAP_START + page_idx * PAGE_SIZE);
		printk("nalloc : %u, [%u]:0x%x\n", cur->nalloc, page_idx, VH_TO_RH(whereReal));
		cur = cur->next;
	}*/
}
