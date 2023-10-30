/*
 * Copyright (c) 2023, Jesús Lapastora <cyber.gsuscode@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Format.h>
#include <AK/Vector.h>
#include <LibELF/ELFABI.h>
#include <LibJIT/GDBImage.h>
#include <sys/mman.h>

namespace JIT {

OwnPtr<GDBImage> GDBImage::create_from_code(ReadonlyBytes generated_code)
{

    // Create an ELF file that contains the code as a section on it
    // Target ELF memory:
    // <elf header> <program header> <.text header> <.shstrtab header> <.shstrtab contents> [padding] <page boundary> <.text code>

    // NOTE: Since the .text section needs a valid (positive) offset for
    // `sh_offset`, we have to move the code to somewhere after all of the above
    // headers. To ensure it's still executable, we'll align it with a page
    // boundary. To avoid extending the in-memory size too much, we'll add the
    // code last.
    // Some settings of addresses/offsets of sections might seem a bit out of
    // place when thinking about "memory" vs "file" image differences, so take
    // into account that we're merging both into the same memory.

    Vector<u8> shstrtab;

    auto add_string_to_shstrtab = [&shstrtab](StringView name) -> u64 {
        u64 const index = static_cast<u64>(shstrtab.size());
        shstrtab.ensure_capacity(shstrtab.size() + name.length() + 1);
        // name must not have NULL terminators
        for (auto const cp : name) {
            VERIFY(cp != 0);
            shstrtab.unchecked_append(cp);
        }
        shstrtab.unchecked_append(0);

        return index;
    };

    // ensure we add to shstrtab *before* we compute the final image size

    auto const text_name_index = add_string_to_shstrtab(".text"sv);
    auto const shstrtab_name_index = add_string_to_shstrtab(".shstrtab"sv);

    auto const page_size = bit_cast<u64>(sysconf(_SC_PAGESIZE));

    Checked<u64> total_image_size = 0;
    // 1 ELF Header
    total_image_size += sizeof(Elf64_Ehdr);
    auto const phdr_offset = total_image_size.value_unchecked();
    // 1 Program Header (Only 1 section: executable instructions)
    total_image_size += sizeof(Elf64_Phdr);
    auto const shdr_offset = total_image_size.value_unchecked();
    // 2 Section Headers
    total_image_size += 2 * sizeof(Elf64_Shdr);
    auto const shstrtab_content_offset = total_image_size.value_unchecked();
    // shstrtab contents
    total_image_size += shstrtab.size();

    // align_up_to with verifications
    total_image_size += page_size - 1;
    total_image_size = total_image_size.value() & ~(page_size - 1);
    auto const code_offset = total_image_size.value_unchecked();

    total_image_size += generated_code.size();

    void* const mapped = mmap(NULL, total_image_size.value(), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (mapped == MAP_FAILED) {
        dbgln("mmap: {}", strerror(errno));
        return nullptr;
    }

    auto const mapped_addr = reinterpret_cast<uintptr_t>(mapped);

    auto const text_section_index = static_cast<u64>(0);
    auto const shstrtab_section_index = static_cast<u64>(1);

    auto section_headers = Span<Elf64_Shdr> {
        reinterpret_cast<Elf64_Shdr*>(mapped_addr + shdr_offset),
        2
    };

    auto shstrtab_contents = Bytes {
        reinterpret_cast<uint8_t*>(mapped_addr + shstrtab_content_offset),
        shstrtab.size()
    };

    auto const code_addr = mapped_addr + code_offset;

    auto code_contents = Bytes {
        reinterpret_cast<uint8_t*>(code_addr),
        generated_code.size()
    };

    {
        auto* elf = reinterpret_cast<Elf64_Ehdr*>(mapped);
        elf->e_ident[EI_MAG0] = 0x7f;
        elf->e_ident[EI_MAG1] = 'E';
        elf->e_ident[EI_MAG2] = 'L';
        elf->e_ident[EI_MAG3] = 'F';
        elf->e_ident[EI_CLASS] = ELFCLASS64;
        // FIXME: This is an x86-ism. This should be what the host platform is,
        // since we're writing everything with the endianness of the host
        // platform.
        elf->e_ident[EI_DATA] = ELFDATA2LSB;
        elf->e_ident[EI_VERSION] = EV_CURRENT;
        // This ELF format is generated so that GDB can read it.
        // It may not follow System V ABI, since addresses are hardcoded into it.
        elf->e_ident[EI_OSABI] = ELFOSABI_STANDALONE;
        elf->e_ident[EI_ABIVERSION] = 0;
        memset(&elf->e_ident[EI_PAD], 0, EI_NIDENT - EI_PAD);

        // No file type.
        // NOTE: This might make GDB reject the ELF image, so fiddle with it in
        // case it doesn't work.
        elf->e_type = ET_NONE;
        // FIXME: This is an x86-ism. This should be the host platform.
        elf->e_machine = EM_AMD64;
        elf->e_version = EV_CURRENT;
        // No entry point; this image is just a reference for code & symbols.
        elf->e_entry = 0;
        elf->e_phoff = phdr_offset;
        elf->e_shoff = shdr_offset;
        elf->e_flags = 0;
        elf->e_ehsize = sizeof(*elf);
        elf->e_phentsize = sizeof(Elf64_Phdr);
        elf->e_phnum = 1;
        elf->e_shentsize = sizeof(Elf64_Shdr);
        elf->e_shnum = section_headers.size();
        // PERF: We could set no section header name table, if that ends
        // up taking too much space.
        elf->e_shstrndx = shstrtab_name_index;
    }

    {
        auto* exe_segment_hdr = reinterpret_cast<Elf64_Phdr*>(mapped_addr + phdr_offset);
        exe_segment_hdr->p_flags = PF_X | PF_R;
        exe_segment_hdr->p_memsz = generated_code.size();
        exe_segment_hdr->p_type = PT_LOAD;
        exe_segment_hdr->p_vaddr = code_addr;
        // NOTE: Although GDB only requires the Virtual Address of the code section
        // to be set, we'll set the Physical Address to the same value, since GCC
        // and Clang also do this in their outputs.
        exe_segment_hdr->p_paddr = code_addr;
        exe_segment_hdr->p_filesz = exe_segment_hdr->p_memsz;
        exe_segment_hdr->p_align = page_size;
        exe_segment_hdr->p_offset = code_offset;
    }

    {
        auto* text_section_hdr = section_headers.offset(text_section_index);
        text_section_hdr->sh_name = text_name_index;
        text_section_hdr->sh_type = SHT_PROGBITS;
        text_section_hdr->sh_flags = SHF_ALLOC | SHF_EXECINSTR;
        text_section_hdr->sh_addr = code_addr;
        text_section_hdr->sh_offset = code_offset;
        text_section_hdr->sh_size = generated_code.size();
        text_section_hdr->sh_link = 0; // it is zero in ELF files outputted by
                                       // GCC as well.
        text_section_hdr->sh_info = 0;
        // NOTE: This field _should_ be useless, since we're giving GDB the _real_
        // addresses of the executed code. Nevertheless, that is the value
        // that GCC gives to the .text section in x86_64 binaries.
        text_section_hdr->sh_addralign = 16;
        // no extra info here.
        text_section_hdr->sh_entsize = 0;
    }

    {
        auto* shstrtab_section_hdr = section_headers.offset(shstrtab_section_index);
        shstrtab_section_hdr->sh_name = shstrtab_name_index;
        shstrtab_section_hdr->sh_type = SHT_STRTAB;
        shstrtab_section_hdr->sh_flags = 0;
        shstrtab_section_hdr->sh_addr = 0;
        shstrtab_section_hdr->sh_offset = shstrtab_content_offset;
        shstrtab_section_hdr->sh_size = shstrtab.size();
        shstrtab_section_hdr->sh_link = 0;
        shstrtab_section_hdr->sh_info = 0;
        shstrtab_section_hdr->sh_addralign = 1;
        shstrtab_section_hdr->sh_entsize = 0;
    }

    memcpy(shstrtab_contents.data(), shstrtab.data(), shstrtab.size());
    memcpy(code_contents.data(), generated_code.data(), generated_code.size());
    // ensure the mapped code is executable
    if (mprotect(code_contents.offset(0), code_contents.size(), PROT_READ | PROT_EXEC)
        == -1) {
        dbgln("mprotect: {}", strerror(errno));
        munmap(mapped, total_image_size.value_unchecked());
        return nullptr;
    }

    return make<GDBImage>(Bytes {
        reinterpret_cast<uint8_t*>(mapped),
        total_image_size.value_unchecked(),
    });
}

GDBImage::~GDBImage()
{
    if (m_registered)
        unregister_from_gdb();

    munmap(m_elf_image.data(), m_elf_image.size());
}

}
