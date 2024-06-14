#include <am.h>
#include <spinlock.h>

void spin_lock(spin_lock_t* lk) {
  while (atomic_xchg(lk, 1));
}

void spin_unlock(spin_lock_t *lk) {
  atomic_xchg(lk, 0);
}

int spin_trylock(spin_lock_t* lk) {
  return atomic_xchg(lk, 1);
}
