#!/usr/bin/env -S PYTHONPATH=../../../tools/extract-utils python3
#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

from extract_utils.fixups_blob import (
    blob_fixup,
    blob_fixups_user_type,
)
from extract_utils.fixups_lib import (
    lib_fixups,
    lib_fixups_user_type,
)
from extract_utils.main import (
    ExtractUtils,
    ExtractUtilsModule,
)

namespace_imports = [
    'hardware/mediatek',
    'hardware/xiaomi',
    'vendor/xiaomi/mt6893-common',
]

lib_fixups: lib_fixups_user_type = {
    **lib_fixups,
}

blob_fixups: blob_fixups_user_type = {
    'vendor/lib/hw/audio.primary.mt6893.so': blob_fixup()
        .replace_needed('libalsautils.so', 'libalsautils-v30.so')
        .replace_needed('libtinyxml2.so', 'libtinyxml2-v34.so')
        .add_needed('libstagefright_foundation-v33.so'),
    ('vendor/lib64/hw/android.hardware.camera.provider@2.6-impl-mediatek.so', 'vendor/lib64/libmtkcam_stdutils.so'): blob_fixup()
        .replace_needed('libutils.so', 'libutils-v32.so'),
    # Make BaseImageBufferHeap::helpMapPhyAddr() always take its ION branch.
    #
    # The camera HAL is an Android 13 prebuilt that still uses ION and picks
    # the physical-address path like this:
    #
    #     helpMapPhyAddr() { return CheckIonSupport() ? Ion() : Imem(); }
    #
    # Its Imem() branch was compiled without MTK's IMEM driver and contains
    # nothing but __android_log_assert("no define IMEM DRV"), so reaching it
    # aborts camerahalserver during the first 3A allocation and no capture
    # session can ever be configured.
    #
    # AOSP hardcodes BufferAllocator::CheckIonSupport() to false now that
    # ION is gone upstream, so the condition always fails and the aborting
    # branch is always taken. This kernel still provides /dev/ion and its
    # ION ioctls work, so the Ion() branch is the one that can succeed.
    #
    # At file offset 0x2551c the branch is "tbz w0, #0, 0x25528"
    # (60 00 00 36); padding it to a nop (1f 20 03 d5) makes the next
    # instruction -- "ldr x8, [x8, #456]", the Ion vtable slot -- execute
    # unconditionally. One instruction, no AOSP source touched.
    # BufferAllocator::CheckIonSupport() answers false unconditionally in
    # this AOSP release, and everything that needs an IOVA asks it first.
    # The camera HAL's CcuMemMgr is one of those: with false it allocates
    # through dma-heap, which never returns an IOVA, so CcuBuffer's mva
    # stays 0, logBufMva[0] comes out 0, CcuDrvImp::init() reports
    # "buffer address violation" and CcuDrvImp::boot() memsets a NULL
    # pointer one megabyte wide.
    #
    # lib3a.ccuif.so resolves this symbol from libdmabufheap.so (it is
    # earlier in that library's DT_NEEDED than anything else providing it),
    # so patching the answer here is the only place that takes effect --
    # repointing the HAL's own DT_NEEDED or interposing from another
    # library gets overridden. Only libmtkcam_imgbuf.so and
    # lib3a.ccuif.so call it; gralloc, codec2 and libui do not, so this
    # does not disturb buffer allocation elsewhere.
    #
    # mov w0, wzr -> mov w0, #1, both being 8 bytes including the ret.
    # APUSYS (the APU/MDLA runtime behind AI 3A) resolves ion_import from the
    # same broken libion.so. Every inference buffer import failed with
    #
    #   apusys: import: fail to get ion buffer by import (devFd=18, shared_fd=227)
    #   neuron : MDLA failed to import ion fd (372) in HintFrontendBuffer!
    #
    # even though devFd 18 is a perfectly good /dev/ion. Point it at the shim
    # too -- it needs exactly the same symbol set as libcamdrv_imem.so, which
    # this library already covers.
    ('vendor/lib64/libapusys.so',): blob_fixup()
        .binary_regex_replace(
            b'libion.so\x00',
            b'libio2.so\x00',
            occurances=1,
        ),
    ('system/lib64/libdmabufheap.so', 'vendor/lib64/libdmabufheap.so'): blob_fixup()
        .binary_regex_replace(
            b'\xe0\x03\x1f\x2a\xc0\x03\x5f\xd6',
            b'\x20\x00\x80\x52\xc0\x03\x5f\xd6',
            occurances=1,
        ),
    'vendor/lib64/libmtkcam_imgbuf.so': blob_fixup()
        .binary_regex_replace(
            b'\x60\x00\x00\x36', b'\x1f\x20\x03\xd5',
            occurances=1,
        )
        # helpUnmapPhyAddr() branches on the same BufferAllocator::
        # CheckIonSupport() answer as helpMapPhyAddr(), and its non-ION side
        # is a single __android_log_assert("no define IMEM DRV") because this
        # kernel has no IMEM driver. Patching only the map half therefore
        # moves the crash from the mapping to the unmapping:
        #
        #   helpUnmapPhyAddr()+736 -> __android_log_assert -> abort
        #
        # NOP the branch so both halves stay on the ION path, which this
        # kernel does implement; 86 successful helpMapPhyAddrIon calls
        # fetching physical addresses out of /dev/ion were observed on the
        # device before this was added.
        .binary_regex_replace(
            b'\x80\x0d\x00\x36', b'\x1f\x20\x03\xd5',
            occurances=1,
        ),
    ('vendor/lib64/libSQLiteModule_VER_ALL.so', 'vendor/lib64/lib3a.flash.so', 'vendor/lib64/lib3a.ae.stat.so', 'vendor/lib64/lib3a.sensors.color.so', 'vendor/lib64/lib3a.sensors.flicker.so', 'vendor/lib64/libaaa_ltm.so'): blob_fixup()
        .add_needed('liblog.so'),
    'vendor/bin/hw/camerahalserver': blob_fixup()
        .binary_regex_replace(b'/system/lib64', b'/vendor/lib64'),
}  # fmt: skip

module = ExtractUtilsModule(
    'chopin',
    'xiaomi',
    blob_fixups=blob_fixups,
    lib_fixups=lib_fixups,
    namespace_imports=namespace_imports,
    add_firmware_proprietary_file=True,
)

if __name__ == '__main__':
    utils = ExtractUtils.device_with_common(module, 'mt6893-common', module.vendor)
    utils.run()
