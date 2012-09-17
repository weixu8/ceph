/**
 * All operations via the rados gateway are carried out by
 * small classes known as RGWOps. This class contains a req_state
 * and each possible command is a subclass of this with a defined
 * execute() method that does whatever the subclass name implies.
 * These subclasses must be further subclassed (by interface type)
 * to provide additional virtual methods such as send_response or get_params.
 */
#ifndef CEPH_RGW_OP_H
#define CEPH_RGW_OP_H

#include <string>

#include "rgw_common.h"
#include "rgw_rados.h"
#include "rgw_user.h"
#include "rgw_acl.h"

using namespace std;

struct req_state;
class RGWHandler;

void rgw_get_request_metadata(struct req_state *s, map<string, bufferlist>& attrs);

/**
 * Provide the base class for all ops.
 */
class RGWOp {
protected:
  struct req_state *s;
  RGWHandler *dialect_handler;
public:
  RGWOp() {}
  virtual ~RGWOp() {}

  virtual void init(struct req_state *s, RGWHandler *dialect_handler) {
    this->s = s;
    this->dialect_handler = dialect_handler;
  }
  virtual int verify_params() { return 0; }
  virtual bool prefetch_data() { return false; }
  virtual int verify_permission() = 0;
  virtual void execute() = 0;
  virtual void send_response() {}
  virtual void complete() { send_response(); }
  virtual const char *name() = 0;
};

class RGWGetObj : public RGWOp {
protected:
  const char *range_str;
  const char *if_mod;
  const char *if_unmod;
  const char *if_match;
  const char *if_nomatch;
  off_t ofs;
  uint64_t len;
  uint64_t total_len;
  off_t start;
  off_t end;
  time_t mod_time;
  time_t lastmod;
  time_t unmod_time;
  time_t *mod_ptr;
  time_t *unmod_ptr;
  map<string, bufferlist> attrs;
  int ret;
  bool get_data;
  bool partial_content;
  rgw_obj obj;

  int init_common();
public:
  RGWGetObj() {
    range_str = NULL;
    if_mod = NULL;
    if_unmod = NULL;
    if_match = NULL;
    if_nomatch = NULL;
    start = 0;
    ofs = 0;
    len = 0;
    total_len = 0;
    end = -1;
    mod_time = 0;
    lastmod = 0;
    unmod_time = 0;
    mod_ptr = NULL;
    unmod_ptr = NULL;
    partial_content = false;
    ret = 0;
 }

  virtual bool prefetch_data() { return true; }

  void set_get_data(bool get_data) {
    this->get_data = get_data;
  }
  int verify_permission();
  void execute();
  int read_user_manifest_part(rgw_bucket& bucket, RGWObjEnt& ent, RGWAccessControlPolicy *bucket_policy, off_t start_ofs, off_t end_ofs);
  int iterate_user_manifest_parts(rgw_bucket& bucket, string& obj_prefix, RGWAccessControlPolicy *bucket_policy,
                                  uint64_t *ptotal_len, bool read_data);
  int handle_user_manifest(const char *prefix);

  virtual int get_params() = 0;
  virtual int send_response(bufferlist& bl) = 0;

  virtual const char *name() { return "get_obj"; }
};

class RGWListBuckets : public RGWOp {
protected:
  int ret;
  RGWUserBuckets buckets;

public:
  RGWListBuckets() {}

  int verify_permission();
  void execute();

  virtual int get_params() = 0;
  virtual void send_response() = 0;

  virtual const char *name() { return "list_buckets"; }
};

class RGWStatAccount : public RGWOp {
protected:
  int ret;
  uint32_t buckets_count;
  uint64_t buckets_objcount;
  uint64_t buckets_size;
  uint64_t buckets_size_rounded;

public:
  RGWStatAccount() {
    ret = 0;
    buckets_count = 0;
    buckets_objcount = 0;
    buckets_size = 0;
    buckets_size_rounded = 0;
  }

  int verify_permission();
  void execute();

  virtual void send_response() = 0;
  virtual const char *name() { return "stat_account"; }
};

class RGWListBucket : public RGWOp {
protected:
  string prefix;
  string marker; 
  string max_keys;
  string delimiter;
  int max;
  int ret;
  vector<RGWObjEnt> objs;
  map<string, bool> common_prefixes;

  int default_max;
  bool is_truncated;

  int parse_max_keys();

public:
  RGWListBucket() {
    max = 0;
    ret = 0;
    is_truncated = false;
  }
  int verify_permission();
  void execute();

  virtual int get_params() = 0;
  virtual void send_response() = 0;
  virtual const char *name() { return "list_bucket"; }
};

class RGWStatBucket : public RGWOp {
protected:
  int ret;
  RGWBucketEnt bucket;

public:
  RGWStatBucket() : ret(0) {}
  ~RGWStatBucket() {}

  int verify_permission();
  void execute();

  virtual void send_response() = 0;
  virtual const char *name() { return "stat_bucket"; }
};

class RGWCreateBucket : public RGWOp {
protected:
  int ret;
  RGWAccessControlPolicy policy;

public:
  RGWCreateBucket() : ret(0) {}

  int verify_permission();
  void execute();
  virtual void init(struct req_state *s, RGWHandler *h) {
    RGWOp::init(s, h);
    policy.set_ctx(s->cct);
  }
  virtual int get_params() { return 0; }
  virtual void send_response() = 0;
  virtual const char *name() { return "create_bucket"; }
};

class RGWDeleteBucket : public RGWOp {
protected:
  int ret;

public:
  RGWDeleteBucket() {
    ret = 0;
  }

  int verify_permission();
  void execute();

  virtual void send_response() = 0;
  virtual const char *name() { return "delete_bucket"; }
};

class RGWPutObjProcessor
{
protected:
  struct req_state *s;
public:
  virtual ~RGWPutObjProcessor() {}
  virtual int prepare(struct req_state *_s) {
    s = _s;
    return 0;
  };
  virtual int handle_data(bufferlist& bl, off_t ofs, void **phandle) = 0;
  virtual int throttle_data(void *handle) = 0;
  virtual int complete(string& etag, map<string, bufferlist>& attrs) = 0;
};

class RGWPutObj : public RGWOp {

  friend class RGWPutObjProcessor;

protected:
  int ret;
  off_t ofs;
  const char *supplied_md5_b64;
  const char *supplied_etag;
  string etag;
  bool chunked_upload;
  RGWAccessControlPolicy policy;
  const char *obj_manifest;

public:
  RGWPutObj() {
    ret = 0;
    ofs = 0;
    supplied_md5_b64 = NULL;
    supplied_etag = NULL;
    chunked_upload = false;
    obj_manifest = NULL;
  }

  virtual void init(struct req_state *s, RGWHandler *h) {
    RGWOp::init(s, h);
    policy.set_ctx(s->cct);
  }

  RGWPutObjProcessor *select_processor();
  void dispose_processor(RGWPutObjProcessor *processor);

  int verify_permission();
  void execute();

  virtual int get_params() = 0;
  virtual int get_data(bufferlist& bl) = 0;
  virtual void send_response() = 0;
  virtual const char *name() { return "put_obj"; }
};

class RGWPutMetadata : public RGWOp {
protected:
  int ret;
  map<string, bufferlist> attrs;
  bool has_policy;
  RGWAccessControlPolicy policy;

public:
  RGWPutMetadata() {
    has_policy = false;
    ret = 0;
  }

  virtual void init(struct req_state *s, RGWHandler *h) {
    RGWOp::init(s, h);
    policy.set_ctx(s->cct);
  }
  int verify_permission();
  void execute();

  virtual int get_params() = 0;
  virtual void send_response() = 0;
  virtual const char *name() { return "put_obj_metadata"; }
};

class RGWDeleteObj : public RGWOp {
protected:
  int ret;

public:
  RGWDeleteObj() {
    ret = 0;
  }

  int verify_permission();
  void execute();

  virtual void send_response() = 0;
  virtual const char *name() { return "delete_obj"; }
};

class RGWCopyObj : public RGWOp {
protected:
  RGWAccessControlPolicy dest_policy;
  const char *if_mod;
  const char *if_unmod;
  const char *if_match;
  const char *if_nomatch;
  off_t ofs;
  off_t len;
  off_t end;
  time_t mod_time;
  time_t unmod_time;
  time_t *mod_ptr;
  time_t *unmod_ptr;
  int ret;
  map<string, bufferlist> attrs;
  string src_bucket_name;
  rgw_bucket src_bucket;
  string src_object;
  string dest_bucket_name;
  rgw_bucket dest_bucket;
  string dest_object;
  time_t mtime;
  bool replace_attrs;

  int init_common();

protected:
  bool parse_copy_location(const char *src, string& bucket_name, string& object);

public:
  RGWCopyObj() {
    if_mod = NULL;
    if_unmod = NULL;
    if_match = NULL;
    if_nomatch = NULL;
    ofs = 0;
    len = 0;
    end = -1;
    mod_ptr = NULL;
    unmod_ptr = NULL;
    ret = 0;
    mtime = 0;
    replace_attrs = false;
  }

  virtual void init(struct req_state *s, RGWHandler *h) {
    RGWOp::init(s, h);
    dest_policy.set_ctx(s->cct);
  }
  int verify_permission();
  void execute();

  virtual int init_dest_policy() { return 0; }
  virtual int get_params() = 0;
  virtual void send_response() = 0;
  virtual const char *name() { return "copy_obj"; }
};

class RGWGetACLs : public RGWOp {
protected:
  int ret;
  string acls;

public:
  RGWGetACLs() {
    ret = 0;
  }

  int verify_permission();
  void execute();

  virtual void send_response() = 0;
  virtual const char *name() { return "get_acls"; }
};

class RGWPutACLs : public RGWOp {
protected:
  int ret;
  size_t len;
  char *data;

public:
  RGWPutACLs() {
    ret = 0;
    len = 0;
    data = NULL;
  }

  int verify_permission();
  void execute();

  virtual int get_canned_policy(ACLOwner& owner, stringstream& ss) { return 0; }
  virtual int get_params() = 0;
  virtual void send_response() = 0;
  virtual const char *name() { return "put_acls"; }
};

class RGWInitMultipart : public RGWOp {
protected:
  int ret;
  string upload_id;
  RGWAccessControlPolicy policy;

public:
  RGWInitMultipart() {
    ret = 0;
  }

  virtual void init(struct req_state *s, RGWHandler *h) {
    RGWOp::init(s, h);
    policy.set_ctx(s->cct);
  }
  int verify_permission();
  void execute();

  virtual int get_params() = 0;
  virtual void send_response() = 0;
  virtual const char *name() { return "init_multipart"; }
};

class RGWCompleteMultipart : public RGWOp {
protected:
  int ret;
  string upload_id;
  string etag;
  char *data;
  int len;

public:
  RGWCompleteMultipart() {
    ret = 0;
    data = NULL;
    len = 0;
  }

  int verify_permission();
  void execute();

  virtual int get_params() = 0;
  virtual void send_response() = 0;
  virtual const char *name() { return "complete_multipart"; }
};

class RGWAbortMultipart : public RGWOp {
protected:
  int ret;

public:
  RGWAbortMultipart() {
    ret = 0;
  }

  int verify_permission();
  void execute();

  virtual void send_response() = 0;
  virtual const char *name() { return "abort_multipart"; }
};

class RGWListMultipart : public RGWOp {
protected:
  int ret;
  string upload_id;
  map<uint32_t, RGWUploadPartInfo> parts;
  int max_parts;
  int marker;
  RGWAccessControlPolicy policy;

public:
  RGWListMultipart() {
    ret = 0;
    max_parts = 1000;
    marker = 0;
  }

  virtual void init(struct req_state *s, RGWHandler *h) {
    RGWOp::init(s, h);
    policy = RGWAccessControlPolicy(s->cct);
  }
  int verify_permission();
  void execute();

  virtual int get_params() = 0;
  virtual void send_response() = 0;
  virtual const char *name() { return "list_multipart"; }
};

#define MP_META_SUFFIX ".meta"

class RGWMPObj {
  string oid;
  string prefix;
  string meta;
  string upload_id;
public:
  RGWMPObj() {}
  RGWMPObj(string& _oid, string& _upload_id) {
    init(_oid, _upload_id);
  }
  void init(string& _oid, string& _upload_id) {
    if (_oid.empty()) {
      clear();
      return;
    }
    oid = _oid;
    upload_id = _upload_id;
    prefix = oid;
    prefix.append(".");
    prefix.append(upload_id);
    meta = prefix;
    meta.append(MP_META_SUFFIX);
  }
  string& get_meta() { return meta; }
  string get_part(int num) {
    char buf[16];
    snprintf(buf, 16, ".%d", num);
    string s = prefix;
    s.append(buf);
    return s;
  }
  string get_part(string& part) {
    string s = prefix;
    s.append(".");
    s.append(part);
    return s;
  }
  string& get_upload_id() {
    return upload_id;
  }
  string& get_key() {
    return oid;
  }
  bool from_meta(string& meta) {
    int end_pos = meta.rfind('.'); // search for ".meta"
    if (end_pos < 0)
      return false;
    int mid_pos = meta.rfind('.', end_pos - 1); // <key>.<upload_id>
    if (mid_pos < 0)
      return false;
    oid = meta.substr(0, mid_pos);
    upload_id = meta.substr(mid_pos + 1, end_pos - mid_pos - 1);
    init(oid, upload_id);
    return true;
  }
  void clear() {
    oid = "";
    prefix = "";
    meta = "";
    upload_id = "";
  }
};

struct RGWMultipartUploadEntry {
  RGWObjEnt obj;
  RGWMPObj mp;

  void clear() {
    obj.clear();
    string empty;
    mp.init(empty, empty);
  }
};

class RGWListBucketMultiparts : public RGWOp {
protected:
  string prefix;
  RGWMPObj marker; 
  RGWMultipartUploadEntry next_marker; 
  int max_uploads;
  string delimiter;
  int ret;
  vector<RGWMultipartUploadEntry> uploads;
  map<string, bool> common_prefixes;
  bool is_truncated;
  int default_max;

public:
  RGWListBucketMultiparts() {
    max_uploads = default_max;
    max_uploads = default_max;
    ret = 0;
    is_truncated = false;
  }
  int verify_permission();
  void execute();

  virtual int get_params() = 0;
  virtual void send_response() = 0;
  virtual const char *name() { return "list_bucket_multiparts"; }
};

class RGWDeleteMultiObj : public RGWOp {
protected:
  int ret;
  int max_to_delete;
  size_t len;
  char *data;
  string bucket_name;
  rgw_bucket bucket;
  bool quiet;
  bool status_dumped;


public:
  RGWDeleteMultiObj() {
    ret = 0;
    max_to_delete = 1000;
    len = 0;
    data = NULL;
    quiet = false;
    status_dumped = false;
  }
  int verify_permission();
  void execute();

  virtual int get_params() = 0;
  virtual void send_status() = 0;
  virtual void begin_response() = 0;
  virtual void send_partial_response(pair<string,int>& result) = 0;
  virtual void end_response() = 0;
  virtual const char *name() { return "multi_object_delete"; }
};


class RGWHandler {
protected:
  struct req_state *s;

  int do_read_permissions(RGWOp *op, bool only_bucket);
  virtual int validate_bucket_name(const string& bucket) = 0;
  virtual int validate_object_name(const string& object) = 0;
public:
  RGWHandler() {}
  virtual ~RGWHandler();
  virtual int init(struct req_state *_s, FCGX_Request *fcgx);
  virtual bool filter_request(struct req_state *s) = 0;

  virtual RGWOp *get_op() = 0;
  virtual void put_op(RGWOp *op) = 0;
  virtual int read_permissions(RGWOp *op) = 0;
  virtual int authorize() = 0;
};

#endif
