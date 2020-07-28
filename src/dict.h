/* Hash Tables Implementation.
 *
 * This file implements in-memory hash tables with insert/del/replace/find/
 * get-random-element operations. Hash tables will auto-resize if needed
 * tables of power of two in size are used, collisions are handled by
 * chaining. See the source code for more information... :)
 *
 * Copyright (c) 2006-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>

#ifndef __DICT_H
#define __DICT_H

#define DICT_OK 0
#define DICT_ERR 1

/* Unused arguments generate annoying warnings... */
#define DICT_NOTUSED(V) ((void) V)

//哈希表节点，每个dictEntry保存一个键值对
typedef struct dictEntry {
    //键
    void *key;
    //值,union联合，联合共享内存，一个union只能有一个值有效，写入新值会冲去旧值，从共享的地址写入，如果前一个变量地址长，则只会覆盖部分地址，其他地址的值不变
    union {
        //键值对中的值可以是一个任意类型的指针
        void *val;
        //也可以是uint64_t整数
        uint64_t u64;
        //也可以是int64_t整数
        int64_t s64;
        //也可以是一个浮点数
        double d;
    } v;
    //指向相同索引的下一个哈希表节点，解决了哈希冲突（collision）
    struct dictEntry *next;
} dictEntry;

//字典类型，保存了用于操作对象类型键值对的函数
typedef struct dictType {
    //计算哈希值的函数，哈希函数
    uint64_t (*hashFunction)(const void *key);
    //复制键的函数，pirvdata是字典传过来的参数
    void *(*keyDup)(void *privdata, const void *key);
    //复制特定值的函数
    void *(*valDup)(void *privdata, const void *obj);
    //对比特定键的函数
    int (*keyCompare)(void *privdata, const void *key1, const void *key2);
    //销毁特定键的函数
    void (*keyDestructor)(void *privdata, void *key);
    //销毁特定值的函数
    void (*valDestructor)(void *privdata, void *obj);
} dictType;

/* This is our hash table structure. Every dictionary has two of this as we
 * implement incremental rehashing, for the old to the new table.
 * 哈希表，包含了多个键值对，也就是多个哈希表节点 */
typedef struct dictht {
    //指向哈希表节点数组的指针
    dictEntry **table;
    //哈希表大小，表示能容纳多少索引，单位为个数
    unsigned long size;
    //哈希表大小掩码，总等于size-1，用来计算索引值
    unsigned long sizemask;
    //哈希表已有哈希节点个数，注意这里个数不是和size等价的，size是指索引数，哈希表节点数可以远大于索引数。
    unsigned long used;
} dictht;

//字典
typedef struct dict {
    //为创建多态字典而设置的，类型特定函数，redis会为用途不同的字典设置不同的类型特定参数
    dictType *type;
    //为创建多态字典而设置的，私有数据，保存了需要穿给哪些类型特定函数的可选参数
    void *privdata;
    //两个哈希表的数组，一般情况下字典只使用ht[0]的哈希表，ht[1]挚爱对ht[0]进行rehash(重新散列)时候使用
    //当负载因子过大或者过小会引发rehash，进行扩容或者缩容
    dictht ht[2];
    //记录是否在rehash，和rehash的进度，当没有在rehash时候为-1
    long rehashidx; /* rehashing not in progress if rehashidx == -1 */
    //正在运行的迭代器的数量
    unsigned long iterators; /* number of iterators currently running */
} dict;

/* If safe is set to 1 this is a safe iterator, that means, you can call
 * dictAdd, dictFind, and other functions against the dictionary even while
 * iterating. Otherwise it is a non safe iterator, and only dictNext()
 * should be called while iterating. 
 * 迭代器*/
typedef struct dictIterator {
    //被迭代的字典
    dict *d;
    //迭代器当前所指向的哈希表索引位置
    long index;
    //table表示正迭代的哈希表号码，ht[0]或ht[1]。safe表示这个迭代器是否安全。
    int table, safe;
    //entry指向当前迭代的哈希表节点，nextEntry则指向当前节点的下一个节点
    dictEntry *entry, *nextEntry;
    /* unsafe iterator fingerprint for misuse detection. */
    //避免不安全迭代器的指纹标记
    /*
    迭代器分为安全迭代器，和非安全迭代器
*非安全迭代器只能进行Get等读的操作，而安全迭代器则可以进行itreator支持的任何操作。
*由于dict结构中保存了safe itreator的数量，如果数量不为0，是不能进行下一步的rehash
的，因此安全迭代器的存在保证了遍历数据的准确性。
*在非安全迭代器的迭代过程中，会通过fingerprint方法来校验iterator在初始化与释放时
字典的hash值是否一致；如果不一致说明迭代过程中发生了非法操作。*/
    long long fingerprint;
} dictIterator;

typedef void (dictScanFunction)(void *privdata, const dictEntry *de);
typedef void (dictScanBucketFunction)(void *privdata, dictEntry **bucketref);

/* This is the initial size of every hash table */
#define DICT_HT_INITIAL_SIZE     4

/* ------------------------------- Macros ------------------------------------*/
#define dictFreeVal(d, entry) \
    if ((d)->type->valDestructor) \
        (d)->type->valDestructor((d)->privdata, (entry)->v.val)

#define dictSetVal(d, entry, _val_) do { \
    if ((d)->type->valDup) \
        (entry)->v.val = (d)->type->valDup((d)->privdata, _val_); \
    else \
        (entry)->v.val = (_val_); \
} while(0)

#define dictSetSignedIntegerVal(entry, _val_) \
    do { (entry)->v.s64 = _val_; } while(0)

#define dictSetUnsignedIntegerVal(entry, _val_) \
    do { (entry)->v.u64 = _val_; } while(0)

#define dictSetDoubleVal(entry, _val_) \
    do { (entry)->v.d = _val_; } while(0)

#define dictFreeKey(d, entry) \
    if ((d)->type->keyDestructor) \
        (d)->type->keyDestructor((d)->privdata, (entry)->key)

#define dictSetKey(d, entry, _key_) do { \
    if ((d)->type->keyDup) \
        (entry)->key = (d)->type->keyDup((d)->privdata, _key_); \
    else \
        (entry)->key = (_key_); \
} while(0)

#define dictCompareKeys(d, key1, key2) \
    (((d)->type->keyCompare) ? \
        (d)->type->keyCompare((d)->privdata, key1, key2) : \
        (key1) == (key2))

#define dictHashKey(d, key) (d)->type->hashFunction(key)
#define dictGetKey(he) ((he)->key)
#define dictGetVal(he) ((he)->v.val)
#define dictGetSignedIntegerVal(he) ((he)->v.s64)
#define dictGetUnsignedIntegerVal(he) ((he)->v.u64)
#define dictGetDoubleVal(he) ((he)->v.d)
#define dictSlots(d) ((d)->ht[0].size+(d)->ht[1].size)
#define dictSize(d) ((d)->ht[0].used+(d)->ht[1].used)
#define dictIsRehashing(d) ((d)->rehashidx != -1)

/* API */
//创建一个新字典O(1)
dict *dictCreate(dictType *type, void *privDataPtr);
int dictExpand(dict *d, unsigned long size);
//将给定的键值对添加到字典中
int dictAdd(dict *d, void *key, void *val);
dictEntry *dictAddRaw(dict *d, void *key, dictEntry **existing);
dictEntry *dictAddOrFind(dict *d, void *key);
//将给定的键值对添加到字典中，如果键已经存在，则更新
int dictReplace(dict *d, void *key, void *val);
int dictDelete(dict *d, const void *key);
dictEntry *dictUnlink(dict *ht, const void *key);
void dictFreeUnlinkedEntry(dict *d, dictEntry *he);
void dictRelease(dict *d);
dictEntry * dictFind(dict *d, const void *key);
//返回给定键的值
void *dictFetchValue(dict *d, const void *key);
int dictResize(dict *d);
dictIterator *dictGetIterator(dict *d);
dictIterator *dictGetSafeIterator(dict *d);
dictEntry *dictNext(dictIterator *iter);
void dictReleaseIterator(dictIterator *iter);
dictEntry *dictGetRandomKey(dict *d);
dictEntry *dictGetFairRandomKey(dict *d);
unsigned int dictGetSomeKeys(dict *d, dictEntry **des, unsigned int count);
void dictGetStats(char *buf, size_t bufsize, dict *d);
uint64_t dictGenHashFunction(const void *key, int len);
uint64_t dictGenCaseHashFunction(const unsigned char *buf, int len);
void dictEmpty(dict *d, void(callback)(void*));
void dictEnableResize(void);
void dictDisableResize(void);
int dictRehash(dict *d, int n);
int dictRehashMilliseconds(dict *d, int ms);
void dictSetHashFunctionSeed(uint8_t *seed);
uint8_t *dictGetHashFunctionSeed(void);
unsigned long dictScan(dict *d, unsigned long v, dictScanFunction *fn, dictScanBucketFunction *bucketfn, void *privdata);
uint64_t dictGetHash(dict *d, const void *key);
dictEntry **dictFindEntryRefByPtrAndHash(dict *d, const void *oldptr, uint64_t hash);

/* Hash table types */
extern dictType dictTypeHeapStringCopyKey;
extern dictType dictTypeHeapStrings;
extern dictType dictTypeHeapStringCopyKeyValue;

#endif /* __DICT_H */
