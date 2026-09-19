/*
 * Copyright 2026 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "th1520_g2d/th1520_g2d.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <etnaviv_drmif.h>

/* Register descriptions generated from the permissively licensed rnndb. */
#include "cmdstream.xml.h"
#include "state.xml.h"
#include "state_2d.xml.h"

struct th1520_g2d {
	int fd;
	struct etna_device *dev;
	struct etna_gpu *gpu;
	struct etna_pipe *pipe;
	pthread_mutex_t lock;
};

static void emit_load_state(struct etna_cmd_stream *stream, uint16_t offset,
			    uint16_t count)
{
	etna_cmd_stream_emit(stream,
		VIV_FE_LOAD_STATE_HEADER_OP_LOAD_STATE |
		VIV_FE_LOAD_STATE_HEADER_OFFSET(offset) |
		(VIV_FE_LOAD_STATE_HEADER_COUNT(count) &
		 VIV_FE_LOAD_STATE_HEADER_COUNT__MASK));
}

static void set_state(struct etna_cmd_stream *stream, uint32_t address,
		      uint32_t value)
{
	etna_cmd_stream_reserve(stream, 2);
	emit_load_state(stream, address >> 2, 1);
	etna_cmd_stream_emit(stream, value);
}

static void set_state_bo(struct etna_cmd_stream *stream, uint32_t address,
			 struct etna_bo *bo, uint32_t flags, uint32_t offset)
{
	struct etna_reloc reloc = {
		.bo = bo,
		.flags = flags,
		.offset = offset,
	};

	etna_cmd_stream_reserve(stream, 2);
	emit_load_state(stream, address >> 2, 1);
	etna_cmd_stream_reloc(stream, &reloc);
}

static bool is_rgb8888(uint32_t format)
{
	return format == TH1520_G2D_FORMAT_XRGB8888 ||
	       format == TH1520_G2D_FORMAT_ARGB8888 ||
	       format == TH1520_G2D_FORMAT_XBGR8888 ||
	       format == TH1520_G2D_FORMAT_ABGR8888;
}

static uint32_t rgb_de_format(uint32_t format)
{
	return format == TH1520_G2D_FORMAT_XRGB8888 ||
	       format == TH1520_G2D_FORMAT_XBGR8888 ?
	       DE_FORMAT_X8R8G8B8 : DE_FORMAT_A8R8G8B8;
}

static uint32_t rgb_de_swizzle(uint32_t format)
{
	return format == TH1520_G2D_FORMAT_XBGR8888 ||
	       format == TH1520_G2D_FORMAT_ABGR8888 ?
	       DE_SWIZZLE_ABGR : DE_SWIZZLE_ARGB;
}

static int validate_rgb(const struct th1520_g2d_image *image,
			const struct th1520_g2d_rect *rect)
{
	uint64_t last_byte;

	if (!image || !rect || !is_rgb8888(image->format) ||
	    image->num_planes != 1 || image->modifier != 0 ||
	    image->planes[0].dmabuf_fd < 0 || !rect->width || !rect->height)
		return -EINVAL;
	if (rect->x > image->width || rect->y > image->height ||
	    rect->width > image->width - rect->x ||
	    rect->height > image->height - rect->y)
		return -ERANGE;
	if (image->planes[0].stride < image->width * 4u ||
	    (image->planes[0].offset & 3u))
		return -EINVAL;

	last_byte = (uint64_t)image->planes[0].offset +
		    (uint64_t)(image->height - 1u) * image->planes[0].stride +
		    (uint64_t)image->width * 4u;
	if (last_byte > UINT32_MAX)
		return -E2BIG;

	return 0;
}

static int check_bo_size(struct etna_bo *bo,
			 const struct th1520_g2d_image *image)
{
	uint64_t required = (uint64_t)image->planes[0].offset +
		(uint64_t)(image->height - 1u) * image->planes[0].stride +
		(uint64_t)image->width * 4u;

	return required <= etna_bo_size(bo) ? 0 : -ENOSPC;
}

static int validate_nv12(const struct th1520_g2d_image *image,
			 const struct th1520_g2d_rect *rect)
{
	uint64_t y_end, uv_end;

	if (!image || !rect || image->format != TH1520_G2D_FORMAT_NV12 ||
	    image->num_planes != 2 || image->modifier != 0 ||
	    image->planes[0].dmabuf_fd < 0 ||
	    image->planes[1].dmabuf_fd < 0 || !image->width ||
	    !image->height || !rect->width || !rect->height)
		return -EINVAL;
	if ((image->width | image->height | rect->x | rect->y |
	     rect->width | rect->height) & 1u)
		return -EINVAL;
	if (rect->x > image->width || rect->y > image->height ||
	    rect->width > image->width - rect->x ||
	    rect->height > image->height - rect->y)
		return -ERANGE;
	if (image->planes[0].stride < image->width ||
	    image->planes[1].stride < image->width)
		return -EINVAL;
	/* The GC620 one-pass NV12 writer needs 64-byte pitches. A 544-byte
	 * destination shears rows even with a padded RGB source; 576 is exact.
	 * Reject unsupported layouts instead of submitting corrupted output.
	 */
	if ((image->planes[0].stride | image->planes[1].stride) & 63u)
		return -EINVAL;

	y_end = (uint64_t)image->planes[0].offset +
		(uint64_t)(image->height - 1u) * image->planes[0].stride +
		image->width;
	uv_end = (uint64_t)image->planes[1].offset +
		(uint64_t)(image->height / 2u - 1u) *
		image->planes[1].stride + image->width;
	if (y_end > UINT32_MAX || uv_end > UINT32_MAX)
		return -E2BIG;

	return 0;
}

static int check_plane_size(struct etna_bo *bo, uint32_t offset,
			    uint32_t stride, uint32_t rows,
			    uint32_t row_bytes)
{
	uint64_t required;

	if (!rows)
		return -EINVAL;
	required = (uint64_t)offset + (uint64_t)(rows - 1u) * stride +
		row_bytes;
	return required <= etna_bo_size(bo) ? 0 : -ENOSPC;
}

static void emit_state_block(struct etna_cmd_stream *stream, uint32_t address,
			     const uint32_t *values, uint16_t count)
{
	etna_cmd_stream_reserve(stream, count + 1u);
	emit_load_state(stream, address >> 2, count);
	for (uint16_t i = 0; i < count; i++)
		etna_cmd_stream_emit(stream, values[i]);
}

/* 17 phases x 9 taps, packed as two signed 1.14 coefficients per word. */
static void make_identity_filter(uint32_t kernel[77])
{
	uint16_t coefficients[154] = { 0 };

	for (unsigned int phase = 0; phase < 17; phase++)
		coefficients[phase * 9u + 4u] = 0x4000;
	for (unsigned int i = 0; i < 77; i++)
		kernel[i] = coefficients[i * 2u] |
			    ((uint32_t)coefficients[i * 2u + 1u] << 16);
}

static void emit_common_state(struct etna_cmd_stream *stream)
{
	set_state(stream, VIVS_DE_ROP,
		  VIVS_DE_ROP_ROP_FG(0xcc) |
		  VIVS_DE_ROP_ROP_BG(0xcc) |
		  VIVS_DE_ROP_TYPE_ROP4);
	set_state(stream, VIVS_DE_CONFIG, 0);
	set_state(stream, VIVS_DE_SRC_ORIGIN_FRACTION, 0);
	set_state(stream, VIVS_DE_ALPHA_CONTROL, 0);
	set_state(stream, VIVS_DE_ALPHA_MODES, 0);
	set_state(stream, VIVS_DE_DEST_ROTATION_HEIGHT, 0);
	set_state(stream, VIVS_DE_SRC_ROTATION_HEIGHT, 0);
	set_state(stream, VIVS_DE_ROT_ANGLE, 0);
	set_state(stream, VIVS_DE_DEST_COLOR_KEY, 0);
	set_state(stream, VIVS_DE_GLOBAL_SRC_COLOR, 0);
	set_state(stream, VIVS_DE_GLOBAL_DEST_COLOR, 0);
	set_state(stream, VIVS_DE_COLOR_MULTIPLY_MODES, 0);
	set_state(stream, VIVS_DE_PE_TRANSPARENCY, 0);
	set_state(stream, VIVS_DE_PE_CONTROL, 0);
	set_state(stream, VIVS_DE_PE_DITHER_LOW, UINT32_MAX);
	set_state(stream, VIVS_DE_PE_DITHER_HIGH, UINT32_MAX);
}

static void emit_clip(struct etna_cmd_stream *stream,
		      const struct th1520_g2d_image *dst)
{
	set_state(stream, VIVS_DE_CLIP_TOP_LEFT,
		  VIVS_DE_CLIP_TOP_LEFT_X(0) |
		  VIVS_DE_CLIP_TOP_LEFT_Y(0));
	set_state(stream, VIVS_DE_CLIP_BOTTOM_RIGHT,
		  VIVS_DE_CLIP_BOTTOM_RIGHT_X(dst->width) |
		  VIVS_DE_CLIP_BOTTOM_RIGHT_Y(dst->height));
}

static void emit_draw_rect(struct etna_cmd_stream *stream,
			   const struct th1520_g2d_rect *rect)
{
	etna_cmd_stream_reserve(stream, 4);
	etna_cmd_stream_emit(stream, VIV_FE_DRAW_2D_HEADER_OP_DRAW_2D |
				       VIV_FE_DRAW_2D_HEADER_COUNT(1));
	etna_cmd_stream_emit(stream, 0);
	etna_cmd_stream_emit(stream,
		VIV_FE_DRAW_2D_TOP_LEFT_X(rect->x) |
		VIV_FE_DRAW_2D_TOP_LEFT_Y(rect->y));
	etna_cmd_stream_emit(stream,
		VIV_FE_DRAW_2D_BOTTOM_RIGHT_X(rect->x + rect->width) |
		VIV_FE_DRAW_2D_BOTTOM_RIGHT_Y(rect->y + rect->height));

	/* Padding/NOPs required by older 2D command processors. */
	set_state(stream, 1, 0);
	set_state(stream, 1, 0);
	set_state(stream, 1, 0);
	set_state(stream, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_PE2D);
}

static int submit(struct etna_cmd_stream *stream, int in_fence_fd,
		  int *out_fence_fd)
{
	if (out_fence_fd) {
		*out_fence_fd = -1;
		etna_cmd_stream_flush2(stream, in_fence_fd, out_fence_fd);
		return *out_fence_fd >= 0 ? 0 : -EIO;
	}

	if (in_fence_fd >= 0) {
		int fence_fd = -1;
		etna_cmd_stream_flush2(stream, in_fence_fd, &fence_fd);
		if (fence_fd < 0)
			return -EIO;
		close(fence_fd);
		return 0;
	}

	etna_cmd_stream_finish(stream);
	return 0;
}

struct th1520_g2d *th1520_g2d_open(const char *render_node)
{
	struct th1520_g2d *g2d;
	uint64_t features;

	if (!render_node) {
		errno = EINVAL;
		return NULL;
	}

	g2d = calloc(1, sizeof(*g2d));
	if (!g2d)
		return NULL;
	g2d->fd = -1;

	g2d->fd = open(render_node, O_RDWR | O_CLOEXEC);
	if (g2d->fd < 0)
		goto fail;
	g2d->dev = etna_device_new(g2d->fd);
	if (!g2d->dev)
		goto fail;

	for (unsigned int core = 0; core < 16; core++) {
		g2d->gpu = etna_gpu_new(g2d->dev, core);
		if (!g2d->gpu)
			break;
		if (!etna_gpu_get_param(g2d->gpu, ETNA_GPU_FEATURES_0,
					&features) && (features & (1u << 9)))
			break;
		etna_gpu_del(g2d->gpu);
		g2d->gpu = NULL;
	}
	if (!g2d->gpu) {
		errno = ENODEV;
		goto fail;
	}

	g2d->pipe = etna_pipe_new(g2d->gpu, ETNA_PIPE_2D);
	if (!g2d->pipe)
		goto fail;
	if (pthread_mutex_init(&g2d->lock, NULL))
		goto fail;

	return g2d;

fail:
	if (g2d->pipe)
		etna_pipe_del(g2d->pipe);
	if (g2d->gpu)
		etna_gpu_del(g2d->gpu);
	if (g2d->dev)
		etna_device_del(g2d->dev);
	if (g2d->fd >= 0)
		close(g2d->fd);
	free(g2d);
	return NULL;
}

void th1520_g2d_close(struct th1520_g2d *g2d)
{
	if (!g2d)
		return;
	pthread_mutex_destroy(&g2d->lock);
	etna_pipe_del(g2d->pipe);
	etna_gpu_del(g2d->gpu);
	etna_device_del(g2d->dev);
	close(g2d->fd);
	free(g2d);
}

int th1520_g2d_clear(struct th1520_g2d *g2d,
		     const struct th1520_g2d_image *dst,
		     const struct th1520_g2d_rect *rect,
		     uint32_t argb8888, int in_fence_fd, int *out_fence_fd)
{
	struct etna_cmd_stream *stream = NULL;
	struct etna_bo *dst_bo = NULL;
	int ret;

	if (!g2d)
		return -EINVAL;
	ret = validate_rgb(dst, rect);
	if (ret)
		return ret;

	pthread_mutex_lock(&g2d->lock);
	dst_bo = etna_bo_from_dmabuf(g2d->dev, dst->planes[0].dmabuf_fd);
	if (!dst_bo) {
		ret = errno ? -errno : -EIO;
		goto out;
	}
	ret = check_bo_size(dst_bo, dst);
	if (ret)
		goto out;
	stream = etna_cmd_stream_new(g2d->pipe, 0x100, NULL, NULL);
	if (!stream) {
		ret = -ENOMEM;
		goto out;
	}

	set_state(stream, VIVS_DE_SRC_STRIDE, 0);
	set_state(stream, VIVS_DE_SRC_ROTATION_CONFIG, 0);
	set_state(stream, VIVS_DE_SRC_CONFIG, 0);
	set_state(stream, VIVS_DE_SRC_ORIGIN, 0);
	set_state(stream, VIVS_DE_SRC_SIZE, 0);
	set_state(stream, VIVS_DE_SRC_COLOR_BG, 0);
	set_state(stream, VIVS_DE_SRC_COLOR_FG, 0);
	set_state(stream, VIVS_DE_STRETCH_FACTOR_LOW, 0);
	set_state(stream, VIVS_DE_STRETCH_FACTOR_HIGH, 0);
	set_state_bo(stream, VIVS_DE_DEST_ADDRESS, dst_bo,
		     ETNA_RELOC_WRITE, dst->planes[0].offset);
	set_state(stream, VIVS_DE_DEST_STRIDE, dst->planes[0].stride);
	set_state(stream, VIVS_DE_DEST_ROTATION_CONFIG, 0);
	set_state(stream, VIVS_DE_DEST_CONFIG,
		  VIVS_DE_DEST_CONFIG_FORMAT(DE_FORMAT_A8R8G8B8) |
		  VIVS_DE_DEST_CONFIG_COMMAND_CLEAR |
		  VIVS_DE_DEST_CONFIG_SWIZZLE(DE_SWIZZLE_ARGB) |
		  VIVS_DE_DEST_CONFIG_TILED_DISABLE |
		  VIVS_DE_DEST_CONFIG_MINOR_TILED_DISABLE);
	emit_clip(stream, dst);
	emit_common_state(stream);
	set_state(stream, VIVS_DE_CLEAR_PIXEL_VALUE32, argb8888);
	set_state(stream, VIVS_DE_CLEAR_BYTE_MASK, UINT32_MAX);
	set_state(stream, VIVS_DE_CLEAR_PIXEL_VALUE_LOW, argb8888);
	set_state(stream, VIVS_DE_CLEAR_PIXEL_VALUE_HIGH, argb8888);
	emit_draw_rect(stream, rect);
	ret = submit(stream, in_fence_fd, out_fence_fd);

out:
	if (stream)
		etna_cmd_stream_del(stream);
	if (dst_bo)
		etna_bo_del(dst_bo);
	pthread_mutex_unlock(&g2d->lock);
	return ret;
}

static int blit_rgb(struct th1520_g2d *g2d,
		    const struct th1520_g2d_image *src,
		    const struct th1520_g2d_rect *src_rect,
		    const struct th1520_g2d_image *dst,
		    const struct th1520_g2d_rect *dst_rect,
		    int in_fence_fd, int *out_fence_fd, bool source_over)
{
	struct etna_cmd_stream *stream = NULL;
	struct etna_bo *src_bo = NULL;
	struct etna_bo *dst_bo = NULL;
	int ret;

	if (!g2d)
		return -EINVAL;
	ret = validate_rgb(src, src_rect);
	if (ret)
		return ret;
	ret = validate_rgb(dst, dst_rect);
	if (ret)
		return ret;
	if (src_rect->width != dst_rect->width ||
	    src_rect->height != dst_rect->height)
		return -ENOTSUP;
	if (src->format == TH1520_G2D_FORMAT_XRGB8888 ||
	    src->format == TH1520_G2D_FORMAT_XBGR8888)
		source_over = false;

	pthread_mutex_lock(&g2d->lock);
	src_bo = etna_bo_from_dmabuf(g2d->dev, src->planes[0].dmabuf_fd);
	dst_bo = etna_bo_from_dmabuf(g2d->dev, dst->planes[0].dmabuf_fd);
	if (!src_bo || !dst_bo) {
		ret = errno ? -errno : -EIO;
		goto out;
	}
	ret = check_bo_size(src_bo, src);
	if (ret)
		goto out;
	ret = check_bo_size(dst_bo, dst);
	if (ret)
		goto out;
	stream = etna_cmd_stream_new(g2d->pipe, 0x100, NULL, NULL);
	if (!stream) {
		ret = -ENOMEM;
		goto out;
	}

	set_state_bo(stream, VIVS_DE_SRC_ADDRESS, src_bo, ETNA_RELOC_READ,
		     src->planes[0].offset);
	set_state(stream, VIVS_DE_SRC_STRIDE, src->planes[0].stride);
	set_state(stream, VIVS_DE_SRC_ROTATION_CONFIG, 0);
	set_state(stream, VIVS_DE_SRC_CONFIG,
		  VIVS_DE_SRC_CONFIG_SOURCE_FORMAT(rgb_de_format(src->format)) |
		  VIVS_DE_SRC_CONFIG_SWIZZLE(rgb_de_swizzle(src->format)) |
		  VIVS_DE_SRC_CONFIG_TILED_DISABLE |
		  VIVS_DE_SRC_CONFIG_LOCATION_MEMORY);
	set_state(stream, VIVS_DE_SRC_ORIGIN,
		  VIVS_DE_SRC_ORIGIN_X(src_rect->x) |
		  VIVS_DE_SRC_ORIGIN_Y(src_rect->y));
	set_state(stream, VIVS_DE_SRC_SIZE,
		  VIVS_DE_SRC_SIZE_X(src_rect->width) |
		  VIVS_DE_SRC_SIZE_Y(src_rect->height));
	set_state(stream, VIVS_DE_SRC_COLOR_BG, 0);
	set_state(stream, VIVS_DE_SRC_COLOR_FG, 0);
	set_state(stream, VIVS_DE_STRETCH_FACTOR_LOW,
		  VIVS_DE_STRETCH_FACTOR_LOW_X(1u << 16));
	set_state(stream, VIVS_DE_STRETCH_FACTOR_HIGH,
		  VIVS_DE_STRETCH_FACTOR_HIGH_Y(1u << 16));

	set_state_bo(stream, VIVS_DE_DEST_ADDRESS, dst_bo,
		     ETNA_RELOC_WRITE | (source_over ? ETNA_RELOC_READ : 0),
		     dst->planes[0].offset);
	set_state(stream, VIVS_DE_DEST_STRIDE, dst->planes[0].stride);
	set_state(stream, VIVS_DE_DEST_ROTATION_CONFIG, 0);
	set_state(stream, VIVS_DE_DEST_CONFIG,
		  VIVS_DE_DEST_CONFIG_FORMAT(rgb_de_format(dst->format)) |
		  VIVS_DE_DEST_CONFIG_COMMAND_BIT_BLT |
		  VIVS_DE_DEST_CONFIG_SWIZZLE(rgb_de_swizzle(dst->format)) |
		  VIVS_DE_DEST_CONFIG_TILED_DISABLE |
		  VIVS_DE_DEST_CONFIG_MINOR_TILED_DISABLE);
	emit_clip(stream, dst);
	emit_common_state(stream);
	if (source_over) {
		/* Premultiplied Porter-Duff OVER: Cs + Cd * (1 - As).
		 * No global alpha or straight-alpha multiply is applied.
		 */
		set_state(stream, VIVS_DE_ALPHA_CONTROL,
			  VIVS_DE_ALPHA_CONTROL_ENABLE_ON);
		set_state(stream, VIVS_DE_ALPHA_MODES,
			  VIVS_DE_ALPHA_MODES_SRC_BLENDING_MODE(DE_BLENDMODE_ONE) |
			  VIVS_DE_ALPHA_MODES_DST_BLENDING_MODE(DE_BLENDMODE_INVERSED));
	}
	emit_draw_rect(stream, dst_rect);
	ret = submit(stream, in_fence_fd, out_fence_fd);

out:
	if (stream)
		etna_cmd_stream_del(stream);
	if (dst_bo)
		etna_bo_del(dst_bo);
	if (src_bo)
		etna_bo_del(src_bo);
	pthread_mutex_unlock(&g2d->lock);
	return ret;
}

int th1520_g2d_blit(struct th1520_g2d *g2d,
		    const struct th1520_g2d_image *src,
		    const struct th1520_g2d_rect *src_rect,
		    const struct th1520_g2d_image *dst,
		    const struct th1520_g2d_rect *dst_rect,
		    int in_fence_fd, int *out_fence_fd)
{
	return blit_rgb(g2d, src, src_rect, dst, dst_rect, in_fence_fd,
			out_fence_fd, false);
}

int th1520_g2d_blend(struct th1520_g2d *g2d,
		     const struct th1520_g2d_image *src,
		     const struct th1520_g2d_rect *src_rect,
		     const struct th1520_g2d_image *dst,
		     const struct th1520_g2d_rect *dst_rect,
		     int in_fence_fd, int *out_fence_fd)
{
	return blit_rgb(g2d, src, src_rect, dst, dst_rect, in_fence_fd,
			out_fence_fd, true);
}

int th1520_g2d_rgb_to_nv12(struct th1520_g2d *g2d,
			   const struct th1520_g2d_image *src,
			   const struct th1520_g2d_rect *src_rect,
			   const struct th1520_g2d_image *dst,
			   const struct th1520_g2d_rect *dst_rect,
			   int in_fence_fd, int *out_fence_fd)
{
	struct etna_cmd_stream *stream = NULL;
	struct etna_bo *src_bo = NULL;
	struct etna_bo *y_bo = NULL;
	struct etna_bo *uv_bo = NULL;
	uint32_t kernel[77];
	int ret;

	if (!g2d)
		return -EINVAL;
	ret = validate_rgb(src, src_rect);
	if (ret)
		return ret;
	ret = validate_nv12(dst, dst_rect);
	if (ret)
		return ret;
	if (src->width != dst->width || src->height != dst->height ||
	    src_rect->x || src_rect->y || dst_rect->x || dst_rect->y ||
	    src_rect->width != src->width ||
	    src_rect->height != src->height ||
	    dst_rect->width != dst->width || dst_rect->height != dst->height)
		return -ENOTSUP;

	pthread_mutex_lock(&g2d->lock);
	src_bo = etna_bo_from_dmabuf(g2d->dev, src->planes[0].dmabuf_fd);
	y_bo = etna_bo_from_dmabuf(g2d->dev, dst->planes[0].dmabuf_fd);
	uv_bo = etna_bo_from_dmabuf(g2d->dev, dst->planes[1].dmabuf_fd);
	if (!src_bo || !y_bo || !uv_bo) {
		ret = errno ? -errno : -EIO;
		goto out;
	}
	ret = check_bo_size(src_bo, src);
	if (ret)
		goto out;
	ret = check_plane_size(y_bo, dst->planes[0].offset,
			       dst->planes[0].stride, dst->height, dst->width);
	if (ret)
		goto out;
	ret = check_plane_size(uv_bo, dst->planes[1].offset,
			       dst->planes[1].stride, dst->height / 2u,
			       dst->width);
	if (ret)
		goto out;

	stream = etna_cmd_stream_new(g2d->pipe, 0x400, NULL, NULL);
	if (!stream) {
		ret = -ENOMEM;
		goto out;
	}
	make_identity_filter(kernel);

	set_state_bo(stream, VIVS_DE_SRC_ADDRESS, src_bo, ETNA_RELOC_READ,
		     src->planes[0].offset);
	set_state(stream, VIVS_DE_SRC_STRIDE, src->planes[0].stride);
	/* GC620's one-pass source block walker uses logical pixels. */
	set_state(stream, 0x00012980, src->planes[0].stride / 4u);
	set_state(stream, VIVS_DE_UPLANE_ADDRESS, 0);
	set_state(stream, VIVS_DE_UPLANE_STRIDE, 0);
	set_state(stream, VIVS_DE_VPLANE_ADDRESS, 0);
	set_state(stream, VIVS_DE_VPLANE_STRIDE, 0);
	set_state(stream, VIVS_DE_SRC_ROTATION_CONFIG,
		  VIVS_DE_SRC_ROTATION_CONFIG_WIDTH(src->width) |
		  VIVS_DE_SRC_ROTATION_CONFIG_ROTATION_DISABLE);
	set_state(stream, VIVS_DE_SRC_CONFIG,
		  VIVS_DE_SRC_CONFIG_PE10_SOURCE_FORMAT(rgb_de_format(src->format)) |
		  VIVS_DE_SRC_CONFIG_SOURCE_FORMAT(rgb_de_format(src->format)) |
		  VIVS_DE_SRC_CONFIG_SWIZZLE(rgb_de_swizzle(src->format)) |
		  VIVS_DE_SRC_CONFIG_TILED_DISABLE |
		  VIVS_DE_SRC_CONFIG_LOCATION_MEMORY);
	set_state(stream, VIVS_DE_SRC_ORIGIN, 0);
	set_state(stream, VIVS_DE_SRC_SIZE,
		  VIVS_DE_SRC_SIZE_X(src->width) |
		  VIVS_DE_SRC_SIZE_Y(src->height));
	set_state(stream, VIVS_DE_STRETCH_FACTOR_LOW,
		  VIVS_DE_STRETCH_FACTOR_LOW_X(1u << 16));
	set_state(stream, VIVS_DE_STRETCH_FACTOR_HIGH,
		  VIVS_DE_STRETCH_FACTOR_HIGH_Y(1u << 16));

	set_state_bo(stream, VIVS_DE_DEST_ADDRESS, y_bo, ETNA_RELOC_WRITE,
		     dst->planes[0].offset);
	set_state(stream, VIVS_DE_DEST_STRIDE, dst->planes[0].stride);
	/* NV12's luma plane is one byte per logical pixel. */
	set_state(stream, 0x000013a0, dst->planes[0].stride);
	set_state_bo(stream, VIVS_DE_DE_PLANE2_ADDRESS, uv_bo,
		     ETNA_RELOC_WRITE, dst->planes[1].offset);
	set_state(stream, VIVS_DE_DE_PLANE2_STRIDE,
		  dst->planes[1].stride);
	/* GC620 expresses the NV12 chroma-plane logical stride in bytes. */
	set_state(stream, 0x000013a4, dst->planes[1].stride);
	set_state(stream, VIVS_DE_DE_PLANE3_ADDRESS, 0);
	set_state(stream, VIVS_DE_DE_PLANE3_STRIDE, 0);
	set_state(stream, VIVS_DE_DEYUV_CONVERSION,
		  VIVS_DE_DEYUV_CONVERSION_ENABLE_OFF);
	set_state(stream, VIVS_DE_DEST_ROTATION_CONFIG,
		  VIVS_DE_DEST_ROTATION_CONFIG_WIDTH(dst->width) |
		  VIVS_DE_DEST_ROTATION_CONFIG_ROTATION_DISABLE);
	set_state(stream, VIVS_DE_DEST_CONFIG,
		  VIVS_DE_DEST_CONFIG_FORMAT(DE_FORMAT_NV12) |
		  VIVS_DE_DEST_CONFIG_COMMAND_ONE_PASS_FILTER_BLT |
		  VIVS_DE_DEST_CONFIG_SWIZZLE(DE_SWIZZLE_ARGB) |
		  VIVS_DE_DEST_CONFIG_TILED_DISABLE |
		  VIVS_DE_DEST_CONFIG_MINOR_TILED_DISABLE);
	emit_clip(stream, dst);
	emit_common_state(stream);
	/* GC620 one-pass source/destination pixel-group descriptors. */
	set_state(stream, 0x00001324, 0x00030007u);
	/* The newer one-pass rasterizer uses these even without rotation. */
	set_state(stream, VIVS_DE_SRC_ROTATION_HEIGHT,
		  VIVS_DE_SRC_ROTATION_HEIGHT_HEIGHT(src->height));
	set_state(stream, VIVS_DE_DEST_ROTATION_HEIGHT,
		  VIVS_DE_DEST_ROTATION_HEIGHT_HEIGHT(dst->height));

	/*
	 * GC620's one-pass path has independent horizontal and vertical tap
	 * fields.  The upper field is not described by the old public rnndb;
	 * This GC620 advertises 2D_ONE_PASS_FILTER_TAP, so the Vivante HAL
	 * maps its default 7/9-tap kernels to the hardware's 5-tap mode.
	 * 0xfff2be5f is the corresponding horizontal/vertical 5-tap value,
	 * including the register write-mask bits.
	 */
	set_state(stream, VIVS_DE_VR_CONFIG_EX, 0xfff2be5fu);
	/*
	 * GC620 does not auto-select a useful block walk for one-pass YUV
	 * output.  These are the vendor HAL's tap5_norot_32to16 entry for an
	 * exact 1:1 stretch factor: 32x16 pixels, horizontal mask 0xffe0 and
	 * vertical mask 0xffff.  NV12 is classified as a 16-bit destination
	 * by gcoHARDWARE_ConvertFormat for this table lookup.
	 */
	set_state(stream, VIVS_DE_BW_CONFIG, 0xffff7667u);
	set_state(stream, VIVS_DE_BW_BLOCK_SIZE,
		  VIVS_DE_BW_BLOCK_SIZE_WIDTH(32) |
		  VIVS_DE_BW_BLOCK_SIZE_HEIGHT(16));
	set_state(stream, VIVS_DE_BW_BLOCK_MASK,
		  VIVS_DE_BW_BLOCK_MASK_HORIZONTAL(0xffe0) |
		  VIVS_DE_BW_BLOCK_MASK_VERTICAL(0xffff));
	set_state(stream, VIVS_DE_VR_SOURCE_IMAGE_LOW, 0);
	set_state(stream, VIVS_DE_VR_SOURCE_IMAGE_HIGH,
		  VIVS_DE_VR_SOURCE_IMAGE_HIGH_RIGHT(src->width) |
		  VIVS_DE_VR_SOURCE_IMAGE_HIGH_BOTTOM(src->height));
	set_state(stream, VIVS_DE_VR_SOURCE_ORIGIN_LOW, 0);
	set_state(stream, VIVS_DE_VR_SOURCE_ORIGIN_HIGH, 0);
	set_state(stream, VIVS_DE_VR_TARGET_WINDOW_LOW, 0);
	set_state(stream, VIVS_DE_VR_TARGET_WINDOW_HIGH,
		  VIVS_DE_VR_TARGET_WINDOW_HIGH_RIGHT(dst->width) |
		  VIVS_DE_VR_TARGET_WINDOW_HIGH_BOTTOM(dst->height));
	emit_state_block(stream, VIVS_DE_HORI_FILTER_KERNEL(0), kernel, 77);
	emit_state_block(stream, VIVS_DE_VERTI_FILTER_KERNEL(0), kernel, 77);
	/* FilterBlit uses ROP3 even though ordinary bit blits use ROP4. */
	set_state(stream, VIVS_DE_ROP,
		  VIVS_DE_ROP_ROP_FG(0xcc) |
		  VIVS_DE_ROP_ROP_BG(0xcc) |
		  VIVS_DE_ROP_TYPE_ROP3);
	set_state(stream, VIVS_DE_VR_CONFIG,
		  VIVS_DE_VR_CONFIG_START_ONE_PASS_BLIT);
	set_state(stream, 1, 0);
	set_state(stream, 1, 0);
	set_state(stream, 1, 0);
	set_state(stream, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_PE2D);
	ret = submit(stream, in_fence_fd, out_fence_fd);

out:
	if (stream)
		etna_cmd_stream_del(stream);
	if (uv_bo)
		etna_bo_del(uv_bo);
	if (y_bo)
		etna_bo_del(y_bo);
	if (src_bo)
		etna_bo_del(src_bo);
	pthread_mutex_unlock(&g2d->lock);
	return ret;
}
