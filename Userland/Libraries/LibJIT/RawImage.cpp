/*
 * Copyright (c) 2023, Jesús Lapastora <cyber.gsuscode@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Format.h>
#include <LibJIT/RawImage.h>
#include <sys/mman.h>

namespace JIT {
RawImage::RawImage(void* addr, size_t size)
    : ExecutableImage(ReadonlyBytes { addr, size })
{
}

RawImage::~RawImage()
{
    // NOTE: It's ok to const_cast here, since we set the address to the
    //  mmap'd code through the constructor.
    munmap(
        reinterpret_cast<void*>(const_cast<uint8_t*>(m_code.data())),
        m_code.size());
}

OwnPtr<RawImage> RawImage::create_from_code(ReadonlyBytes generated_code)
{
    auto* const addr = mmap(NULL, generated_code.size(), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED) {
        dbgln("mmap: {}", strerror(errno));
        return nullptr;
    }
    memcpy(addr, generated_code.data(), generated_code.size());

    if (mprotect(addr, generated_code.size(), PROT_READ | PROT_EXEC) == -1) {
        dbgln("mprotect: {}", strerror(errno));
        return nullptr;
    }

    return make<RawImage>(addr, generated_code.size());
}

}
