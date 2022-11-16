/* **********************************************************
 * Copyright (c) 2016-2020 Google, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef _ADDRESS_SPACE_H_
#define _ADDRESS_SPACE_H_ 1

#include <memory>
#include <mutex>
#include <map>
#include <unordered_map>
#include <string>
#include <fstream>
#include <assert.h>
#include <iostream>
#include "analysis_tool.h"
#include "address_space_create.h"
#include "memref.h"
// #include "tbb/concurrent_unordered_map.h"


// We see noticeable overhead in release build with an if() that directly
// checks knob_verbose, so for debug-only uses we turn it into something the
// compiler can remove for better performance without going so far as ifdef-ing
// big code chunks and impairing readability.

struct line_ref_t;
struct line_ref_list_t;

class address_space_t : public analysis_tool_t {
public:
    address_space_t(const address_space_knobs_t &knobs);
    ~address_space_t() override;
    bool
    process_memref(const memref_t &memref) override;
    bool
    print_results() override;
    bool
    parallel_shard_supported() override;
    void *
    parallel_shard_init(int shard_index, void *worker_data) override;
    void *
    parallel_shard_init(uint32_t tid, uint32_t win_id, std::string trace_path, void *worker_data) override;
    bool
    parallel_shard_exit(void *shard_data) override;
    bool
    parallel_shard_memref(void *shard_data, const memref_t &memref) override;
    std::string
    parallel_shard_error(void *shard_data) override;
    void 
    do_synchronization() override;
    
    // Global value for use in non-member code.
    static unsigned int knob_verbose;

protected:
    // We assume that the shard unit is the unit over which we should measure
    // distance.  By default this is a traced thread.  For serial operation we look
    // at the tid values and enforce it to be a thread, but for parallel we just use
    // the shards we're given.  This is for simplicity and to give the user a method
    // for computing over different units if for some reason that was desired.
    struct shard_data_t {
        shard_data_t(uint32_t tid_, uint32_t window_id_, const std::string& trace_path_) {
            tid = tid_;
            window_id = window_id_;
            trace_path = trace_path_;
            error = "";
            mem_locs = 0;
            num_refs = 0;
            num_branches = 0;
            num_non_branches = 0;
            memref_after_ts = false;
            heap_loc = heap_ref = stack_loc = stack_ref = 0;
        }

        std::map<addr_t, uint32_t> ref_map;
        std::vector<std::pair<addr_t, addr_t>> ts_vec;
        bool memref_after_ts;
        std::string error;
        std::string trace_path;
        uint64_t mem_locs;
        uint64_t num_refs;
        uint64_t num_branches;
        uint64_t num_non_branches;
        uint32_t tid;
        uint32_t window_id;
        uint64_t heap_loc, heap_ref, stack_loc, stack_ref;
    };

    std::map<uint32_t, std::map<addr_t, uint32_t>*> tid_map;
    // std::map<uint32_t, std::vector<std::pair<addr_t, uint32_t>>* > tid_ts_map;

    void
    print_shard_results(shard_data_t *shard);

    void 
    print_shard_timestamps(const shard_data_t *shard);

    void
    print_total_results(uint32_t tid, const std::string& trace_path);

    void
    print_total_timestamps(uint32_t tid, const std::string& trace_path);

    const size_t line_size_bits_;
    static const std::string TOOL_NAME;

    // In parallel operation the keys are "shard indices": just ints.
    std::map<std::pair<uint32_t, uint32_t>, shard_data_t *> shard_map_;
    // This mutex is only needed in parallel_shard_init.  In all other accesses to
    // shard_map (process_memref, print_results) we are single-threaded.
    std::mutex shard_map_mutex_;
    std::vector<uint32_t> tid_lst;
    std::map<uint32_t, std::vector<uint32_t>> win_lst;

    static const addr_t stack_bound = 0xfff00000000;
};


#endif /* _ADDRESS_SPACE_H_ */