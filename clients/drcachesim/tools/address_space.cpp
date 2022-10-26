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

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>
#include "../common/utils.h"
#include "address_space.h"

analysis_tool_t *
address_space_t_tool_create(const address_space_knobs_t &knobs)
{
    return new address_space_t(knobs);
}

address_space_t::address_space_t(const address_space_knobs_t &knobs)
    : knobs_(knobs)
    , line_size_bits_(compute_log2((int)knobs_.line_size))
    , max_window(0)
{

}

address_space_t::~address_space_t()
{
    for (auto &shard : shard_map_) {
        delete shard.second;
    }
}

bool
address_space_t::parallel_shard_supported()
{
    return true;
}

void*
address_space_t::parallel_shard_init(int shard_index, void *worker_data)
{
    // shard_data_t *shard = new shard_data_t(shard_index);
    // std::lock_guard<std::mutex> guard(shard_map_mutex_);
    // shard_map_[shard_index] = shard;
    // return reinterpret_cast<void *>(shard);
    // disabled
    return nullptr;
}

void*
address_space_t::parallel_shard_init(uint32_t tid, uint32_t win_id, std::string trace_path, void *worker_data)
{
    shard_data_t *shard = new shard_data_t(tid, win_id, trace_path);
    if (std::find(tid_lst.begin(), tid_lst.end(), tid) == tid_lst.end()) {
        tid_lst.push_back(tid);
    }
    max_window = std::max(max_window, win_id);
    std::lock_guard<std::mutex> guard(shard_map_mutex_);
    shard_map_[std::make_pair(tid, win_id)] = shard;
    return reinterpret_cast<void *>(shard);
}

bool
address_space_t::parallel_shard_exit(void *shard_data)
{
    // Nothing (we read the shard data in print_results).
    return true;
}

std::string
address_space_t::parallel_shard_error(void *shard_data)
{
    shard_data_t *shard = reinterpret_cast<shard_data_t *>(shard_data);
    return shard->error;
}

void 
address_space_t::do_synchronization() 
{
    // TODO;
}

bool
address_space_t::parallel_shard_memref(void *shard_data, const memref_t &memref)
{
    shard_data_t *shard = reinterpret_cast<shard_data_t *>(shard_data);

    // if (memref.data.type == TRACE_TYPE_THREAD_EXIT) {
    //     shard->tid = memref.exit.tid;
    //     return true;
    // }

    if (// type_is_instr(memref.instr.type) || type_is_prefetch(memref.data.type)
        memref.data.type == TRACE_TYPE_READ || memref.data.type == TRACE_TYPE_WRITE) 
    {
        ++shard->total_refs;
        addr_t key = memref.data.addr >> line_size_bits_;
        std::map<addr_t, uint32_t>::iterator it = shard->ref_map.find(key);
        if (it == shard->ref_map.end()) {
            shard->ref_map.insert(std::pair<addr_t, uint32_t>(key, 1));
        } 
        else {
            shard->ref_map[key] += 1;
        }
    }
    return true;
}

bool
address_space_t::process_memref(const memref_t &memref)
{
    // disabled
    return false;
}

void
address_space_t::print_shard_results(const shard_data_t *shard)
{
    printf("Total memory accesses for thread %d window %d: %ld\n", shard->tid, shard->window_id, shard->total_refs);
    
    std::ofstream out_file;
    out_file.open(shard->trace_path + DIRSEP + "analysis." + std::to_string(shard->tid) + ".out");
    out_file << "addr,refs" << std::endl;
    for (auto it = shard->ref_map.begin(); it != shard->ref_map.end(); ++it) {
        addr_t addr = it->first; 
        uint32_t refs = it->second;
        out_file << std::hex << addr << ',' << std::dec << refs << std::endl;
        if (total_map.find(addr) == total_map.end()) {
            total_map.insert(std::pair<addr_t, uint32_t>(addr, refs));
        }
        else {
            total_map[addr] += refs;
        }
    }
    out_file.close();
    // printf("Result for window %d\n", shard->window_id);   
    // printf("Total accesses: %ld\n", shard->total_refs);
    // for (auto it = shard->ref_map.begin(); it != shard->ref_map.end(); ++it) {
    //     printf("%p %d\n", (void*) it->first, it->second);
    // }
    // printf("\n");
}

void
address_space_t::print_total_results(uint32_t tid, const std::string& trace_path) {
    std::ofstream out_file;
    out_file.open(trace_path + DIRSEP + "total_analysis." + std::to_string(tid) + ".out");
    out_file << "addr,refs" << std::endl;
    for (auto it = total_map.begin(); it != total_map.end(); ++it) {
        addr_t addr = it->first; 
        uint32_t refs = it->second;
        out_file << std::hex << addr << ',' << std::dec << refs << std::endl;
    }
    out_file.close();
}

bool
address_space_t::print_results()
{
    for (uint32_t tid : tid_lst) {
        printf("tid: %d, max_window: %d\n", tid, max_window);
        total_map.clear();

        for (uint32_t win = 0; win <= max_window; ++win) {
            print_shard_results(shard_map_[std::make_pair(tid, win)]);
        }

        shard_data_t *shard = shard_map_[std::make_pair(tid, 0)];
        std::string trace_path = shard->trace_path;
        std::string win_subdir = "/window.0000";
        std::size_t found = trace_path.rfind(win_subdir);
        if (found != std::string::npos) {
            trace_path.replace(found, win_subdir.length(), "");
            print_total_results(tid, trace_path);
        }
    }
    
    return true;
}

