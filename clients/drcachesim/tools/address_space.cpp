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
{

}

address_space_t::~address_space_t()
{
    for (auto &mp : tid_map) {
        delete mp.second;
    }
    
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
        tid_map[tid] = new std::map<addr_t, uint32_t>();
        // tid_ts_map[tid] = new std::vector<std::pair<addr_t, uint32_t>>();
        std::vector<uint32_t> win_vec;
        win_vec.push_back(win_id);
        win_lst.insert(std::pair<uint32_t, std::vector<uint32_t>>(tid, win_vec)); 
    }
    else {
        win_lst[tid].push_back(win_id);
    }
    std::lock_guard<std::mutex> guard(shard_map_mutex_);
    shard_map_[std::make_pair(tid, win_id)] = shard;
    return reinterpret_cast<void *>(shard);
}

bool
address_space_t::parallel_shard_exit(void *shard_data)
{
    shard_data_t *shard = reinterpret_cast<shard_data_t *>(shard_data);
    print_shard_results(shard);
    print_shard_timestamps(shard);
    shard->mem_locs = shard->ref_map.size();
    shard->ref_map.clear();
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

    if (memref.marker.type == TRACE_TYPE_MARKER) {
        if (memref.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP) {
            shard->ts_vec.push_back(std::make_pair(memref.marker.marker_value, 0));
            shard->memref_after_ts = true;
        }
    }
    else if (memref.data.type == TRACE_TYPE_INSTR) {
        ++shard->num_non_branches;
    }
    else if (memref.data.type >= TRACE_TYPE_INSTR_DIRECT_JUMP && 
        memref.data.type <= TRACE_TYPE_INSTR_RETURN) {
            ++shard->num_branches;
        }
    else if (// type_is_instr(memref.instr.type) || type_is_prefetch(memref.data.type)
        memref.data.type == TRACE_TYPE_READ || memref.data.type == TRACE_TYPE_WRITE) 
    {
        if (shard->memref_after_ts) {
            assert(shard->ts_vec.size() > 0);
            shard->ts_vec[(int) shard->ts_vec.size() - 1].second = memref.data.addr >> line_size_bits_;
            shard->memref_after_ts = false;
        }
        ++shard->num_refs;
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
    printf("Summary for thread %d window %d: %ld mem_locs, %ld mem_refs, %ld branch instrs, %ld instrs\n", 
        shard->tid, shard->window_id, shard->ref_map.size(), shard->num_refs, shard->num_branches, shard->num_branches + shard->num_non_branches);
    
    assert(tid_map.find(shard->tid) != tid_map.end());
    auto total_map = tid_map[shard->tid];

    std::ofstream out_file;
    out_file.open(shard->trace_path + DIRSEP + "analysis." + std::to_string(shard->tid) + ".out");
    out_file << "addr,refs" << std::endl;
    for (auto it = shard->ref_map.begin(); it != shard->ref_map.end(); ++it) {
        addr_t addr = it->first; 
        uint32_t refs = it->second;
        out_file << std::hex << addr << ',' << std::dec << refs << std::endl;
        if (total_map->find(addr) == total_map->end()) {
            total_map->insert(std::pair<addr_t, uint32_t>(addr, refs));
        }
        else {
            (*total_map)[addr] += refs;
        }
    }
    out_file.close();
    // printf("Result for window %d\n", shard->window_id);   
    // printf("Total accesses: %ld\n", shard->num_refs);
    // for (auto it = shard->ref_map.begin(); it != shard->ref_map.end(); ++it) {
    //     printf("%p %d\n", (void*) it->first, it->second);
    // }
    // printf("\n");
}

void 
address_space_t::print_shard_timestamps(const shard_data_t *shard) {
    printf("#Timestamps for thread %d window %d: %ld\n", 
        shard->tid, shard->window_id, shard->ts_vec.size());
    
    // assert(tid_ts_map.find(shard->tid) != tid_ts_map.end());
    // tid_ts_map[shard->tid]->push_back(std::make_pair(shard->ts_vec[0].first, shard->window_id));

    std::ofstream out_file;
    out_file.open(shard->trace_path + DIRSEP + "timestamp." + std::to_string(shard->tid) + ".out");
    out_file << "timestamp,addr" << std::endl;
    for (std::pair<addr_t, addr_t> p : shard->ts_vec) {
        out_file << std::dec << (uint64_t) p.first << ',' << std::hex << p.second << std::endl;
    }
    out_file.close();
}

void
address_space_t::print_total_results(uint32_t tid, const std::string& trace_path) {
    assert(tid_map.find(tid) != tid_map.end());
    auto total_map = tid_map[tid];

    std::ofstream out_file;
    out_file.open(trace_path + DIRSEP + "total_analysis." + std::to_string(tid) + ".out");
    out_file << "addr,refs" << std::endl;
    for (auto it = total_map->begin(); it != total_map->end(); ++it) {
        addr_t addr = it->first; 
        uint32_t refs = it->second;
        out_file << std::hex << addr << ',' << std::dec << refs << std::endl;
    }
    out_file.close();
}

void
address_space_t::print_total_timestamps(uint32_t tid, const std::string& trace_path) {
    // assert(tid_ts_map.find(tid) != tid_ts_map.end());
    // auto total_ts_map = tid_ts_map[tid];
    // sort(total_ts_map->begin(), total_ts_map->end());

    uint64_t start_ts = (uint64_t) shard_map_[std::make_pair(tid, 0)]->ts_vec[0].first;
    std::ofstream out_file;
    out_file.open(trace_path + DIRSEP + "total_timestamp." + std::to_string(tid) + ".out");
    out_file << "timestamp,addr" << std::endl;
    for (uint32_t win_id : win_lst[tid]) {
        shard_data_t *shard = shard_map_[std::make_pair(tid, win_id)];
        for (std::pair<addr_t, addr_t> p : shard->ts_vec) {
            out_file << std::dec << (uint64_t) p.first - start_ts << ',' << std::hex << p.second << std::endl;
        }
    }
    out_file.close();
}

bool
address_space_t::print_results()
{
    for (uint32_t tid : tid_lst) {
         assert(tid_map.find(tid) != tid_map.end());
        auto total_map = tid_map[tid];

        std::sort(win_lst[tid].begin(), win_lst[tid].end());
        
        // for (uint32_t win : win_lst[tid]) {
        //     print_shard_results(shard_map_[std::make_pair(tid, win)]);
        // }

        shard_data_t *shard = shard_map_[std::make_pair(tid, 0)];
        std::string trace_path = shard->trace_path;
        std::string win_subdir = "/trace/window.0000";
        std::size_t found = trace_path.rfind(win_subdir);
        if (found != std::string::npos) {
            trace_path.replace(found, win_subdir.length(), "");
            print_total_results(tid, trace_path);
            print_total_timestamps(tid, trace_path);
        }

        std::ofstream summary_file;
        summary_file.open(trace_path + DIRSEP + "instr_summary." + std::to_string(tid) + ".out");
        summary_file << "win_id,mem_locs,mem_refs,instrs,branches" << std::endl;
        uint64_t total_refs = 0, total_instrs = 0,  total_branches = 0;
        for (uint32_t win : win_lst[tid]) {
            shard_data_t* shard = shard_map_[std::make_pair(tid, win)];
            summary_file << win << "," << shard->mem_locs << "," << shard->num_refs << "," << shard->num_non_branches + shard->num_branches << "," << shard->num_branches << std::endl;
            total_refs += shard->num_refs;
            total_instrs += shard->num_non_branches + shard->num_branches;
            total_branches += shard->num_branches;
        }
        summary_file << "all" << "," << total_map->size() << "," << total_refs << "," << total_instrs << "," << total_branches << std::endl;
        summary_file.close();
    }
    
    return true;
}

