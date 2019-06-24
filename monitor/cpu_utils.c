#define _GNU_SOURCE
#include <sched.h>
#include "cpu_utils.h"
void _set_little_affinity(void) {
    AFF_IN_SCOPE
    SET_AFF_LITTLE
}

void _set_big_affinity(void) {
    AFF_IN_SCOPE
    SET_AFF_BIG
}
void _set_all_affinity(void) {
    AFF_IN_SCOPE
    SET_AFF_ALL
}
