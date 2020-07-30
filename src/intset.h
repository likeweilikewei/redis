/*
 * Copyright (c) 2009-2012, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 * Copyright (c) 2009-2012, Salvatore Sanfilippo <antirez at gmail dot com>
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
//整数集合，是集合键的底层实现之一，当一个集合中只包含整数值元素时候并且这个集合元素不多时候，就会使用整数集合，
#ifndef __INTSET_H
#define __INTSET_H
#include <stdint.h>

//整数集合
typedef struct intset {
    //编码方式，有三种INTSET_ENC_INT16 INTSET_ENC_INT32 INTSET_ENC_INT64
    uint32_t encoding;
    //集合包含的元素个数
    uint32_t length;
    //保存元素的数组，虽然是int8_t,但是真正的类型取决于encoding，如果encoding属性为INTSET_ENC_INT16时候采用int16_t类型的元素，每个元素都是这个类型，无论数值大小
    //元素类型并不一定是ini8_t类型，柔性数组不占intset结构体大小，并且数组中的元素从小到大排列。
    int8_t contents[];
} intset;

//创建一个新的整数集合
intset *intsetNew(void);
//将给定元素添加到整数集合里，O（N）
intset *intsetAdd(intset *is, int64_t value, uint8_t *success);
//从整数集合中移除给定元素O(n)
intset *intsetRemove(intset *is, int64_t value, int *success);
//检查给定值是否存在，o（logn）
uint8_t intsetFind(intset *is, int64_t value);
//从整数集合中随机返回一个元素
int64_t intsetRandom(intset *is);
//取出数组中指定索引的元素
uint8_t intsetGet(intset *is, uint32_t pos, int64_t *value);
//返回元素个数O(1)
uint32_t intsetLen(const intset *is);
//返回整数集合占用的字节内存O(1)，包括整数集合结构体
size_t intsetBlobLen(intset *is);

#ifdef REDIS_TEST
int intsetTest(int argc, char *argv[]);
#endif

#endif // __INTSET_H
