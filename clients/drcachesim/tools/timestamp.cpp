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
#include "timestamp.h"

analysis_tool_t *
timestamp_t_tool_create(const timestamp_knobs_t &knobs)
{
    return new timestamp_t(knobs);
}

timestamp_t::timestamp_t(const timestamp_knobs_t &knobs)
    : trace_dir(knobs.trace_dir)
    , line_size_bits_(compute_log2((int)knobs.line_size))
{   
    ts_file_[0] = new lz4_istream_t(knobs.timestamp_file_0);
    ts_file_[1] = new lz4_istream_t(knobs.timestamp_file_1);
    // printf("read_bbcount_file\n");
    read_bbcount_file();
    // printf("read_timestamp_trace\n");
    read_timestamp_trace(0);
    read_timestamp_trace(1);
}

timestamp_t::~timestamp_t()
{
    for (auto &shard : shard_map_) {
        delete shard.second;
    }

    for (int i = 0; i < 2; ++i) {
        for (auto &win_vec : win2tsvec_[i]) {
            delete win_vec.second;
        }    
    }
}

void
timestamp_t::read_timestamp_trace(int idx)
{
    std::istream *ts_file = ts_file_[idx];
    std::map<uint32_t, std::vector<std::pair<uint64_t, int64_t>>*> &win2tsvec = win2tsvec_[idx];
    
    offline_entry_t entry;
    uint32_t cur_win = 0;
    uint64_t cur_bbcount = 0;
    std::vector<std::pair<uint64_t, int64_t>> *cur_vec; 
    uint64_t cur_ts = 0, start_ts = 0, cur_bbidx = 0;

    while (ts_file->read((char *)&entry, sizeof(entry))) {
        if (win2tsvec.find(cur_win) == win2tsvec.end()) {
            if (cur_win >= win2bbcount.size())
                break;
            cur_bbcount = win2bbcount[cur_win];
            cur_vec = new std::vector<std::pair<uint64_t, int64_t>>();
            win2tsvec.insert(std::make_pair(cur_win, cur_vec));
            if (cur_bbidx > 0) {
                cur_vec->push_back(std::make_pair(cur_ts, cur_bbidx));
            }
        }
        if (entry.timestamp.type == OFFLINE_TYPE_TIMESTAMP) {
            cur_ts = entry.timestamp.usec;
            if (start_ts == 0)
                start_ts = cur_ts;
            cur_ts -= start_ts;
        }
        else if (entry.addr.type == OFFLINE_TYPE_MEMREF) {
            cur_bbidx += entry.addr.addr;
            if (cur_bbidx > cur_bbcount) {
                cur_win += 1;
                cur_bbidx -= cur_bbcount;
            }
            else {
                cur_vec->push_back(std::make_pair(cur_bbidx, cur_ts));
                // printf("%d, %ld, %ld\n", cur_win, cur_ts, cur_bbidx);
            }
        }
    }
}

std::string
timestamp_t::read_bbcount_file() 
{    
    std::string filename = trace_dir + DIRSEP + "bb_count.out";
    FILE* file = fopen(filename.c_str(), "r");

    // printf("bbcount file: %s\n", filename.c_str());
    
    if (file == NULL) {
        return "no such file.";
    }

    char tmp[100];
    int f_res = fscanf(file, "%s", tmp);
    assert(f_res > 0);
    
    uint32_t win_id;
    uint64_t bb_count;
    for (uint32_t i = 0; ; ++i) {
        f_res = fscanf(file, "%d,%ld", &win_id, &bb_count);
        if (f_res != 2) break;
        win2bbcount.push_back(bb_count);
    }

    return tmp != nullptr ? "" : "error";
}

bool
timestamp_t::parallel_shard_supported()
{
    return true;
}

void*
timestamp_t::parallel_shard_init(int shard_index, void *worker_data)
{
    return nullptr;
}

void*
timestamp_t::parallel_shard_init(uint32_t tid, uint32_t win_id, std::string trace_path, void *worker_data)
{
    shard_data_t *shard = new shard_data_t(tid, win_id, trace_path, win2tsvec_[0][win_id], win2tsvec_[1][win_id]);
    if (std::find(tid_lst.begin(), tid_lst.end(), tid) == tid_lst.end()) {
        tid_lst.push_back(tid);
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
timestamp_t::parallel_shard_exit(void *shard_data)
{
    return true;
}

std::string
timestamp_t::parallel_shard_error(void *shard_data)
{
    shard_data_t *shard = reinterpret_cast<shard_data_t *>(shard_data);
    return shard->error;
}

bool
timestamp_t::parallel_shard_memref(void *shard_data, const memref_t &memref)
{
    return true;
}

bool
timestamp_t::process_memref(const memref_t &memref)
{
    // disabled
    return false;
}

void 
timestamp_t::print_shard_timestamps(const shard_data_t *shard) {
    // printf("#Timestamps for thread %d window %d: %ld\n", 
    //     shard->tid, shard->window_id, shard->ts_vec[0]->size());
    
    std::ofstream file_0;
    file_0.open(shard->trace_path + DIRSEP + "timestamp_0.out");
    file_0 << "bb_idx,timestamp" << std::endl;
    for (std::pair<uint64_t, int64_t> p : *(shard->ts_vec[0])) {
        file_0 << p.first << ',' << p.second << std::endl;
    }
    file_0.close();

    std::ofstream file_1;
    file_1.open(shard->trace_path + DIRSEP + "timestamp_1.out");
    file_1 << "bb_idx,timestamp" << std::endl;
    for (std::pair<uint64_t, int64_t> p : *(shard->ts_vec[1])) {
        file_1 << p.first << ',' << p.second << std::endl;
    }
    file_1.close();

    std::ofstream file_cmp;
    file_cmp.open(shard->trace_path + DIRSEP + "timestamp_cmp.out");
    file_cmp << "bb_idx_0,bb_idx_1,timestamp_cmp" << std::endl;
    std::vector<std::pair<uint64_t, int64_t>>* ts_vec_0 = shard->ts_vec[0];
    std::vector<std::pair<uint64_t, int64_t>>* ts_vec_1 = shard->ts_vec[1]; 
    for (size_t i = 0; i < std::min(ts_vec_0->size(), ts_vec_1->size()); ++i) {
        file_cmp << (*ts_vec_0)[i].first << ' ' << (*ts_vec_1)[i].first << ' ' << (*ts_vec_1)[i].second - (*ts_vec_0)[i].second << std::endl;
    }
    file_cmp.close();
}


bool
timestamp_t::print_results()
{
    for (uint32_t tid : tid_lst) {
        std::sort(win_lst[tid].begin(), win_lst[tid].end());
        
        for (uint32_t win : win_lst[tid]) {
            shard_data_t* shard = shard_map_[std::make_pair(tid, win)];
            print_shard_timestamps(shard);
        }
    }
    
    return true;
}

