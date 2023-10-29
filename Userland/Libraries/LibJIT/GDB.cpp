/*
 * Copyright (c) 2023, Jesús Lapastora <cyber.gsuscode@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/OwnPtr.h>
#include <LibJIT/GDB.h>

// Declarations from https://sourceware.org/gdb/current/onlinedocs/gdb.html/Declarations.html#Declarations.
// NOTE: If the JIT is multi-threaded, then it is important that the JIT synchronize any modifications to this global data properly, which can easily be done by putting a global mutex around modifications to these structures.
extern "C" {
typedef enum {
    JIT_NOACTION = 0,
    JIT_REGISTER_FN,
    JIT_UNREGISTER_FN
} jit_actions_t;

struct jit_code_entry {
    struct jit_code_entry* next_entry;
    struct jit_code_entry* prev_entry;
    char const* symfile_addr;
    u64 symfile_size;
};

struct jit_descriptor {
    u32 version;
    /* This type should be jit_actions_t, but we use uint32_t
       to be explicit about the bitwidth.  */
    u32 action_flag;
    struct jit_code_entry* relevant_entry;
    struct jit_code_entry* first_entry;
};

/* GDB puts a breakpoint in this function.  */
void __attribute__((noinline)) __jit_debug_register_code() { }

/* Make sure to specify the version statically, because the
   debugger may check the version before we can set it.  */
struct jit_descriptor __jit_debug_descriptor = { 1, 0, 0, 0 };
}

namespace JIT::GDB {

static jit_code_entry* find_code_entry(ReadonlyBytes data)
{
    auto const search_addr = bit_cast<size_t>(data.offset(0));
    for (jit_code_entry* curr = __jit_debug_descriptor.first_entry; curr != NULL; curr = curr->next_entry) {
        auto const entry_addr = bit_cast<size_t>(curr->symfile_addr);
        if (entry_addr == search_addr) {
            VERIFY(curr->symfile_size == data.size());
            return curr;
        }
    }
    return NULL;
}

void unregister_from_gdb(ReadonlyBytes data)
{
    // https://sourceware.org/gdb/current/onlinedocs/gdb.html/Unregistering-Code.html#Unregistering-Code
    //  30.3 Unregistering Code
    //  Remove the code entry corresponding to the code from the linked list.
    auto may_have_entry = AK::adopt_own_if_nonnull(find_code_entry(data));
    VERIFY(may_have_entry);
    auto entry = may_have_entry.release_nonnull();
    if (entry->prev_entry) {
        entry->prev_entry->next_entry = entry->next_entry;
    }
    if (entry->next_entry) {
        entry->next_entry->prev_entry = entry->prev_entry;
    }
    // Point the relevant_entry field of the descriptor at the code entry.
    __jit_debug_descriptor.relevant_entry = entry;
    // Set action_flag to JIT_UNREGISTER and call __jit_debug_register_code.
    __jit_debug_descriptor.action_flag = JIT_UNREGISTER_FN;
    __jit_debug_register_code();
}

void register_into_gdb(ReadonlyBytes data)
{
    // https://sourceware.org/gdb/current/onlinedocs/gdb.html/Registering-Code.html#Registering-Code
    //    To register code with GDB, the JIT should follow this protocol:

    // Generate an object file in memory with symbols and other desired debug information. The file must include the virtual addresses of the sections.
    // NOTE: this is done by the client of `Object`, since the client may want to specify custom readers: https://sourceware.org/gdb/current/onlinedocs/gdb.html/Writing-JIT-Debug-Info-Readers.html#Writing-JIT-Debug-Info-Readers

    // Create a code entry for the file, which gives the start and size of the symbol file.
    auto new_entry = MUST(AK::adopt_nonnull_own_or_enomem(new (nothrow) jit_code_entry));

    // Add it to the linked list in the JIT descriptor.
    auto* const leaked_entry = new_entry.leak_ptr();
    leaked_entry->symfile_addr = bit_cast<char const*>(data.offset(0));
    leaked_entry->symfile_size = data.size();
    leaked_entry->next_entry = __jit_debug_descriptor.first_entry;
    if (__jit_debug_descriptor.first_entry) {
        VERIFY(!__jit_debug_descriptor.first_entry->prev_entry);
        __jit_debug_descriptor.first_entry->prev_entry = leaked_entry;
    }
    leaked_entry->prev_entry = NULL;
    __jit_debug_descriptor.first_entry = leaked_entry;
    // Point the relevant_entry field of the descriptor at the entry.
    __jit_debug_descriptor.relevant_entry = leaked_entry;
    // Set action_flag to JIT_REGISTER and call __jit_debug_register_code.
    __jit_debug_descriptor.action_flag = JIT_REGISTER_FN;
    __jit_debug_register_code();
}
}
