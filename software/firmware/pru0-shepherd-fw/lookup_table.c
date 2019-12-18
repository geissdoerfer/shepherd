#include "lookup_table.h"
#include <stdint.h>
#include <stdio.h>
#include "virtcap.h"

/* ---------------------------------------------------------------------
 * Lookup Table
 *
 * input: current
 * output: efficiency
 *
 * The lookup table returns the efficiency corresponding to the given
 * input current. Lookup table is divided into 4 sections, which each have
 * a different x-axis scale. First is determined in which section the current
 * points to. Then the current is scaled to a value between 0--9, which is used
 * as index in the lookup table.
 *
 * Figure 4: 'Charger Efficiency vs Input Current' in the datatsheet on
 * https://www.ti.com/lit/ds/symlink/bq25570.pdf shows how those 4 sections are
 * defined.
 *
 * This code is written as part of the thesis of Boris Blokland
 * Any questions on this code can be send to borisblokland@gmail.com
 * ----------------------------------------------------------------------
 */

int32_t max_t1;
int32_t max_t2;
int32_t max_t3;
int32_t max_t4;

int32_t scale_index_t1;
int32_t scale_index_t2;
int32_t scale_index_t3;
int32_t scale_index_t4;

#define SHIFT 26

void lookup_init() {
  max_t1 = current_ua_to_logic(0.1 * 1e3);
  max_t2 = current_ua_to_logic(1 * 1e3);
  max_t3 = current_ua_to_logic(10 * 1e3);
  max_t4 = current_ua_to_logic(100 * 1e3);

  scale_index_t1 = 9 * (1 << SHIFT) / max_t1;
  scale_index_t2 = 9 * (1 << SHIFT) / max_t2;
  scale_index_t3 = 9 * (1 << SHIFT) / max_t3;
  scale_index_t4 = 9 * (1 << SHIFT) / max_t4;
}

int32_t lookup(int32_t table[][9], int32_t current) {
  if (current < max_t1) {
    int32_t index = current * scale_index_t1 >> SHIFT;
    return table[0][index];
  } else if (current < max_t2) {
    int32_t index = current * scale_index_t2 >> SHIFT;
    return table[1][index];
  } else if (current < max_t3) {
    int32_t index = current * scale_index_t3 >> SHIFT;
    return table[2][index];
  } else if (current < max_t4) {
    int32_t index = current * scale_index_t4 >> SHIFT;
    return table[3][index];
  } else {
    // Should never get here
    return table[3][8];
  }
}
