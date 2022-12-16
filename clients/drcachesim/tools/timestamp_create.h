/* **********************************************************
 * Copyright (c) 2018 Google, Inc.  All rights reserved.
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


#ifndef _TIMESTAMP_CAEATE_H_
#define _TIMESTAMP_CAEATE_H_ 1

#include "analysis_tool.h"

/**
 * The options for timestamp_tool_create().
 * The options are currently documented in \ref sec_drcachesim_ops.
 */
// These options are currently documented in ../common/options.cpp.
struct timestamp_knobs_t {
    timestamp_knobs_t()
        : trace_dir("")
        , timestamp_file_0("")
        , timestamp_file_1("")
        , line_size(64)
        , verbose(0)
        , main_tid(0)
    {
    }
    std::string trace_dir;
    std::string timestamp_file_0;
    std::string timestamp_file_1;
    unsigned int line_size;
    unsigned int verbose;
    uint32_t main_tid;
};

/** Creates an analysis tool which computes reuse distance. */
analysis_tool_t *
timestamp_t_tool_create(const timestamp_knobs_t &knobs);

#endif /* _timestamp_CREATE_H_ */
