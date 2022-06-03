cmake_minimum_required(VERSION 3.23)
project(tiny-csgo-client)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

include_directories (
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${HL2SDK-CSGO}/common/protobuf-2.5.0/src
    ${HL2SDK-CSGO}/public/engine/protobuf
    ${HL2SDK-CSGO}/public/steam
    ${HL2SDK-CSGO}/public/tier0
    ${HL2SDK-CSGO}/public
    ${HL2SDK-CSGO}/public/tier1
    ${ASIO-SRC}/include
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
    -D __cpp_lib_format
    -D __cpp_lib_ranges
    -D _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
    )
    
    set(STATIC_LIB_LIST 
        tier1.lib 
        tier0.lib 
        legacy_stdio_definitions.lib 
        steam_api.lib 
        mathlib.lib 
        vstdlib.lib 
        libprotobuf.lib
    )
    
    link_directories(${HL2SDK-CSGO}/lib/win32/release/vs2017)
    add_compile_options(/MT)
    add_compile_options(/wd4819)
endif()

add_executable(tiny-csgo-client ${SRC_LIST})

if(MSVC)
    target_link_libraries(tiny-csgo-client ${STATIC_LIB_LIST})
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT tiny-csgo-client)
endif()

