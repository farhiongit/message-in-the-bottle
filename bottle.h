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
// Date:    08/05/2017
// Contact: lfarhi@sfr.fr
////////////////////////////////////////////////////

/* A thread-safe, template, implementation of a message queue */

#pragma once

#ifndef __BOTTLE_H__
#define __BOTTLE_H__

#include "vfunc.h"
#include <pthread.h>
#include <errno.h>

// To be translated.
#define TBT(text) (text)

#define ASSERT(cond) \
  do {\
    if (!(cond)) \
    {\
      fprintf(stderr, TBT("Thread %5$#lx: %1$s (condition '%1$s' is false in function %2$s at %3$s:%4$i)\n"),#cond,__func__,__FILE__,__LINE__, pthread_self());\
      pthread_exit(0) ;\
    }\
  } while(0)

enum { INFINITE_CAPACITY = 0, UNBUFFERED = 1 };

#define DECLARE_BOTTLE( TYPE )     \
\
  struct _BOTTLE_##TYPE;           \
\
  typedef struct _BOTTLE_VTABLE_##TYPE                            \
  {                                                               \
    int (*Fill) (struct _BOTTLE_##TYPE *self, TYPE message);      \
    int  (*TryFill) (struct _BOTTLE_##TYPE *self, TYPE message);  \
    int (*Drain) (struct _BOTTLE_##TYPE *self, TYPE *message);    \
    int (*TryDrain) (struct _BOTTLE_##TYPE *self, TYPE *message); \
    void (*Dry) (struct _BOTTLE_##TYPE *self);                    \
    void (*Destroy) (struct _BOTTLE_##TYPE *self);                \
  } _BOTTLE_VTABLE_##TYPE;                                        \
\
  struct _elem_##TYPE  {                    \
    TYPE v;                                 \
    struct _elem_##TYPE *next;              \
  };                                        \
\
  typedef struct _BOTTLE_##TYPE             \
  {                                         \
    struct _queue_##TYPE {                  \
      struct _elem_##TYPE *front;           \
      struct _elem_##TYPE *back;            \
      size_t size;                          \
    } queue;                                \
    size_t                       capacity;  \
    int                          closed;    \
    int                          frozen;    \
    pthread_mutex_t              mutex;     \
    pthread_cond_t               not_empty; \
    pthread_cond_t               not_full;  \
    const _BOTTLE_VTABLE_##TYPE *vtable;    \
  } BOTTLE_##TYPE;                          \
\
  BOTTLE_##TYPE *BOTTLE_CREATE_##TYPE( size_t capacity );  \

#define BOTTLE( TYPE ) \
  BOTTLE_##TYPE

#define BOTTLE_CREATE1( TYPE ) \
  BOTTLE_CREATE_##TYPE(1)
#define BOTTLE_CREATE2( TYPE, capacity ) \
  BOTTLE_CREATE_##TYPE(capacity)
#define BOTTLE_CREATE(...) VFUNC(BOTTLE_CREATE, __VA_ARGS__)

#define BOTTLE_FILL(self, message)  \
  ((self)->vtable->Fill ((self), (message)))

#define BOTTLE_TRY_FILL(self, message)  \
  ((self)->vtable->TryFill ((self), (message)))

#define BOTTLE_DRAIN(self, message)  \
  ((self)->vtable->Drain ((self), &(message)))

#define BOTTLE_TRY_DRAIN(self, message)  \
  ((self)->vtable->TryDrain ((self), &(message)))

#define BOTTLE_PLUG(self)  \
  do { ASSERT (!pthread_mutex_lock (&(self)->mutex));   \
       (self)->frozen = 1 ;                             \
       ASSERT (!pthread_mutex_unlock (&(self)->mutex)); \
     } while(0)

#define BOTTLE_UNPLUG(self)  \
  do { ASSERT (!pthread_mutex_lock (&(self)->mutex));   \
       (self)->frozen = 0 ;                             \
       ASSERT (!pthread_mutex_unlock (&(self)->mutex)); \
       ASSERT (!pthread_cond_signal (&self->not_full)); \
     } while(0)

#define BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY(self)  \
  do { (self)->vtable->Dry ((self)); } while (0)

#define BOTTLE_DESTROY(self)  \
  do { (self)->vtable->Destroy ((self)); } while (0)

#define BOTTLE_CAPACITY(self) ((self)->capacity)

#endif
