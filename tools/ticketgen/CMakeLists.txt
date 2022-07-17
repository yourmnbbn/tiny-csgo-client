cmake_minimum_required(VERSION 3.23)
project(ticketgen)

include_directories (${HL2SDK-CSGO}/public/steam)
link_directories(${HL2SDK-CSGO}/lib/public)
add_compile_options(/wd4819)

add_executable(ticketgen ticketgen.cpp)
target_link_libraries(ticketgen steam_api.lib)


