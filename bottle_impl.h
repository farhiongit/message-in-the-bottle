/*******
 * Copyright 2018-2025 Laurent Farhi
 *
 *  This file is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This file is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this file.  If not, see <http://www.gnu.org/licenses/>.
 *****/

////////////////////////////////////////////////////
// Author:  Laurent Farhi
// Date:    08/05/2018
// Contact: lfarhi@sfr.fr
////////////////////////////////////////////////////

#ifndef __BOTTLE_IMPL_H__
#define __BOTTLE_IMPL_H__

#include "bottle.h"
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>

#ifdef LIMITED_BUFFER
#  undef LIMITED_BUFFER
#  define LIMITED_BUFFER 1
#else
#  define LIMITED_BUFFER 0
#endif

// To be translated.
#define TBT(text) (text)

#define BOTTLE_ASSERT3(cond, msg, fatal) \
  do {\
    if (!(cond)) \
    {\
      const char* _msg = msg; \
      if (_msg && *_msg) fprintf(stderr, TBT("%1$s: %2$s"), fatal ? TBT("FATAL ERROR") : TBT("WARNING"), TBT(_msg));\
      if (fatal) thrd_exit (0) ;\
    }\
  } while(0)
#define BOTTLE_ASSERT(cond) \
  do {\
    if (!(cond)) \
    {\
      fprintf(stderr, TBT("%6$s: Thread %5$#lx: %1$s (condition '%1$s' is false in function %2$s at %3$s:%4$i)\n"),#cond,__func__,__FILE__,__LINE__, thrd_current (), TBT("FATAL ERROR"));\
      thrd_exit (0) ;\
    }\
  } while(0)

#define QUEUE_IS_EXHAUSTED(queue) ((queue).reader_head == (queue).writer_head)
#define QUEUE_IS_FULL(queue) (!((queue).unlimited && (queue).capacity < (size_t) -1) && QUEUE_IS_EXHAUSTED(queue))  // An unbounded queue can't be full (almost)
#define QUEUE_IS_EMPTY(queue) ((queue).reader_head == 0)
#define QUEUE_CAPACITY(queue) ((queue).capacity)
#define QUEUE_SIZE(queue) ((queue).size)
#define QUEUE_UNLIMITED_CAPACITY_GROWTH_RULE(capacity) ((capacity) * 2)

#define DEFINE_BOTTLE( TYPE )                                                 \
  static int  BOTTLE_FILL_##TYPE (BOTTLE_##TYPE *self, TYPE message);         \
  static int  BOTTLE_TRY_FILL_##TYPE (BOTTLE_##TYPE *self, TYPE message);     \
  static int  BOTTLE_DRAIN_##TYPE (BOTTLE_##TYPE *self, TYPE *message);       \
  static int  BOTTLE_TRY_DRAIN_##TYPE (BOTTLE_##TYPE *self, TYPE *message);   \
  static void BOTTLE_PLUG_##TYPE (BOTTLE_##TYPE *self);                       \
  static void BOTTLE_UNPLUG_##TYPE (BOTTLE_##TYPE *self);                     \
  static void BOTTLE_CLOSE_##TYPE (BOTTLE_##TYPE *self);                      \
  static void BOTTLE_DESTROY_##TYPE (BOTTLE_##TYPE *self);                    \
  static TYPE __dummy__##TYPE;                                                \
\
  static const _BOTTLE_VTABLE_##TYPE BOTTLE_VTABLE_##TYPE =  \
  {                                                      \
    BOTTLE_FILL_##TYPE,                                  \
    BOTTLE_TRY_FILL_##TYPE,                              \
    BOTTLE_DRAIN_##TYPE,                                 \
    BOTTLE_TRY_DRAIN_##TYPE,                             \
    BOTTLE_PLUG_##TYPE,                                  \
    BOTTLE_UNPLUG_##TYPE,                                \
    BOTTLE_CLOSE_##TYPE,                                 \
    BOTTLE_DESTROY_##TYPE,                               \
  };                                                     \
\
  static void QUEUE_INIT_##TYPE (struct _queue_##TYPE *q, size_t capacity) \
  {                                                            \
    q->unlimited = (capacity == (size_t) -1);                        \
    BOTTLE_ASSERT3 (!q->unlimited || !LIMITED_BUFFER, "Unauthorised use of UNLIMITED buffer.\n", 1); \
    q->capacity = ((q->unlimited || capacity == 0) ? 1 : capacity);  \
    q->size = 0;                                                     \
    BOTTLE_ASSERT (q->buffer = malloc (q->capacity * sizeof (*q->buffer))); \
    q->reader_head = 0;                                              \
    q->writer_head = q->buffer;                                      \
  }                                                                  \
\
  static void QUEUE_DISPOSE_##TYPE (struct _queue_##TYPE *q)   \
  {                                                            \
    free (q->buffer);                                          \
  }                                                            \
\
  static int QUEUE_PUSH_##TYPE (struct _queue_##TYPE *q, TYPE message) \
  {                                                            \
    if (QUEUE_IS_EXHAUSTED (*q) && q->unlimited && q->capacity < (size_t) -1) \
    {                                                          \
      size_t oldc = q->capacity;                               \
      q->capacity = QUEUE_UNLIMITED_CAPACITY_GROWTH_RULE (q->capacity); \
      if (q->capacity <= oldc) /* overflow ? */                \
        q->capacity = (size_t) -1;                             \
      ptrdiff_t reader_offset = q->reader_head - q->buffer;    \
      ptrdiff_t writer_offset = q->writer_head - q->buffer;    \
      BOTTLE_ASSERT (q->buffer = realloc (q->buffer, q->capacity * sizeof (*q->buffer))); \
      q->reader_head = q->buffer + reader_offset + (q->capacity - oldc); \
      q->writer_head = q->buffer + writer_offset;             \
      for (TYPE* p = q->buffer + q->capacity - 1 ; p >= q->reader_head ; p--) \
        *p = *(p - (q->capacity - oldc));                      \
    }                                                          \
    if (QUEUE_IS_EXHAUSTED (*q))                               \
    {                                                          \
      errno = EPERM;                                           \
      return 0;                                                \
    }                                                          \
    *q->writer_head = message; /* copy */                      \
    if (!q->reader_head)                                       \
      q->reader_head = q->writer_head;                         \
    q->writer_head++;                                          \
    if (q->writer_head == q->buffer + q->capacity)             \
      q->writer_head = q->buffer;                              \
    q->size++;                                                 \
    return 1;                                                  \
  }                                                            \
\
  static int QUEUE_POP_##TYPE (struct _queue_##TYPE *q, TYPE *message) \
  {                                                            \
    if (QUEUE_IS_EMPTY (*q))                                   \
    {                                                          \
      errno = EPERM;                                           \
      return 0;                                                \
    }                                                          \
    *message = *q->reader_head; /* copy */                     \
    q->reader_head++;                                          \
    if (q->reader_head == q->buffer + q->capacity)             \
      q->reader_head = q->buffer;                              \
    if (q->reader_head == q->writer_head) /* empty queue */    \
      q->reader_head = 0;                                      \
    q->size--;                                                 \
    if (q->unlimited && q->size && q->reader_head &&           \
        QUEUE_UNLIMITED_CAPACITY_GROWTH_RULE (q->size) <= q->capacity) \
    {                                                          \
      size_t oldc = q->capacity;                               \
      q->capacity = q->size;                                   \
      if (q->reader_head >= q->writer_head + (oldc - q->capacity)) \
      {                                                        \
        q->reader_head = q->reader_head - (oldc - q->capacity);\
        for (TYPE* p = q->reader_head ; p < q->buffer + q->capacity ; p++) \
          *p = *(p + (oldc - q->capacity));                    \
      }                                                        \
      else if (q->reader_head < q->writer_head)                \
      {                                                        \
        for (TYPE* p = q->reader_head ; p < q->writer_head ; p++) \
          *(q->buffer + (p - q->reader_head)) = *p;            \
        q->writer_head = q->buffer + (q->writer_head - q->reader_head); \
        q->reader_head = q->buffer;                            \
      }                                                        \
      else                                                     \
        BOTTLE_ASSERT3 (0, "Unexpected.\n", 1);                \
      if (q->writer_head == q->buffer + q->capacity)           \
        q->writer_head = q->buffer;                            \
      ptrdiff_t reader_offset = q->reader_head - q->buffer;    \
      ptrdiff_t writer_offset = q->writer_head - q->buffer;    \
      BOTTLE_ASSERT (q->buffer = realloc (q->buffer, q->capacity * sizeof (*q->buffer))); \
      q->reader_head = q->buffer + reader_offset;              \
      q->writer_head = q->buffer + writer_offset;              \
    }                                                          \
    return 1;                                                  \
  }                                                            \
\
  void BOTTLE_INIT_##TYPE (BOTTLE_##TYPE *self, size_t capacity) \
  {                                                            \
    self->vtable = &BOTTLE_VTABLE_##TYPE;                      \
    self->closed = 0;                                          \
    self->frozen = 0;                                          \
    self->__dummy__ = __dummy__##TYPE;                         \
    BOTTLE_ASSERT (mtx_init (&self->mutex, mtx_plain) == thrd_success); \
    BOTTLE_ASSERT (cnd_init (&self->not_empty) == thrd_success); \
    BOTTLE_ASSERT (cnd_init (&self->not_full) == thrd_success);  \
    self->not_reading = self->not_writing = 1;                 \
    BOTTLE_ASSERT (cnd_init (&self->reading) == thrd_success); \
    BOTTLE_ASSERT (cnd_init (&self->writing) == thrd_success); \
    self->capacity = capacity;                                 \
    QUEUE_INIT_##TYPE (&self->queue, capacity);                \
  }                                                            \
\
  BOTTLE_##TYPE *BOTTLE_CREATE_##TYPE (size_t capacity)  \
  {                                                      \
    BOTTLE_##TYPE *b = malloc( sizeof( *b ) );           \
    BOTTLE_ASSERT (b);                                   \
                                                         \
    BOTTLE_INIT_##TYPE (b, capacity);                    \
                                                         \
    return b;                                            \
  }                                                      \
\
  static int BOTTLE_FILL_##TYPE (BOTTLE_##TYPE *self, TYPE message) \
  {                                                            \
    int ret = 0;                                               \
    BOTTLE_ASSERT (mtx_lock (&self->mutex) == thrd_success);   \
    if (!self->closed && self->capacity == 0) /* unbuffered */ \
    /* Barrier to synchronise the sender and the receiver */   \
    {                                                          \
      /* The thread declares it is attempting to write */      \
      self->not_writing = 0;                                   \
      BOTTLE_ASSERT (cnd_signal (&self->writing) == thrd_success); \
      /* blocks until there is another thread attempting to receive a message ... */ \
      while (!self->closed && self->not_reading)               \
        BOTTLE_ASSERT (cnd_wait (&self->reading, &self->mutex) == thrd_success); \
      self->not_reading = 1; /* The writer declares the end of reading (asap to avoid writing twice) */ \
    }                                                          \
    while (!self->closed &&                                    \
           (self->frozen || QUEUE_IS_FULL (self->queue)))      \
      BOTTLE_ASSERT (cnd_wait (&self->not_full, &self->mutex) == thrd_success); \
    if (self->closed)                                          \
    {                                                          \
      BOTTLE_ASSERT (mtx_unlock (&self->mutex) == thrd_success); \
      return self->not_writing = 1, errno = ECONNABORTED, ret; \
    }                                                          \
    else if (!self->frozen && !QUEUE_IS_FULL (self->queue))    \
    {                                                          \
      QUEUE_PUSH_##TYPE (&self->queue, message);               \
      BOTTLE_ASSERT (cnd_signal (&self->not_empty) == thrd_success); \
      if (self->capacity == 0) /* unbuffered */                \
        /* ... at which point the receiving thread gets the message and both threads continue execution */ \
        while (QUEUE_IS_FULL (self->queue))                    \
          BOTTLE_ASSERT (cnd_wait (&self->not_full, &self->mutex) == thrd_success); \
      ret = 1;                                                 \
    }                                                          \
    else                                                       \
      BOTTLE_ASSERT3 (0, "Unexpected.\n", 1);                  \
    BOTTLE_ASSERT (mtx_unlock (&self->mutex) == thrd_success); \
    return ret;                                                \
  }                                                            \
\
  static int BOTTLE_TRY_FILL_##TYPE (BOTTLE_##TYPE *self, TYPE message) \
  {                                                            \
    int ret = 0;                                               \
    BOTTLE_ASSERT (mtx_lock (&self->mutex) == thrd_success);   \
    if (self->closed)                                          \
    {                                                          \
      BOTTLE_ASSERT (mtx_unlock (&self->mutex) == thrd_success); \
      return self->not_writing = 1, errno = ECONNABORTED, ret; \
    }                                                          \
    if (self->frozen || (self->capacity == 0 /* unbuffered */ && self->not_reading)) \
    {                                                          \
      BOTTLE_ASSERT (mtx_unlock (&self->mutex) == thrd_success); \
      return ret;                                              \
    }                                                          \
    if (!QUEUE_IS_FULL (self->queue))                          \
    {                                                          \
      QUEUE_PUSH_##TYPE (&self->queue, message);               \
      BOTTLE_ASSERT (cnd_signal (&self->not_empty) == thrd_success); \
      if (self->capacity == 0 /* unbuffered */ && !self->not_reading)\
      {                                                        \
        /* The thread declares it is attempting to write */    \
        self->not_writing = 0;                                 \
        BOTTLE_ASSERT (cnd_signal (&self->writing) == thrd_success); \
        self->not_reading = 1;                                 \
        /* Wait for the receiving (reading) thread to get the message */ \
        while (QUEUE_IS_FULL (self->queue))                    \
          BOTTLE_ASSERT (cnd_wait (&self->not_full, &self->mutex) == thrd_success); \
      }                                                        \
      ret = 1;                                                 \
    }                                                          \
    BOTTLE_ASSERT (mtx_unlock (&self->mutex) == thrd_success); \
    return ret;                                                \
  }                                                            \
\
  static int BOTTLE_DRAIN_##TYPE (BOTTLE_##TYPE *self, TYPE *message) \
  {                                                            \
    int ret = 0;                                               \
    BOTTLE_ASSERT (mtx_lock (&self->mutex) == thrd_success);   \
    /* Barrier to synchronise the sender and the receiver */   \
    if (!self->closed && self->capacity == 0) /* unbuffered */ \
    {                                                          \
      /* The thread declares it is attempting to read */       \
      self->not_reading = 0;                                   \
      BOTTLE_ASSERT (cnd_signal (&self->reading) == thrd_success); \
      /* blocks until there is another thread attempting to send a message,
         at which point both threads continue execution. */    \
      while (!self->closed && self->not_writing)               \
        BOTTLE_ASSERT (cnd_wait (&self->writing, &self->mutex) == thrd_success); \
      self->not_writing = 1; /* The reader declares the end of writing (at once to avoid reading twice) */ \
    }                                                          \
    while (!self->closed && QUEUE_IS_EMPTY (self->queue))      \
      BOTTLE_ASSERT (cnd_wait (&self->not_empty, &self->mutex) == thrd_success); \
    if (!QUEUE_IS_EMPTY (self->queue))                         \
    {                                                          \
      QUEUE_POP_##TYPE (&self->queue, message);                \
      BOTTLE_ASSERT (cnd_signal (&self->not_full) == thrd_success); \
      ret = 1;                                                 \
    }                                                          \
    else if (self->closed)                                     \
      self->not_reading = 1, errno = ECONNABORTED;             \
    else                                                       \
      BOTTLE_ASSERT3 (0, "Unexpected.\n", 1);                  \
    BOTTLE_ASSERT (mtx_unlock (&self->mutex) == thrd_success); \
    return ret;                                                \
  }                                                            \
\
  static int BOTTLE_TRY_DRAIN_##TYPE (BOTTLE_##TYPE *self, TYPE *message) \
  {                                                            \
    int ret = 0;                                               \
    BOTTLE_ASSERT (mtx_lock (&self->mutex) == thrd_success);   \
    if (self->closed && QUEUE_IS_EMPTY (self->queue))          \
    {                                                          \
      BOTTLE_ASSERT (mtx_unlock (&self->mutex) == thrd_success); \
      return self->not_reading = 1, errno = ECONNABORTED, ret; \
    }                                                          \
    if (self->capacity == 0 /* unbuffered */ && self->not_writing) \
    {                                                          \
      BOTTLE_ASSERT (mtx_unlock (&self->mutex) == thrd_success); \
      return ret;                                              \
    }                                                          \
    if (self->capacity == 0 /* unbuffered */ && !self->not_writing) \
    {                                                          \
      /* The thread declares it is attempting to read */       \
      self->not_reading = 0;                                   \
      BOTTLE_ASSERT (cnd_signal (&self->reading) == thrd_success); \
      while (!self->closed && QUEUE_IS_EMPTY (self->queue))    \
        BOTTLE_ASSERT (cnd_wait (&self->not_empty, &self->mutex) == thrd_success); \
      self->not_writing = 1;                                   \
    }                                                          \
    if (!QUEUE_IS_EMPTY (self->queue))                         \
    {                                                          \
      QUEUE_POP_##TYPE (&self->queue, message);                \
      BOTTLE_ASSERT (cnd_signal (&self->not_full) == thrd_success); \
      ret = 1;                                                 \
    }                                                          \
    else if (self->closed)                                     \
    {                                                          \
      BOTTLE_ASSERT (mtx_unlock (&self->mutex) == thrd_success); \
      return self->not_reading = 1, errno = ECONNABORTED, ret; \
    }                                                          \
    BOTTLE_ASSERT (mtx_unlock (&self->mutex) == thrd_success); \
    return ret;                                                \
  }                                                            \
\
  static void BOTTLE_PLUG_##TYPE (BOTTLE_##TYPE *self)         \
  {                                                            \
    BOTTLE_ASSERT (mtx_lock (&self->mutex) == thrd_success);   \
    self->frozen = 1;                                          \
    BOTTLE_ASSERT (mtx_unlock (&self->mutex) == thrd_success); \
  }                                                            \
\
  static void BOTTLE_UNPLUG_##TYPE (BOTTLE_##TYPE *self)       \
  {                                                            \
    BOTTLE_ASSERT (mtx_lock (&self->mutex) == thrd_success);   \
    self->frozen = 0;                                          \
    BOTTLE_ASSERT (mtx_unlock (&self->mutex) == thrd_success); \
    BOTTLE_ASSERT (cnd_signal (&self->not_full) == thrd_success); \
  }                                                            \
\
  static void BOTTLE_CLOSE_##TYPE (BOTTLE_##TYPE *self)        \
  {                                                            \
    BOTTLE_ASSERT (mtx_lock (&self->mutex) == thrd_success);   \
    self->closed = 1;                                          \
    BOTTLE_ASSERT (mtx_unlock (&self->mutex) == thrd_success); \
    BOTTLE_ASSERT (cnd_broadcast (&self->not_empty) == thrd_success);\
    BOTTLE_ASSERT (cnd_broadcast (&self->not_full) == thrd_success); \
    BOTTLE_ASSERT (cnd_broadcast (&self->reading) == thrd_success);  \
    BOTTLE_ASSERT (cnd_broadcast (&self->writing) == thrd_success);  \
  }                                                            \
  \
  static void BOTTLE_CLEANUP_##TYPE (BOTTLE_##TYPE *self)      \
  {                                                            \
    BOTTLE_ASSERT (mtx_lock (&self->mutex) == thrd_success);   \
    BOTTLE_ASSERT3 (QUEUE_IS_EMPTY (self->queue),              \
                    "Some '" #TYPE "s' have been lost.\n", 0); \
    BOTTLE_ASSERT (mtx_unlock (&self->mutex) == thrd_success); \
    mtx_destroy (&self->mutex);                                \
    cnd_destroy (&self->not_empty);                            \
    cnd_destroy (&self->not_full);                             \
    cnd_destroy (&self->reading);                              \
    cnd_destroy (&self->writing);                              \
    QUEUE_DISPOSE_##TYPE (&self->queue);                       \
  }                                                            \
\
  static void BOTTLE_DESTROY_##TYPE (BOTTLE_##TYPE *self)      \
  {                                                            \
    BOTTLE_CLEANUP_##TYPE (self);                              \
    free (self);                                               \
  }                                                            \
  struct __useless_struct_to_allow_trailing_semicolon__

#endif
