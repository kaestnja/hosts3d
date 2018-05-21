/* llist.cpp - 03 Apr 09
   Hosts3D - 3D Real-Time Network Monitor
   Copyright (c) 2006-2009  Del Castle

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "llist.h"

#define UINT32_MAX  (4294967295U)  //max unsigned int

MyLL::Item::Item(void *dat, Item *nxt, Item *prv)
{
  Data = dat;
  Next = nxt;
  Prev = prv;
}

MyLL::Item::~Item() { }

MyLL::MyLL()
{
  pFront = 0;
  pBack = 0;
  wCnt = 0;
  dCnt = 0;
}

unsigned int MyLL::Num()
{
  if (wCnt < dCnt) return ((UINT32_MAX - dCnt) + wCnt + 1);
  return (wCnt - dCnt);
}

void MyLL::Start(unsigned char pc)
{
  pCurr[pc] = pFront;
}

void MyLL::End(unsigned char pc)
{
  pCurr[pc] = pBack;
}

void *MyLL::Write(void *dat)
{
  if (pFront)
  {
    pBack->Next = new Item(dat, pBack->Next, pBack);
    pBack = pBack->Next;
  }
  else
  {
    pFront = new Item(dat, pBack, pFront);
    pBack = pFront;
  }
  wCnt++;
  return pBack->Data;
}

void *MyLL::Read(unsigned char pc)
{
  if (pCurr[pc]) return pCurr[pc]->Data;
  return 0;
}

void MyLL::Next(unsigned char pc)
{
  if (pCurr[pc]) pCurr[pc] = pCurr[pc]->Next;
}

void MyLL::Prev(unsigned char pc)
{
  if (pCurr[pc]) pCurr[pc] = pCurr[pc]->Prev;
}

void *MyLL::Find(unsigned short itm)
{
  Item *tp = pFront;
  for (unsigned short cnt = 1; cnt < itm; cnt++)
  {
    if (tp) tp = tp->Next;
  }
  if (tp) return tp->Data;
  return 0;
}

void MyLL::Delete(unsigned char pc)
{
  dCnt++;
  Item *tp = pCurr[pc];
  Next(pc);
  (tp->Prev ? tp->Prev->Next : pFront) = tp->Next;
  (tp->Next ? tp->Next->Prev : pBack) = tp->Prev;
  delete tp;
}

void MyLL::Destroy()
{
  Start(0);
  while (pCurr[0]) Delete(0);
}

MyLL::~MyLL()
{
  Destroy();
}

MyHT::MyHT() {
  for (unsigned i = 0; i < HT_BUCKETS; i++) {
    buckets[i] = 0;
  }
  count = 0;
}

MyHT::~MyHT() {
  destroy();
}

int MyHT::Num() {
  return count;
}

void MyHT::destroy() {
  for (unsigned i = 0; i < HT_BUCKETS; i++) {
    if (buckets[i]) {
      buckets[i]->Destroy();
      buckets[i] = 0;
    }
  }
  count = 0;
}

void * MyHT::newEntryAtBucket(HtKeyType key, void *data) {
  void *ret;
  MyLL *bucket = buckets[key];
  if (!bucket) {
    bucket = new MyLL;
    buckets[key] = bucket;
  }
  ret = bucket->Write(data);
  count++;
  return ret;
}

bool MyHT::deleteEntryAtBucket(unsigned char pc, HtKeyType key, void *data) {
  MyLL *bucket = buckets[key];

  if (!bucket) {
    return false;
  }

  bucket->Start(pc);
  while (1) {
    void *curr = bucket->Read(pc);
    if (!curr) {
      return false;
    }
    if (curr == data) {
      count--;
      bucket->Delete(pc);
      if (bucket->Num() == 0) {
	delete bucket;
	buckets[key] = 0;
      }
      return true;
    } else {
      bucket->Next(pc);
    }
  }
}

void MyHT::forEach(unsigned char pc, HtForEachCallback cb, long arg1, long arg2,
		   long arg3, long arg4) {
  for (unsigned i = 0; i < HT_BUCKETS; i++) {
    MyLL *bucket = buckets[i];
    if (!bucket) {
      continue;
    }

    void *data;
    bucket->Start(pc);
    while ((data = bucket->Read(pc))) {
      (*cb)(&data, arg1, arg2, arg3, arg4);
      if (!data) {
	// callback deleted the data, so we delete the linked list item
	count--;
	bucket->Delete(pc);
	// if the linked list is empty, delete the linked list itself
	if (bucket->Num() == 0) {
	  delete bucket;
	  buckets[i] = 0;
	  break;
	}
      } else {
	bucket->Next(pc);
      }
    }
  }
}

void *MyHT::firstThat(HtKeyType key, unsigned char pc,
		      HtFirstThatCallback cb, long arg1,
		      long arg2, long arg3, long arg4) {
  void *data;
  MyLL *bucket = buckets[key];
  if (!bucket) {
    return 0;
  }

  bucket->Start(pc);
  while ((data = bucket->Read(pc))) {
    if ((*cb)(data, arg1, arg2, arg3, arg4)) {
      return data;
    } else {
      bucket->Next(pc);
    }
  }

  // Still here
  return 0;
}

// Version that walks over all buckets, i.e. doesn't need a key
void *MyHT::firstThat(unsigned char pc, HtFirstThatCallback cb, long arg1,
		      long arg2, long arg3, long arg4) {
  for (unsigned key = 0; key < HT_BUCKETS; key++) {
    void *data = firstThat((HtKeyType) key, pc, cb, arg1, arg2, arg3, arg4);
    if (data) {
      return data;
    }
  }
  // Still here
  return 0;
}
