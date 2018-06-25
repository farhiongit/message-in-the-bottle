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

#define DEFINE_BOTTLE( TYPE )                                                 \
  static int  BOTTLE_FILL_##TYPE (BOTTLE_##TYPE *self, TYPE message);         \
  static int  BOTTLE_TRY_FILL_##TYPE (BOTTLE_##TYPE *self, TYPE message);     \
  static int  BOTTLE_DRAIN_##TYPE (BOTTLE_##TYPE *self, TYPE *message);       \
  static int  BOTTLE_TRY_DRAIN_##TYPE (BOTTLE_##TYPE *self, TYPE *message);   \
  static void BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY_##TYPE (BOTTLE_##TYPE *self); \
  static void BOTTLE_DESTROY_##TYPE (BOTTLE_##TYPE *self);                    \
  static TYPE __dummy__##TYPE;                                                \
\
  static const _BOTTLE_VTABLE_##TYPE BOTTLE_VTABLE_##TYPE =  \
  {                                                      \
    BOTTLE_FILL_##TYPE,                                  \
    BOTTLE_TRY_FILL_##TYPE,                              \
    BOTTLE_DRAIN_##TYPE,                                 \
    BOTTLE_TRY_DRAIN_##TYPE,                             \
    BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY_##TYPE,            \
    BOTTLE_DESTROY_##TYPE,                               \
  };                                                     \
\
  static void QUEUE_INIT_##TYPE (struct _queue_##TYPE *q, size_t capacity) \
  {                                                            \
    ASSERT (capacity);                                         \
    q->capacity = capacity;                                    \
    q->size = 0;                                               \
    ASSERT (q->buffer = malloc (capacity * sizeof (*q->buffer))); \
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
    if (QUEUE_IS_FULL (*q))                                    \
    {                                                          \
      errno = EPERM;                                           \
      return 0;                                                \
    }                                                          \
    *q->writer_head = message;                                 \
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
    *message = *q->reader_head;                                \
    q->reader_head++;                                          \
    if (q->reader_head == q->buffer + q->capacity)             \
      q->reader_head = q->buffer;                              \
    if (q->reader_head == q->writer_head)                      \
      q->reader_head = 0;                                      \
    q->size--;                                                 \
    return 1;                                                  \
  }                                                            \
\
  BOTTLE_##TYPE *BOTTLE_CREATE_##TYPE( size_t capacity ) \
  {                                                      \
    BOTTLE_##TYPE *b = malloc( sizeof( *b ) );           \
    ASSERT (b);                                          \
                                                         \
    b->vtable = &BOTTLE_VTABLE_##TYPE;                   \
    b->closed = 0;                                       \
    b->frozen = 0;                                       \
    b->__dummy__ = __dummy__##TYPE;                      \
    ASSERT (!pthread_mutex_init (&b->mutex, 0));         \
    ASSERT (!pthread_cond_init (&b->not_empty, 0));      \
    ASSERT (!pthread_cond_init (&b->not_full, 0));       \
    QUEUE_INIT_##TYPE (&b->queue, capacity);             \
                                                         \
    return b;                                            \
  }                                                      \
\
  static int BOTTLE_FILL_##TYPE (BOTTLE_##TYPE *self, TYPE message) \
  {                                                            \
    int ret = 0;                                               \
    ASSERT (!pthread_mutex_lock (&self->mutex));               \
    if (self->closed)                                          \
    {                                                          \
      ASSERT (!pthread_mutex_unlock (&self->mutex));           \
      return errno = ECONNABORTED, ret;                        \
    }                                                          \
    while (!self->closed &&                                    \
           (self->frozen || QUEUE_IS_FULL (self->queue)))      \
      ASSERT (!pthread_cond_wait (&self->not_full, &self->mutex));   \
    if (self->closed)                                          \
    {                                                          \
      ASSERT (!pthread_mutex_unlock (&self->mutex));           \
      return errno = ECONNABORTED, ret;                        \
    }                                                          \
    if (!self->frozen && !QUEUE_IS_FULL (self->queue))         \
    {                                                          \
      QUEUE_PUSH_##TYPE (&self->queue, message);               \
      DEBUG (fprintf (stderr, "Bottle %p level = %zu.\n", self, BOTTLE_LEVEL (self))); \
      ASSERT (!pthread_cond_signal (&self->not_empty));        \
      ret = 1;                                                 \
    }                                                          \
    ASSERT (!pthread_mutex_unlock (&self->mutex));             \
    return ret;                                                \
  }                                                            \
\
  static int BOTTLE_TRY_FILL_##TYPE (BOTTLE_##TYPE *self, TYPE message) \
  {                                                            \
    int ret = 0;                                               \
    ASSERT (!pthread_mutex_lock (&self->mutex));               \
    if (self->closed)                                          \
    {                                                          \
      ASSERT (!pthread_mutex_unlock (&self->mutex));           \
      return errno = ECONNABORTED, ret;                        \
    }                                                          \
    if (self->frozen)                                          \
    {                                                          \
      ASSERT (!pthread_mutex_unlock (&self->mutex));           \
      return errno = EWOULDBLOCK, ret;                         \
    }                                                          \
    if (!QUEUE_IS_FULL (self->queue))                          \
    {                                                          \
      QUEUE_PUSH_##TYPE (&self->queue, message);               \
      DEBUG (fprintf (stderr, "Bottle %p level = %zu.\n", self, BOTTLE_LEVEL (self))); \
      ASSERT (!pthread_cond_signal (&self->not_empty));        \
      ret = 1;                                                 \
    }                                                          \
    else                                                       \
      errno = EWOULDBLOCK;                                     \
    ASSERT (!pthread_mutex_unlock (&self->mutex));             \
    return ret;                                                \
  }                                                            \
\
  static int BOTTLE_DRAIN_##TYPE (BOTTLE_##TYPE *self, TYPE *message) \
  {                                                            \
    int ret = 0;                                               \
    ASSERT (!pthread_mutex_lock (&self->mutex));               \
    while (!self->closed && QUEUE_IS_EMPTY (self->queue))      \
      ASSERT (!pthread_cond_wait (&self->not_empty, &self->mutex)); \
    if (!QUEUE_IS_EMPTY (self->queue))                         \
    {                                                          \
      QUEUE_POP_##TYPE (&self->queue, message);                \
      DEBUG (fprintf (stderr, "Bottle %p level = %zu.\n", self, BOTTLE_LEVEL (self))); \
      ASSERT (!pthread_cond_signal (&self->not_full));         \
      ret = 1;                                                 \
    }                                                          \
    else if (self->closed)                                     \
      errno = ECONNABORTED;                                    \
    ASSERT (!pthread_mutex_unlock (&self->mutex));             \
    return ret;                                                \
  }                                                            \
\
  static int BOTTLE_TRY_DRAIN_##TYPE (BOTTLE_##TYPE *self, TYPE *message) \
  {                                                            \
    int ret = 0;                                               \
    ASSERT (!pthread_mutex_lock (&self->mutex));               \
    if (!QUEUE_IS_EMPTY (self->queue))                         \
    {                                                          \
      QUEUE_POP_##TYPE (&self->queue, message);                \
      DEBUG (fprintf (stderr, "Bottle %p level = %zu.\n", self, BOTTLE_LEVEL (self))); \
      ASSERT (!pthread_cond_signal (&self->not_full));         \
      ret = 1;                                                 \
    }                                                          \
    else if (self->closed)                                     \
      errno = ECONNABORTED;                                    \
    else                                                       \
      errno = EWOULDBLOCK;                                     \
    ASSERT (!pthread_mutex_unlock (&self->mutex));             \
    return ret;                                                \
  }                                                            \
\
  static void BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY_##TYPE (BOTTLE_##TYPE *self) \
  {                                                            \
    ASSERT (!pthread_mutex_lock (&self->mutex));               \
    self->closed = 1;                                          \
    while (!QUEUE_IS_EMPTY (self->queue))                      \
      ASSERT (!pthread_cond_wait (&self->not_full, &self->mutex)); \
    ASSERT (!pthread_mutex_unlock (&self->mutex));             \
    ASSERT (!pthread_cond_broadcast (&self->not_empty));       \
    ASSERT (!pthread_cond_broadcast (&self->not_full));        \
  }                                                            \
  \
  static void BOTTLE_CLEANUP_##TYPE (BOTTLE_##TYPE *self)      \
  {                                                            \
    ASSERT (!pthread_mutex_lock (&self->mutex));               \
    self->closed = 1;                                          \
    ASSERT (!pthread_mutex_unlock (&self->mutex));             \
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
#endif
