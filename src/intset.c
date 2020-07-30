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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "intset.h"
#include "zmalloc.h"
#include "endianconv.h"

/* Note that these encodings are ordered, so:
 * INTSET_ENC_INT16 < INTSET_ENC_INT32 < INTSET_ENC_INT64. */
#define INTSET_ENC_INT16 (sizeof(int16_t))
#define INTSET_ENC_INT32 (sizeof(int32_t))
#define INTSET_ENC_INT64 (sizeof(int64_t))

/* Return the required encoding for the provided value. 
判断元素的大小格式*/
static uint8_t _intsetValueEncoding(int64_t v) {
    if (v < INT32_MIN || v > INT32_MAX)
        return INTSET_ENC_INT64;
    else if (v < INT16_MIN || v > INT16_MAX)
        return INTSET_ENC_INT32;
    else
        return INTSET_ENC_INT16;
}

/* Return the value at pos, given an encoding. 
用给定类型，从指定索引返回整数集合元素*/
static int64_t _intsetGetEncoded(intset *is, int pos, uint8_t enc) {
    int64_t v64;
    int32_t v32;
    int16_t v16;

    if (enc == INTSET_ENC_INT64) {
        //拷贝元素
        memcpy(&v64,((int64_t*)is->contents)+pos,sizeof(v64));
        //因为inset采用小端模式，动态判断是否需要转换为大端模式
        memrev64ifbe(&v64);
        //返回元素
        return v64;
    } else if (enc == INTSET_ENC_INT32) {
        memcpy(&v32,((int32_t*)is->contents)+pos,sizeof(v32));
        memrev32ifbe(&v32);
        return v32;
    } else {
        memcpy(&v16,((int16_t*)is->contents)+pos,sizeof(v16));
        memrev16ifbe(&v16);
        return v16;
    }
}

/* Return the value at pos, using the configured encoding. 
用指定的编码类型查找指定索引位置的元素*/
static int64_t _intsetGet(intset *is, int pos) {
    return _intsetGetEncoded(is,pos,intrev32ifbe(is->encoding));
}

/* Set the value at pos, using the configured encoding. 
在指定索引处，用指定元素更新*/
static void _intsetSet(intset *is, int pos, int64_t value) {
    //得到当前编码
    uint32_t encoding = intrev32ifbe(is->encoding);
    if (encoding == INTSET_ENC_INT64) {
        //设置指定索引的元素
        ((int64_t*)is->contents)[pos] = value;
        //进行可能需要的小端转大端
        memrev64ifbe(((int64_t*)is->contents)+pos);
    } else if (encoding == INTSET_ENC_INT32) {
        ((int32_t*)is->contents)[pos] = value;
        memrev32ifbe(((int32_t*)is->contents)+pos);
    } else {
        ((int16_t*)is->contents)[pos] = value;
        memrev16ifbe(((int16_t*)is->contents)+pos);
    }
}

/* Create an empty intset. 
创建一个新的整数集合*/
intset *intsetNew(void) {
    //申请空间大小
    intset *is = zmalloc(sizeof(intset));
    //把16位的字节大小经过大小端转化，如果需要的话
    //设置默认编码格式INTSET_ENC_INT16 
    is->encoding = intrev32ifbe(INTSET_ENC_INT16);
    is->length = 0;
    return is;
}

/* Resize the intset 
扩容到指定元素个大小
*/
static intset *intsetResize(intset *is, uint32_t len) {
    uint32_t size = len*intrev32ifbe(is->encoding);
    //尝试重新调整之前调用 malloc 或 calloc 所分配的 ptr 所指向的内存块的大小
    is = zrealloc(is,sizeof(intset)+size);
    return is;
}

/* Search for the position of "value". Return 1 when the value was found and
 * sets "pos" to the position of the value within the intset. Return 0 when
 * the value is not present in the intset and sets "pos" to the position
 * where "value" can be inserted. */
 //在整数集合中查找元素，当找到时候，将索引位置传给pos，并返回1。当没找到时候，将可以插入的索引位置传给pos，并返回0
static uint8_t intsetSearch(intset *is, int64_t value, uint32_t *pos) {
    int min = 0, max = intrev32ifbe(is->length)-1, mid = -1;
    int64_t cur = -1;

    /* The value can never be found when the set is empty */
    //先判断特殊情况，包括空集合，value大于最大值，或者小于最小值
    if (intrev32ifbe(is->length) == 0) {
        if (pos) *pos = 0;
        return 0;
    } else {
        /* Check for the case where we know we cannot find the value,
         * but do know the insert position. */
        if (value > _intsetGet(is,max)) {
            //如果是大值，则插入位置是数组的长度
            if (pos) *pos = intrev32ifbe(is->length);
            return 0;
        } else if (value < _intsetGet(is,0)) {
            if (pos) *pos = 0;
            return 0;
        }
    }

    //没有特殊情况，则二分查找，当找到或者找完了则终止
    while(max >= min) {
        mid = ((unsigned int)min + (unsigned int)max) >> 1;
        cur = _intsetGet(is,mid);
        if (value > cur) {
            min = mid+1;
        } else if (value < cur) {
            max = mid-1;
        } else {
            break;
        }
    }

    //如果找到了则返回该位置
    if (value == cur) {
        if (pos) *pos = mid;
        return 1;
    } else {
        //如果没找到返回可以插入的位置，注意这里的插入位置是min，因为插入的值需要插入到min指示的这个大值之前
        if (pos) *pos = min;
        return 0;
    }
}

/* Upgrades the intset to a larger encoding and inserts the given integer. */
//升级和添加新元素
static intset *intsetUpgradeAndAdd(intset *is, int64_t value) {
    //得到旧编码方式，具体为类型的字节大小
    uint8_t curenc = intrev32ifbe(is->encoding);
    //得到新编码方式
    uint8_t newenc = _intsetValueEncoding(value);
    int length = intrev32ifbe(is->length);
    //因为是升级，新元素必然是大于或者小于所有元素，则负数必定插到头部，正数必定插到尾部，因此这里做一个判断
    int prepend = value < 0 ? 1 : 0;

    /* First set new encoding and resize */
    //设置新的编码方式，方便用下面的扩容函数进行扩容
    is->encoding = intrev32ifbe(newenc);
    //扩容，按照is的编码，扩容到指定大小
    is = intsetResize(is,intrev32ifbe(is->length)+1);

    /* Upgrade back-to-front so we don't overwrite values.
     * Note that the "prepend" variable is used to make sure we have an empty
     * space at either the beginning or the end of the intset. */
     //从尾部开始移动元素，顺序保持不变
    while(length--)
        //在新整数数组的原来的索引上设置旧值，后者是获取旧数组上的指定索引位置的元素
        //注意prepend保证了新元素的位置
        _intsetSet(is,length+prepend,_intsetGetEncoded(is,length,curenc));

    /* Set the value at the beginning or the end. */
    //如果插入到头部
    if (prepend)
        _intsetSet(is,0,value);
    else
    //否则插入到尾部
        _intsetSet(is,intrev32ifbe(is->length),value);
        //更新整数数组元素长度
    is->length = intrev32ifbe(intrev32ifbe(is->length)+1);
    //返回新的元素数组
    return is;
}

//将整数集合从form指向的元素和后面所有的元素的起始索引移动到to索引
static void intsetMoveTail(intset *is, uint32_t from, uint32_t to) {
    //源地址 目标地址，用来进行拷贝自己的标记
    void *src, *dst;
    //得到整数集合中需要移动的元素个数
    uint32_t bytes = intrev32ifbe(is->length)-from;
    //得到整数集合的编码方式，也就是类型大小字节数，这里为了安全，没有直接使用这个encoding作为一个字节的大小
    uint32_t encoding = intrev32ifbe(is->encoding);
    //根据编码类型选择源地址 目标地址 和需要拷贝的字节数
    if (encoding == INTSET_ENC_INT64) {
        //得到指向源地址指针
        src = (int64_t*)is->contents+from;
        //得到目标地址指针
        dst = (int64_t*)is->contents+to;
        //得到需要拷贝的字节数
        bytes *= sizeof(int64_t);
    } else if (encoding == INTSET_ENC_INT32) {
        src = (int32_t*)is->contents+from;
        dst = (int32_t*)is->contents+to;
        bytes *= sizeof(int32_t);
    } else {
        src = (int16_t*)is->contents+from;
        dst = (int16_t*)is->contents+to;
        bytes *= sizeof(int16_t);
    }
    //从src地址开始拷贝bytes字节大小到src，从而实现内容移动
    memmove(dst,src,bytes);
}

/* Insert an integer in the intset */
intset *intsetAdd(intset *is, int64_t value, uint8_t *success) {
    //得到新添加元素的编码方式
    uint8_t valenc = _intsetValueEncoding(value);
    uint32_t pos;
    if (success) *success = 1;

    /* Upgrade encoding if necessary. If we need to upgrade, we know that
     * this value should be either appended (if > 0) or prepended (if < 0),
     * because it lies outside the range of existing values. */
     //判断是否需要类型升级，如果新加入的元素类型大于当前整数集合元素类型，则升级
    if (valenc > intrev32ifbe(is->encoding)) {
        /* This always succeeds, so we don't need to curry *success. */
        //升级和添加新元素，先按照新元素类型大小申请信空间，从后开始调整旧元素位置，最后放入新元素，并改变整数集合编码方式和元素长度
        return intsetUpgradeAndAdd(is,value);
    } else {
        //不需要升级时候，找到插入元素的位置，进行扩容，移动
        /* Abort if the value is already present in the set.
         * This call will populate "pos" with the right position to insert
         * the value when it cannot be found. */
         //在整数集合is中查找value，如果找到则pos为索引位置，并返回1。如果没找到则返回可以插入的位置，并返回0
         //判断如果找到则不用插入，并标记插入失败，默认是插入成功的标志
        if (intsetSearch(is,value,&pos)) {
            if (success) *success = 0;
            return is;
        }

        //决定插入的时候，扩容到指定个数大小
        is = intsetResize(is,intrev32ifbe(is->length)+1);
        //当插入到整数集合中间时候，移动后面的元素一个单位
        if (pos < intrev32ifbe(is->length)) intsetMoveTail(is,pos,pos+1);
    }

    _intsetSet(is,pos,value);
    is->length = intrev32ifbe(intrev32ifbe(is->length)+1);
    return is;
}

/* Delete integer from intset */
intset *intsetRemove(intset *is, int64_t value, int *success) {
    uint8_t valenc = _intsetValueEncoding(value);
    uint32_t pos;
    if (success) *success = 0;

    if (valenc <= intrev32ifbe(is->encoding) && intsetSearch(is,value,&pos)) {
        uint32_t len = intrev32ifbe(is->length);

        /* We know we can delete */
        if (success) *success = 1;

        /* Overwrite value with tail and update length */
        if (pos < (len-1)) intsetMoveTail(is,pos+1,pos);
        is = intsetResize(is,len-1);
        is->length = intrev32ifbe(len-1);
    }
    return is;
}

/* Determine whether a value belongs to this set */
uint8_t intsetFind(intset *is, int64_t value) {
    uint8_t valenc = _intsetValueEncoding(value);
    return valenc <= intrev32ifbe(is->encoding) && intsetSearch(is,value,NULL);
}

/* Return random member */
int64_t intsetRandom(intset *is) {
    return _intsetGet(is,rand()%intrev32ifbe(is->length));
}

/* Get the value at the given position. When this position is
 * out of range the function returns 0, when in range it returns 1. 
 获取指定索引的元素到value中，返回成功或者失败的标志*/
uint8_t intsetGet(intset *is, uint32_t pos, int64_t *value) {
    if (pos < intrev32ifbe(is->length)) {
        *value = _intsetGet(is,pos);
        return 1;
    }
    return 0;
}

/* Return intset length */
uint32_t intsetLen(const intset *is) {
    return intrev32ifbe(is->length);
}

/* Return intset blob size in bytes. */
size_t intsetBlobLen(intset *is) {
    return sizeof(intset)+intrev32ifbe(is->length)*intrev32ifbe(is->encoding);
}

#ifdef REDIS_TEST
#include <sys/time.h>
#include <time.h>

#if 0
static void intsetRepr(intset *is) {
    for (uint32_t i = 0; i < intrev32ifbe(is->length); i++) {
        printf("%lld\n", (uint64_t)_intsetGet(is,i));
    }
    printf("\n");
}

static void error(char *err) {
    printf("%s\n", err);
    exit(1);
}
#endif

static void ok(void) {
    printf("OK\n");
}

static long long usec(void) {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return (((long long)tv.tv_sec)*1000000)+tv.tv_usec;
}

#define assert(_e) ((_e)?(void)0:(_assert(#_e,__FILE__,__LINE__),exit(1)))
static void _assert(char *estr, char *file, int line) {
    printf("\n\n=== ASSERTION FAILED ===\n");
    printf("==> %s:%d '%s' is not true\n",file,line,estr);
}

static intset *createSet(int bits, int size) {
    uint64_t mask = (1<<bits)-1;
    uint64_t value;
    intset *is = intsetNew();

    for (int i = 0; i < size; i++) {
        if (bits > 32) {
            value = (rand()*rand()) & mask;
        } else {
            value = rand() & mask;
        }
        is = intsetAdd(is,value,NULL);
    }
    return is;
}

static void checkConsistency(intset *is) {
    for (uint32_t i = 0; i < (intrev32ifbe(is->length)-1); i++) {
        uint32_t encoding = intrev32ifbe(is->encoding);

        if (encoding == INTSET_ENC_INT16) {
            int16_t *i16 = (int16_t*)is->contents;
            assert(i16[i] < i16[i+1]);
        } else if (encoding == INTSET_ENC_INT32) {
            int32_t *i32 = (int32_t*)is->contents;
            assert(i32[i] < i32[i+1]);
        } else {
            int64_t *i64 = (int64_t*)is->contents;
            assert(i64[i] < i64[i+1]);
        }
    }
}

#define UNUSED(x) (void)(x)
int intsetTest(int argc, char **argv) {
    uint8_t success;
    int i;
    intset *is;
    srand(time(NULL));

    UNUSED(argc);
    UNUSED(argv);

    printf("Value encodings: "); {
        assert(_intsetValueEncoding(-32768) == INTSET_ENC_INT16);
        assert(_intsetValueEncoding(+32767) == INTSET_ENC_INT16);
        assert(_intsetValueEncoding(-32769) == INTSET_ENC_INT32);
        assert(_intsetValueEncoding(+32768) == INTSET_ENC_INT32);
        assert(_intsetValueEncoding(-2147483648) == INTSET_ENC_INT32);
        assert(_intsetValueEncoding(+2147483647) == INTSET_ENC_INT32);
        assert(_intsetValueEncoding(-2147483649) == INTSET_ENC_INT64);
        assert(_intsetValueEncoding(+2147483648) == INTSET_ENC_INT64);
        assert(_intsetValueEncoding(-9223372036854775808ull) ==
                    INTSET_ENC_INT64);
        assert(_intsetValueEncoding(+9223372036854775807ull) ==
                    INTSET_ENC_INT64);
        ok();
    }

    printf("Basic adding: "); {
        is = intsetNew();
        is = intsetAdd(is,5,&success); assert(success);
        is = intsetAdd(is,6,&success); assert(success);
        is = intsetAdd(is,4,&success); assert(success);
        is = intsetAdd(is,4,&success); assert(!success);
        ok();
    }

    printf("Large number of random adds: "); {
        uint32_t inserts = 0;
        is = intsetNew();
        for (i = 0; i < 1024; i++) {
            is = intsetAdd(is,rand()%0x800,&success);
            if (success) inserts++;
        }
        assert(intrev32ifbe(is->length) == inserts);
        checkConsistency(is);
        ok();
    }

    printf("Upgrade from int16 to int32: "); {
        is = intsetNew();
        is = intsetAdd(is,32,NULL);
        assert(intrev32ifbe(is->encoding) == INTSET_ENC_INT16);
        is = intsetAdd(is,65535,NULL);
        assert(intrev32ifbe(is->encoding) == INTSET_ENC_INT32);
        assert(intsetFind(is,32));
        assert(intsetFind(is,65535));
        checkConsistency(is);

        is = intsetNew();
        is = intsetAdd(is,32,NULL);
        assert(intrev32ifbe(is->encoding) == INTSET_ENC_INT16);
        is = intsetAdd(is,-65535,NULL);
        assert(intrev32ifbe(is->encoding) == INTSET_ENC_INT32);
        assert(intsetFind(is,32));
        assert(intsetFind(is,-65535));
        checkConsistency(is);
        ok();
    }

    printf("Upgrade from int16 to int64: "); {
        is = intsetNew();
        is = intsetAdd(is,32,NULL);
        assert(intrev32ifbe(is->encoding) == INTSET_ENC_INT16);
        is = intsetAdd(is,4294967295,NULL);
        assert(intrev32ifbe(is->encoding) == INTSET_ENC_INT64);
        assert(intsetFind(is,32));
        assert(intsetFind(is,4294967295));
        checkConsistency(is);

        is = intsetNew();
        is = intsetAdd(is,32,NULL);
        assert(intrev32ifbe(is->encoding) == INTSET_ENC_INT16);
        is = intsetAdd(is,-4294967295,NULL);
        assert(intrev32ifbe(is->encoding) == INTSET_ENC_INT64);
        assert(intsetFind(is,32));
        assert(intsetFind(is,-4294967295));
        checkConsistency(is);
        ok();
    }

    printf("Upgrade from int32 to int64: "); {
        is = intsetNew();
        is = intsetAdd(is,65535,NULL);
        assert(intrev32ifbe(is->encoding) == INTSET_ENC_INT32);
        is = intsetAdd(is,4294967295,NULL);
        assert(intrev32ifbe(is->encoding) == INTSET_ENC_INT64);
        assert(intsetFind(is,65535));
        assert(intsetFind(is,4294967295));
        checkConsistency(is);

        is = intsetNew();
        is = intsetAdd(is,65535,NULL);
        assert(intrev32ifbe(is->encoding) == INTSET_ENC_INT32);
        is = intsetAdd(is,-4294967295,NULL);
        assert(intrev32ifbe(is->encoding) == INTSET_ENC_INT64);
        assert(intsetFind(is,65535));
        assert(intsetFind(is,-4294967295));
        checkConsistency(is);
        ok();
    }

    printf("Stress lookups: "); {
        long num = 100000, size = 10000;
        int i, bits = 20;
        long long start;
        is = createSet(bits,size);
        checkConsistency(is);

        start = usec();
        for (i = 0; i < num; i++) intsetSearch(is,rand() % ((1<<bits)-1),NULL);
        printf("%ld lookups, %ld element set, %lldusec\n",
               num,size,usec()-start);
    }

    printf("Stress add+delete: "); {
        int i, v1, v2;
        is = intsetNew();
        for (i = 0; i < 0xffff; i++) {
            v1 = rand() % 0xfff;
            is = intsetAdd(is,v1,NULL);
            assert(intsetFind(is,v1));

            v2 = rand() % 0xfff;
            is = intsetRemove(is,v2,NULL);
            assert(!intsetFind(is,v2));
        }
        checkConsistency(is);
        ok();
    }

    return 0;
}
#endif
