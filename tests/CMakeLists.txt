cmake_minimum_required(VERSION 3.22)

find_package(Qt6 6.2 QUIET COMPONENTS Core Test WebEngineCore)

if(Qt6Test_FOUND)
    # Functional blocker tests.
    qt_add_executable(test_adblock_phase3
        unit/test_adblock_phase3.cpp
        ../src/adblock/AdBlockEngine.cpp
        ../src/adblock/FilterParser.cpp
        ../src/adblock/FilterCompiler.cpp
        ../src/adblock/RuleIndex.cpp
        ../src/adblock/DomainTrie.cpp
        ../src/adblock/AhoCorasick.cpp
        ../src/adblock/CosmeticEngine.cpp
        ../src/network/RequestClassifier.cpp
        ../src/app/Paths.cpp
    )
    target_include_directories(test_adblock_phase3 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    target_link_libraries(test_adblock_phase3 PRIVATE Qt6::Core Qt6::Test Qt6::WebEngineCore)
    virgin_set_warnings(test_adblock_phase3)
    if(VIRGIN_ENABLE_SANITIZERS)
        virgin_enable_sanitizers(test_adblock_phase3)
    endif()
    add_test(NAME AdBlockPhase3 COMMAND test_adblock_phase3)

    qt_add_executable(test_security_core
        unit/test_security_core.cpp
        ../src/security/RuntimeSecurityCheck.cpp
        ../src/security/UrlSafety.cpp
        ../src/network/HttpsFirstPolicy.cpp
        ../src/storage/SettingsStore.cpp
        ../src/privacy/CookiePolicy.cpp
    )
    target_include_directories(test_security_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
    target_link_libraries(test_security_core PRIVATE Qt6::Core Qt6::Test Qt6::Network Qt6::WebEngineCore)
    virgin_set_warnings(test_security_core)
    if(VIRGIN_ENABLE_SANITIZERS)
        virgin_enable_sanitizers(test_security_core)
    endif()
    add_test(NAME SecurityCore COMMAND test_security_core)

    qt_add_executable(test_storage
        unit/test_storage.cpp
        ../src/storage/Database.cpp
        ../src/storage/HistoryStore.cpp
        ../src/storage/SessionStore.cpp
    )
    target_include_directories(test_storage PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
    target_link_libraries(test_storage PRIVATE Qt6::Core Qt6::Sql)
    virgin_set_warnings(test_storage)
    if(VIRGIN_ENABLE_SANITIZERS)
        virgin_enable_sanitizers(test_storage)
    endif()
    add_test(NAME Storage COMMAND test_storage)

    # Release benchmark gate. The executable reports host-specific latency and
    # returns non-zero when its explicit latency budget is missed.
    qt_add_executable(adblock_benchmark
        benchmarks/adblock_benchmark.cpp
        ../src/adblock/AdBlockEngine.cpp
        ../src/adblock/FilterParser.cpp
        ../src/adblock/FilterCompiler.cpp
        ../src/adblock/RuleIndex.cpp
        ../src/adblock/DomainTrie.cpp
        ../src/adblock/AhoCorasick.cpp
        ../src/adblock/CosmeticEngine.cpp
        ../src/network/RequestClassifier.cpp
        ../src/app/Paths.cpp
    )
    target_include_directories(adblock_benchmark PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    target_link_libraries(adblock_benchmark PRIVATE Qt6::Core Qt6::WebEngineCore)
    virgin_set_warnings(adblock_benchmark)
    if(NOT VIRGIN_ENABLE_SANITIZERS)
        add_test(NAME AdBlockBenchmark COMMAND adblock_benchmark)
        set_tests_properties(AdBlockBenchmark PROPERTIES LABELS "benchmark")
    endif()

    # Fuzz smoke test (Sec 53)
    qt_add_executable(filter_parser_fuzz_smoke
        fuzz/filter_parser_fuzz.cpp
        ../src/adblock/FilterParser.cpp
    )
    target_compile_definitions(filter_parser_fuzz_smoke PRIVATE FUZZ_STANDALONE)
    target_include_directories(filter_parser_fuzz_smoke PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    target_link_libraries(filter_parser_fuzz_smoke PRIVATE Qt6::Core)
    virgin_set_warnings(filter_parser_fuzz_smoke)
    if(VIRGIN_ENABLE_SANITIZERS)
        virgin_enable_sanitizers(filter_parser_fuzz_smoke)
    endif()
    add_test(NAME FilterParserFuzzSmoke COMMAND filter_parser_fuzz_smoke)
else()
    message(STATUS "Qt6 Test not found — skipping tests")
endif()
