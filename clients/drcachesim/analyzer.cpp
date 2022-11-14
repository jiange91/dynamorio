/* **********************************************************
 * Copyright (c) 2016-2022 Google, Inc.  All rights reserved.
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

#include <iostream>
#include <thread>
#include <algorithm>
#include "analysis_tool.h"
#include "analyzer.h"
#include "reader/file_reader.h"
#ifdef HAS_ZLIB
#    include "reader/compressed_file_reader.h"
#endif
#ifdef HAS_ZIP
#    include "reader/zipfile_file_reader.h"
#endif
#ifdef HAS_SNAPPY
#    include "reader/snappy_file_reader.h"
#endif
#include "common/utils.h"

#ifdef HAS_ZLIB
// Even if the file is uncompressed, zlib's gzip interface is faster than
// file_reader_t's fstream in our measurements, so we always use it when
// available.
typedef compressed_file_reader_t default_file_reader_t;
#else
typedef file_reader_t<std::ifstream *> default_file_reader_t;
#endif

analyzer_t::analyzer_t()
    : success_(true)
    , num_tools_(0)
    , tools_(NULL)
    , parallel_(true)
    , worker_count_(0)
    // ADD 
    , need_sync(false)
    // END
{
    /* Nothing else: child class needs to initialize. */
}

#if defined(HAS_SNAPPY) || defined(HAS_ZIP)
static bool
ends_with(const std::string &str, const std::string &with)
{
    size_t pos = str.rfind(with);
    if (pos == std::string::npos)
        return false;
    return (pos + with.size() == str.size());
}
#endif

static std::unique_ptr<reader_t>
get_reader(const std::string &path, int verbosity)
{
#if defined(HAS_SNAPPY) || defined(HAS_ZIP)
#    ifdef HAS_SNAPPY
    if (ends_with(path, ".sz"))
        return std::unique_ptr<reader_t>(new snappy_file_reader_t(path, verbosity));
#    endif
#    ifdef HAS_ZIP
    if (ends_with(path, ".zip"))
        return std::unique_ptr<reader_t>(new zipfile_file_reader_t(path, verbosity));
#    endif
    // If path is a directory, and any file in it ends in .sz, return a snappy reader.
    if (directory_iterator_t::is_directory(path)) {
        directory_iterator_t end;
        directory_iterator_t iter(path);
        if (!iter) {
            ERRMSG("Failed to list directory %s: %s", path.c_str(),
                   iter.error_string().c_str());
            return nullptr;
        }
        for (; iter != end; ++iter) {
#    ifdef HAS_SNAPPY
            if (ends_with(*iter, ".sz")) {
                return std::unique_ptr<reader_t>(
                    new snappy_file_reader_t(path, verbosity));
            }
#    endif
#    ifdef HAS_ZIP
            if (ends_with(*iter, ".zip")) {
                return std::unique_ptr<reader_t>(
                    new zipfile_file_reader_t(path, verbosity));
            }
#    endif
        }
    }
#endif
    // No snappy/zlib support, or didn't find a .sz/.zip file.
    return std::unique_ptr<reader_t>(new default_file_reader_t(path, verbosity));
}

bool
analyzer_t::init_file_reader(const std::string &trace_path, int verbosity, uint32_t main_tid, std::vector<uint32_t> win_subset)
{
    verbosity_ = verbosity;
    if (trace_path.empty()) {
        ERRMSG("Trace file name is empty\n");
        return false;
    }
    for (int i = 0; i < num_tools_; ++i) {
        if (parallel_ && !tools_[i]->parallel_shard_supported()) {
            parallel_ = false;
            break;
        }
    }
    if (parallel_ && directory_iterator_t::is_directory(trace_path)) {
        // ADDED
        std::vector<std::string> windirs;
        std::vector<uint32_t> win_ids;
        if (trace_path.rfind("window") != std::string::npos || 
            !directory_iterator_t::is_directory(trace_path + std::string(DIRSEP) + "window.0000")) {
            // if (raw2trace_directory_t::window_subdir_if_present(trace_path) == trace_path) {
            // if windir exists and trace_path contains windir 
            // Or if windir doesn't exsit at all.
            windirs.push_back(trace_path);
            win_ids.push_back(0);
        }
        else {
            // windir exists but trace_path doesn't contain windir
            // bool windir_present = false;
            for (int window_id = 0; ; ++window_id) {
                char subdir[20];
                sprintf(subdir, "window.%04d", window_id);

                std::string windir = trace_path + std::string(DIRSEP) + std::string(subdir);
                if (directory_iterator_t::is_directory(windir)) {
                    if (win_subset.size() == 0 ||
                        std::find(win_subset.begin(), win_subset.end(), window_id) != win_subset.end()) {
                        windirs.push_back(windir);
                        win_ids.push_back(window_id);
                        // printf("analyze_win: %d\n", window_id);
                    }
                }
                else {
                    break;
                }
            }
            // DR_ASSERT(windir_present == true);
        }

        for (size_t i = 0; i < windirs.size(); ++i) {
            std::string windir = windirs[i];
            uint32_t window_id = win_ids[i];
            directory_iterator_t end;
            directory_iterator_t iter(windir);
            if (!iter) {
                ERRMSG("Failed to list directory %s: %s", windir.c_str(),
                    iter.error_string().c_str());
                return false;
            }
            for (; iter != end; ++iter) {
                const std::string fname = *iter;
                if (fname == "." || fname == "..")
                    continue;
                // ADDED
                if (fname.find("analysis") != std::string::npos 
                   || fname.find("timestamp") != std::string::npos
                   || fname.find("instr_summary") != std::string::npos
                   || fname.find("bb_count") != std::string::npos)
                    continue;
                // END

                uint32_t tid, tmp;
                sscanf(fname.c_str(), "drmemtrace.%*[0-9A-Za-z_].%d.%d.trace.gz", &tid, &tmp); // TODO
                
                if (main_tid != 0 && main_tid != tid)
                    continue;

                const std::string file_path = windir + DIRSEP + fname;
                std::unique_ptr<reader_t> reader = get_reader(file_path, verbosity);
                if (!reader) {
                    return false;
                }
                thread_data_.push_back(analyzer_shard_data_t(
                    static_cast<int>(thread_data_.size()), std::move(reader), file_path, windir, tid, window_id));
                VPRINT(this, 2, "Opened reader for %s\n", file_path.c_str());
            }
        }
        
        // END

        // DELETED
        // directory_iterator_t end;
        // directory_iterator_t iter(trace_path);
        // if (!iter) {
        //     ERRMSG("Failed to list directory %s: %s", trace_path.c_str(),
        //            iter.error_string().c_str());
        //     return false;
        // }
        // for (; iter != end; ++iter) {
        //     const std::string fname = *iter;
        //     if (fname == "." || fname == "..")
        //         continue;
        //     const std::string path = trace_path + DIRSEP + fname;
        //     std::unique_ptr<reader_t> reader = get_reader(path, verbosity);
        //     if (!reader) {
        //         return false;
        //     }
        //     thread_data_.push_back(analyzer_shard_data_t(
        //         static_cast<int>(thread_data_.size()), std::move(reader), path));
        //     VPRINT(this, 2, "Opened reader for %s\n", path.c_str());
        // }
        // END

        // Like raw2trace, we use a simple round-robin static work assigment.  This
        // could be improved later with dynamic work queue for better load balancing.
        if (worker_count_ <= 0)
            worker_count_ = std::thread::hardware_concurrency();
        worker_tasks_.resize(worker_count_);
        printf("analyzer workers: %d\n", worker_count_);
        int worker = 0;
        for (size_t i = 0; i < thread_data_.size(); ++i) {
            VPRINT(this, 2, "Worker %d assigned trace shard %zd\n", worker, i);
            worker_tasks_[worker].push_back(&thread_data_[i]);
            thread_data_[i].worker = worker;
            worker = (worker + 1) % worker_count_;
        }
    } else {
        parallel_ = false;
        // ADDED
        std::string path = trace_path + std::string(DIRSEP) + "window.0000";
        path = directory_iterator_t::is_directory(path) ? path : trace_path;
        // printf("open2: %s\n", path.c_str());
        // END
        serial_trace_iter_ = get_reader(/* trace_path */ path, verbosity);
        if (!serial_trace_iter_) {
            return false;
        }
        VPRINT(this, 2, "Opened serial reader for %s\n", /*trace_path.c_str()*/ path.c_str());
    }
    // It's ok if trace_end_ is a different type from serial_trace_iter_, they
    // will still compare true if both at EOF.
    trace_end_ = std::unique_ptr<default_file_reader_t>(new default_file_reader_t());
    return true;
}

analyzer_t::analyzer_t(const std::string &trace_path, analysis_tool_t **tools,
                       int num_tools, int worker_count)
    : success_(true)
    , num_tools_(num_tools)
    , tools_(tools)
    , parallel_(true)
    , worker_count_(worker_count)
{
    for (int i = 0; i < num_tools; ++i) {
        if (tools_[i] == NULL || !*tools_[i]) {
            success_ = false;
            error_string_ = "Tool is not successfully initialized";
            if (tools_[i] != NULL)
                error_string_ += ": " + tools_[i]->get_error_string();
            return;
        }
        const std::string error = tools_[i]->initialize();
        if (!error.empty()) {
            success_ = false;
            error_string_ = "Tool failed to initialize: " + error;
            return;
        }
    }
    if (!init_file_reader(trace_path))
        success_ = false;
    // ADDED
    // if (need_sync) {
    //     create_sync_worker();
    // }
    // END
}

analyzer_t::analyzer_t(const std::string &trace_path)
    : success_(true)
    , num_tools_(0)
    , tools_(NULL)
    // This external-iterator interface does not support parallel analysis.
    , parallel_(false)
    , worker_count_(0)
{
    if (!init_file_reader(trace_path))
        success_ = false;
    // ADDED
    // if (need_sync) {
    //     create_sync_worker();
    // }
    // END
}

analyzer_t::~analyzer_t()
{
    // Empty.
    // ADDED
    if (need_sync) {
        destory_sync_worker();
    }
    // END
}

// Work around clang-format bug: no newline after return type for single-char operator.
// clang-format off
bool
analyzer_t::operator!()
// clang-format on
{
    return !success_;
}

std::string
analyzer_t::get_error_string()
{
    return error_string_;
}

// Used only for serial iteration.
bool
analyzer_t::start_reading()
{
    if (!serial_trace_iter_->init()) {
        ERRMSG("Failed to read from trace\n");
        return false;
    }
    return true;
}

void
analyzer_t::process_tasks(/*ADDED*/ uint32_t worker_id, /*END*/ std::vector<analyzer_shard_data_t *> *tasks)
{
    if (tasks->empty()) {
        // ADDED
        // if (need_sync) {
        //     finished[worker_id] = true;
        //     ++ finished_workers;
        // }
        // END
        VPRINT(this, 1, "Worker has no tasks\n");
        return;
    }

    VPRINT(this, 1, "Worker %d assigned %zd task(s)\n", (*tasks)[0]->worker,
           tasks->size());
    std::vector<void *> worker_data(num_tools_);
    for (int i = 0; i < num_tools_; ++i)
        worker_data[i] = tools_[i]->parallel_worker_init((*tasks)[0]->worker);
    for (analyzer_shard_data_t *tdata : *tasks) {
        VPRINT(this, 1, "Worker %d starting on trace shard %d\n", tdata->worker,
               tdata->index);
        if (!tdata->iter->init()) {
            tdata->error = "Failed to read from trace" + tdata->trace_file;
            return;
        }
        std::vector<void *> shard_data(num_tools_);
        for (int i = 0; i < num_tools_; ++i) {
            // ADDED
            if (tools_[i]->analyzer_name == "address_space" || tools_[i]->analyzer_name == "timestamp") {
                shard_data[i] = tools_[i]->parallel_shard_init(tdata->tid, tdata->win_id, tdata->trace_path, worker_data[i]);
            }
            else 
            // END
                shard_data[i] = tools_[i]->parallel_shard_init(tdata->index, worker_data[i]);
        }

        if (tools_[0]->analyzer_name == "timestamp")
            continue;

        VPRINT(this, 1, "shard_data[0] is %p\n", shard_data[0]);
        for (; *tdata->iter != *trace_end_; ++(*tdata->iter)) {
            for (int i = 0; i < num_tools_; ++i) {
                const memref_t &memref = **tdata->iter;
                if (!tools_[i]->parallel_shard_memref(shard_data[i], memref)) {
                    tdata->error = tools_[i]->parallel_shard_error(shard_data[i]);
                    VPRINT(this, 1,
                           "Worker %d hit shard memref error %s on trace shard %d\n",
                           tdata->worker, tdata->error.c_str(), tdata->index);
                    return;
                }
            }
        }
        VPRINT(this, 1, "Worker %d finished trace shard %d\n", tdata->worker,
               tdata->index);
        for (int i = 0; i < num_tools_; ++i) {
            if (!tools_[i]->parallel_shard_exit(shard_data[i])) {
                tdata->error = tools_[i]->parallel_shard_error(shard_data[i]);
                VPRINT(this, 1, "Worker %d hit shard exit error %s on trace shard %d\n",
                       tdata->worker, tdata->error.c_str(), tdata->index);
                return;
            }
        }

        // if (need_sync) {
        //     task_finished(worker_id);
        //     // printf("task_finished\n");
        //     // TODO;
        // }
    }
    for (int i = 0; i < num_tools_; ++i) {
        const std::string error = tools_[i]->parallel_worker_exit(worker_data[i]);
        if (!error.empty()) {
            (*tasks)[0]->error = error;
            VPRINT(this, 1, "Worker %d hit worker exit error %s\n", (*tasks)[0]->worker,
                   error.c_str());
            return;
        }
    }

    // ADDED
    // if (need_sync) {
    //     finished[worker_id] = true;
    //     ++ finished_workers;
    //     printf("finished_workers: %d\n", finished_workers);
    // }
    // END
}

// ADDED

void 
analyzer_t::create_sync_worker() {
    finished_workers = 0;
    finished = new bool[worker_count_];
    sem = new sem_t[worker_count_];
    for (int i = 0; i < worker_count_; ++i) {
        finished[i] = false;
        sem_init(&sem[i], 0, 0);
    }

    sync_worker = std::move(std::thread([this] {
        while (true) {
            for (int i = 0; i < worker_count_; ++i) {
                while (sem_trywait(&sem[i]) != 0 && !finished[i]);
                printf("Loop %d ends\n", i);
            }
            for (int i = 0; i < num_tools_; ++i) {
                tools_[i]->do_synchronization();
            }
            if (finished_workers == worker_count_)
                break;
        }
    }));
}

void 
analyzer_t::destory_sync_worker() {
    sync_worker.join();
    printf("delete sem\n");
    delete[] sem;
}

void 
analyzer_t::task_finished(uint32_t worker_id) {
    // TODO
    sem_post(&sem[worker_id]);
}
// END

bool
analyzer_t::run()
{
    // XXX i#3286: Add a %-completed progress message by looking at the file sizes.
    if (!parallel_) {
        if (!start_reading())
            return false;
        for (; *serial_trace_iter_ != *trace_end_; ++(*serial_trace_iter_)) {
            for (int i = 0; i < num_tools_; ++i) {
                memref_t memref = **serial_trace_iter_;
                // We short-circuit and exit on an error to avoid confusion over
                // the results and avoid wasted continued work.
                if (!tools_[i]->process_memref(memref)) {
                    error_string_ = tools_[i]->get_error_string();
                    return false;
                }
            }
        }
        return true;
    }
    if (worker_count_ <= 0) {
        error_string_ = "Invalid worker count: must be > 0";
        return false;
    }
    std::vector<std::thread> threads;
    VPRINT(this, 1, "Creating %d worker threads\n", worker_count_);
    threads.reserve(worker_count_);
    for (int i = 0; i < worker_count_; ++i) {
        threads.emplace_back(
            std::thread(&analyzer_t::process_tasks, this,  /*ADDED*/ i, /*END*/ &worker_tasks_[i]));
    }
    for (std::thread &thread : threads)
        thread.join();
    for (auto &tdata : thread_data_) {
        if (!tdata.error.empty()) {
            error_string_ = tdata.error;
            return false;
        }
    }
    return true;
}

bool
analyzer_t::print_stats()
{
    for (int i = 0; i < num_tools_; ++i) {
        // Each tool should reset i/o state, but we reset the format here just in case.
        std::cerr << std::dec;
        if (!tools_[i]->print_results()) {
            error_string_ = tools_[i]->get_error_string();
            return false;
        }
        if (i + 1 < num_tools_) {
            // Separate tool output.
            std::cerr << "\n=========================================================="
                         "=================\n";
        }
    }
    return true;
}

// XXX i#3287: Figure out how to support parallel operation with this external
// iterator interface.
reader_t &
analyzer_t::begin()
{
    if (!start_reading())
        return *trace_end_;
    return *serial_trace_iter_;
}

reader_t &
analyzer_t::end()
{
    return *trace_end_;
}
