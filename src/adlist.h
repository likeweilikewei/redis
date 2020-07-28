/* adlist.h - A generic doubly linked list implementation
 * redis的链表实现
 * 
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

#ifndef __ADLIST_H__
#define __ADLIST_H__

/* Node, List, and Iterator are the only data structures used currently. */
//链表节点
typedef struct listNode {
    //前置节点
    struct listNode *prev;
    //后置节点
    struct listNode *next;
    //节点的值，设置为void*可以接收各种类型的值
    void *value;
} listNode;

//链表迭代器
typedef struct listIter {
    //返回“后一个”元素节点的指针
    listNode *next;
    //direction决定了迭代器是沿着next指针向后迭代(AL_START_HEAD)，还是沿着prev指针向前迭代(AL_START_TAIL)，这个值可以是adlist.h中的AL_START_HEAD常量或AL_START_TAIL常量：
    int direction;
} listIter;

typedef struct list {
    // 表头指针指向第一个节点
    listNode *head;
    // 表尾指针指向最后一个节点
    listNode *tail;
    //以下三个实现多态链表所需的类型特定函数
    void *(*dup)(void *ptr);
    void (*free)(void *ptr);
    int (*match)(void *ptr, void *key);
    // 链表长度计数器
    unsigned long len;
} list;

/* Functions implemented as macros */
//返回链表长度，len属性直接获得，O(1)
#define listLength(l) ((l)->len)
//返回表头节点，可以通过链表的head属性获得 O(1)
#define listFirst(l) ((l)->head)
//返回表尾节点，tail属性获得，O(1)
#define listLast(l) ((l)->tail)
//返回当前节点的前置节点，prev获得，O(1)
#define listPrevNode(n) ((n)->prev)
//返回当前节点的后置节点，next获得，O(1)
#define listNextNode(n) ((n)->next)
//但会当前节点值，value直接获得，O(1)
#define listNodeValue(n) ((n)->value)

// 将给定的函数设置为链表的节点复制函数，复制函数可以通过链表的dup属性直接获得，时间复杂度O（1）
#define listSetDupMethod(l,m) ((l)->dup = (m))
// 将给定的函数设置为链表的节点释放函数，O（1）
#define listSetFreeMethod(l,m) ((l)->free = (m))
// 将给定的函数设置为链表的节点值对比函数，判断节点值是否和另一个输入值相等，O（1）
#define listSetMatchMethod(l,m) ((l)->match = (m))
// 返回链表当前正在使用的节点值复制函数
#define listGetDupMethod(l) ((l)->dup)
//返回当前节点值释放函数
#define listGetFreeMethod(l) ((l)->free)
//返回链表正在看使用的节点值对比函数
#define listGetMatchMethod(l) ((l)->match)

/* Prototypes */
//创建一个不包含任何节点的新链表
list *listCreate(void);
//释放给定链表以及链表中的所有节点
void listRelease(list *list);
// 删除所有节点但不删除链表
void listEmpty(list *list);
//将一个包含给定值的新节点添加到链表表头
list *listAddNodeHead(list *list, void *value);
//将一个包含给定值的新节点添加到链表尾部
list *listAddNodeTail(list *list, void *value);
//将新节点插入到old_node的之前或者之后
list *listInsertNode(list *list, listNode *old_node, void *value, int after);
//从链表中删除给定节点，O(1)
void listDelNode(list *list, listNode *node);
//获取指定方向的迭代器
listIter *listGetIterator(list *list, int direction);
//返回当前迭代器的下一个节点指针
listNode *listNext(listIter *iter);
//释放迭代器内存
void listReleaseIterator(listIter *iter);
//复制链表，并返回新链表
list *listDup(list *orig);
listNode *listSearchKey(list *list, void *key);
listNode *listIndex(list *list, long index);
//创建一个从头开始的迭代器
void listRewind(list *list, listIter *li);
//创建一个从尾部开始的迭代器
void listRewindTail(list *list, listIter *li);
void listRotateTailToHead(list *list);
void listRotateHeadToTail(list *list);
void listJoin(list *l, list *o);

/* Directions for iterators */
//决定了迭代器的方向，为AL_START_HEAD时候，从头开始向后遍历
#define AL_START_HEAD 0
//从尾部开始向头部遍历，都是遍历到节点为NULL结束
#define AL_START_TAIL 1

#endif /* __ADLIST_H__ */
