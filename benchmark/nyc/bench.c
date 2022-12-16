#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "hardcode.h"
#include <numa.h>

const uint64_t heapL1 = 0xfdf329c0000, heapR1 = 0xfdf512c0000; // 0xfdf329c0000 -- 0xfdf512c0000
const uint64_t heapL2 = 0xfdf992dc600, heapR2 = 0xfdf99ae2600; // 0xfdf992dc608 -- 0xfdf99ae2600
const uint64_t stackL = 0xfffe9ab0400, stackR = 0xfffe9ab4800; // 0xfffe9ab0405 -- 0xfffe9ab4800
const uint32_t stack_size = 34 * 512; // 34 pages
const uint32_t heap1_size = 1001472 * 512, heap2_size = 16432 * 512;
const uint32_t heap_size = heap1_size + heap2_size; // 1017904 pages


const int N = 1143864;
// const int N = 1163819;

int main() {
    numa_run_on_node(0);

    int i, j, k, p;
    
    uint64_t stack[stack_size];
    for (i = 0; i < stack_size; ++i) {
        stack[i] = i * i;
    }
   
    uint64_t *heap = (uint64_t*) malloc(heap_size << 3); 
    for (i = 0; i < heap_size; ++i) {
        heap[i] = 1LL * i * i;
    }

    uint64_t sum = 0;
    for (i = 0; i < N; ++i) {
        uint64_t start = mem_lst[i].s.start;
        uint32_t page_num = mem_lst[i].s.page_num;
        uint16_t memloc_num = mem_lst[i].s.memloc_num;
        uint64_t *space;

        uint16_t is_stack = 0;

        if (start < heapR1) {
            space = heap + (start - heapL1);
        }
        else if (start < heapR2) {
            space = heap + (start - heapL2 + heap1_size);
        }
        else {
            continue;
            // space = stack + start - stackL;
            // is_stack = 1;
        }

        if (page_num == 1 && memloc_num <= 16) {
            uint32_t loc_in_lst = mem_lst[i].r.loc_in_lst;
            for (j = 0; j < memloc_num; ++j) {
                uint16_t addr = loc_lst[loc_in_lst].addr;
                uint32_t ref = loc_lst[loc_in_lst].ref;
                ref = (is_stack && ref > 1) ? (ref >> 2) : ref;
                // int ref_ = (int)ref - 6;
                //  for (k = 0; k < ref_; k += 8) {
                //     // sum += space[addr] + space[addr] + space[addr] + space[addr] + space[addr] + space[addr] + space[addr] + space[addr];
                //     // sum += space[addr];
                //     // sum += space[addr];
                //     // sum += space[addr];
                //     // sum += space[addr];
                //     // sum += space[addr];
                //     // sum += space[addr];
                //     // sum += space[addr];
                //     // sum += space[addr];
                //     space[addr] += 1;
                //     space[addr] += 1;
                //     space[addr] += 1;
                //     space[addr] += 1;
                // }
                for (k = 0; k < ref; k += 2) {
                    space[addr] += 1;
                }
                ++loc_in_lst;
            }
        }
        else {
            uint32_t ref = mem_lst[i].s.ref;
            uint16_t stride = mem_lst[i].s.stride;
            
            ref = (is_stack && ref > 1) ? (ref >> 2) : ref;
                
            uint16_t stride8 = stride << 3;
            int page_num_ = (int) page_num - 7;
            int memloc_num_ = (int) memloc_num - 7;
            
            uint64_t *space2 = space + 512;
            uint64_t *space3 = space2 + 512;
            uint64_t *space4 = space3 + 512;
            uint64_t *space5 = space4 + 512;
            uint64_t *space6 = space5 + 512;
            uint64_t *space7 = space6 + 512;
            uint64_t *space8 = space7 + 512;

            for (p = 0; p < page_num_; p += 8) {
                uint32_t addr = 0;
                uint32_t addr2 = addr + stride;
                uint32_t addr3 = addr2 + stride;
                uint32_t addr4 = addr3 + stride;
                uint32_t addr5 = addr4 + stride;
                uint32_t addr6 = addr5 + stride;
                uint32_t addr7 = addr6 + stride;
                uint32_t addr8 = addr7 + stride;
                for (j = 0; j < memloc_num_; j += 8) {
                    for (k = 0; k < ref; ++k) {
                        // sum += space[addr] + space[addr2] + space[addr3] + space[addr4] + space[addr5] + space[addr6] + space[addr7] + space[addr8];
                        // sum += space2[addr] + space2[addr2] + space2[addr3] + space2[addr4] + space2[addr5] + space2[addr6] + space2[addr7] + space2[addr8];
                        // sum += space3[addr] + space3[addr2] + space3[addr3] + space3[addr4] + space3[addr5] + space3[addr6] + space3[addr7] + space3[addr8];
                        // sum += space4[addr] + space4[addr2] + space4[addr3] + space4[addr4] + space4[addr5] + space4[addr6] + space4[addr7] + space4[addr8];
                        // sum += space5[addr] + space5[addr2] + space5[addr3] + space5[addr4] + space5[addr5] + space5[addr6] + space5[addr7] + space5[addr8];
                        // sum += space6[addr] + space6[addr2] + space6[addr3] + space6[addr4] + space6[addr5] + space6[addr6] + space6[addr7] + space6[addr8];
                        // sum += space7[addr] + space7[addr2] + space7[addr3] + space7[addr4] + space7[addr5] + space7[addr6] + space7[addr7] + space7[addr8];
                        // sum += space8[addr] + space8[addr2] + space8[addr3] + space8[addr4] + space8[addr5] + space8[addr6] + space8[addr7] + space8[addr8];
                        sum += space[addr] + space[addr2] + space[addr3] + space[addr4] + space[addr5] + space[addr6] + space[addr7] + space[addr8]
                        + space2[addr] + space2[addr2] + space2[addr3] + space2[addr4] + space2[addr5] + space2[addr6] + space2[addr7] + space2[addr8]
                        + space3[addr] + space3[addr2] + space3[addr3] + space3[addr4] + space3[addr5] + space3[addr6] + space3[addr7] + space3[addr8]
                        + space4[addr] + space4[addr2] + space4[addr3] + space4[addr4] + space4[addr5] + space4[addr6] + space4[addr7] + space4[addr8]
                        + space5[addr] + space5[addr2] + space5[addr3] + space5[addr4] + space5[addr5] + space5[addr6] + space5[addr7] + space5[addr8]
                        + space6[addr] + space6[addr2] + space6[addr3] + space6[addr4] + space6[addr5] + space6[addr6] + space6[addr7] + space6[addr8]
                        + space7[addr] + space7[addr2] + space7[addr3] + space7[addr4] + space7[addr5] + space7[addr6] + space7[addr7] + space7[addr8]
                        + space8[addr] + space8[addr2] + space8[addr3] + space8[addr4] + space8[addr5] + space8[addr6] + space8[addr7] + space8[addr8];
                    }
                    addr += stride8;
                    addr2 += stride8;
                    addr3 += stride8;
                    addr4 += stride8;
                    addr5 += stride8;
                    addr6 += stride8;
                    addr7 += stride8;
                    addr8 += stride8;
                }
                // printf("%d %d\n", j, memloc_num);
                for (; j < memloc_num; ++j) { 
                    for (k = 0; k < ref; ++k) {
                        sum += space[addr] + space2[addr] + space3[addr] + space4[addr] + space5[addr] + space6[addr] + space7[addr] + space8[addr];
                    }
                    addr += stride;
                }
                space += 4096;
                space2 += 4096;
                space3 += 4096;
                space4 += 4096;
                space5 += 4096;
                space6 += 4096;
                space7 += 4096;
                space8 += 4096;
            }

            for (; p < page_num; ++p) {
                uint32_t addr = 0;
                uint32_t addr2 = addr + stride;
                uint32_t addr3 = addr2 + stride;
                uint32_t addr4 = addr3 + stride;
                uint32_t addr5 = addr4 + stride;
                uint32_t addr6 = addr5 + stride;
                uint32_t addr7 = addr6 + stride;
                uint32_t addr8 = addr7 + stride;
                for (j = 0; j < memloc_num_; j += 8) {
                    for (k = 0; k < ref; ++k) {
                        sum += space[addr] + space[addr2] + space[addr3] + space[addr4] + space[addr5] + space[addr6] + space[addr7] + space[addr8];
                    }
                    addr += stride8;
                    addr2 += stride8;
                    addr3 += stride8;
                    addr4 += stride8;
                    addr5 += stride8;
                    addr6 += stride8;
                    addr7 += stride8;
                    addr8 += stride8;
                }
                for (; j < memloc_num; ++j) { 
                    for (k = 0; k < ref; ++k) {
                        sum += space[addr];
                    }
                    addr += stride;
                }
                space += 512;
            }
        }
    }

    printf("%lu\n", sum);

   return 0;

}