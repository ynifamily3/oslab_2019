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

uint8_t fill_values(level_bucket *cur_bucket, int i, uint32_t idx, uint32_t key, uint32_t value, int istop)
{
    if (cur_bucket->token[i] == 0) {
        cur_bucket->token[i] = 1;
        cur_bucket->slot[i].key = key;
        cur_bucket->slot[i].value = value;
        if (istop) 
            printk("hash value inserted in top level : idx : %d, key : %d, value : %x\n", idx, key, value);
        else
            printk("hash value inserted in bottom level : idx : %d, key : %d, value : %x\n", idx, key, value);
        return 1;
    }
    return 0;
}

void insert_hash_table(uint32_t V_address)
{
    int i ,j;

    uint32_t idx;
    uint32_t key;
    uint32_t value;

    // 이동하더라도 key와 value의 정합성은 만족해야 한다.

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

    // 첫번째 시도 :  top bucket에 삽입 시도.
    for (i = 0; i < (SLOT_NUM << 1); ++i) {
        // 원투원투 원투원투
        if ( 1 ==
            fill_values(
                ~i&1  ?
                    first_bucket :
                    second_bucket, i >> 1, ~i&1 ?
                        first_hashed  :
                            second_hashed, key, value, 1)
            ) return;
    }

    // 두번째 시도 :  bottom bucket에 삽입 시도.
    for (i = 0; i < (SLOT_NUM << 1); ++i) {
        // 원투원투 원투원투
        if ( 1 ==
            fill_values(
                ~i&1  ?
                    third_bucket :
                    fouth_bucket, i >> 1, ~i&1 ?
                        first_hashed>>1  :
                            second_hashed>>1, key, value, 0)
            ) return;
    }
    // 세번째 시도
    // top 쫒아낼 녀석을 고른다. 다른 해쉬를 적용한 곳에 쫒아내기 시도.
    for (i = 0; i < SLOT_NUM * 2; ++i) {
        if (i % 2 == 0) {
            uint32_t VADR = (uint32_t)RH_TO_VH(first_bucket->slot[i/2].value);
            uint32_t rehash1 = F_IDX(VADR, CAPACITY);
            uint32_t rehash2 = S_IDX(VADR, CAPACITY);
            // first hashed, second hashed와 어느 것에도 같지 않은 곳으로 보낸다.
            // 0 3, 0 7
            if (rehash1 != first_hashed && rehash1 != second_hashed) {
                // rehash1로 보내기
                level_bucket *new_bucket = &(hash_table.top_buckets[rehash1]);
                for (j = 0; j < SLOT_NUM; j++) {
                    if(new_bucket->token[j] == 0) {
                        // 새로운 보금자리로 쫒아냄.
                        new_bucket->token[j] = 1;
                        new_bucket->slot[j].key = first_bucket->slot[i/2].key;
                        new_bucket->slot[j].value = first_bucket->slot[i/2].value;
                        break;
                    }
                }
                if (j != SLOT_NUM) {
                    // 이제 그 빈 자리를 새롭게 들어온 애가 채운다.
                    first_bucket->slot[i/2].key = key;
                    first_bucket->slot[i/2].value = value;
                    return;
                }
            } else {
                // rehash2로 보내기
                level_bucket *new_bucket = &(hash_table.top_buckets[rehash2]);
                for (j = 0; j < SLOT_NUM; j++) {
                    if(new_bucket->token[j] == 0) {
                        // 새로운 보금자리로 쫒아냄.
                        new_bucket->token[j] = 1;
                        new_bucket->slot[j].key = first_bucket->slot[i/2].key;
                        new_bucket->slot[j].value = first_bucket->slot[i/2].value;
                        break;
                    }
                }
                if (j != SLOT_NUM) {
                    // 이제 그 빈 자리를 새롭게 들어온 애가 채운다.
                    first_bucket->slot[i/2].key = key;
                    first_bucket->slot[i/2].value = value;
                    return;
                }
            }
        } else { // i % 2 == 1
            uint32_t VADR = (uint32_t)RH_TO_VH(second_bucket->slot[i/2].value);
            uint32_t rehash1 = F_IDX(VADR, CAPACITY);
            uint32_t rehash2 = S_IDX(VADR, CAPACITY);
            // first hashed, second hashed와 어느 것에도 같지 않은 곳으로 보낸다.
            // 0 3, 0 7
            if (rehash1 != first_hashed && rehash1 != second_hashed) {
                // rehash1로 보내기
                level_bucket *new_bucket = &(hash_table.top_buckets[rehash1]);
                for (j = 0; j < SLOT_NUM; j++) {
                    if(new_bucket->token[j] == 0) {
                        // 새로운 보금자리로 쫒아냄.
                        new_bucket->token[j] = 1;
                        new_bucket->slot[j].key = second_bucket->slot[i/2].key;
                        new_bucket->slot[j].value = second_bucket->slot[i/2].value;
                        break;
                    }
                }
                if (j != SLOT_NUM) {
                    // 이제 그 빈 자리를 새롭게 들어온 애가 채운다.
                    second_bucket->slot[i/2].key = key;
                    second_bucket->slot[i/2].value = value;
                    return;
                }
            } else {
                // rehash2로 보내기
                level_bucket *new_bucket = &(hash_table.top_buckets[rehash2]);
                for (j = 0; j < SLOT_NUM; j++) {
                    if(new_bucket->token[j] == 0) {
                        // 새로운 보금자리로 쫒아냄.
                        new_bucket->token[j] = 1;
                        new_bucket->slot[j].key = second_bucket->slot[i/2].key;
                        new_bucket->slot[j].value = second_bucket->slot[i/2].value;
                        break;
                    }
                }
                if (j != SLOT_NUM) {
                    // 이제 그 빈 자리를 새롭게 들어온 애가 채운다.
                    second_bucket->slot[i/2].key = key;
                    second_bucket->slot[i/2].value = value;
                    return;
                }
            }
        }
    }

    // 네번째 시도
    // bottom 쫒아낼 녀석을 고른다. 다른 해쉬를 적용한 곳에 쫓아내기 시도.
    for (i = 0; i < SLOT_NUM * 2; ++i) {
        if (i % 2 == 0) {
            uint32_t VADR = (uint32_t)RH_TO_VH(third_bucket->slot[i/2].value);
            uint32_t rehash1 = F_IDX(VADR, CAPACITY);
            uint32_t rehash2 = S_IDX(VADR, CAPACITY);
            // first hashed, second hashed와 어느 것에도 같지 않은 곳으로 보낸다.
            // 0 3, 0 7
            if (rehash1 != first_hashed && rehash1 != second_hashed) {
                // rehash1로 보내기
                level_bucket *new_bucket = &(hash_table.bottom_buckets[rehash1>>1]);
                for (j = 0; j < SLOT_NUM; j++) {
                    if(new_bucket->token[j] == 0) {
                        // 새로운 보금자리로 쫒아냄.
                        new_bucket->token[j] = 1;
                        new_bucket->slot[j].key = third_bucket->slot[i/2].key;
                        new_bucket->slot[j].value = third_bucket->slot[i/2].value;
                        break;
                    }
                }
                if (j != SLOT_NUM) {
                    // 이제 그 빈 자리를 새롭게 들어온 애가 채운다.
                    third_bucket->slot[i/2].key = key;
                    third_bucket->slot[i/2].value = value;
                    return;
                }
            } else {
                // rehash2로 보내기
                level_bucket *new_bucket = &(hash_table.bottom_buckets[rehash2>>1]);
                for (j = 0; j < SLOT_NUM; j++) {
                    if(new_bucket->token[j] == 0) {
                        // 새로운 보금자리로 쫒아냄.
                        new_bucket->token[j] = 1;
                        new_bucket->slot[j].key = third_bucket->slot[i/2].key;
                        new_bucket->slot[j].value = third_bucket->slot[i/2].value;
                        break;
                    }
                }
                if (j != SLOT_NUM) {
                    // 이제 그 빈 자리를 새롭게 들어온 애가 채운다.
                    third_bucket->slot[i/2].key = key;
                    third_bucket->slot[i/2].value = value;
                    return;
                }
            }
        } else { // i % 2 == 1
            uint32_t VADR = (uint32_t)RH_TO_VH(fouth_bucket->slot[i/2].value);
            uint32_t rehash1 = F_IDX(VADR, CAPACITY);
            uint32_t rehash2 = S_IDX(VADR, CAPACITY);
            // first hashed, second hashed와 어느 것에도 같지 않은 곳으로 보낸다.
            // 0 3, 0 7
            if (rehash1 != first_hashed && rehash1 != second_hashed) {
                // rehash1로 보내기
                level_bucket *new_bucket = &(hash_table.bottom_buckets[rehash1>>1]);
                for (j = 0; j < SLOT_NUM; j++) {
                    if(new_bucket->token[j] == 0) {
                        // 새로운 보금자리로 쫒아냄.
                        new_bucket->token[j] = 1;
                        new_bucket->slot[j].key = fouth_bucket->slot[i/2].key;
                        new_bucket->slot[j].value = fouth_bucket->slot[i/2].value;
                        break;
                    }
                }
                if (j != SLOT_NUM) {
                    // 이제 그 빈 자리를 새롭게 들어온 애가 채운다.
                    fouth_bucket->slot[i/2].key = key;
                    fouth_bucket->slot[i/2].value = value;
                    return;
                }
            } else {
                // rehash2로 보내기
                level_bucket *new_bucket = &(hash_table.bottom_buckets[rehash2>>1]);
                for (j = 0; j < SLOT_NUM; j++) {
                    if(new_bucket->token[j] == 0) {
                        // 새로운 보금자리로 쫒아냄.
                        new_bucket->token[j] = 1;
                        new_bucket->slot[j].key = fouth_bucket->slot[i/2].key;
                        new_bucket->slot[j].value = fouth_bucket->slot[i/2].value;
                        break;
                    }
                }
                if (j != SLOT_NUM) {
                    // 이제 그 빈 자리를 새롭게 들어온 애가 채운다.
                    fouth_bucket->slot[i/2].key = key;
                    fouth_bucket->slot[i/2].value = value;
                    return;
                }
            }
        }
    }

}

void delete_hash_table(uint32_t V_address)
{
    int i;

    uint32_t idx;
    uint32_t key;
    uint32_t value;

    uint32_t first_hashed = F_IDX((uint32_t)V_address, CAPACITY);
    uint32_t second_hashed = S_IDX((uint32_t)V_address, CAPACITY);
    
    // FSFS FSFS
    level_bucket *first_bucket = &(hash_table.top_buckets[first_hashed]);
    level_bucket *second_bucket = &(hash_table.top_buckets[second_hashed]);
    level_bucket *third_bucket = &(hash_table.bottom_buckets[first_hashed>>1]);
    level_bucket *fouth_bucket = &(hash_table.bottom_buckets[second_hashed>>1]);

    // 테이블 인덱스와 key 값으로 탐색
    // 해당 슬롯의 token을 0으로 변경
    key = ((uint32_t)V_address - VKERNEL_HEAP_START) / PAGE_SIZE;
    value = (uint32_t)VH_TO_RH(V_address);
    
    // 첫번째 시도 :  top bucket에서 찾아봄.
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
                printk("hash value deleted : idx : %d, key : %d, value : %x\n", idx, key, value);
                return;
            }
        }
    }

    // 두번째 시도 :  bottom bucket에서 찾아봄.
    for (i = 0; i < (SLOT_NUM << 1); ++i) {
        if (i%2==0) {
            idx = first_hashed/2;
            if(third_bucket->token[i/2] == 1 && third_bucket->slot[i/2].key == key) {
                third_bucket->token[i/2] = 0;
                printk("hash value deleted : idx : %d, key : %d, value : %x\n", idx, key, value);
                return;
            }
        } else {
            idx = second_hashed/2;
            if(fouth_bucket->token[i/2] == 1 && fouth_bucket->slot[i/2].key == key) {
                fouth_bucket->token[i/2] = 0;
                printk("hash value deleted : idx : %d, key : %d, value : %x\n", idx, key, value);
                return;
            }
        }
    }
/*
    // 이쪽에 없으면 다른 집 갔을 것이다.
    first_hashed = F_IDX((uint32_t)VH_TO_RH(V_address), CAPACITY);
    second_hashed = S_IDX((uint32_t)VH_TO_RH(V_address), CAPACITY);

    first_bucket = &(hash_table.top_buckets[first_hashed]);
    second_bucket = &(hash_table.top_buckets[second_hashed]);
    third_bucket = &(hash_table.bottom_buckets[first_hashed>>1]);
    fouth_bucket = &(hash_table.bottom_buckets[second_hashed>>1]);

    // 세번째 시도 : 새로운 해쉬의 top을 찾음.
    for (i = 0; i < SLOT_NUM * 2; ++i) {
        if (i % 2 == 0) {
            idx = first_hashed;
            if (first_bucket->token[i/2] == 1 && first_bucket->slot[i/2].key == key) {
                first_bucket->token[i/2] = 0;
                printk("hash value deleted : idx : %d, key : %d, value : %x\n", idx, key, value);
                return;
            }
        } else {
            idx = second_hashed;
            if (second_bucket->token[i/2] == 1 && second_bucket->slot[i/2].key == key) {
                second_bucket->token[i/2] = 0;
                printk("hash value deleted : idx : %d, key : %d, value : %x\n", idx, key, value);
                return;
            }
        }
    }

    // 네번째 시도 : 새로운 해쉬의 bottom을 찾음.
    for (i = 0; i < SLOT_NUM * 2; ++i) {
        if (i % 2 == 0) {
            idx = first_hashed / 2;
            if (third_bucket->token[i/2] == 1 && third_bucket->slot[i/2].key == key) {
                third_bucket->token[i/2] = 0;
                printk("hash value deleted : idx : %d, key : %d, value : %x\n", idx, key, value);
                return;
            }
        } else {
            idx = second_hashed / 2;
            if (fouth_bucket->token[i/2] == 1 && fouth_bucket->slot[i/2].key == key) {
                fouth_bucket->token[i/2] = 0;
                printk("hash value deleted : idx : %d, key : %d, value : %x\n", idx, key, value);
                return;
            }
        }
    }
*/
}