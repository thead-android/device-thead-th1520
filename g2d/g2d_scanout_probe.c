// SPDX-License-Identifier: Apache-2.0
// Read the actual portrait CRTC framebuffer, not SF's GPU-recomposed screenshot.
#include <fcntl.h>
#include <linux/dma-buf.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    int drm = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (drm < 0) return 3;
    drmModeRes *resources = drmModeGetResources(drm);
    if (!resources) return 4;
    uint32_t fb_id = 0;
    for (int i = 0; i < resources->count_crtcs; ++i) {
        drmModeCrtc *crtc = drmModeGetCrtc(drm, resources->crtcs[i]);
        if (crtc && crtc->mode_valid && crtc->mode.hdisplay == 1080 &&
            crtc->mode.vdisplay == 2160) fb_id = crtc->buffer_id;
        if (crtc) drmModeFreeCrtc(crtc);
    }
    if (!fb_id) return 5;
    drmModeFB2 *fb = drmModeGetFB2(drm, fb_id);
    if (!fb || fb->modifier || !fb->handles[0] || fb->width > 4096 ||
        fb->height > 4096 || fb->pitches[0] < fb->width * 4) return 6;
    int bgra = fb->pixel_format == DRM_FORMAT_XRGB8888 ||
               fb->pixel_format == DRM_FORMAT_ARGB8888;
    if (!bgra && fb->pixel_format != DRM_FORMAT_XBGR8888 &&
        fb->pixel_format != DRM_FORMAT_ABGR8888) return 7;
    int fd = -1;
    if (drmPrimeHandleToFD(drm, fb->handles[0], DRM_CLOEXEC, &fd)) return 8;
    size_t size = fb->offsets[0] + (size_t)fb->pitches[0] * fb->height;
    struct dma_buf_sync sync = { .flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ };
    if (ioctl(fd, DMA_BUF_IOCTL_SYNC, &sync)) return 9;
    uint8_t *mapping = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) return 10;
    FILE *output = fopen(argv[1], "wb");
    if (!output) return 11;
    uint8_t *row = malloc((size_t)fb->width * 4);
    if (!row) return 12;
    for (uint32_t y = 0; y < fb->height; ++y) {
        const uint8_t *src = mapping + fb->offsets[0] + (size_t)y * fb->pitches[0];
        for (uint32_t x = 0; x < fb->width; ++x) {
            row[x * 4] = src[x * 4 + (bgra ? 2 : 0)];
            row[x * 4 + 1] = src[x * 4 + 1];
            row[x * 4 + 2] = src[x * 4 + (bgra ? 0 : 2)];
            row[x * 4 + 3] = 255;
        }
        if (fwrite(row, 4, fb->width, output) != fb->width) return 13;
    }
    if (fclose(output)) return 14;
    sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ;
    if (ioctl(fd, DMA_BUF_IOCTL_SYNC, &sync)) return 15;
    printf("SCANOUT fb=%u size=%ux%u format=%08x pitch=%u\n",
           fb_id, fb->width, fb->height, fb->pixel_format, fb->pitches[0]);
    free(row);
    munmap(mapping, size);
    close(fd);
    drmModeFreeFB2(fb);
    drmModeFreeResources(resources);
    close(drm);
    return 0;
}
