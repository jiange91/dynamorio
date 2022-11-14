time build/bin64/drrun -t drcachesim -trace_for_instrs 2000000000 -retrace_every_instrs 1 -offline -- ../AIFM/aifm/DataFrame/original/build/bin/nyc

time build/bin64/drrun -t drcachesim -simulator_type address_space -line_size 8 -indir drmemtrace.linpack_1000.1246095.8142.dir

time build/bin64/drrun -t drcachesim -simulator_type timestamp -timestamp_file drmemtrace.linpack_100.1785304.8711.dir/raw/drmemtrace.linpack_100.1785304.1788.raw.lz4 -line_size 8 -indir drmemtrace.linpack_100.1785251.7381.dir/ 

time build/bin64/drrun -t drcachesim -simulator_type timestamp -timestamp_file_0 drmemtrace.linpack_100.1813526.6665.dir/raw/drmemtrace.linpack_100.1813526.3502.raw.lz4 -timestamp_file_1 drmemtrace.linpack_100.1823141.1757.dir/raw/drmemtrace.linpack_100.1823141.7530.raw.lz4 -indir drmemtrace.linpack_100.1831033.7377.dir

time build/bin64/drrun -t drcachesim -only_trace_timestamp -offline -- ./test/linpack_100/linpack_100 

time build/bin64/drrun -t drcachesim -log_window_limit 5 -offline -- ./test/linpack_100/linpack_100

time build/bin64/drrun -t drcachesim  -trace_for_instrs 1000000 -retrace_every_instrs 1 -offline -- ./test/linpack_100/linpack_100

time build/bin64/drrun -t drcachesim -simulator_type address_space -line_size 8 -only_analyze_main_thread -indir drmemtrace.nyc.2841584.3218.dir
./test/nyc/AIFM/aifm/DataFrame/original/build/bin/nyc

time build/bin64/drrun -t drcachesim -simulator_type address_space -line_size 8 -jobs 3 -analyze_window_subset 0,2,4 -indr drmemtrace.sample.2936316.8302.dir