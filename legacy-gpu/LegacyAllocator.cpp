// SPDX-License-Identifier: Apache-2.0
#define LOG_TAG "TH1520LegacyAllocator"
#include <aidl/android/hardware/graphics/allocator/BnAllocator.h>
#include <aidl/android/hardware/graphics/allocator/AllocationError.h>
#include <android/hardware/graphics/allocator/4.0/IAllocator.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <hidl/HidlTransportSupport.h>
#include <log/log.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/dma-buf.h>
#include <sys/ioctl.h>
#include <cerrno>
#include <limits>

namespace aa = aidl::android::hardware::graphics::allocator;
using Backend = android::hardware::graphics::allocator::V4_0::IAllocator;
using HalError = android::hardware::graphics::mapper::V4_0::Error;

static ndk::ScopedAStatus error(aa::AllocationError value) {
    return ndk::ScopedAStatus::fromServiceSpecificError(static_cast<int32_t>(value));
}

class LegacyAllocator final : public aa::BnAllocator {
public:
    bool init() {
        backend_ = Backend::getService();
        return backend_ != nullptr;
    }

    ndk::ScopedAStatus allocate(const std::vector<uint8_t>& encoded, int32_t count,
                               aa::AllocationResult* result) override {
        if (count < 0) return error(aa::AllocationError::BAD_DESCRIPTOR);
        if (count > 64) return error(aa::AllocationError::NO_RESOURCES);
        android::hardware::hidl_vec<uint8_t> descriptor;
        descriptor.setToExternal(const_cast<uint8_t*>(encoded.data()), encoded.size());
        auto status = error(aa::AllocationError::NO_RESOURCES);
        auto transaction = backend_->allocate(descriptor, count,
            [&](HalError rc, uint32_t stride, const auto& handles) {
                if (rc != HalError::NONE) {
                    status = error(rc == HalError::BAD_DESCRIPTOR ? aa::AllocationError::BAD_DESCRIPTOR
                                   : rc == HalError::UNSUPPORTED ? aa::AllocationError::UNSUPPORTED
                                   : aa::AllocationError::NO_RESOURCES);
                    return;
                }
                if (handles.size() != static_cast<size_t>(count) ||
                    stride > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) return;
                aa::AllocationResult output;
                output.stride = stride;
                for (const auto& handleWrapper : handles) {
                    const native_handle_t* handle = handleWrapper.getNativeHandle();
                    if (!handle || handle->numFds < 0 || handle->numInts < 0) return;
                    // The vendor handle exports pixel storage first and a
                    // separate shared metadata DMA-BUF second. Publish the
                    // allocator's CPU-written metadata before transferring it.
                    if (handle->numFds >= 2) {
                        for (uint64_t flags : {uint64_t(DMA_BUF_SYNC_START | DMA_BUF_SYNC_WRITE),
                                               uint64_t(DMA_BUF_SYNC_END | DMA_BUF_SYNC_WRITE)}) {
                            dma_buf_sync sync{flags};
                            int syncResult;
                            do { syncResult = ioctl(handle->data[1], DMA_BUF_IOCTL_SYNC, &sync); }
                            while (syncResult < 0 && errno == EINTR);
                            if (syncResult) {
                                ALOGE("metadata publication sync failed: %d", errno);
                                return;
                            }
                        }
                    }
                    aidl::android::hardware::common::NativeHandle parcel;
                    for (int index = 0; index < handle->numFds; ++index) {
                        int fd = fcntl(handle->data[index], F_DUPFD_CLOEXEC, 0);
                        if (fd < 0) return;
                        parcel.fds.emplace_back(fd);
                    }
                    parcel.ints.assign(handle->data + handle->numFds,
                                       handle->data + handle->numFds + handle->numInts);
                    output.buffers.emplace_back(std::move(parcel));
                }
                *result = std::move(output);
                status = ndk::ScopedAStatus::ok();
            });
        if (!transaction.isOk()) return error(aa::AllocationError::NO_RESOURCES);
        return status;
    }
private:
    android::sp<Backend> backend_;
};

int main() {
    // Init supervises the HIDL backend separately. Never spawn a child service
    // or grant execute_no_trans merely to support a graphics-stack hot switch.
    android::hardware::configureRpcThreadpool(1, false);
    ABinderProcess_setThreadPoolMaxThreadCount(0);
    auto service = ndk::SharedRefBase::make<LegacyAllocator>();
    if (!service->init()) return 1;
    const std::string name = std::string(aa::IAllocator::descriptor) + "/default";
    if (AServiceManager_addService(service->asBinder().get(), name.c_str()) != STATUS_OK) return 1;
    ALOGI("AIDL allocator v1 registered: original mapper4 descriptors forwarded unchanged to allocator4");
    ABinderProcess_joinThreadPool();
    return 1;
}
