/*
 * Copyright (c) 2023, Jesús Lapastora <cyber.gsuscode@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Span.h>
#include <AK/Types.h>

namespace JIT {
// An image is a container of JITted code that is ready to be run.
// Any image type that subclasses this object must set `m_code_address` so that
// it points to a block of code ready to be executed.
struct ExecutableImage {

public:
    // Returns the code that is ready to be run
    ReadonlyBytes runnable_code() const { return m_code; }

protected:
    ExecutableImage(ReadonlyBytes code)
        : m_code(code)
    {
    }
    ReadonlyBytes m_code;
};
}
