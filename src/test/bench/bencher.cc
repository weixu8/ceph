// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-

#include "bencher.h"
#include "include/utime.h"
#include <unistd.h>
#include <tr1/memory>

struct OnDelete {
  Context *c;
  OnDelete(Context *c) : c(c) {}
  ~OnDelete() { c->complete(0); }
};

struct Cleanup : public Context {
  Bencher *bench;
  Cleanup(Bencher *bench) : bench(bench) {}
  void finish(int r) {
    bench->complete_op();
  }
};

struct OnWriteApplied : public Context {
  Bencher *bench;
  uint64_t seq;
  std::tr1::shared_ptr<OnDelete> on_delete;
  OnWriteApplied(
    Bencher *bench, uint64_t seq,
    std::tr1::shared_ptr<OnDelete> on_delete
    ) : bench(bench), seq(seq), on_delete(on_delete) {}
  void finish(int r) {
    bench->stat_collector->write_applied(seq);
  }
};

struct OnWriteCommit : public Context {
  Bencher *bench;
  uint64_t seq;
  std::tr1::shared_ptr<OnDelete> on_delete;
  OnWriteCommit(
    Bencher *bench, uint64_t seq,
    std::tr1::shared_ptr<OnDelete> on_delete
    ) : bench(bench), seq(seq), on_delete(on_delete) {}
  void finish(int r) {
    bench->stat_collector->write_committed(seq);
  }
};

struct OnReadComplete : public Context {
  Bencher *bench;
  uint64_t seq;
  boost::scoped_ptr<bufferlist> bl;
  OnReadComplete(Bencher *bench, uint64_t seq, bufferlist *bl) :
    bench(bench), seq(seq), bl(bl) {}
  void finish(int r) {
    bench->stat_collector->read_complete(seq);
    bench->complete_op();
  }
};

void Bencher::start_op() {
  Mutex::Locker l(lock);
  while (open_ops >= max_in_flight)
    open_ops_cond.Wait(lock);
  ++open_ops;
}

void Bencher::complete_op() {
  Mutex::Locker l(lock);
  assert(open_ops > 0);
  --open_ops;
  open_ops_cond.Signal();
}

void Bencher::run_bench()
{
  time_t end = time(0) + max_duration;
  uint64_t ops = 0;

  while ((!max_duration || time(0) < end) && (!max_ops || ops < max_ops)) {
    start_op();
    uint64_t seq = stat_collector->next_seq();
    switch ((*op_type_gen)()) {
      case WRITE: {
	std::tr1::shared_ptr<OnDelete> on_delete(
	  new OnDelete(new Cleanup(this)));
	bufferlist bl;
	uint64_t length = (*length_gen)();
	stat_collector->start_write(seq, length);
	for (uint64_t i = 0; i < length; ++i) {
	  bl.append(rand());
	}
	backend->write(
	  (*object_gen)(),
	  (*offset_gen)(),
	  bl,
	  new OnWriteApplied(
	    this, seq, on_delete),
	  new OnWriteCommit(
	    this, seq, on_delete)
	  );
	break;
      }
      case READ: {
	uint64_t length = (*length_gen)();
	stat_collector->start_read(seq, length);
	bufferlist *bl = new bufferlist;
	backend->read(
	  (*object_gen)(),
	  (*offset_gen)(),
	  length,
	  bl,
	  new OnReadComplete(
	    this, seq, bl)
	  );
	break;
      }
      default: {
	assert(0);
      }
    }
  }
}
