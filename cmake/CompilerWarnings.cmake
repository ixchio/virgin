function(virgin_set_warnings target)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Wformat=2
        -Wunused
        -Wnon-virtual-dtor
        -Woverloaded-virtual
        -Wnull-dereference
        -Wdouble-promotion
    )

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target} PRIVATE
            -Wduplicated-cond
            -Wduplicated-branches
            -Wlogical-op
        )
    endif()

    if(VIRGIN_WARNINGS_AS_ERRORS)
        target_compile_options(${target} PRIVATE -Werror)
    endif()
endfunction()
