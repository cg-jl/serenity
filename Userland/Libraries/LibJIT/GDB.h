/*
 * Copyright (c) 2023, Jesús Lapastora <cyber.gsuscode@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Span.h>

namespace JIT::GDB {

// A JIT GDB Object represents some in-memory object file that can be registered
// with GDB to make JITted code easier to debug.
// This Object only stores the object file's span to be registered, since the
// client might want to register a different parser for a custom debug info
// format:
// https://sourceware.org/gdb/current/onlinedocs/gdb.html/Custom-Debug-Info.html#Custom-Debug-Info
//
// Otherwise, the client should ensure `data` contains an in-memory object file
// before calling `register_into_gdb()`:
// https://sourceware.org/gdb/current/onlinedocs/gdb.html/Registering-Code.html#Registering-Code
//
// > Generate an object file in memory with symbols and other desired debug
// information. The file must include the virtual addresses of the sections.

void register_into_gdb(ReadonlyBytes data);
void unregister_from_gdb(ReadonlyBytes data);

}
