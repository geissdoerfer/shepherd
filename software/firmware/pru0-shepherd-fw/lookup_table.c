#include <stdint.h>
#include <stdio.h>
#include "lookup_table.h"
#include "virtcap.h"

int32_t max_t1;
int32_t max_t2;
int32_t max_t3;
int32_t max_t4;

int32_t scale_index_t1;
int32_t scale_index_t2;
int32_t scale_index_t3;
int32_t scale_index_t4;

#define SHIFT 26

void lookup_init(int32_t dbg[])
{
  max_t1 = current_ua_to_logic(0.1 * 1e3);
  max_t2 = current_ua_to_logic(1 * 1e3);
  max_t3 = current_ua_to_logic(10 * 1e3);
  max_t4 = current_ua_to_logic(100 * 1e3);

  scale_index_t1 = 9 * (1 << SHIFT) / max_t1;
  scale_index_t2 = 9 * (1 << SHIFT) / max_t2;
  scale_index_t3 = 9 * (1 << SHIFT) / max_t3;
  scale_index_t4 = 9 * (1 << SHIFT) / max_t4;

  // printf("scale_index t1: %d\n", scale_index_t1);
  // printf("scale_index t2: %d\n", scale_index_t2);
  // printf("scale_index t3: %d\n", scale_index_t3);
  // printf("scale_index t4: %d\n", scale_index_t4);

  dbg[0] = max_t1;
  dbg[1] = max_t2;
  dbg[2] = max_t3;
  dbg[3] = max_t4;
}


int32_t lookup(int16_t table[][9], int32_t current, int32_t dbg[])
{
  // static int counter = 0;

  // dbg[0] = 1;
  // dbg[1] = counter % 9;
  // dbg[2] = kLookupInput[1][counter % 9];
  // return kLookupInput[1][counter++ % 9];
  

  if (current < max_t1)
  {
    int32_t index = current * scale_index_t1 >> SHIFT;
    // printf("current: %d, current * scale_index_t1: %d, index: %d\n", current, current * scale_index_t1, index);
    dbg[0] = 0;
    dbg[1] = index;
    dbg[2] = table[0][index];
    return table[0][index];
    // return table[index];
  }
  else if (current < max_t2)
  {
    int32_t index = current * scale_index_t2 >> SHIFT;
    // printf("current: %d, current * scale_index_t2: %d, index: %d\n", current, current * scale_index_t2, index);
    dbg[0] = 1;
    dbg[1] = index;
    dbg[2] = table[1][index];
    return table[1][index];
    // return table[index + 9];
  }
  else if (current < max_t3)
  {
    int32_t index = current * scale_index_t3 >> SHIFT;
    // printf("current: %d, current * scale_index_t3: %d, index: %d\n", current, current * scale_index_t3, index);
    dbg[0] = 2;
    dbg[1] = index;
    dbg[2] = table[2][index];
    return table[2][index];
    // return table[index + 18];
  }
  else if (current < max_t4)
  {
    int32_t index = current * scale_index_t4 >> SHIFT;
    // printf("current: %d, current * scale_index_t4: %d, index: %d\n", current, current * scale_index_t4, index);
    dbg[0] = 3;
    dbg[1] = index;
    dbg[2] = table[3][index];
    return table[3][index];
    // return table[index + 27];
  }
  else
  {
    // Should never get here
    dbg[0] = -1;
    dbg[1] = -1;
    dbg[2] = table[3][8];
    return table[3][8];
  }
}
