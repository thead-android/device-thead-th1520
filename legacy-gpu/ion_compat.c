/*
 * Minimal ION ABI bridge for the proprietary TH1520 PowerVR gralloc.
 *
 * That binary only imports ion_open(), ion_alloc_fd(), and ion_close().
 * Linux 7.1 no longer provides /dev/ion, so translate those calls to the
 * modern DMA-BUF heap allocation ioctl. Require CMA because TH1520's DPU
 * does not sit behind an IOMMU; scattered system-heap buffers cannot scan out.
 */

#include <errno.h>
#include <fcntl.h>
#include <linux/dma-heap.h>
#include <stddef.h>
#include <sys/ioctl.h>
#include <unistd.h>

static const char* const kHeapPaths[] = {
    "/dev/dma_heap/linux,cma",
    "/dev/dma_heap/reserved",
};

int ion_open(void) {
    size_t i;

    for (i = 0; i < sizeof(kHeapPaths) / sizeof(kHeapPaths[0]); ++i) {
        int fd = open(kHeapPaths[i], O_RDONLY | O_CLOEXEC);
        if (fd >= 0) {
            return fd;
        }
    }

    return -errno;
}

int ion_close(int fd) {
    return close(fd);
}

int ion_alloc_fd(int heap_fd, size_t len, size_t align,
                 unsigned int heap_mask, unsigned int flags, int* buffer_fd) {
    struct dma_heap_allocation_data data = {
        .len = len,
        .fd_flags = O_RDWR | O_CLOEXEC,
        .heap_flags = 0,
    };

    (void)align;
    (void)heap_mask;
    (void)flags;

    if (buffer_fd == NULL || len == 0) {
        return -EINVAL;
    }

    if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &data) < 0) {
        return -errno;
    }

    *buffer_fd = (int)data.fd;
    return 0;
}
