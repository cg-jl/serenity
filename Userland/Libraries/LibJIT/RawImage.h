/*
 * Copyright (c) 2023, Jesús Lapastora <cyber.gsuscode@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/OwnPtr.h>
#include <AK/Span.h>
#include <AK/Types.h>
#include <LibJIT/ExecutableImage.h>

namespace JIT {
// A code image that only consists of the mapped code that's ready to be
// executed, without any wrapping.
class RawImage : public ExecutableImage {
    size_t m_size;

public:
    // NOTE: Making this public so it is accessible to make().
    // The target constructor to call is `create_from_code`.
    RawImage(void* addr, size_t size);
    ~RawImage();
    static OwnPtr<RawImage> create_from_code(ReadonlyBytes generated_code);
};
}
