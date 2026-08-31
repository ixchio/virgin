function(virgin_enable_sanitizers target)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
            -fno-optimize-sibling-calls
        )
        target_link_options(${target} PRIVATE
            -fsanitize=address,undefined
        )
        message(STATUS "Sanitizers enabled: ASan+UBSan")
    endif()
endfunction()
