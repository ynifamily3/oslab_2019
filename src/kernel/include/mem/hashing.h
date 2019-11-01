#ifndef __HASHING_H__
#define __HASHING_H__

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <type.h>
#include <proc/proc.h>
#include <ssulib.h>

#define SLOT_NUM 4							// The number of slots in a bucket
#define CAPACITY 256						// level hash table capacity

typedef struct entry{ // 한 슬롯에 들어있는 것.
    uint32_t key;   // 
    uint32_t value;
} entry;

typedef struct level_bucket
{
    uint8_t token[SLOT_NUM]; // 한 버킷에 토큰 4개 (사용중인지 아닌지 기록하는 용도)
    entry slot[SLOT_NUM]; // 슬롯 4개가 있음.
} level_bucket;

typedef struct level_hash {
    level_bucket top_buckets[CAPACITY]; // top레벨 버킷 수 256개
    level_bucket bottom_buckets[CAPACITY / 2]; // bottom레벨 버킷 수 128개
} level_hash;

level_hash hash_table; // 레벨 해쉬 테이블

void init_hash_table(void);

void insert_hash_table(uint32_t V_address);
void delete_hash_table(uint32_t V_address);

uint32_t F_IDX(uint32_t addr, uint32_t capacity);	// Get first index to use at table
uint32_t S_IDX(uint32_t addr, uint32_t capacity);	// Get second index to use at table

#endif