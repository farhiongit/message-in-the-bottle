/*******
 * Copyright 2018 Laurent Farhi
 *
 *  This file is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This file is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this file.  If not, see <http://www.gnu.org/licenses/>.
 *****/

////////////////////////////////////////////////////
// Author:  Laurent Farhi
// Date:    08/05/2018
// Contact: lfarhi@sfr.fr
////////////////////////////////////////////////////

#pragma once

#ifndef __BOTTLE_IMPL_H__
#define __BOTTLE_IMPL_H__

#include "bottle.h"
#include <stdlib.h>
#include <errno.h>

#ifdef DEBUG
#  undef DEBUG
#  define DEBUG(stmt) (stmt)
#else
#  define DEBUG(stmt)
#endif

#define QUEUE_IS_FULL(queue) ((queue).reader_head == (queue).writer_head)
#define QUEUE_IS_EMPTY(queue) ((queue).reader_head == 0)
#define QUEUE_CAPACITY(queue) ((queue).capacity)
#define QUEUE_SIZE(queue) ((queue).size)
#define QUEUE_UNLIMITED_CAPACITY_GROWTH_RULE(capacity) ((capacity)*2)

#define DEFINE_BOTTLE1( TYPE )                                                \
  static int  BOTTLE_FILL_##TYPE (BOTTLE_##TYPE *self, TYPE message);         \
  static int  BOTTLE_TRY_FILL_##TYPE (BOTTLE_##TYPE *self, TYPE message);     \
  static int  BOTTLE_DRAIN_##TYPE (BOTTLE_##TYPE *self, TYPE *message);       \
  static int  BOTTLE_TRY_DRAIN_##TYPE (BOTTLE_##TYPE *self, TYPE *message);   \
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
    BOTTLE_CLOSE_##TYPE,                                 \
    BOTTLE_DESTROY_##TYPE,                               \
  };                                                     \
\
  static void QUEUE_INIT_##TYPE (struct _queue_##TYPE *q, size_t capacity) \
  {                                                            \
    q->capacity = (capacity == 0 ? 1 : capacity);              \
    q->unlimited = (capacity == 0);                            \
    q->size = 0;                                               \
    BOTTLE_ASSERT (q->buffer = malloc (q->capacity * sizeof (*q->buffer))); \
    q->reader_head = 0;                                        \
    q->writer_head = q->buffer;                                \
  }                                                            \
\
  static void QUEUE_DISPOSE_##TYPE (struct _queue_##TYPE *q)   \
  {                                                            \
    free (q->buffer);                                          \
  }                                                            \
\
  static int QUEUE_PUSH_##TYPE (struct _queue_##TYPE *q, TYPE message) \
  {                                                            \
    if (QUEUE_IS_FULL (*q) && q->unlimited)                    \
    {                                                          \
      size_t oldc = q->capacity;                               \
      q->capacity = QUEUE_UNLIMITED_CAPACITY_GROWTH_RULE (q->capacity); \
      BOTTLE_ASSERT (q->capacity > oldc);                      \
      TYPE *oldq = q->buffer;                                  \
      BOTTLE_ASSERT (q->buffer = realloc (q->buffer, q->capacity * sizeof (*q->buffer))); \
      q->reader_head = q->buffer + (q->reader_head - oldq) + (q->capacity - oldc);\
      q->writer_head = q->buffer + (q->writer_head - oldq);    \
      for (TYPE* p = q->buffer + q->capacity - 1 ; p >= q->reader_head ; p--) \
        *p = *(p - (q->capacity - oldc));                      \
    }                                                          \
    else if (QUEUE_IS_FULL (*q))                               \
    {                                                          \
      errno = EPERM;                                           \
      return 0;                                                \
    }                                                          \
    *q->writer_head = message; /* hollow copy */               \
    if (!q->reader_head)                                       \
      q->reader_head = q->writer_head;                         \
    q->writer_head++;                                          \
    if (q->writer_head == q->buffer + q->capacity)             \
      q->writer_head = q->buffer;                              \
    q->size++;                                                 \
    return 1;                                                  \
  }                                                            \
\
  static int QUEUE_POP_##TYPE (struct _queue_##TYPE *q, TYPE *message)  \
  {                                                            \
    if (QUEUE_IS_EMPTY (*q))                                   \
    {                                                          \
      errno = EPERM;                                           \
      return 0;                                                \
    }                                                          \
    *message = *q->reader_head; /* hollow copy */              \
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
      TYPE *oldq = q->buffer;                                  \
      BOTTLE_ASSERT (q->buffer = realloc (q->buffer, q->capacity * sizeof (*q->buffer))); \
      q->reader_head = q->buffer + (q->reader_head - oldq);    \
      q->writer_head = q->buffer + (q->writer_head - oldq);    \
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
    BOTTLE_ASSERT (!pthread_mutex_init (&self->mutex, 0));     \
    BOTTLE_ASSERT (!pthread_cond_init (&self->not_empty, 0));  \
    BOTTLE_ASSERT (!pthread_cond_init (&self->not_full, 0));   \
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
    BOTTLE_ASSERT (!pthread_mutex_lock (&self->mutex));        \
    if (self->closed)                                          \
    {                                                          \
      BOTTLE_ASSERT (!pthread_mutex_unlock (&self->mutex));    \
      return errno = ECONNABORTED, ret;                        \
    }                                                          \
    while (!self->closed &&                                    \
           (self->frozen || BOTTLE_IS_FULL (self)))            \
      BOTTLE_ASSERT (!pthread_cond_wait (&self->not_full, &self->mutex));   \
    if (self->closed) /* Again. In case the bottle would have been closed while waiting */ \
    {                                                          \
      BOTTLE_ASSERT (!pthread_mutex_unlock (&self->mutex));    \
      return errno = ECONNABORTED, ret;                        \
    }                                                          \
    else if (!self->frozen && !BOTTLE_IS_FULL (self))          \
    {                                                          \
      QUEUE_PUSH_##TYPE (&self->queue, message);               \
      BOTTLE_ASSERT (!pthread_cond_signal (&self->not_empty)); \
      ret = 1;                                                 \
    }                                                          \
    else                                                       \
      BOTTLE_ASSERT3 (0, "Unexpected.\n", 1);  \
    BOTTLE_ASSERT (!pthread_mutex_unlock (&self->mutex));      \
    return ret;                                                \
  }                                                            \
\
  static int BOTTLE_TRY_FILL_##TYPE (BOTTLE_##TYPE *self, TYPE message) \
  {                                                            \
    int ret = 0;                                               \
    BOTTLE_ASSERT (!pthread_mutex_lock (&self->mutex));        \
    if (self->closed)                                          \
    {                                                          \
      BOTTLE_ASSERT (!pthread_mutex_unlock (&self->mutex));    \
      return errno = ECONNABORTED, ret;                        \
    }                                                          \
    else if (self->frozen)                                     \
    {                                                          \
      BOTTLE_ASSERT (!pthread_mutex_unlock (&self->mutex));    \
      return errno = EWOULDBLOCK, ret;                         \
    }                                                          \
    else if (!BOTTLE_IS_FULL (self))                           \
    {                                                          \
      QUEUE_PUSH_##TYPE (&self->queue, message);               \
      BOTTLE_ASSERT (!pthread_cond_signal (&self->not_empty)); \
      ret = 1;                                                 \
    }                                                          \
    else                                                       \
      errno = EWOULDBLOCK;                                     \
    BOTTLE_ASSERT (!pthread_mutex_unlock (&self->mutex));      \
    return ret;                                                \
  }                                                            \
\
  static int BOTTLE_DRAIN_##TYPE (BOTTLE_##TYPE *self, TYPE *message) \
  {                                                            \
    int ret = 0;                                               \
    BOTTLE_ASSERT (!pthread_mutex_lock (&self->mutex));        \
    while (!self->closed && BOTTLE_IS_EMPTY (self))            \
      BOTTLE_ASSERT (!pthread_cond_wait (&self->not_empty, &self->mutex)); \
    if (!BOTTLE_IS_EMPTY (self))                               \
    {                                                          \
      QUEUE_POP_##TYPE (&self->queue, message);                \
      BOTTLE_ASSERT (!pthread_cond_signal (&self->not_full));  \
      ret = 1;                                                 \
    }                                                          \
    else if (self->closed)                                     \
      errno = ECONNABORTED;                                    \
    else                                                       \
      BOTTLE_ASSERT3 (0, "Unexpected.\n", 1);  \
    BOTTLE_ASSERT (!pthread_mutex_unlock (&self->mutex));      \
    return ret;                                                \
  }                                                            \
\
  static int BOTTLE_TRY_DRAIN_##TYPE (BOTTLE_##TYPE *self, TYPE *message) \
  {                                                            \
    int ret = 0;                                               \
    BOTTLE_ASSERT (!pthread_mutex_lock (&self->mutex));        \
    if (!BOTTLE_IS_EMPTY (self))                               \
    {                                                          \
      QUEUE_POP_##TYPE (&self->queue, message);                \
      BOTTLE_ASSERT (!pthread_cond_signal (&self->not_full));  \
      ret = 1;                                                 \
    }                                                          \
    else if (self->closed)                                     \
      errno = ECONNABORTED;                                    \
    else                                                       \
      errno = EWOULDBLOCK;                                     \
    BOTTLE_ASSERT (!pthread_mutex_unlock (&self->mutex));      \
    return ret;                                                \
  }                                                            \
\
  static void BOTTLE_CLOSE_##TYPE (BOTTLE_##TYPE *self)        \
  {                                                            \
    BOTTLE_ASSERT (!pthread_mutex_lock (&self->mutex));        \
    self->closed = 1;                                          \
    BOTTLE_ASSERT (!pthread_mutex_unlock (&self->mutex));      \
    BOTTLE_ASSERT (!pthread_cond_broadcast (&self->not_empty));\
    BOTTLE_ASSERT (!pthread_cond_broadcast (&self->not_full)); \
  }                                                            \
  \
  static void BOTTLE_CLEANUP_##TYPE (BOTTLE_##TYPE *self)      \
  {                                                            \
    BOTTLE_ASSERT (!pthread_mutex_lock (&self->mutex));        \
    self->closed = 1;                                          \
    BOTTLE_ASSERT3 (BOTTLE_IS_EMPTY (self),                    \
                    "Some '" #TYPE "s' are lost.\n", 0);       \
    BOTTLE_ASSERT (!pthread_mutex_unlock (&self->mutex));      \
    pthread_mutex_destroy (&self->mutex);                      \
    pthread_cond_destroy (&self->not_empty);                   \
    pthread_cond_destroy (&self->not_full);                    \
    QUEUE_DISPOSE_##TYPE (&self->queue);                       \
  }                                                            \
\
  static void BOTTLE_DESTROY_##TYPE (BOTTLE_##TYPE *self)      \
  {                                                            \
    BOTTLE_CLEANUP_##TYPE (self);                              \
    free (self);                                               \
  }                                                            \
  struct __useless_struct_to_allow_trailing_semicolon__

#define DEFINE_BOTTLE0() DEFINE_BOTTLE1(BOTTLE_DUMMY_TYPE)
#define DEFINE_BOTTLE(...) VFUNC(DEFINE_BOTTLE, __VA_ARGS__)

#define BOTTLE_IS_EMPTY(self) (QUEUE_IS_EMPTY((self)->queue))
#define BOTTLE_IS_FULL(self) (!(self)->queue.unlimited && QUEUE_IS_FULL((self)->queue))

#endif
