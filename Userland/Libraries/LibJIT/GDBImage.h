/*
 * Copyright (c) 2023, Jesús Lapastora <cyber.gsuscode@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/Noncopyable.h>
#include <AK/OwnPtr.h>
#include <AK/Platform.h>
#include <AK/Span.h>
#include <LibJIT/GDB.h>

// FIXME: There are x86-isms here. Mainly the ELF header.
#if ARCH(X86_64)

namespace JIT {

// ELF Image that is only compatible with GDB's JIT interface.
// It merges the "file" and "memory" concepts of an image into the same place.
// More information in GDBImage.cpp!
class GDBImage {
    AK_MAKE_NONMOVABLE(GDBImage);
    AK_MAKE_NONCOPYABLE(GDBImage);

    Bytes m_elf_image;
    bool m_registered { false };

    constexpr explicit GDBImage(Bytes elf_image)
        : m_elf_image(elf_image)

    {
    }

public:
    Bytes elf_image() const { return m_elf_image; }
    void* executable_code()
    {
        return reinterpret_cast<void*>(m_elf_image.data());
    }
    void register_into_gdb() const { GDB::register_into_gdb(m_elf_image); }
    void unregister_from_gdb() const { GDB::register_into_gdb(m_elf_image); }
    ~GDBImage();

    static OwnPtr<GDBImage> create_from_code(ReadonlyBytes generated_code);
};
}
#endif
