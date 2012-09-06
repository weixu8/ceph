// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2004-2006 Sage Weil <sage@newdream.net>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */

#ifndef ASYNC_RESERVER_H
#define ASYNC_RESERVER_H

#include <map>
#include <utility>
#include <list>

#include "common/Mutex.h"
#include "common/Finisher.h"

template <typename T>
class AsyncReserver {
  Finisher *f;
  const unsigned max_allowed;
  Mutex lock;

  list<typename map<T, Context*>::iterator> queue;
  map<T, Context*> in_queue;
  map<T, typename list<typename map<T, Context*>::iterator>::iterator > queue_pointers;
  set<T> in_progress;

  void do_queues() {
    while (in_progress.size() < max_allowed &&
           in_queue.size()) {
      pair<T, Context*> p = *(queue.front());
      queue_pointers.erase(p.first);
      in_queue.erase(queue.front());
      queue.pop_front();
      f->queue(p.second);
      in_progress.insert(p.first);
    }
  }
public:
  AsyncReserver(
    Finisher *f,
    unsigned max_allowed)
    : f(f), max_allowed(max_allowed), lock("AsyncReserver::lock") {}

  void request_reservation(
    T item,
    Context *on_reserved) {
    Mutex::Locker l(lock);
    assert(!in_queue.count(item) &&
      !in_progress.count(item));
    in_queue.insert(make_pair(item, on_reserved));
    queue.push_back(in_queue.find(item));
    queue_pointers.insert(make_pair(item, --queue.end()));
    do_queues();
  }

  void cancel_reservation(
    T item) {
    Mutex::Locker l(lock);
    if (in_queue.count(item)) {
      queue.erase(queue_pointers.find(item)->second);
      queue_pointers.erase(item);
      in_queue.erase(item);
    } else {
      in_progress.erase(item);
    }
    do_queues();
  }
};

#endif
