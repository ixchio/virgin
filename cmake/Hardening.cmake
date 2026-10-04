function(virgin_enable_hardening target)
    if(NOT VIRGIN_ENABLE_HARDENING)
        return()
    endif()

    if(MSVC)
        target_compile_options(${target} PRIVATE /guard:cf /sdl)
        target_link_options(${target} PRIVATE /guard:cf /DYNAMICBASE /NXCOMPAT)
        return()
    endif()

    include(CheckCXXCompilerFlag)
    include(CheckLinkerFlag)

    # Compiler hardening — check before adding
    check_cxx_compiler_flag("-fstack-protector-strong" HAS_STACK_PROTECTOR)
    if(HAS_STACK_PROTECTOR)
        target_compile_options(${target} PRIVATE -fstack-protector-strong)
    endif()

    check_cxx_compiler_flag("-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3" HAS_FORTIFY)
    if(HAS_FORTIFY)
        # Ubuntu's GCC specs define _FORTIFY_SOURCE=2 by default. Undefining
        # it first makes the desired level portable and avoids a Werror build
        # failure from redefining the macro.
        target_compile_options(${target} PRIVATE -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3)
    endif()

    check_cxx_compiler_flag("-fPIE" HAS_FPIE)
    if(HAS_FPIE)
        target_compile_options(${target} PRIVATE -fPIE)
    endif()

    check_cxx_compiler_flag("-fcf-protection=full" HAS_CFC)
    if(HAS_CFC)
        target_compile_options(${target} PRIVATE -fcf-protection=full)
    endif()

    if(UNIX)
        target_compile_options(${target} PRIVATE -fvisibility=hidden)
    endif()

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
