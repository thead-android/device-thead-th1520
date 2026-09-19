// SPDX-License-Identifier: Apache-2.0
// Diagnostic only: add enough real SurfaceFlinger layers to exceed DPU planes.
#include <binder/ProcessState.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <ui/DisplayState.h>
#include <utils/String8.h>

#include <cstdio>
#include <unistd.h>
#include <vector>

using namespace android;
static int Fill(const sp<Surface> &surface, uint32_t color) {
    ANativeWindow_Buffer buffer{};
    if (surface->lock(&buffer, nullptr) != NO_ERROR) return 4;
    auto *pixels = static_cast<uint32_t *>(buffer.bits);
    for (int y = 0; y < buffer.height; ++y)
        for (int x = 0; x < buffer.width; ++x)
            pixels[y * buffer.stride + x] = color;
    return surface->unlockAndPost() == NO_ERROR ? 0 : 5;
}
int main() {
    ProcessState::self()->startThreadPool();
    auto client = sp<SurfaceComposerClient>::make();
    if (client->initCheck() != NO_ERROR) return 1;
    ui::DisplayState display{};
    bool found = false;
    for (auto id : SurfaceComposerClient::getPhysicalDisplayIds()) {
        auto token = SurfaceComposerClient::getPhysicalDisplayToken(id);
        if (SurfaceComposerClient::getDisplayState(token, &display) == NO_ERROR &&
            display.layerStackSpaceRect.height > display.layerStackSpaceRect.width) {
            found = true;
            break;
        }
    }
    if (!found) return 2;
    std::vector<sp<SurfaceControl>> controls;
    std::vector<sp<Surface>> surfaces;
    for (int i = 0; i < 8; ++i) {
        auto control = client->createSurface(String8::format("GC620-repro-%d", i),
                                             400, 160, PIXEL_FORMAT_RGBA_8888);
        if (!control || !control->isValid()) return 3;
        auto surface = control->getSurface();
        // RGBA bytes, premultiplied alpha=128. Varying channels reveal swizzles.
        const uint32_t color = 0x80000000U | uint32_t(20 + i * 9) << 16 |
                               uint32_t(80 - i * 6) << 8 | uint32_t(30 + i * 11);
        if (Fill(surface, color)) return 5;
        SurfaceComposerClient::Transaction transaction;
        transaction.setLayerStack(control, display.layerStack)
            .setLayer(control, 100000 + i)
            .setPosition(control, 80 + i * 18, 240 + i * 110)
            .show(control);
        if (transaction.apply() != NO_ERROR) return 6;
        controls.push_back(control);
        surfaces.push_back(surface);
    }
    puts("SCENE_READY 8 premultiplied RGBA surfaces");
    fflush(stdout);
    for (int frame = 0; frame < 180; ++frame) {
        SurfaceComposerClient::Transaction transaction;
        for (int i = 0; i < 8; ++i)
            transaction.setPosition(controls[i], 80 + i * 18 + (frame < 60 ? frame % 32 : 0),
                                     240 + i * 110);
        if (transaction.apply() != NO_ERROR) return 7;
        // Keep the scene pixel-stable for paired KMS/SF readback, but post a
        // fresh identical buffer often enough to avoid static-scene flattening.
        if (frame >= 60 && frame % 5 == 0 && Fill(surfaces[0], 0x8014501eU))
            return 8;
        usleep(50000);
    }
    puts("SCENE_COMPLETE");
    return 0;
}
