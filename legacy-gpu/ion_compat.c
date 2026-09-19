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
    /* Import-only clients also initialize gralloc, but must not open a
     * physical CMA allocator. Keep a DMA-heap context for the legacy driver's
     * heap capability probes, using the unprivileged system heap here. CMA
     * access is deferred to actual allocation in the allocator service.
     */
    int fd = open("/dev/dma_heap/system", O_RDONLY | O_CLOEXEC);
    return fd < 0 ? -errno : fd;
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
    if (fcntl(heap_fd, F_GETFD) < 0) return -errno;
    int allocation_heap = -1;
    for (size_t i = 0; i < sizeof(kHeapPaths) / sizeof(kHeapPaths[0]); ++i) {
        allocation_heap = open(kHeapPaths[i], O_RDONLY | O_CLOEXEC);
        if (allocation_heap >= 0) break;
    }
    if (allocation_heap < 0) return -errno;
    int result = ioctl(allocation_heap, DMA_HEAP_IOCTL_ALLOC, &data);
    int saved_errno = errno;
    close(allocation_heap);
    if (result < 0) return -saved_errno;

    *buffer_fd = (int)data.fd;
    return 0;
}
