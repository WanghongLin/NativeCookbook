//
// Created by mutter on 5/15/16.
//

#include <libavcodec/avcodec.h>
#include "ffmpeg.h"
#include "libavfilter/avfilter.h"

AVFilterGraph* filter_graph;
AVFilterContext* buffer_ctx;
AVFilterContext* buffersink_ctx;

int init_video_filter(AVCodecContext *ic_ctx)
{
    avfilter_register_all();
    int ret = 0;

    AVFilter* buffer = avfilter_get_by_name("buffer");
    AVFilter* buffersink = avfilter_get_by_name("buffersink");
    AVFilter* transpose = avfilter_get_by_name("transpose");
    AVFilterContext* transpose_ctx;
    filter_graph = avfilter_graph_alloc();
    char buffer_args[256] = { 0 };

    if (!buffer || !buffersink || !transpose || !filter_graph) {
        ret = AVERROR(ENOMEM);
        LOGD("Cannot get filter %s\n", av_err2str(AVERROR(ENOMEM)));
        goto end;
    }

    snprintf(buffer_args, sizeof(buffer_args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             ic_ctx->width, ic_ctx->height,
             ic_ctx->pix_fmt,
             ic_ctx->time_base.num, ic_ctx->time_base.den,
             ic_ctx->sample_aspect_ratio.num, ic_ctx->sample_aspect_ratio.den);
    if ((ret = avfilter_graph_create_filter(&buffer_ctx, buffer, "in", buffer_args, NULL, filter_graph)) < 0) {
        LOGD("Cannot create buffer filter %s\n", av_err2str(ret));
        goto end;
    }

    if ((ret = avfilter_graph_create_filter(&transpose_ctx, transpose, "transpose", "1:portrait", NULL, filter_graph)) < 0) {
        LOGD("Cannot create transpose filter %s\n", av_err2str(ret));
        goto end;
    }

    if ((ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", NULL, NULL, filter_graph)) < 0) {
        LOGD("Cannot create buffer sink %s\n", av_err2str(ret));
        goto end;
    }

    if (avfilter_link(buffer_ctx, 0, transpose_ctx, 0) == 0) {
        if (avfilter_link(transpose_ctx, 0, buffersink_ctx, 0) == 0) {
            LOGD("Link filters");
        } else {
            goto end;
        }
    } else {
        goto end;
    }

    ret = avfilter_graph_config(filter_graph, NULL);
    if (ret < 0) {
        LOGD("Cannot config filter graph\n");
        goto end;
    }

    end:
    return ret;
}