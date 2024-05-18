/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* sema->waiters()*/
bool compare_sema_priority(const struct list_elem *selem_a,
							const struct list_elem *selem_b,
							void *aux);
bool compare_priority_donations(const struct list_elem *a,
					const struct list_elem *b,
					void *aux);
/* resolves nested donation. */
void donate();
/* resolves multiple donation. */
void update_donation();

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;
	list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function. */
void
sema_down (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);
	ASSERT (!intr_context ());

	old_level = intr_disable ();
	while (sema->value == 0) {		// 0일 동안, 즉 다른 스레드가 lock을 가지고 있는 동안
		struct thread *curr = thread_current();

		/* 현재 sema->waiters에는 한 개의 스레드만 포함되므로 
			push_back을 해주어도 문제는 없다.
			하지만, 추후에 sema->waiters에 여러 스레드가 포함되는 상황을 고려하여
			insert_ordered를 해주는 것이 좋다. */
		// list_push_back (&sema->waiters, &curr->elem);	// init된 sema->waiter 안에 실제로 스레드가 포함된다.
		list_insert_ordered(&sema->waiters, &curr->elem, compare_priority, NULL);

		thread_block ();			// 현재 스레드는 블락 상태이다.
	}
	sema->value--;
	intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) {
	enum intr_level old_level;
	bool success;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level (old_level);

	return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (!list_empty (&sema->waiters)) {
		/* cond의 waiter에 붙어 오는 sema의 경우, 
			sema->waiters에는 항상 1개의 스레드만 존재한다.
			그러나, cond를 사용하지 않는 경우 여러 스레드가 sema->waiters에 기다릴 수 있기 때문에 sort 해준다. */

		/* compare_priority를 그대로 사용하는 이유? 
			sema_down에서 list에 push될 때, 
			thread::elem이 그대로 넘어온다.
			즉, semaphore_elem::elem이 아니므로 (cond의 경우)
			기존의 우선순위 비교 함수를 그대로 사용할 수 있다. */
		list_sort(&sema->waiters, compare_priority, NULL);
		thread_unblock (list_entry (list_pop_front (&sema->waiters),	
					struct thread, elem));		// 새로운 스레드가 ready list에 추가된다.
	}
	sema->value++;							// 왜 먼저 업???
	thread_preempt();						// 따라서, 선점 여부를 다시 확인해야 한다.
	intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) {
	struct semaphore sema[2];
	int i;

	printf ("Testing semaphores...");
	sema_init (&sema[0], 0);
	sema_init (&sema[1], 0);
	thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up (&sema[0]);
		sema_down (&sema[1]);
	}
	printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock));

	/* lock이 이미 사용중이면,
		1. 해당 lock을 wait_on_lock 필드에 저장한다. 
		2. 현재 스레드의 우선순위를 저장하고, 
			해당 lock을 들고 있는 스레드의 donations list에 현재 스레드의 이름을 올린다. 
		3. 우선순위 기부를 통해 갱신한다. */
	struct thread *curr = thread_current();
	if (lock->holder != NULL) {		
		curr->wait_on_lock = lock;

		// curr->tmp_priority = curr->priority;
		list_insert_ordered(&lock->holder->donations, &curr->d_elem, compare_priority_donations, NULL);

		/* 8중 for문으로 모든 donation list 스레드의 priority 갱신*/
		if (!thread_mlfqs)
			donate();
	}

	sema_down (&lock->semaphore);
	curr->wait_on_lock = NULL;	// 이 Lock을 취득하였다.
	lock->holder = thread_current ();
}

/* resolve nested */
void 
donate() {
	struct thread *t = thread_current();
	/* t->wait_on_lock이 NULL일 수 있으므로 선언만 해놓고 안에서 지정 */
	struct thread *holder;	// 현재 lock holder

	/* 최대 8개 스레드까지 대기할 수 있다. */
	for (int i = 0; i < 8; i++) {
		/* printf하니까 interrupt */
		if (t->wait_on_lock == NULL)	// wait_on_lock이 NULL일 수 있으니까 체크
			return;

		holder = t->wait_on_lock->holder;

		if (holder->priority < t->priority) {
			holder->priority = t->priority;
		}

		t = holder;
	}
}

/* resolve multiple */
/* lock->holder의 priority를 갱신 */
void
update_donation() {
	struct thread *curr = thread_current();
	/* 밑에 함수들이 return할 수도 있으니까 여기서 이전 priority 복구해주어야한다. */
	curr->priority = curr->original_priority;

	if (list_empty(&curr->donations))
		return;

	/* donations list를 우선순위로 정렬하여 
		첫 d_elem의 priority와 lock->holder의 tmp_priority를 비교하여
		더 큰 값으로 갱신한다. */
	list_sort(&curr->donations, compare_priority_donations, NULL);

	struct thread *t = list_entry(list_begin(&curr->donations), struct thread, d_elem);
	int new_priority = (t->priority > curr->original_priority) ? t->priority : curr->original_priority;
	curr->priority = new_priority;
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)	// lock을 acquire하였다.
		lock->holder = thread_current ();
	return success;
}

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));

	/* lock이 release되면,
		1. donations list를 돌면서 해당 lock을 기다리고 있는 스레드를 remove 
		2. 현재 스레드의 priority를 갱신한다. */
	
	struct thread *curr = thread_current();
	struct list_elem *e;

	for (e = list_begin(&curr->donations); e != list_end(&curr->donations);) {
		struct thread *t = list_entry(e, struct thread, d_elem);
		if (t->wait_on_lock == lock) {
			e = list_remove(&t->d_elem);		// t->d_elem으로 바꿔보기
		}
		else {
			e = list_next(&t->d_elem);
		}
	}

	/* multiple donation 상황에서 lock이 release됨으로써 
		lock->holder의 priority를 갱신해주어야 하는 경우를 처리한다. */
	if (!thread_mlfqs) 
		update_donation();
	
	lock->holder = NULL;
	sema_up (&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem {
	struct list_elem elem;              /* List element. */
	struct semaphore semaphore;         /* This semaphore. */
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */

/* condition wait을 통해 스레드들을 충분히 기다린 후,
	cond->waiters가 우선순위대로 정렬이 되었을 때
	그제서야 signal/broadcast를 통해 높은 우선순위의 스레드를 실행시킨다.
	like 강에 둑을 치는 것... */
void
cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	sema_init (&waiter.semaphore, 0);

	/* cond->waiters list에 semaphore 구조체를 넣어준다. 
		실제로 락을 기다리는 것은 스레드인데, 왜 세마포어를 넣어줄까?
		semaphore 안에 value와 waiters가 존재하기 때문이다. 
		- value를 통해 semaphore를 제어할 수 있고,
		- semaphore->waiters 안에는 1개의 스레드만 존재하게 된다. */

	/* semaphore_elem::elem의 형태로 cond->waiters list에 추가된다.
		따라서, 이렇게 랩핑된 요소를 추출해 비교하는 새로운 함수가 필요하다.*/

	/* insert_ordered 전 init을 하기 때문에 waiter.elem이 NULL(?)인 상황이다. 
		따라서, insert_ordered 대신 push_back 후 
		sema_down에서 sort하는 것이 논리적으로 옳다. */
	// list_insert_ordered(&cond->waiters, &waiter.elem, compare_sema_priority, NULL);
	
	/* cond_signal에서 pop하기 전에 정렬을 하면 push_back으로도 동작한다. */
	list_push_back (&cond->waiters, &waiter.elem);
	lock_release (lock);
	sema_down (&waiter.semaphore);
	lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrup-t handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters)) {
		list_sort(&cond->waiters, compare_sema_priority, NULL);
		sema_up (&list_entry (list_pop_front (&cond->waiters),
					struct semaphore_elem, elem)->semaphore);
	}
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}

/* semaphore_elem::elem인 list_elem에서 priority를 비교하는 함수 */
bool compare_sema_priority(const struct list_elem *a,
							const struct list_elem *b,
							void *aux) {
	const struct semaphore_elem *selem_a = list_entry(a, struct semaphore_elem, elem);
	const struct semaphore_elem *selem_b = list_entry(b, struct semaphore_elem, elem);

	const struct list waiters_a = selem_a->semaphore.waiters;
	const struct list waiters_b = selem_b->semaphore.waiters;

	/* waiters 안에는 1개의 스레드만 있다. */
	const struct thread *thread_a = list_entry(list_begin(&waiters_a), struct thread, elem);
	const struct thread *thread_b = list_entry(list_begin(&waiters_b), struct thread, elem);
	
	return thread_a->priority > thread_b->priority;
}

/* priority 내림차순으로 정렬하기 위한 비교 함수 */
bool compare_priority_donations(const struct list_elem *a,
					const struct list_elem *b,
					void *aux) {
	const struct thread *thread_a = list_entry(a, struct thread, d_elem);
    const struct thread *thread_b = list_entry(b, struct thread, d_elem);

	// 우선순위가 더 높은 것을 반환한다.
	return thread_a->priority > thread_b->priority;
}