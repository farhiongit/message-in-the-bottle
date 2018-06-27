Thread-safe message queue for thread synchronization
====================================================

> This project implements in C the concept of channels like the one found in Go language.
>
> The messages exchanged through those channels (here called bottles) are strongly typed.
>
> Look at the simple example below.

I have recently read about the Go language.
The minimalistic grammar and overall simplicity (as compared to C++ or Java) are nice features,
even if the language still needs optimization and is sometime tricky.

Nevertheless, I noticed the use of the gracious message queue pattern (called channels in Go) for
thread (called goroutines) synchronization.

This pattern is a FIFO thread-safe message queue suited for thread synchronization.

The pattern of message driven synchronization is of interest because it is intuitive and of a higher level
of abstraction than mutexes and conditions:

- some threads `A` (usually called the senders or the producers) fill the message queue ;
- some other threads `B` (usually called the receivers or the consumers) eat up the message queue.

Threads B are gracefully synchronized with threads A.

Therefore, I grabbed my copy of the (excellent) book "Programming with POSIX threads" by Butenhof, where the pattern is mentionned, to
implement it for my favorite C language.

And here it is.

# Insights of the user interface

The user interface is available in two styles, a macro-like and a C-like style.
Those two are strictly equivalent:

|| Description          |Macro-like style                     | C-like style|
|-|---------------------|-------------------------------------|-------------|
|**Declaration**        |
||Type                  | `BOTTLE(`*T*`)`                     | `bottle_t(`*T*`)`
|**Life cycle**         |
||Create                | `BOTTLE_CREATE`                     | `bottle_create`
||Destroy               | `BOTTLE_DESTROY`                    | `bottle_destroy`
|**Actions**            |
||Send message          | `BOTTLE_FILL`                       | `bottle_send`
||Receive message       | `BOTTLE_DRAIN`                      | `bottle_recv`
||Close sending end     | `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY` | `bottle_close`
|**Other actions**      |
||Try sending message   | `BOTTLE_TRY_FILL`                   | `bottle_try_send`
||Try receiving message | `BOTTLE_TRY_DRAIN`                  | `bottle_try_recv`
||Plug                  | `BOTTLE_PLUG`                       | `bottle_plug`
||Unplug                | `BOTTLE_UNPLUG`                     | `bottle_unplug`
|**Properties**         |
||Is closed             | `BOTTLE_IS_CLOSED`                  | `bottle_is_closed`
||Get buffer capacity   | `BOTTLE_CAPACITY`                   | `bottle_capacity`

The following text uses the macro-like style but the equivalent C-like style can be used instead.

## Creation of a bottle

> BOTTLE (*T*) \* **BOTTLE_CREATE** (*T*, [size_t capacity = UNBUFFERED])
>
> *The second argument is optional and defaults to `UNBUFFERED` (see below).*

To transport messages of type *T*, just create a pointer to a message queue with:

`BOTTLE(` *T* `) *`*bottle* ` = BOTTLE_CREATE (` *T* `);`

*T* could be any standard or user defined (`typedef`) type, simple or composed structure (`struct`).

For instance, to create a pointer to a message queue *b* for exchanging integers between threads, use:

`BOTTLE (int) *b = BOTTLE_CREATE (int);`

The message queue is **a strongly typed** (it is a hand-made template container) FIFO queue.

### Unbuffered message queue

The created queue is unbuffered by default.
Such an unbuffered queue is suitable to synchronize threads running concurrently.

### Buffered message queue

Even if unbuffred queues are efficient in most cases, buffered queues can be needed for instance:

- to limit the number of thread workers,
- in case of a very high rate (more than 100k per second) of exchanged messages
  between the sender thread and the receiver thread,
  where context switch overhead between threads would be counter-productive. The buffer capacity will allow to process
  sending and receiving messages by chunks, therefore reducing the number of context switch.

To create a pointer to a **buffered** message queue, pass its *capacity* as an optional (positive integer) second argument of `BOTTLE_CREATE` :

`BOTTLE(` *T* `) *`*bottle* ` = BOTTLE_CREATE (` *T* `, ` *capacity* `);`

- A *capacity* set to a positive integer defines a buffered queue of limited capacity.

    This could be used to manage tokens: *capacity* is then the number of available tokens.
    Call `BOTTLE_TRY_FILL` to request a token, and call `BOTTLE_TRY_DRAIN` to release a token.
    The bottle is then used as a container of controlled capacity.
    `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY` need not be used in this case.

- A *capacity* set to `UNBUFFERED` defines an unbuffered queue (default).

Buffered queues are implemented as preallocated arrays rather than as conventional linked-list: elements of the array are reused to transport all messages, whereas with a linked list, each message would require a dynamically allocated new element in the list, adding memory management pverhead. This design is inspired by the [LMAX Disruptor pattern](https://lmax-exchange.github.io/disruptor/).

*The usage of buffered queues is neither required nor recommended for thread synchronization.
It would partly unsynchronize threads and uses a larger amount of memory.*

> size_t **BOTTLE_CAPACITY** (BOTTLE (*T*) \*bottle)

`BOTTLE_CAPACITY` returns the capacity of the bottle (as defined at creation with `BOTTLE_CREATE`).

### Automatic variable

Rather than creating pointers to bottles, automatic variables of type BOTTLE (*T*) can be declared and initialized with `BOTTLE_INIT`.

> **BOTTLE_DECL** (*T*, *variable name*, [size_t capacity = UNBUFFERED])
>
> `BOTTLE_DECL` can only be applied to auto function scope variables; it may not be applied to parameters or variables
> with static storage duration.
>
> This functionality is only available with compilers `gcc` and `clang`.

Here is an example. The variable *b* is declared as a `bottle_t (time_t)` and initialized for usage.
It is destroyed when the variable goes out of scope.

```c
#include <time.h>
#include "bottle_impl.h"

DECLARE_BOTTLE (time_t);
DEFINE_BOTTLE (time_t);

int main (void)
{
  BOTTLE_DECL (time_t, b);       // Declares an automatic variable b of type bottle_t (time_t)
  bottle_send (&b, time (0));
  time_t val;
  bottle_recv (&b, val);
  // b is deallocated automatically when going out of scope.
}
```

## Exchanging messages between thread

Sender threads communicate synchronously with receiver threads by exchanging messages through the bottle:
the bottle has a mouth where it can be filled with messages and a tap from where it can be drained.

![alt text](Bottle.jpg "A classy FIFO message queue")

<small>(c) Davis & Waddell - EcoGlass Oil Bottle with Tap Large 5 Litre | Peter's of Kensington</small>

### Receiving messages

> int **BOTTLE_DRAIN** (BOTTLE (*T*) \*bottle, [*T*& message])
>
> *The second argument `message` is optional. If omitted, the bottle is drained but the message is not fetched and is lost.*
>
> *It is of type T&.
> It is passed as a **reference**, and not as a pointer to T.
> Therefore, it might be modified by the callee (just a little bit of macro magic here).*


The receivers can receive messages (draining form the tap), as long as the bottle is not closed, by calling `BOTTLE_DRAIN (`*bottle*`, `*message*`)`.

- `BOTTLE_DRAIN` returns 0 (with `errno` set to `ECONNABORTED`) if there is no data to receive and the bottle was
closed (by `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY`).

    This condition (returned value equal to 0 and `errno` equal to `ECONNABORTED`) should be handled
    by the eaters and stop the reception of any data from the bottle.

- If there is data to receive, `BOTTLE_DRAIN` receives a *message* from the bottle
  (it modifies the value of the second argument *message* passed by *"reference"* and returns 1.

  Messages are received in the **exact order** they have been sent, whatever the capacity of the buffer defined by `BOTTLE_CREATE`.

- Otherwise (there is no data to receive and the bottle is not closed), `BOTTLE_DRAIN` waits
  until there is data to receive.

Therefore `BOTTLE_DRAIN` returns 1 if a message has been sucessfully received from the bottle.

### Sending messages

> int **BOTTLE_FILL** (BOTTLE (*T*) \*bottle, [*T* message])
>
> *The second argument is optional. If omitted, an arbitrary unspecified dummy message is used to fill the bottle.*

The senders can send messages (filling in through the mouth of the bottle)
by calling `BOTTLE_FILL (`*bottle*`, `*message*`)`, as long as the mouth is open
and the botlle is not closed.

- `BOTTLE_FILL` returns 0 (with `errno` set to `ECONNABORTED`) in those cases:

    - immediately, without waiting, if the bottle was closed (by `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY`).
    - as soon as the bottle is closed (by `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY`).

      This most probably indicates an error in the logic of the user program as it should be avoided to close a bottle
      while senders are still using it.

      Anyhow, this condition (returned value equal to 0 and `errno` equal to `ECONNABORTED`) should be handled
      by the senders and stop the transmission of any data to the bottle.

- `BOTTLE_FILL` waits in those cases:

    - If the bottle was plugged (by `BOTTLE_PLUG`).
    - If the message queue is unbuffered, `BOTTLE_FILL` blocks until some receiver has received
    the value sent by a previous sucessful call to `BOTTLE_FILL` or `BOTTLE_TRY_FILL`.
    - If it is buffered and the buffer is full, `BOTTLE_FILL` blocks until some receiver has retrieved a value
    (with `BOTTLE_DRAIN` or `BOTTLE_TRY_DRAIN`).


- In other cases, `BOTTLE_FILL` sends the message in the bottle and returns 1.

Therefore `BOTTLE_FILL` returns 1 if a message has been sucessfully sent in the bottle.

### Resources management

In case the message type *T* would be or would contain (in a structure) allocated resources
(such as memory with `malloc`/`free` or file descriptor with `fopen`/`fclose` for instance),
the user program must respect those simple rules:

- The **sender**:

    - *must* allocate resources of the message before sending it (i.e. before the call to functions `BOTTLE_FILL` or `BOTTLE_TRY_FILL`);

    - *should not* try to access those allocated ressources after the message has been sent (i.e. after the call to functions `BOTTLE_FILL` or `BOTTLE_TRY_FILL`).

    Indeed, from this point,
      the message is owned by the receiver (which could modify it) and does not belong to the sender anymore.


- The **receiver** *must*, after receiving a message
  (i.e. after the call to functions `BOTTLE_DRAIN` or `BOTTLE_TRY_DRAIN`), and after use of the message,
  deallocate all the resources of the message (those previously allocated by the sender).

## Closing communication

> void **BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY** (BOTTLE (*T*) \*bottle)

When concurrent threads are synchronized by an exchange of messages, the senders must inform the receivers
when they have finished sending messages, so that receivers won't need to wait for extra messages.

In other words, as soon as all messages have been sent through the bottle (it won't be filled with any more messages), we can close it.

To do so,
the function `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY` seals the mouth of the bottle
(i.e. close the transmitter side of the bottle),
unblock all receivers and wait for the bottle to be empty.

This behavior:

1. prevents any new message from being sent in the bottle (just in case) :
  `BOTTLE_FILL` and `BOTTLE_TRY_FILL` will return 0 immediately (without blocking).
2. waits for the bottle to be emptied (by the calls to `BOTTLE_DRAIN` in the receivers).
3. asks for any remaining blocked calls to `BOTTLE_DRAIN` (called by the receivers) to unblock and to finish their job:
  `BOTTLE_DRAIN` will be asked to return immediately with value 0.

`BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY`:

- acts as if it were sending an end-of-file in the bottle.

    Therefore, the call to `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY` *must be done*
    after the feeders have finished their work (either at the end of the feeder treatment,
    or sequentially just after the feeder treatment).

- waits for all the eaters to finish their work before returning.

After the call to `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY`, eaters are still able to (and *should*) process the remaining messages in the bottle to avoid any memory leak due to unprocessed remaining messages.

The user program *must* wait for `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY` to return before destroying the bottle (with `BOTTLE_DESTROY`).

As said above, `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY` is *only useful when the bottle is used to synchronize concurrent
threads* and need not be used in other cases (thread-safe shared FIFO queue).

## Destruction of a bottle

> void **BOTTLE_DESTROY** (BOTTLE (*T*) \*bottle)

Thereafter, once *all the receivers are done* in the user program, and the bottle is not needed anymore,
it can be destroyed safely with `BOTTLE_DESTROY`.

Notes:
`BOTTLE_DESTROY` does not call `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY` by default because the user program *should
ensure* that all receivers have returned from calls to `BOTTLE_DRAIN` between `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY`
and `BOTTLE_DESTROY` (usually, waiting for the receivers to finish with a `pthread_join` might suffice).

## Other features

### Unblocking message queue functions

> int **BOTTLE_TRY_DRAIN** (BOTTLE (*T*) \*bottle, [*T* message])
>
> *The second argument is optional. If omitted, the bottle is drained but the message is lost.*

> int **BOTTLE_TRY_FILL** (BOTTLE (*T*) \*bottle, [*T* message])
>
> *The second argument is optional. If omitted, an arbitrary unspecified dummy message is used.*

There are also unblocking versions of the filling and draining functions.

These functions return immediately without blocking. *They are **not** suited for thread synchronization* and are of limited use.

When used, the message queue looses its thread-synchronization feature and
behaves like a simple thread-safe FIFO message queue:

- Receivers can receive messages without blocking with `BOTTLE_TRY_DRAIN (`*bottle*`, `*message*`)`.

    - `BOTTLE_TRY_DRAIN` returns 0 (with `errno` set to `ECONNABORTED`) if the bottle is empty and closed.
    - `BOTTLE_TRY_DRAIN` returns 0 (with `errno` set to `EWOULDBLOCK`) if the bottle is empty.
    - Otherwise, it receives a *message* (it modifies the value of the second argument *message*)
      from the bottle and returns 1.

- Senders can send messages without blocking with `BOTTLE_TRY_FILL (`*bottle*`, `*message*`)`.

    - `BOTTLE_TRY_FILL` returns 0 if the bottle is closed (with `errno` set `ECONNABORTED`).
    This most probably indicates an error in the user program as it should be avoided to close a bottle
    while senders are still using it.
    - Otherwise, `BOTTLE_TRY_FILL` returns 0 if the bottle is plugged (with `errno` set `EWOULDBLOCK`) or already full.
    This indicates that a call to `BOTTLE_FILL` would have blocked.
    - Otherwise, it sends a *message* in the bottle and returns 1.

Notice that the second argument *message* is of type *T*, and not a pointer to *T*,
even though it might be modified by `BOTTLE_TRY_DRAIN` (macro magic here).

### Halting communication

> void **BOTTLE_PLUG** (BOTTLE (*T*) \*bottle)

> void **BOTTLE_UNPLUG** (BOTTLE (*T*) \*bottle)

The communication between sender and receiver threads can be stopped and restarted at will if needed.
The mouth of the bottle can be:

- plugged (stopping communication) with `BOTTLE_PLUG`,
- unplugged (to restart communication) with `BOTTLE_UNPLUG`.

### Hidden data

If the content of the messages is not needed, the argument *message* can be *omitted* in calls to
`BOTTLE_TRY_DRAIN`, `BOTTLE_DRAIN`, `BOTTLE_TRY_FILL` and `BOTTLE_FILL`.

In this case, a bottle is simply used as a synchronization method or a token counter.

# Usage

Before use:
1. include header file `bottle_impl.h` before usage.
2. instanciate a template of a given type of messages with `DECLARE_BOTTLE` and `DEFINE_BOTTLE`.

For instance, to exchange message texts between threads:
```c
#include "bottle_impl.h"
typedef const char * Message;   // A user-defined type of messages.
DECLARE_BOTTLE (Message);       // Declares the usage of bottles for the user-defined type.
DEFINE_BOTTLE (Message);        // Defines the usage of bottles for the user-defined type.
```

## Source files

- [`bottle.h`](bottle.h) declares the user interface documented here (about 100 lines of source code).
- [`bottle_impl.h`](bottle_impl.h) defines the user programming interface (about 200 lines of source code).
- [`vfunc.h`](vfunc.h) is used by [`bottle.h`](bottle.h). It permits usage of optional parameters in function signatures.

# Examples

All sources should be compiled with the option `-pthread`.

## Unbuffered bottle: Thread synchronization

### Simple example

The source below is a simple example of the standard use of the API for thread synchronization.
It uses the C-like style.

```c
#include <unistd.h>
#include <stdio.h>

#include "bottle_impl.h"
typedef const char *Message;
DECLARE_BOTTLE (Message);       // Declares the template for type 'Message'
DEFINE_BOTTLE (Message);        // Defines the template for type 'Message'

static void *
eat (void *arg)                 // The thread that receives the messages
{
  bottle_t (Message) * bottle = arg;
  Message m;
  while (bottle_recv (bottle, m))
    printf ("%s\n", m);
  return 0;
}

static void *
feed (void *arg)
{
  bottle_t (Message) * bottle = arg;
  Message police[] = { "I'll send an SOS to the world", "I hope that someone gets my", "Message in a bottle" };
  for (size_t i = 0; i < 2 * sizeof (police) / sizeof (*police); i++)
  {
    bottle_send (bottle, police[i / 2]);
    sleep (1);
  }
  return 0;
}

int
main (void)                     // The (main) thread that creates the bottle
{
  bottle_t (Message) * bottle = bottle_create (Message);

  pthread_t eater;
  pthread_create (&eater, 0, eat, bottle);

#ifndef FEEDER_THREAD           // Option 1: the sender is in the same thread as the one that created it.

  feed (bottle);

#else                           // Option 2: the sender is in a dedicated thread.

  pthread_t feeder;
  pthread_create (&feeder, 0, feed, bottle);
  pthread_join (feeder, 0);     // Waits for the sender to finish.

#endif

  bottle_close (bottle);        // Tells the receiver thread that the sending thread has finished sending messages
  pthread_join (eater, 0);      // Waits for the receiver thread to finish its work (it uses the bottle).
  bottle_destroy (bottle);      // Destroys the bottle once both threads are over.
}
```

Comments:

- A bottle is created (`bottle_create` or `BOTTLE_CREATE`) to communicate synchronously between two threads, a sender and a receiver:
  it is a template container which type is `bottle_t (Message)` or `BOTTLE (Message)`, where `Message` is a user-defined type.

- The `eat` function (the receiver which calls `bottle_recv` or `BOTTLE_DRAIN`) is in one thread `eater` (but there could be several),
  which pace is synchronized with the sender.

  That's what it's all about: synchonizing threads with messages !

- The sending loop (in the sender function `feed` which calls `bottle_send` or `BOTTLE_FILL`) is:
  - either in the same thread (here the `main` thread)
  as the one which created the bottle
  - or in an alternate dedicated thread (when compiled with option `FEEDER_THREAD`).


- The thread that created the bottle:

  1. waits for the feeder process to finish.
  1. closes the bottle (`bottle_close` or `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY`)

     Closing the bottle in the thread that created it is a good practice:
     - it protects against the bottle being closed during the transmission phase
     - it is suitable with the use of several concurrent feeder thread if needed.
     - it helps keep things easier.

  1. waits for the eater thread to finish (`pthread_join (eater, 0)`)
  1. destroys the bottle (`bottle_destroy` or `BOTTLE_DESTROY`).

### Advanced example

[`bottle_example.c`](bottle_example.c) is a complete example of a program (compile with option `-pthread`)
using a synchronized thread-safe FIFO message queue.

## Buffered bottle

### High performance message exchanges

When high performance of exchanges is required between threads (more than 100 000 messages per seconds), a buffered bottle is a good choice (reminder: in other cases, an unbuffered bottle is far enough.)

In the following example, this enhances performance by a factor of about 40, because it cuts off the concurrency overhead.
On my computer (AMD A6 4 cores), it takes about 20 seconds to exchange 25 millions messages between synchronized threads.

```c
#include "bottle_impl.h"
DECLARE_BOTTLE (int);
DEFINE_BOTTLE (int);

#define NB_MESSAGES 25000000
#define BUFFER_SIZE 1000
//#define BUFFER_SIZE UNBUFFERED

static size_t LEVEL[2 * NB_MESSAGES];
static size_t nb_p, nb_c;

static void *eat (void *arg)
{
  bottle_t (int) * bottle = arg;
  while (bottle_recv (bottle))
    LEVEL[nb_c++ + nb_p] = QUEUE_SIZE (bottle->queue);
  return 0;
}

int
main (void)
{
  bottle_t (int) * bottle = bottle_create (int, BUFFER_SIZE);
  pthread_t eater;
  pthread_create (&eater, 0, eat, bottle);

  for (size_t i = 0; i < NB_MESSAGES && bottle_send (bottle); i++)
    LEVEL[nb_c + nb_p++] = QUEUE_SIZE (bottle->queue);
  bottle_close (bottle);
  pthread_join (eater, 0);
  bottle_destroy (bottle);
  printf ("%zu messages produced, %zu messages consumed.\n", nb_p, nb_c);
}
```

### Token management

Tokens can be managed with a buffered bottle, in this very naive model:

```c
#include <stdio.h>
#include "bottle_impl.h"
typedef int Token;
DECLARE_BOTTLE (Token);
DEFINE_BOTTLE (Token);

#define PRINT_TOKEN printf (" (%lu/%lu).\n", QUEUE_SIZE((tokens_in_use).queue), BOTTLE_CAPACITY (&tokens_in_use))
#define GET_TOKEN   printf ("Token requested: %s", BOTTLE_TRY_FILL (&tokens_in_use) ? "OK" : "NOK")
#define LET_TOKEN   printf ("Token released:  %s", BOTTLE_TRY_DRAIN (&tokens_in_use) ? "OK" : "NOK")

int main (void)
{
  BOTTLE_DECL (Token, tokens_in_use, 3);

  GET_TOKEN ; PRINT_TOKEN ;
  GET_TOKEN ; PRINT_TOKEN ;
  GET_TOKEN ; PRINT_TOKEN ;
  GET_TOKEN ; PRINT_TOKEN ;
  GET_TOKEN ; PRINT_TOKEN ;

  LET_TOKEN ; PRINT_TOKEN ;

  GET_TOKEN ; PRINT_TOKEN ;
}
```

displays:

```
Token requested: OK (1/3).
Token requested: OK (2/3).
Token requested: OK (3/3).
Token requested: NOK (3/3).
Token requested: NOK (3/3).
Token released:  OK (2/3).
Token requested: OK (3/3).
```

Note how optional arguments *message* are omitted in calls to `BOTTLE_TRY_DRAIN` and `BOTTLE_TRY_FILL`.

**Have fun !**
