/*
 * Copyright 2026 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 */

#include "th1520_g2d/th1520_g2d.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <etnaviv_drmif.h>

enum {
	/* Match Android's scrcpy encoder surface exactly. */
	WIDTH = 1280,
	HEIGHT = 720,
	STRIDE = WIDTH * 4,
	BO_SIZE = STRIDE * HEIGHT,
	NV12_STRIDE = WIDTH,
	NV12_Y_SIZE = NV12_STRIDE * HEIGHT,
	NV12_SIZE = NV12_Y_SIZE + NV12_STRIDE * HEIGHT / 2,
};

static int wait_fence(int fd)
{
	struct pollfd pfd = { .fd = fd, .events = POLLIN };
	int ret;

	do {
		ret = poll(&pfd, 1, 5000);
	} while (ret < 0 && errno == EINTR);
	if (ret == 0)
		return -ETIMEDOUT;
	if (ret < 0)
		return -errno;
	if (pfd.revents & POLLERR)
		return -EIO;
	return (pfd.revents & POLLIN) ? 0 : -EIO;
}

static void scalar_fill(struct etna_bo *bo, uint32_t value)
{
	volatile uint32_t *p = etna_bo_map(bo);

	for (size_t i = 0; i < BO_SIZE / sizeof(*p); i++)
		p[i] = value;
}

static int verify_color(struct etna_bo *bo, uint32_t expected)
{
	volatile uint32_t *p = etna_bo_map(bo);

	for (size_t i = 0; i < BO_SIZE / sizeof(*p); i++) {
		if (p[i] != expected) {
			fprintf(stderr, "mismatch at %zu: %08x != %08x\n",
				i, p[i], expected);
			return -1;
		}
	}
	return 0;
}

static void scalar_fill_bytes(struct etna_bo *bo, size_t size, uint8_t value)
{
	volatile uint8_t *p = etna_bo_map(bo);

	for (size_t i = 0; i < size; i++)
		p[i] = value;
}

struct test_color {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

static const struct test_color pattern_colors[2][2] = {
	{
		{ .r = 224, .g = 32, .b = 32 },
		{ .r = 32, .g = 224, .b = 32 },
	},
	{
		{ .r = 32, .g = 32, .b = 224 },
		{ .r = 180, .g = 180, .b = 180 },
	},
};

static uint32_t pack_abgr8888(struct test_color color)
{
	return UINT32_C(0xff000000) | ((uint32_t)color.b << 16) |
	       ((uint32_t)color.g << 8) | color.r;
}

static void scalar_fill_pattern(struct etna_bo *bo)
{
	volatile uint32_t *p = etna_bo_map(bo);

	for (unsigned int y = 0; y < HEIGHT; y++) {
		for (unsigned int x = 0; x < WIDTH; x++) {
			struct test_color color =
				pattern_colors[y >= HEIGHT / 2u][x >= WIDTH / 2u];

			p[(size_t)y * WIDTH + x] = pack_abgr8888(color);
		}
	}
}

static uint8_t clamp_byte(int value)
{
	if (value < 0)
		return 0;
	if (value > 255)
		return 255;
	return value;
}

static void bt709_limited(struct test_color color, uint8_t *y, uint8_t *u,
			  uint8_t *v)
{
	*y = clamp_byte(16 + (47 * color.r + 157 * color.g +
			      16 * color.b + 128) / 256);
	*u = clamp_byte(128 + (-26 * color.r - 87 * color.g +
			       112 * color.b + 128) / 256);
	*v = clamp_byte(128 + (112 * color.r - 102 * color.g -
			       10 * color.b + 128) / 256);
}

static int close_to(uint8_t actual, uint8_t expected)
{
	return actual >= expected - 2u && actual <= expected + 2u;
}

static int verify_nv12(struct etna_bo *bo, uint8_t expected_y,
		       uint8_t expected_u, uint8_t expected_v)
{
	volatile uint8_t *p = etna_bo_map(bo);
	size_t changed_y = 0, changed_uv = 0;
	unsigned int shown_y = 0, shown_uv = 0;

	for (unsigned int row = 0; row < HEIGHT; row++) {
		size_t changed = 0;

		for (unsigned int x = 0; x < NV12_STRIDE; x++)
			changed += p[(size_t)row * NV12_STRIDE + x] != 0xcd;
		changed_y += changed;
		if (changed && shown_y++ < 12)
			fprintf(stderr, "Y row %u changed=%zu first=%02x last=%02x\n",
				row, changed, p[(size_t)row * NV12_STRIDE],
				p[(size_t)(row + 1u) * NV12_STRIDE - 1u]);
	}
	for (unsigned int row = 0; row < HEIGHT / 2u; row++) {
		size_t changed = 0;
		size_t base = NV12_Y_SIZE + (size_t)row * NV12_STRIDE;

		for (unsigned int x = 0; x < NV12_STRIDE; x++)
			changed += p[base + x] != 0xcd;
		changed_uv += changed;
		if (changed && shown_uv++ < 12)
			fprintf(stderr,
				"UV row %u changed=%zu first=%02x,%02x last=%02x,%02x\n",
				row, changed, p[base], p[base + 1u],
				p[base + NV12_STRIDE - 2u],
				p[base + NV12_STRIDE - 1u]);
	}
	fprintf(stderr, "NV12 changed bytes: Y=%zu/%u UV=%zu/%u\n",
		changed_y, NV12_Y_SIZE, changed_uv, NV12_SIZE - NV12_Y_SIZE);

	for (size_t i = 0; i < NV12_Y_SIZE; i++) {
		if (!close_to(p[i], expected_y)) {
			fprintf(stderr, "Y mismatch at %zu: %u != %u\n", i,
				p[i], expected_y);
			fprintf(stderr, "first bytes:");
			for (size_t j = 0; j < 16; j++)
				fprintf(stderr, " %02x", p[j]);
			fprintf(stderr, " | UV:");
			for (size_t j = 0; j < 16; j++)
				fprintf(stderr, " %02x", p[NV12_Y_SIZE + j]);
			fprintf(stderr, "\n");
			return -1;
		}
	}
	for (size_t i = NV12_Y_SIZE; i < NV12_SIZE; i += 2) {
		if (!close_to(p[i], expected_u) ||
		    !close_to(p[i + 1], expected_v)) {
			fprintf(stderr,
				"UV mismatch at %zu: %u,%u != %u,%u\n", i,
				p[i], p[i + 1], expected_u, expected_v);
			return -1;
		}
	}
	return 0;
}

static int verify_pattern_nv12(struct etna_bo *bo)
{
	volatile uint8_t *p = etna_bo_map(bo);
	static const unsigned int sample_x[2] = { WIDTH / 4u, WIDTH * 3u / 4u };
	static const unsigned int sample_y[2] = { HEIGHT / 4u, HEIGHT * 3u / 4u };
	int failures = 0;

	for (unsigned int row = 0; row < 2; row++) {
		for (unsigned int col = 0; col < 2; col++) {
			const struct test_color color = pattern_colors[row][col];
			const unsigned int x = sample_x[col];
			const unsigned int y_pos = sample_y[row];
			const size_t y_offset = (size_t)y_pos * NV12_STRIDE + x;
			const size_t uv_offset = NV12_Y_SIZE +
				(size_t)(y_pos / 2u) * NV12_STRIDE + (x & ~1u);
			uint8_t expected_y, expected_u, expected_v;

			bt709_limited(color, &expected_y, &expected_u, &expected_v);
			fprintf(stderr,
				"pattern[%u,%u] xy=%u,%u YUV=%u,%u,%u expected=%u,%u,%u\n",
				row, col, x, y_pos, p[y_offset], p[uv_offset],
				p[uv_offset + 1u], expected_y, expected_u, expected_v);
			if (!close_to(p[y_offset], expected_y) ||
			    !close_to(p[uv_offset], expected_u) ||
			    !close_to(p[uv_offset + 1u], expected_v))
				failures++;
		}
	}

	/* Print a sparse map to make row/column aliasing immediately visible. */
	for (unsigned int y = 8; y < HEIGHT; y += 16) {
		fprintf(stderr, "Y%03u:", y);
		for (unsigned int x = 8; x < WIDTH; x += 16)
			fprintf(stderr, " %02x", p[(size_t)y * NV12_STRIDE + x]);
		fprintf(stderr, "\n");
	}
	for (unsigned int y = 8; y < HEIGHT / 2u; y += 8) {
		fprintf(stderr, "UV%02u:", y);
		for (unsigned int x = 8; x < WIDTH; x += 16) {
			const size_t offset = NV12_Y_SIZE +
				(size_t)y * NV12_STRIDE + (x & ~1u);

			fprintf(stderr, " %02x%02x", p[offset], p[offset + 1u]);
		}
		fprintf(stderr, "\n");
	}

	return failures ? -1 : 0;
}

int main(int argc, char **argv)
{
	const char *node = argc > 1 ? argv[1] : "/dev/dri/renderD129";
	/* Android RGBA byte order for logical R=40, G=100, B=200, A=255. */
	const uint32_t source_color = 0xffc86428;
	const uint32_t destination_color = 0xff2864c8;
	const uint32_t clear_color = 0xff40ff40;
	struct th1520_g2d_rect full = { 0, 0, WIDTH, HEIGHT };
	struct th1520_g2d *g2d = NULL;
	struct etna_device *dev = NULL;
	struct etna_bo *src = NULL;
	struct etna_bo *dst = NULL;
	struct etna_bo *nv12 = NULL;
	struct th1520_g2d_image src_image = { 0 };
	struct th1520_g2d_image dst_image = { 0 };
	struct th1520_g2d_image nv12_image = { 0 };
	int render_fd = -1, src_fd = -1, dst_fd = -1, nv12_fd = -1;
	int fence_fd = -1;
	int ret = 1;

	render_fd = open(node, O_RDWR | O_CLOEXEC);
	if (render_fd < 0) {
		perror(node);
		goto out;
	}
	dev = etna_device_new(render_fd);
	if (!dev)
		goto out;
	for (unsigned int core = 0; core < 16; core++) {
		struct etna_gpu *gpu = etna_gpu_new(dev, core);
		uint64_t value;

		if (!gpu)
			break;
		if (!etna_gpu_get_param(gpu, ETNA_GPU_MODEL, &value))
			printf("core%u model=%" PRIx64, core, value);
		for (unsigned int word = 0; word <= 6; word++) {
			if (!etna_gpu_get_param(gpu, ETNA_GPU_FEATURES_0 + word,
						&value))
				printf(" f%u=%08" PRIx64, word, value);
		}
		printf("\n");
		etna_gpu_del(gpu);
	}
	src = etna_bo_new(dev, BO_SIZE, DRM_ETNA_GEM_CACHE_UNCACHED);
	dst = etna_bo_new(dev, BO_SIZE, DRM_ETNA_GEM_CACHE_UNCACHED);
	nv12 = etna_bo_new(dev, NV12_SIZE, DRM_ETNA_GEM_CACHE_UNCACHED);
	if (!src || !dst || !nv12)
		goto out;
	src_fd = etna_bo_dmabuf(src);
	dst_fd = etna_bo_dmabuf(dst);
	nv12_fd = etna_bo_dmabuf(nv12);
	if (src_fd < 0 || dst_fd < 0 || nv12_fd < 0)
		goto out;

	src_image = (struct th1520_g2d_image) {
		.width = WIDTH,
		.height = HEIGHT,
		.format = TH1520_G2D_FORMAT_ABGR8888,
		.num_planes = 1,
		.planes[0] = { src_fd, 0, STRIDE },
	};
	dst_image = (struct th1520_g2d_image) {
		.width = WIDTH,
		.height = HEIGHT,
		.format = TH1520_G2D_FORMAT_ARGB8888,
		.num_planes = 1,
		.planes[0] = { dst_fd, 0, STRIDE },
	};
	nv12_image = (struct th1520_g2d_image) {
		.width = WIDTH,
		.height = HEIGHT,
		.format = TH1520_G2D_FORMAT_NV12,
		.num_planes = 2,
		.planes[0] = { nv12_fd, 0, NV12_STRIDE },
		.planes[1] = { nv12_fd, NV12_Y_SIZE, NV12_STRIDE },
	};

	if (etna_bo_cpu_prep(src, DRM_ETNA_PREP_WRITE | DRM_ETNA_PREP_NOSYNC) ||
	    etna_bo_cpu_prep(dst, DRM_ETNA_PREP_WRITE | DRM_ETNA_PREP_NOSYNC)) {
		fprintf(stderr, "CPU write preparation failed\n");
		goto out;
	}
	scalar_fill(src, source_color);
	scalar_fill(dst, 0);
	etna_bo_cpu_fini(src);
	etna_bo_cpu_fini(dst);

	g2d = th1520_g2d_open(node);
	if (!g2d) {
		perror("th1520_g2d_open");
		goto out;
	}

	ret = th1520_g2d_clear(g2d, &dst_image, &full, clear_color, -1,
			       &fence_fd);
	if (ret || (ret = wait_fence(fence_fd))) {
		fprintf(stderr, "clear failed: %d\n", ret);
		goto out;
	}
	close(fence_fd);
	fence_fd = -1;
	if (etna_bo_cpu_prep(dst, DRM_ETNA_PREP_READ) ||
	    verify_color(dst, clear_color)) {
		fprintf(stderr, "clear verification failed\n");
		goto out;
	}
	etna_bo_cpu_fini(dst);
	printf("DMA-BUF clear + output fence: PASS\n");

	ret = th1520_g2d_blit(g2d, &src_image, &full, &dst_image, &full, -1,
			      &fence_fd);
	if (ret || (ret = wait_fence(fence_fd))) {
		fprintf(stderr, "blit failed: %d\n", ret);
		goto out;
	}
	close(fence_fd);
	fence_fd = -1;
	if (etna_bo_cpu_prep(dst, DRM_ETNA_PREP_READ) ||
	    verify_color(dst, destination_color)) {
		fprintf(stderr, "blit verification failed\n");
		goto out;
	}
	etna_bo_cpu_fini(dst);
	printf("DMA-BUF RGB blit + output fence: PASS\n");

	if (etna_bo_cpu_prep(nv12,
			     DRM_ETNA_PREP_WRITE | DRM_ETNA_PREP_NOSYNC)) {
		fprintf(stderr, "NV12 CPU write preparation failed\n");
		goto out;
	}
	scalar_fill_bytes(nv12, NV12_SIZE, 0xcd);
	etna_bo_cpu_fini(nv12);
	ret = th1520_g2d_rgb_to_nv12(g2d, &src_image, &full, &nv12_image,
				     &full, -1, &fence_fd);
	if (ret || (ret = wait_fence(fence_fd))) {
		fprintf(stderr, "RGB to NV12 failed: %d\n", ret);
		goto out;
	}
	close(fence_fd);
	fence_fd = -1;
	if (etna_bo_cpu_prep(nv12, DRM_ETNA_PREP_READ)) {
		fprintf(stderr, "NV12 CPU read preparation failed\n");
		ret = -EIO;
		goto out;
	}
	/* GC620's default RGB output matrix is BT.709 limited range. */
	if (verify_nv12(nv12, 97, 178, 98)) {
		fprintf(stderr, "RGB to NV12 verification failed\n");
		etna_bo_cpu_fini(nv12);
		ret = -EIO;
		goto out;
	}
	etna_bo_cpu_fini(nv12);
	printf("DMA-BUF RGB to NV12 one-pass + output fence: PASS\n");

	if (etna_bo_cpu_prep(src,
			     DRM_ETNA_PREP_WRITE | DRM_ETNA_PREP_NOSYNC) ||
	    etna_bo_cpu_prep(nv12,
			     DRM_ETNA_PREP_WRITE | DRM_ETNA_PREP_NOSYNC)) {
		fprintf(stderr, "pattern CPU write preparation failed\n");
		goto out;
	}
	scalar_fill_pattern(src);
	scalar_fill_bytes(nv12, NV12_SIZE, 0xcd);
	etna_bo_cpu_fini(src);
	etna_bo_cpu_fini(nv12);
	ret = th1520_g2d_rgb_to_nv12(g2d, &src_image, &full, &nv12_image,
				     &full, -1, &fence_fd);
	if (ret || (ret = wait_fence(fence_fd))) {
		fprintf(stderr, "pattern RGB to NV12 failed: %d\n", ret);
		goto out;
	}
	close(fence_fd);
	fence_fd = -1;
	if (etna_bo_cpu_prep(nv12, DRM_ETNA_PREP_READ)) {
		fprintf(stderr, "pattern NV12 CPU read preparation failed\n");
		ret = -EIO;
		goto out;
	}
	if (verify_pattern_nv12(nv12)) {
		fprintf(stderr, "pattern RGB to NV12 verification failed\n");
		etna_bo_cpu_fini(nv12);
		ret = -EIO;
		goto out;
	}
	etna_bo_cpu_fini(nv12);
	printf("DMA-BUF patterned RGB to NV12 one-pass: PASS\n");
	ret = 0;

out:
	if (fence_fd >= 0)
		close(fence_fd);
	if (g2d)
		th1520_g2d_close(g2d);
	if (src_fd >= 0)
		close(src_fd);
	if (dst_fd >= 0)
		close(dst_fd);
	if (nv12_fd >= 0)
		close(nv12_fd);
	if (src)
		etna_bo_del(src);
	if (dst)
		etna_bo_del(dst);
	if (nv12)
		etna_bo_del(nv12);
	if (dev)
		etna_device_del(dev);
	if (render_fd >= 0)
		close(render_fd);
	return ret ? 1 : 0;
}
