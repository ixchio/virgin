function(virgin_enable_hardening target)
    if(NOT VIRGIN_ENABLE_HARDENING)
        return()
    endif()

    include(CheckCXXCompilerFlag)
    include(CheckLinkerFlag)

    # Compiler hardening — check before adding
    check_cxx_compiler_flag("-fstack-protector-strong" HAS_STACK_PROTECTOR)
    if(HAS_STACK_PROTECTOR)
        target_compile_options(${target} PRIVATE -fstack-protector-strong)
    endif()

    check_cxx_compiler_flag("-D_FORTIFY_SOURCE=3" HAS_FORTIFY)
    if(HAS_FORTIFY)
        target_compile_definitions(${target} PRIVATE _FORTIFY_SOURCE=3)
    endif()

    check_cxx_compiler_flag("-fPIE" HAS_FPIE)
    if(HAS_FPIE)
        target_compile_options(${target} PRIVATE -fPIE)
    endif()

    check_cxx_compiler_flag("-fcf-protection=full" HAS_CFC)
    if(HAS_CFC)
        target_compile_options(${target} PRIVATE -fcf-protection=full)
    endif()

    target_compile_options(${target} PRIVATE -fvisibility=hidden)

    # Linker hardening
    check_linker_flag(CXX "-pie" HAS_PIE)
    if(HAS_PIE)
        target_link_options(${target} PRIVATE -pie)
    endif()

    check_linker_flag(CXX "-Wl,-z,relro" HAS_RELRO)
    if(HAS_RELRO)
        target_link_options(${target} PRIVATE "LINKER:-z,relro")
    endif()

    check_linker_flag(CXX "-Wl,-z,now" HAS_NOW)
    if(HAS_NOW)
        target_link_options(${target} PRIVATE "LINKER:-z,now")
    endif()

    check_linker_flag(CXX "-Wl,-z,noexecstack" HAS_NOEXECSTACK)
    if(HAS_NOEXECSTACK)
        target_link_options(${target} PRIVATE "LINKER:-z,noexecstack")
    endif()
endfunction()
