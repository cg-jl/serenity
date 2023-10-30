set(SOURCES
    Assembler.cpp
    GDB.cpp
    GDBImage.cpp
    RawImage.cpp
)

serenity_lib(LibJIT jit)
target_link_libraries(LibJIT PRIVATE LibCore)
