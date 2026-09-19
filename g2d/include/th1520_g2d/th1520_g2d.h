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

#ifndef TH1520_G2D_TH1520_G2D_H_
#define TH1520_G2D_TH1520_G2D_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TH1520_G2D_MAX_PLANES 3

/* DRM fourcc values, kept local so callers do not need libdrm headers. */
#define TH1520_G2D_FORMAT_XRGB8888 0x34325258u /* XR24 */
#define TH1520_G2D_FORMAT_ARGB8888 0x34325241u /* AR24 */
#define TH1520_G2D_FORMAT_XBGR8888 0x34324258u /* XB24 */
#define TH1520_G2D_FORMAT_ABGR8888 0x34324241u /* AB24 */
#define TH1520_G2D_FORMAT_NV12     0x3231564eu /* NV12 */

struct th1520_g2d_plane {
	int dmabuf_fd;
	uint32_t offset;
	uint32_t stride;
};

struct th1520_g2d_image {
	uint32_t width;
	uint32_t height;
	uint32_t format;
	uint32_t num_planes;
	uint64_t modifier;
	struct th1520_g2d_plane planes[TH1520_G2D_MAX_PLANES];
};

struct th1520_g2d_rect {
	uint32_t x;
	uint32_t y;
	uint32_t width;
	uint32_t height;
};

struct th1520_g2d;

/* Opens the first 2D-capable etnaviv core exposed by render_node. */
struct th1520_g2d *th1520_g2d_open(const char *render_node);
void th1520_g2d_close(struct th1520_g2d *g2d);

/*
 * All operations return 0 or a negative errno value. The input fence is
 * borrowed. When out_fence_fd is non-NULL, the returned sync_file is owned by
 * the caller and must be closed. Passing -1 as in_fence_fd uses implicit DMA-
 * BUF synchronization.
 */
int th1520_g2d_clear(struct th1520_g2d *g2d,
		     const struct th1520_g2d_image *dst,
		     const struct th1520_g2d_rect *rect,
		     uint32_t argb8888,
		     int in_fence_fd,
		     int *out_fence_fd);

int th1520_g2d_blit(struct th1520_g2d *g2d,
		    const struct th1520_g2d_image *src,
		    const struct th1520_g2d_rect *src_rect,
		    const struct th1520_g2d_image *dst,
		    const struct th1520_g2d_rect *dst_rect,
		    int in_fence_fd,
		    int *out_fence_fd);

/* Premultiplied source-over, 1:1 linear RGB8888 only, with full plane alpha.
 * XRGB/XBGR sources are opaque. No scaling, rotation or color transform.
 */
int th1520_g2d_blend(struct th1520_g2d *g2d,
		     const struct th1520_g2d_image *src,
		     const struct th1520_g2d_rect *src_rect,
		     const struct th1520_g2d_image *dst,
		     const struct th1520_g2d_rect *dst_rect,
		     int in_fence_fd,
		     int *out_fence_fd);

/*
 * Converts an RGB8888 DMA-BUF into a two-plane, linear NV12 DMA-BUF.  The
 * first implementation intentionally accepts only full-frame, 1:1 blits;
 * this is the zero-copy path needed by Android's encoder input surface.
 * Both NV12 plane strides must be multiples of 64 bytes. Padding is allowed;
 * the visible rectangle does not need to have a 64-pixel-aligned width.
 */
int th1520_g2d_rgb_to_nv12(struct th1520_g2d *g2d,
			   const struct th1520_g2d_image *src,
			   const struct th1520_g2d_rect *src_rect,
			   const struct th1520_g2d_image *dst,
			   const struct th1520_g2d_rect *dst_rect,
			   int in_fence_fd,
			   int *out_fence_fd);

#ifdef __cplusplus
}
#endif

#endif  /* TH1520_G2D_TH1520_G2D_H_ */
