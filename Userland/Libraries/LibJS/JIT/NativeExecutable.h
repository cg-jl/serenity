/*
 * Copyright (c) 2023, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Noncopyable.h>
#include <AK/OwnPtr.h>
#include <AK/Types.h>
#include <LibJIT/ExecutableImage.h>
#include <LibJS/Runtime/Completion.h>

namespace JS::JIT {
using ::JIT::ExecutableImage;

class NativeExecutable {
    AK_MAKE_NONCOPYABLE(NativeExecutable);
    AK_MAKE_NONMOVABLE(NativeExecutable);

public:
    NativeExecutable(NonnullOwnPtr<ExecutableImage> code_image);

    void run(VM&) const;
    void dump_disassembly() const;

private:
    NonnullOwnPtr<ExecutableImage> m_image;
};

}
