#include <stdio.h>
#include <stdlib.h>
#include <numa.h>
// #include <gperftools/tcmalloc.h>

typedef struct segment_t_ {
    int size, gap, ref;    
}segment_t;

#define SEG_N 36

segment_t segment[SEG_N] = {
    /*1 */  {.size = 2150 * 512, .gap = 1, .ref = 1},
    /*2 */  {.size = 2150 * 512, .gap = 1, .ref = 1},
    /*3 */  {.size = 2150 * 512, .gap = 1, .ref = 1},
    /*4 */  {.size = 2150 * 512, .gap = 1, .ref = 1},
    /*5 */  {.size = 2150 * 512, .gap = 1, .ref = 1},
    
    /*6 */  {.size = 4, .gap = 1, .ref = 3027883},
    /*7 */  {.size = 73, .gap = 36, .ref = 1835081},
    /*8 */  {.size = 54, .gap = 13, .ref = 1101048},
    /*9 */  {.size = 1, .gap = 1, .ref = 11010487},
    /*10*/  {.size = 258, .gap = 51, .ref = 2914118},
    /*11*/  {.size = 22, .gap = 1, .ref = 355223},
    /*12*/  {.size = 1, .gap = 1, .ref = 153},
    /*13*/  {.size = 7, .gap = 1, .ref = 1893369},
    /*14*/  {.size = 26, .gap = 1, .ref = 79821},
    /*15*/  {.size = 415, .gap = 5, .ref = 98877},
    /*16*/  {.size = 180, .gap = 18, .ref = 4810},
    /*17*/  {.size = 9, .gap = 1, .ref = 1},
    /*18*/  {.size = 24, .gap = 1, .ref = 1827},
    /*19*/  {.size = 3, .gap = 1, .ref = 2167876},
    /*20*/  {.size = 1, .gap = 1, .ref = 1101049},
};

int main() {
    numa_run_on_node(0);

    int N = 0, i;
    for (i = 0; i < SEG_N; ++i) {
        N += (segment[i].size + 511) / 512 * 512;
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
        size += (segment[s].size + 511) / 512 * 512;

        while (ref--) {
            for (i = l; i < r; i += gap) {
                sum += space[i];
            }
        }

        // int j;
        // for (i = l; i < r; i += gap) {
        //     for (j = 0; j < ref; ++j)
        //         sum += space[i];
        // }
    }

    printf("%d: %lld\n", N, sum);
    return 0;
}