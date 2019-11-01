#include <device/io.h>
#include <mem/mm.h>
#include <mem/paging.h>
#include <device/console.h>
#include <proc/proc.h>
#include <interrupt.h>
#include <mem/palloc.h>
#include <ssulib.h>
#include <mem/hashing.h>

uint32_t F_IDX(uint32_t addr, uint32_t capacity) {
    return addr % ((capacity / 2) - 1);
}

uint32_t S_IDX(uint32_t addr, uint32_t capacity) {
    return (addr * 7) % ((capacity / 2) - 1) + capacity / 2;
}

void init_hash_table(void)
{
	// TODO : OS_P5 assignment
    // printk("-*-*-*- start init hash table -*-*-*\n");
    memset((void *)&hash_table, 0, sizeof(hash_table));
    // 이제 넣는거 구현하면 되겠다.
}

uint8_t fill_values(level_bucket *cur_bucket, int i, uint32_t idx, uint32_t key, uint32_t value)
{
    if (cur_bucket->token[i] == 0) {
        cur_bucket->token[i] = 1;
        cur_bucket->slot[i].key = key;
        cur_bucket->slot[i].value = value;
        printk("hash value inserted in top level : idx : %d, key : %d, value : %x\n", idx, key, value);
        return 1;
    }
    return 0;
}

void insert_hash_table(uint32_t V_address)
{
    int i;

    uint32_t idx;
    uint32_t key;
    uint32_t value;

    uint32_t first_hashed = F_IDX(V_address, CAPACITY);
    uint32_t second_hashed = S_IDX(V_address, CAPACITY);

    // 첫번째 해쉬 함수 적용한 TOP위치의 버켓 4개중 비어있는 곳에 삽입.
    // FSFS FSFS
    level_bucket *first_bucket = &(hash_table.top_buckets[first_hashed]);
    level_bucket *second_bucket = &(hash_table.top_buckets[second_hashed]);
    level_bucket *third_bucket = &(hash_table.bottom_buckets[first_hashed>>1]);
    level_bucket *fouth_bucket = &(hash_table.bottom_buckets[second_hashed>>1]);
    

    key = ((uint32_t)V_address - VKERNEL_HEAP_START) / PAGE_SIZE;
    value = (uint32_t)VH_TO_RH(V_address);

    // 첫번째 시도 :  top bucket에 시도.
    for (i = 0; i < (SLOT_NUM << 1); ++i) {
        // 원투원투 원투원투
        if ( 1 ==
            fill_values(
                ~i&1  ?
                    first_bucket :
                    second_bucket, i >> 1, ~i&1 ?
                        first_hashed  :
                            second_hashed, key, value)
            ) return;
    }

    // 두번째 시도 : bottom bucket에 시도.

}

void delete_hash_table(uint32_t V_address)
{
    int i;

    uint32_t idx;
    uint32_t key;
    uint32_t value;

    uint32_t first_hashed = F_IDX(V_address, CAPACITY);
    uint32_t second_hashed = S_IDX(V_address, CAPACITY);
    
    // FSFS FSFS
    level_bucket *first_bucket = &(hash_table.top_buckets[first_hashed]);
    level_bucket *second_bucket = &(hash_table.top_buckets[second_hashed]);
    level_bucket *third_bucket = &(hash_table.bottom_buckets[first_hashed>>1]);
    level_bucket *fouth_bucket = &(hash_table.bottom_buckets[second_hashed>>1]);

    // 테이블 인덱스와 key 값으로 탐색
    // 해당 슬롯의 token을 0으로 변경
    key = ((uint32_t)V_address - VKERNEL_HEAP_START) / PAGE_SIZE;
    value = (uint32_t)VH_TO_RH(V_address);
    
    // 첫번째 시도 :  top bucket에 시도.
    for (i = 0; i < (SLOT_NUM << 1); ++i) {
        if (i%2==0) {
            idx = first_hashed;
            if(first_bucket->token[i/2] == 1 && first_bucket->slot[i/2].key == key) {
                first_bucket->token[i/2] = 0;
                printk("hash value deleted : idx : %d, key : %d, value : %x\n", idx, key, value);
                return;
            }
        } else {
            idx = second_hashed;
            if(second_bucket->token[i/2] == 1 && second_bucket->slot[i/2].key == key) {
                second_bucket->token[i/2] = 0;
                printk("2hash value deleted : idx : %d, key : %d, value : %x\n", idx, key, value);
                return;
            }
        }
    }
}