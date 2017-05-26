/*
 * Varnish "calmdown" vmod
 * This module offers a simple way to rate-limit calls to a specific resource.
 * The code is based on the Token-Bucket algorithm (http://en.wikipedia.org/wiki/Token_bucket)
 * and takes some cues from various sources (mainly varnish's own vmod repository on github)
 *
 * All code is released under the terms of the GNU General Public License version 3.
 */

#include <stdio.h>
#include <stdlib.h>

#include "vcl.h"
#include "vrt.h"
#include "tokenbucket.h"
#include "yamlparser.h"

#include <sys/time.h>
#include "vcc_if.h"

#define TRUE   1
#define FALSE  0

// config file
#define CFGFILE  "/etc/vmod-calmdown/calmdown.yaml"
FILE *yaml_config_file_descriptor;

// bucket list struct
struct __bucketList {
  pthread_mutex_t list_mutex;
  bucket *listHead;
  int gc_count;
};
typedef struct __bucketList bucketList;

// list group
bucketList *buckets;
// seek bucket list pointers
bucketList *get_bucket(int index) {
  return (buckets + (index * sizeof(struct __bucketList)));
}

// global module variables
static pthread_mutex_t global_initialization_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned int need_init = TRUE;

// calculate and update token bucket size
static void calc_tokens(bucket *b, double now) {
  double delta = now - b->lastAccess;
  #ifdef DEBUG_BUCKETQUEUE
    printf("vmod_calmdown.c: calc_tokens() called, current timestamp %f, lastAccess timestamp %f...\n", now, b->lastAccess);
  #endif

  // update tokens with respect to relative delay in requests
  b->tokens += (double) ((delta / b->capacity) * b->ratio);
  if (b->tokens > b->capacity)
    b->tokens = b->capacity;
}

// get timestamp for the current request from varnish loop
// timestamp is got from varnish request context
static double get_ts_now(const struct vrt_ctx *ctx) {
  double now;

  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  if (ctx->req) {
    CHECK_OBJ_NOTNULL(ctx->req, REQ_MAGIC);
    now = ctx->req->t_prev;
  } else {
    CHECK_OBJ_NOTNULL(ctx->bo, BUSYOBJ_MAGIC);
    now = ctx->bo->t_prev;
  }

  return (now);
}

// handle search and allocation of new buckets
bucket *handle_bucket(unsigned char hash[DIGEST_LEN], VCL_STRING requester, VCL_STRING resource, VCL_INT ratio, VCL_DURATION capacity, double now, unsigned int digest_length, bucketList *headOfList) {
  bucket *item;

  // search for an already allocated bucket...
  #ifdef DEBUG_BUCKETQUEUE
    printf("vmod_calmdown.c: handle_bucket() called, searching bucket...\n");
  #endif
  item = searchBucket(headOfList->listHead, hash, digest_length);
  #ifdef DEBUG_BUCKETQUEUE
    printf("vmod_calmdown.c: handle_bucket() called, item is at: %X\n", item);
  #endif

  // ok, return address or allocate new bucket
  if (item) {
    return item;
  } else {
    // allocate and insert new bucket
    item = allocateBucket(hash, (unsigned char *)requester, (unsigned char *)resource, digest_length, ratio, capacity);
    headOfList->listHead = addBucket(item, headOfList->listHead);

    // return new address
    return item;
  }
}

// garbage collector.
// cleans dead entries
static void run_gc(double now, unsigned part) {
  bucketList *v = get_bucket(part);
  #ifdef DEBUG_BUCKETQUEUE
    printf("vmod_calmdown.c: run_gc() called, starting at: 0x%X\n", v->listHead);
  #endif

  //run garbage collector
  cleanBucketQueue(v->listHead, now);
  // unlock mutex
}

// main ban function and parameters
VCL_BOOL vmod_calmdown(const struct vrt_ctx *ctx, VCL_STRING requester, VCL_STRING resource, VCL_INT ratio, VCL_DURATION capacity) {
  unsigned ret = 1;

  // composite requester variable.
  // the bucket requester is "key" from the VCL + "resource" from the VCL
  // for example: client.identity + req.url --> "192.168.0.1" + "/api/resource"
  unsigned char *compound_requester = NULL;
  unsigned int compound_size;

  // requester bucket
  bucket *b;

  // get timestamp from request context
  double now = get_ts_now(ctx);

  // bucket list header
  bucketList *v;
  unsigned char digest[DIGEST_LEN];
  unsigned part;

  if (!requester)
    return (1);

  // build compound requester
  compound_size = strlen(requester) + strlen(resource) + 1;
  compound_requester = (unsigned char*)malloc(compound_size);
  // handle OOM
  if (compound_requester == NULL)
      return (1);

  bzero(compound_requester, compound_size);
  memcpy(compound_requester, requester, strlen(requester));
  memcpy(compound_requester + strlen(requester), resource, strlen(resource));

  // assert MAX_BUCKET_LISTS is a power of 2
  if (global_opts.partitions & (global_opts.partitions -1))
      return (1);

  // initialize SHA256 hash engine
  SHA256_CTX sctx;
  // calculate SHA256 digest of requester (ip address, url location, URI....)
  SHA256_Init(&sctx);
  SHA256_Update(&sctx, compound_requester, compound_size);
  SHA256_Final(digest, &sctx);

  // select list based on hash.
  // use modular arithmetics:
  //
  // X % Y (where y is a power of 2) is equivalent to: X & (Y-1)
  //
  // Take Y=32 for example (which is 2^5) and Y-1=31
  // 32 dec == 00100000 bin
  // 31 dec == 00011111 bin
  //
  // so, any multiple of a number like Y=2^N must end with N bits set as 0
  // the remainder of a division by Y=2^N is the same number with all bits discarded except for
  // the last N
  //
  // by AND-ing the original number with 2^N-1, you mask out all bits except for the N-1 bits
  // significant for the remainder (== 0 if multiple, != 0 if not multiple)
  //
  // get bucket list (16 bit requester..)
  part = (digest[0] << 8 | digest[1]) & (global_opts.partitions - 1);
  v = get_bucket(part);
  #ifdef DEBUG_BUCKETQUEUE
    printf("vmod_calmdown.c: calmdown(): Selected hash container ID %d\n", part);
    printf("vmod_calmdown.c: calmdown(): Selected bucketList 0x%X \n", v);
    printf("vmod_calmdown.c: calmdown(): Selected listHead is at 0x%X \n", v->listHead);
  #endif

  // lock queue mutex
  AZ(pthread_mutex_lock(&v->list_mutex));

  // search and get relevant bucket and calculate tokens
  // if requester is new, calculate SHA256 hash and allocate a new bucket.
  b = handle_bucket(digest, requester, resource, ratio, capacity, now, DIGEST_LEN, v);
  calc_tokens(b, now);
  if (b->tokens > 0) {
    b->tokens -= 1;
    ret = 0;
    b->lastAccess = now;
  }

  #ifdef DEBUG_BUCKETQUEUE
    printf("calmdown(): ratio %f, timedelta %f, tokens %f, capacity %f\n", b->ratio, (now - b->lastAccess), b->tokens, b->capacity);
  #endif

  // run garbage collector....
  v->gc_count++;
  #ifdef DEBUG_BUCKETQUEUE
    printf("vmod_calmdown.c: calmdown(): %d requests done, %d more to trigger Garbage Collection.\n", v->gc_count, global_opts.gc_interval - v->gc_count);
  #endif
  if (v->gc_count == global_opts.gc_interval) {
    #ifdef DEBUG_BUCKETQUEUE
      printf("vmod_calmdown.c: calmdown(): Entering Garbage Collection...\n");
    #endif

    run_gc(now, part);
    v->gc_count = 0;

    #ifdef DEBUG_BUCKETQUEUE
      printf("vmod_calmdown.c: calmdown(): GC END: %d requests done, %d more to trigger Garbage Collection.\n", v->gc_count, global_opts.gc_interval - v->gc_count);
    #endif
  }

  // free resources
  if (compound_requester != NULL)
      free(compound_requester);

  // unlock queue mutex
  AZ(pthread_mutex_unlock(&v->list_mutex));
  return (ret);
}

// module unload cleanup function
static void calmdown_deinit(struct vmod_priv *priv) {
  assert(priv->priv == &need_init);

  // lock global init mutex
  #ifdef DEBUG_BUCKETQUEUE
    printf("vmod_calmdown.c: calmdown_deinit(): Cleaning up vmod_calmdown...\n");
  #endif

  AZ(pthread_mutex_lock(&global_initialization_mutex));
  assert(need_init > 0);

  need_init = FALSE;
  // free resources
  if (need_init == 0) {
    unsigned p;
    // free bucket lists
    for (p = 0; p < global_opts.partitions; p++ ) {
      bucketList *v = get_bucket(p);
      #ifdef DEBUG_BUCKETQUEUE
        printf("vmod_calmdown.c: calmdown_deinit(): Freeing Bucket Queue address 0x%X...\n", v->listHead);
      #endif
      freeBucketQueue(v->listHead);
    }
    // free buckets
    free(buckets);
  }

  // unlock global init mutex
  AZ(pthread_mutex_unlock(&global_initialization_mutex));
}

// initialization function
static int calmdown_prepare(struct vmod_priv *priv) {
  priv->priv = &need_init;
  priv->free = calmdown_deinit;

  // lock global init mutex
  AZ(pthread_mutex_lock(&global_initialization_mutex));

  // open configuration file...
  yaml_config_file_descriptor = load_yaml_file(CFGFILE);
  if (yaml_config_file_descriptor != NULL) {
    parse_yaml_file(yaml_config_file_descriptor);
  } else {
    #ifdef DEBUG_BUCKETQUEUE
      printf("vmod_calmdown.c: calmdown_prepare(): Failed to open config file, fallback to defaults...\n");
    #endif
    global_opts.gc_interval = 100;
    global_opts.partitions = 1;
  }

  // allocate buckets and init mutexes
  if (need_init == TRUE) {
    int p;

    #ifdef DEBUG_BUCKETQUEUE
      printf("vmod_calmdown.c: calmdown_prepare(): Allocating space for %d bucket lists...\n", global_opts.partitions);
    #endif

    buckets = (bucketList *)malloc(global_opts.partitions * sizeof(struct __bucketList));

    for (p = 0; p < global_opts.partitions; p++) {
      AZ(pthread_mutex_init(&(get_bucket(p)->list_mutex), NULL));
      #ifdef DEBUG_BUCKETQUEUE
        printf("vmod_calmdown.c: calmdown_prepare(): Allocating Bucket Queue Slice %d...\n", p);
      #endif
      get_bucket(p)->listHead = NULL;
      get_bucket(p)->gc_count = 0;
    }
  }
  need_init = FALSE;

  // close yaml file descriptor
  if (yaml_config_file_descriptor != NULL)
    close_yaml_file(yaml_config_file_descriptor);

  // unlock global init mutex
  AZ(pthread_mutex_unlock(&global_initialization_mutex));
  return (0);
}

// module load init function
int calmdown_init(const struct vrt_ctx *ctx, struct vmod_priv *priv, enum vcl_event_e e) {
  (void) ctx;
  (void) priv;
  // implement event callbacks:
  switch(e) {
    case VCL_EVENT_LOAD:
      #ifdef DEBUG_BUCKETQUEUE
        printf("vmod_calmdown.c: calmdown_init(): Handling VCL_EVENT_LOAD.");
      #endif

      calmdown_prepare(priv);
      break;
    case VCL_EVENT_DISCARD:
      #ifdef DEBUG_BUCKETQUEUE
        printf("vmod_calmdown.c: calmdown_init(): Handling VCL_EVENT_DISCARD.");
      #endif

      calmdown_deinit(priv);
      break;
    default:
      #ifdef DEBUG_BUCKETQUEUE
        printf("vmod_calmdown.c: calmdown_init(): default event.");
      #endif
      return(0);
  }
}


