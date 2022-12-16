#include <stdio.h>
#include <stdlib.h>
#include <numa.h>

typedef struct segment_t_ {
    int size, stride, ref;    
}segment_t;

#define SEG_N 25

segment_t segment[SEG_N] = {
    /*1 */  {.size = 512 * 512, .stride = 1, .ref = 8}, 
    /*2 */  {.size = 16384 * 512, .stride = 4, .ref = 1}, 
    /*3 */  {.size = 16384 * 512, .stride = 4, .ref = 1}, 
    /*4 */  {.size = 16384 * 512, .stride = 4, .ref = 1}, 
    /*5 */  {.size = 16384 * 512, .stride = 4, .ref = 1}, 
    /*6 */  {.size = 16384 * 512, .stride = 4, .ref = 1}, 
    /*7 */  {.size = 16384 * 512, .stride = 4, .ref = 1}, 
    /*8 */  {.size = 16384 * 512, .stride = 1, .ref = 6}, 
    /*9 */  {.size = 16384 * 512, .stride = 1, .ref = 6}, 
    /*10 */ {.size = 38 * 512, .stride = 1, .ref = 1}, 
    /*11 */ {.size = 16384 * 512, .stride = 4, .ref = 1}, 
    /*12 */ {.size = 16384 * 512, .stride = 4, .ref = 1}, 
    /*13 */ {.size = 16384 * 512, .stride = 4, .ref = 2}, 
    /*14 */ {.size = 16384 * 512, .stride = 4, .ref = 1}, 
    /*15 */ {.size = 8192 * 512, .stride = 4, .ref = 2}, 
            {.size = 8192 * 512, .stride = 4, .ref = 1}, 
    /*16 */ {.size = 16384 * 512, .stride = 1, .ref = 6}, 
    /*17 */ {.size = 16384 * 512, .stride = 1, .ref = 6}, 
    /*18 */ {.size = 16384 * 512, .stride = 4, .ref = 1}, 
    /*19 */ {.size = 8192 * 512, .stride = 4, .ref = 1},     
    /*20 */ {.size = 16384 * 512, .stride = 4, .ref = 2},     
    /*21 */ {.size = 16384 * 512, .stride = 4, .ref = 1},     
    /*22 */ {.size = 4096 * 512, .stride = 4, .ref = 2},     
    /*23 */ {.size = 26624 * 512, .stride = 4, .ref = 1},     

    /*24 */ {.size = 768 * 512, .stride = 2, .ref = 62}     
};


int main() {
    numa_run_on_node(0);

    int N = 0, i;
    long long refs = 0;
    for (i = 0; i < SEG_N; ++i) {
        N += segment[i].size;
        refs += 1LL * segment[i].size * segment[i].ref / segment[i].stride;
    }
    
    long long* space = (long long*) malloc(N * 8);
    for (i = 0; i < N; ++i) {
        space[i] = 1LL * i * i;
    }

    long long sum = 0;
    int s, size = 0;
    for (s = 0; s < SEG_N; ++s) {
        int stride = segment[s].stride, ref = segment[s].ref;
        int l = size, r = l + segment[s].size;
        size += segment[s].size;

        // while (ref--) {
        //     for (i = l; i < r; i += stride) {
        //         sum += space[i];
        //     }
        // }

        int j;
        for (i = l; i < r; i += stride) {
            for (j = 0; j < ref; ++j)
                sum += space[i];
        }
    }

    printf("%d, %lld, %lld\n", N, refs, sum);
    return 0;
}