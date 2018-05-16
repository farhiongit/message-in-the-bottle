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

#ifdef DEBUG
#  undef DEBUG
#  define DEBUG(stmt) (stmt)
#else
#  define DEBUG(stmt)
#endif

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
  BOTTLE_##TYPE *BOTTLE_CREATE_##TYPE( size_t capacity ) \
  {                                                      \
    BOTTLE_##TYPE *b = malloc( sizeof( *b ) );           \
    ASSERT (b);                                          \
                                                         \
    b->vtable = &BOTTLE_VTABLE_##TYPE;                   \
    b->capacity = capacity;                              \
    b->closed = 0;                                       \
    b->frozen = 0;                                       \
    b->__dummy__ = __dummy__##TYPE;                      \
    ASSERT (!pthread_mutex_init (&b->mutex, 0));         \
    ASSERT (!pthread_cond_init (&b->not_empty, 0));      \
    ASSERT (!pthread_cond_init (&b->not_full, 0));       \
                                                         \
    b->queue.size = 0;                                   \
    b->queue.front = 0;                                  \
    b->queue.back = 0;                                   \
                                                         \
    return b;                                            \
  }                                                      \
\
  static void QUEUE_PUSH_##TYPE (struct _queue_##TYPE *q, TYPE message) \
  {                                                            \
    struct _elem_##TYPE *e = malloc (sizeof(*e));              \
    ASSERT (e);                                                \
    e->next = 0;                                               \
    e->v = message;                                            \
    if (q->back)                                               \
    {                                                          \
      q->back->next = e;                                       \
      q->back = e;                                             \
    }                                                          \
    else                                                       \
      q->front = q->back = e;                                  \
    q->size++;                                                 \
  }                                                            \
\
  static int QUEUE_POP_##TYPE (struct _queue_##TYPE *q, TYPE *message)  \
  {                                                            \
    if (q->front)                                              \
    {                                                          \
      struct _elem_##TYPE *e = q->front;                       \
      *message = e->v;                                         \
      if (!(q->front = e->next))                               \
        q->back = 0;                                           \
      free (e);                                                \
      ASSERT (q->size);                                        \
      q->size--;                                               \
      return 1;                                                \
    }                                                          \
    else                                                       \
      return 0;                                                \
  }                                                            \
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
           (self->frozen ||                                    \
            (self->capacity && self->queue.size >= self->capacity))) \
      ASSERT (!pthread_cond_wait (&self->not_full, &self->mutex));   \
    if (self->closed)                                          \
    {                                                          \
      ASSERT (!pthread_mutex_unlock (&self->mutex));           \
      return errno = ECONNABORTED, ret;                        \
    }                                                          \
    if (!self->frozen &&                                       \
        (!self->capacity || self->queue.size < self->capacity))\
    {                                                          \
      QUEUE_PUSH_##TYPE (&self->queue, message);               \
      DEBUG (fprintf (stderr, "Bottle %p level = %zu.\n", self, self->queue.size)); \
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
    if (!self->capacity || self->queue.size < self->capacity)  \
    {                                                          \
      QUEUE_PUSH_##TYPE (&self->queue, message);               \
      DEBUG (fprintf (stderr, "Bottle %p level = %zu.\n", self, self->queue.size)); \
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
    while (!self->closed && self->queue.size == 0)             \
      ASSERT (!pthread_cond_wait (&self->not_empty, &self->mutex)); \
    if (self->queue.size > 0)                                  \
    {                                                          \
      ASSERT (QUEUE_POP_##TYPE (&self->queue, message));       \
      DEBUG (fprintf (stderr, "Bottle %p level = %zu.\n", self, self->queue.size)); \
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
    if (self->queue.size > 0)                                  \
    {                                                          \
      ASSERT (QUEUE_POP_##TYPE (&self->queue, message));       \
      DEBUG (fprintf (stderr, "Bottle %p level = %zu.\n", self, self->queue.size)); \
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
    while (self->queue.size > 0)                               \
      ASSERT (!pthread_cond_wait (&self->not_full, &self->mutex)); \
    ASSERT (!pthread_mutex_unlock (&self->mutex));             \
    ASSERT (!pthread_cond_broadcast (&self->not_empty));       \
    ASSERT (!pthread_cond_broadcast (&self->not_full));        \
  }                                                            \
\
  static void BOTTLE_DESTROY_##TYPE (BOTTLE_##TYPE *self)      \
  {                                                            \
    pthread_mutex_destroy (&self->mutex);                      \
    pthread_cond_destroy (&self->not_empty);                   \
    pthread_cond_destroy (&self->not_full);                    \
    free (self);                                               \
  }                                                            \

#endif
