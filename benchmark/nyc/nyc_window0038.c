#include <stdio.h>
#include <stdlib.h>
#include <numa.h>
// #include <gperftools/tcmalloc.h>

typedef struct segment_t_ {
    int size, gap, ref;    
}segment_t;

#define SEG_N 36

segment_t segment[SEG_N] = {
    /*1 */  {.size = 4096 * 512, .gap = 4, .ref = 1},
            {.size = 100 * 512, .gap = 1, .ref = 1},
    
    /*2 */  {.size = 4096 * 512, .gap = 4, .ref = 1},
            {.size = 100 * 512, .gap = 1, .ref = 1},
    
    /*3 */  {.size = 4096 * 512, .gap = 4, .ref = 1},
            {.size = 100 * 512, .gap = 1, .ref = 1},
    
    /*4 */  {.size = 4096 * 512, .gap = 4, .ref = 1},
            {.size = 100 * 512, .gap = 1, .ref = 1},
    
    /*5 */  {.size = 4096 * 512, .gap = 4, .ref = 1},
            {.size = 100 * 512, .gap = 1, .ref = 1},
    
    /*6 */  {.size = 4096 * 512, .gap = 4, .ref = 1},
            {.size = 100 * 512, .gap = 1, .ref = 1},
    
    /*7 */  {.size = 4096 * 512, .gap = 4, .ref = 1},
            {.size = 100 * 512, .gap = 1, .ref = 1},
    
    /*8 */  {.size = 3148 * 512, .gap = 1, .ref = 6},
            {.size = 100 * 512, .gap = 1, .ref = 3},
    
    /*9 */  {.size = 4096 * 512, .gap = 4, .ref = 1},
            {.size = 4096 * 512, .gap = 4, .ref = 2},
    
    /*10 */ {.size = 4096 * 512, .gap = 4, .ref = 1},
            {.size = 4096 * 512, .gap = 4, .ref = 2},
    
    /*11 */ {.size = 4096 * 512, .gap = 4, .ref = 1},
            {.size = 4096 * 512, .gap = 4, .ref = 2},
    
    /*12 */ {.size = 4096 * 512, .gap = 4, .ref = 1},
            {.size = 4096 * 512, .gap = 4, .ref = 2},
    
    /*13 */ {.size = 6194 * 512, .gap = 4, .ref = 1},
    
    /*14 */ {.size = 50 * 512, .gap = 1, .ref = 2},

    /*15 */ {.size = 6144 * 512, .gap = 4, .ref = 1},

    /*16 */ {.size = 4096 * 512, .gap = 4, .ref = 2},

    /*17 */ {.size = 4096 * 512, .gap = 1, .ref = 6},

    /*18 */ {.size = 1024 * 512, .gap = 4, .ref = 1},

    /*19 */ {.size = 1024 * 512, .gap = 4, .ref = 2},

    /*20 */ {.size = 1024 * 512, .gap = 4, .ref = 2},

    /*21 */ {.size = 562 * 512, .gap = 3, .ref = 1},

    /*22 */ {.size = 4096 * 512, .gap = 4, .ref = 1},
    
    /*23 */ {.size = 4096 * 512, .gap = 4, .ref = 1},

    /*24 */ {.size = 768 * 512, .gap = 2, .ref = 160}     
};

int main() {
    numa_run_on_node(0);

    int N = 0, i;
    for (i = 0; i < SEG_N; ++i) {
        N += segment[i].size;
    }
    
    long long* space = (long long*) malloc(N * 8);
    for (i = 0; i < N; ++i) {
        space[i] = 1LL * i * i;
    }
    
    long long sum = 0;
    int s, size = 0;
    for (s = 0; s < SEG_N; ++s) {
        int gap = segment[s].gap, ref = segment[s].ref;
        int l = size, r = l + segment[s].size;
        size += segment[s].size;

        // while (ref--) {
        //     for (i = l; i < r; i += gap) {
        //         sum += space[i];
        //     }
        // }

        int j;
        for (i = l; i < r; i += gap) {
            for (j = 0; j < ref; ++j)
                sum += space[i];
        }
    }

    printf("%d: %lld\n", N, sum);
    return 0;
}