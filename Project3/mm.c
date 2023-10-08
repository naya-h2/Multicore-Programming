/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your information in the following struct.
 ********************************************************/
team_t team = {
    /* Your student ID */
    "20211550",
    /* Your full name*/
    "Heewon An",
    /* Your email address */
    "hw7666@sogang.ac.kr",
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/*Basic constants and macros*/
#define WSIZE 4 //1word size
#define DSIZE 8 //double word size
#define CHUNKSIZE (1<<12) //extend heap by this amount

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word*/
#define PACK(size, alloc) ((size) | (alloc)) //header 내용 구성

/* Read and write a word at address p */
//포인터 연산에 사용
#define GET(p) (*(unsigned int *)(p)) //p가 참조하는 word 값을 read
#define PUT(p, val) (*(unsigned int *)(p) = (val)) //p가 참조하는 word에 val 값을 write

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7) //H/F에서 size값 확인
#define GET_ALLOC(p) (GET(p) & 0x1) //H/F에서 state(free or alloc) bit 확인

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char*)(bp) - WSIZE) //bp에서 header 찾아가기
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) //bp에서 footer 찾아가기

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) //bp + 현재 블럭 size = 다음 블럭
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) //bp - 이전 블럭 size = 이전 블럭

//FREE BLOCK끼리 연결을 위한 pointer 연산
#define FREE_PREV(bp) (*(char**)(bp))
#define FREE_NEXT(bp) (*(char**)(bp + WSIZE))

static char *heap_listp = 0;
static char *root = 0; //free list의 root

static void deletion(void *bp){
    //free list에서 해당 block을 delete
    
    if(FREE_PREV(bp) == NULL){ //첫번째 free block이였을 경우
        root = FREE_NEXT(bp); //root를 다음 free block으로 연결
    }
    else{ //첫번째 free block이 아닌 경우
        //내 이전 애의 다음에는 내 다음 애를 연결
        FREE_NEXT(FREE_PREV(bp)) = FREE_NEXT(bp);
	}
	//내 다음 애의 이전에는 내 이전 애를 연결
    FREE_PREV(FREE_NEXT(bp)) = FREE_PREV(bp);

	return;
}

static void insertion(void *bp){
	//free list 맨 앞에 insert
	//bp = prev, bp + WSIZE = next

	//1. bp의 next가 root가 가리키고 있는 block을 가리키도록
	FREE_NEXT(bp) = root;
	
    //2. root가 가리키고 있는 block의 prev를 bp랑 연결
	FREE_PREV(root) = bp;

    //3. prev가 NULL을 가리키도록 (맨 앞이니까)
	FREE_PREV(bp) = NULL;
	
    //4. root가 bp를 가리키도록
	root = bp;
}

static void* coalesce(void* bp)
{
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))) || PREV_BLKP(bp) == bp;
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));

	if (prev_alloc && next_alloc) { //alloc + 나 + alloc
		
	}

	else if (prev_alloc && !next_alloc) { //alloc + 나 + free
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		deletion(NEXT_BLKP(bp));
		PUT(HDRP(bp), PACK(size, 0)); //내 헤더를 먼저 새로운 size로 바꿔주고
		PUT(FTRP(bp), PACK(size, 0)); //새로운 footer로
	}

	else if (!prev_alloc && next_alloc) { //free + 나 + alloc
		size += GET_SIZE(HDRP(PREV_BLKP(bp))); //prev_block size를 더해줌
		deletion(PREV_BLKP(bp));
		PUT(FTRP(bp), PACK(size, 0)); //내 footer 먼저 바꿔줌
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}

	else { //free + 나 + free
		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
		deletion(PREV_BLKP(bp));
		deletion(NEXT_BLKP(bp));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}

    insertion(bp);
	return bp;
}

static void* extend_heap(size_t words) //word = 늘릴 chuck 수
{
	char* bp;
	size_t size; //전체 block의 size

	/* Allocate an even number of words to maintain alignment */
	size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; //block 수를 짝수개로
	size = ALIGN(words) * WSIZE;
	if ((long)(bp = mem_sbrk(size)) == -1)
		return NULL;

	/* Initialize free block header/footer and the epilogue header */
	PUT(HDRP(bp), PACK(size, 0));         /* Free block header */
	PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

	/* Coalesce if the previous block was free */
	return coalesce(bp);
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    //초기 HEAP 구성 ; padding + H + F + E
    if ((heap_listp = mem_sbrk(8*WSIZE)) == (void *)-1)
    	return -1;
    PUT(heap_listp, 0);                          /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */ 
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */ 
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));     /* Epilogue header */
    root = heap_listp + (2 * WSIZE); //prologue를 freelist의 root로 사용

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) 
	    return -1;
    return 0;
}

static void *find_fit(size_t asize)
{
    /* First-fit search */
    void *bp;
    //free block만 검사하도록
    for (bp = root; GET_ALLOC(HDRP(bp)) == 0 ; bp = FREE_NEXT(bp)){
        if (!GET_ALLOC(HDRP(bp)) && asize <= GET_SIZE(HDRP(bp))){
	        return bp;
        }
	}
	
    return NULL;  /* No fit */
}

static void place(void *bp, size_t asize) //split the free list if needed
{
    //bp에 asize만큼 할당할거야 !

    size_t csize = GET_SIZE(HDRP(bp)); //free block size
    
    deletion(bp); //bp를 freelist에서 삭제

    if ((csize - asize) >= (2*DSIZE)) { //2*DSIZE 이상 남으면 split
	    PUT(HDRP(bp), PACK(asize, 1));
	    PUT(FTRP(bp), PACK(asize, 1));
	    bp = NEXT_BLKP(bp);
	    PUT(HDRP(bp), PACK(csize-asize, 0));
	    PUT(FTRP(bp), PACK(csize-asize, 0));
        coalesce(bp); //검사하고 freelist에 추가
    }
    else { 
	    PUT(HDRP(bp), PACK(csize, 1));
	    PUT(FTRP(bp), PACK(csize, 1));
    }
   
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    /*int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
	return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }*/

    size_t asize;      /* Adjusted block size */ //실제로 할당하려는 size
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;      

    /* Ignore spurious requests */
    if (size == 0)
	    return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE) //사용자가 할당하기를 원하는 payload size가 8보다 작은 경우
	    asize = 2*DSIZE; //block의 최소 size로 맞춰준다
    else
	    asize = DSIZE * ((size + (DSIZE)+(DSIZE-1))/DSIZE); //할당할 총 byte 계산(H, F 포함)
    
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
    	place(bp, asize);
	    return bp;
    }

    /* No fit found. Get more memory and place the block*/
    extendsize = MAX(asize, CHUNKSIZE); //다 찾았는데 할당할 만큼의 size 없으면 늘리기
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)  
	    return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void* bp)
{
	//end mmfree
	if (bp == 0) {
		return;
	}

	if (heap_listp == 0) {
        mm_init();
    }

	//begin mmfree
	size_t size = GET_SIZE(HDRP(bp));

	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
	coalesce(bp); //앞뒤 확인하고 free할 때 합치기
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void* mm_realloc(void* ptr, size_t size)
{
	size_t psize, nsize;
	int flag = 0; //할당 가능 or 불가능
	if (heap_listp == 0) {
        mm_init();
    }
	if(ptr == NULL){ //ptr이 null값이면 기존 malloc과 동일
		return mm_malloc(size);
	}
	if(size == 0){ //새로 할당할 size가 0이라는 것은 기존 메모리를 삭제하라는 뜻
		mm_free(ptr); //free하고
		return NULL; //null return
	}
	if(size < 0 || !GET_ALLOC(HDRP(ptr))){ //size가 음수거나 ptr이 할당 안 된 block인 경우
		return NULL;
	}

	psize = GET_SIZE(HDRP(ptr)); //ptr의 기존 size
	//nsize 계산
	if (size <= DSIZE) //사용자가 할당하기를 원하는 payload size가 8보다 작은 경우
	    nsize = 2*DSIZE; //block의 최소 size로 맞춰준다
    else
	    nsize = DSIZE * ((size + (DSIZE)+(DSIZE-1))/DSIZE); //할당할 총 byte 계산(H, F 포함)
	
	//이미 ptr에 할당된 메모리 크기보다 nsize가 작은 경우
	if(nsize < psize){
		flag = 1;
		return ptr; //변화 x
	}

	//바로 다음 block이 free && 현재 block + 다음 block >= 새로 할당할 size
	if(!GET_ALLOC(HDRP(NEXT_BLKP(ptr))) && (GET_SIZE(HDRP(NEXT_BLKP(ptr))) + psize >= nsize)){
		//free list에서 next block 제거 & 합치기
		psize += GET_SIZE(HDRP(NEXT_BLKP(ptr))); //psize + next block size
		deletion(NEXT_BLKP(ptr));
		//alloc bit를 1로 설정
		PUT(HDRP(ptr), PACK(psize, 1)); //내 헤더를 먼저 새로운 size로 바꿔주고
		PUT(FTRP(ptr), PACK(psize, 1)); //새로운 footer로
		return ptr;
	}
	
	//nsize만큼의 block을 찾아서 새로 할당
	void* new = mm_malloc(nsize);
	if(new == NULL) return NULL; //nsize 공간 찾기 실패
	place(new, nsize);
	memcpy(new, ptr, GET_SIZE(HDRP(ptr))); //기존 data 복사하고
	mm_free(ptr); //기존 ptr free
	return new;
	
}
