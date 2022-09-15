cmake_minimum_required(VERSION 3.23)
project(tiny-csgo-client-limited)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

include_directories (
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${HL2SDK-CSGO}/common/protobuf-2.5.0/src
    ${ASIO-SRC}/include
    ${HL2SDK-CSGO}/public
    ${HL2SDK-CSGO}/public/engine/protobuf
)

link_directories(${HL2SDK-CSGO}/lib/public)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/include/netmessage NETMSG_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/include/common COMMON_SRC)
set(SRC_LIST cl-main.cpp ${NETMSG_SRC} ${COMMON_SRC})

if(MSVC)
    add_definitions(
        -D _WIN32_WINNT=0x0601
        -D _CRT_SECURE_NO_WARNINGS
        -D _GENERIC
        -D COMPILER_MSVC
        -D COMPILER_MSVC32
        -D __cpp_lib_ranges
        -D _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
    )
    
    set(STATIC_LIB_LIST 
        legacy_stdio_definitions.lib 
        libprotobuf.lib
    )
    
    link_directories(${HL2SDK-CSGO}/lib/win32/release/vs2017)
    add_compile_options(/MT)
    add_compile_options(/wd4819)

elseif(("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))

    add_definitions(
        -D GNUC
        -D POSIX
        -D COMPILER_GCC
        -D _GLIBCXX_USE_CXX11_ABI=0
    )
    
    set(STATIC_LIB_LIST 
        ${HL2SDK-CSGO}/lib/linux32/release/libprotobuf.a
        libpthread.so
    )
        
    add_compile_options(
        -m32 
        -Wno-volatile
        -Wno-format-security
        -Wno-register
        -fcoroutines
    )
    
    add_link_options(-m32)

endif()

add_executable(tiny-csgo-client-limited ${SRC_LIST})
target_link_libraries(tiny-csgo-client-limited ${STATIC_LIB_LIST})

if(MSVC)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT tiny-csgo-client-limited)
endif()

