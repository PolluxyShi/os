#ifndef _SPIN_LOCK_H
#define _SPIN_LOCK_H

typedef int spin_lock_t;
#define SPIN_INIT() 0

void spin_lock(spin_lock_t* lk);

void spin_unlock(spin_lock_t *lk);

int spin_trylock(spin_lock_t* lk);

#endif