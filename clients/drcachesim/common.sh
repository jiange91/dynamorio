build/bin64/drrun -t drcachesim -trace_for_instrs 800000000 -retrace_every_instrs 1 -offline -- ./test/linpack_1000/linpack_1000

time build/bin64/drrun -t drcachesim -simulator_type address_space -indir ...

time build/bin64/drrun -t drcachesim -trace_for_instrs 2000000000 -retrace_every_instrs 1 -offline -- ../AIFM/aifm/DataFrame/original/build/bin/nyc

time build/bin64/drrun -t drcachesim -simulator_type address_space -line_size 8 -indir drmemtrace.linpack_1000.1246095.8142.dir

build/bin64/drrun -t drcachesim -trace_for_instrs 500000000 -retrace_every_instrs 1 -offline 