/*
 * Copyright (C) 2013-2015 OpenHeadend S.A.R.L.
 *
 * Authors: Christophe Massiot
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/** @file
 * @short unit tests for S337 video framer module
 * Unlike MPEG-2, H264 is so broad that it is hopeless to write a unit test
 * with a decent coverage. So for the moment just parse and dump an H264
 * elementary stream.
 */

#undef NDEBUG

#include <upipe/uprobe.h>
#include <upipe/uprobe_stdio.h>
#include <upipe/uprobe_prefix.h>
#include <upipe/uprobe_select_flows.h>
#include <upipe/uprobe_uref_mgr.h>
#include <upipe/uprobe_upump_mgr.h>
#include <upipe/uprobe_ubuf_mem.h>
#include <upipe/umem.h>
#include <upipe/umem_alloc.h>
#include <upipe/udict.h>
#include <upipe/udict_inline.h>
#include <upipe/udict_dump.h>
#include <upipe/uref.h>
#include <upipe/uref_std.h>
#include <upipe/uref_flow.h>
#include <upipe/uref_sound_flow.h>
#include <upipe/uref_clock.h>
#include <upipe/uref_dump.h>
#include <upipe/ubuf.h>
#include <upipe/ubuf_block_mem.h>
#include <upipe/uclock.h>
#include <upipe/upipe.h>
#include <upipe/upump.h>
#include <upump-ev/upump_ev.h>
#include <upipe-modules/upipe_file_source.h>
#include <upipe-modules/upipe_dump.h>
#include <upipe-modules/upipe_block_to_sound.h>
#include <upipe-modules/upipe_null.h>
#include <upipe-framers/upipe_s337_framer.h>

#include <ev.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#define UPROBE_LOG_LEVEL UPROBE_LOG_VERBOSE
#define UDICT_POOL_DEPTH 0
#define UREF_POOL_DEPTH 0
#define UBUF_POOL_DEPTH 0
#define UBUF_SHARED_POOL_DEPTH 0
#define UPUMP_POOL 0
#define UPUMP_BLOCKER_POOL 0

struct globalArgs_t {
    enum format {
        h264 = 1,
        s337 = 2,
    } format;
    char *inputFile;           /* input files */
} globalArgs;

int main(int argc, char **argv)
{

    /* Initialize globalArgs before we get to work. */
    globalArgs.format = 0; /* format: h264 = 1, 337 = 2 */
    globalArgs.inputFile = NULL;

    char *format = NULL;
    int opt = 0;
    while((opt = getopt(argc, argv, "i:f:")) != -1) {
        switch( opt ) {
            case 'f':
                format = optarg;
                if (strcmp("h264", format) == 0) {
                    globalArgs.format = h264; /* true */
                    break;
                }
                else if (strcmp("337", format) == 0) {
                    globalArgs.format = s337; /* true */
                    break;
                }
                break;
            case 'i':
                globalArgs.inputFile = optarg;
                break;
            default:
                /* You won't actually get here. */
                abort();
        }
    }

    int fsrc_size = 1602 * sizeof(uint32_t) * 2;

    const char *file = globalArgs.inputFile;

    /* structures managers */
    struct ev_loop *loop = ev_default_loop(0);
    struct upump_mgr *upump_mgr = upump_ev_mgr_alloc(loop, UPUMP_POOL,
                                                     UPUMP_BLOCKER_POOL);
    assert(upump_mgr != NULL);
    struct umem_mgr *umem_mgr = umem_alloc_mgr_alloc();
    struct udict_mgr *udict_mgr = udict_inline_mgr_alloc(UDICT_POOL_DEPTH,
                                                         umem_mgr, -1, -1);
    struct uref_mgr *uref_mgr = uref_std_mgr_alloc(UREF_POOL_DEPTH, udict_mgr,
                                                   0);
    udict_mgr_release(udict_mgr);

    /* probes */
    struct uprobe *uprobe;
    uprobe = uprobe_stdio_alloc(NULL, stderr, UPROBE_LOG_VERBOSE);
    assert(uprobe != NULL);
    uprobe = uprobe_uref_mgr_alloc(uprobe, uref_mgr);
    assert(uprobe != NULL);
    uprobe = uprobe_upump_mgr_alloc(uprobe, upump_mgr);
    assert(uprobe != NULL);
    uprobe = uprobe_ubuf_mem_alloc(uprobe, umem_mgr, UBUF_POOL_DEPTH,
                                   UBUF_SHARED_POOL_DEPTH);
    assert(uprobe != NULL);
    uref_mgr_release(uref_mgr);
    upump_mgr_release(upump_mgr);
    umem_mgr_release(umem_mgr);

    /* pipes */
    struct upipe_mgr *upipe_fsrc_mgr = upipe_fsrc_mgr_alloc();
    struct upipe *upipe_src = upipe_void_alloc(upipe_fsrc_mgr,
                   uprobe_pfx_alloc(uprobe_use(uprobe), UPROBE_LOG_DEBUG,
                                    "fsrc"));
    upipe_set_output_size(upipe_src, fsrc_size);
    upipe_mgr_release(upipe_fsrc_mgr);
    if (!ubase_check(upipe_set_uri(upipe_src, file))) {
        fprintf(stderr, "invalid file\n");
        exit(EXIT_FAILURE);
    }

    struct upipe *upipe = NULL;

    if ( globalArgs.format == 1) {
        struct upipe_mgr *h264f_mgr = upipe_h264f_mgr_alloc();
        upipe = upipe_void_alloc_output(upipe_src, h264f_mgr,
                    uprobe_pfx_alloc(uprobe_use(uprobe), UPROBE_LOG_DEBUG,
                                     "h264f"));
    }
    else if ( globalArgs.format == 2 ) {
        
        struct uref *flow_def = uref_std_alloc(uref_mgr);
        uref_flow_set_def(flow_def, "sound.s32.");
        uref_sound_flow_set_channels(flow_def, 2);
        uref_sound_flow_set_sample_size(flow_def, 8);
        uref_sound_flow_set_planes(flow_def, 1);
        uref_sound_flow_add_plane(flow_def, "l");
        uref_sound_flow_set_raw_sample_size(flow_def, 20);

        struct upipe_mgr *upipe_block_to_sound_mgr = upipe_block_to_sound_mgr_alloc();
        struct upipe *upipe_block_to_sound  = upipe_flow_alloc_output(upipe_src,
                       upipe_block_to_sound_mgr, uprobe_pfx_alloc(uprobe_use(uprobe),
                       UPROBE_LOG_DEBUG, "block_to_sound"), flow_def);
        assert(upipe_block_to_sound != NULL);
        upipe_release(upipe_block_to_sound);

        struct upipe_mgr *s337f_mgr = upipe_s337f_mgr_alloc();
        upipe = upipe_void_alloc_output(upipe_block_to_sound, s337f_mgr,
                       uprobe_pfx_alloc(uprobe_use(uprobe), UPROBE_LOG_DEBUG,
                                        "s337f"));
    }

    assert(upipe != NULL);

    struct upipe_mgr *null_mgr = upipe_null_mgr_alloc();
    upipe = upipe_void_chain_output(upipe, null_mgr,
                   uprobe_pfx_alloc(uprobe_use(uprobe), UPROBE_LOG_VERBOSE,
                                    "null"));
    assert(upipe != NULL);
    upipe_release(upipe);

    /* main loop */
    ev_loop(loop, 0);

    upipe_release(upipe_src);

    uprobe_release(uprobe);

    ev_default_destroy();
    return 0;
}