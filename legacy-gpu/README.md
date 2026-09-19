# TH1520 DDK 1.17 integration

The AIDL allocator forwards mapper4 descriptors to the original HIDL allocator4
and publishes its shared metadata DMA-BUF before exporting the handle. Init
supervises the two services separately; no test-only fork/execute permission.

`prebuilts/` contains immutable copies of the two unchanged HAL binaries from the manifest's
`vendor/thead/proprietary/prebuilts/chipset` project, revision
`37731dbe2de5b36deb1e0a2c587856c4e36dc192`. Soong disallows symlinks as prebuilt
ELF inputs. Their vendor licensing remains unchanged; update these only together
with the matching DDK userspace. The GPU libraries/firmware still come from the
pinned vendor project, not this directory.

The ION ABI bridge is the archived implementation already validated against the
mainline DMA-BUF heaps. `libion.so` is installed alongside the original gralloc.
The matching Android-sync pvrsrvkm module must be built for the selected kernel;
the older Linux-sync variant is not interchangeable.

This candidate also needs the matching Vulkan WSI and RGBA cursor changes in
frameworks/native, the Codec2 standard-layout/FD/pitch fixes, GC620 HWC selection,
and the HDMI command line without forced-enable `e`. Never combine only a subset.
