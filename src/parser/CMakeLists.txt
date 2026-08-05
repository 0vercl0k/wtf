# Axel '0vercl0k' Souchet - April 18 2020
link_libraries(kdmp-parser)
add_executable(parser parser.cc)
target_compile_options(
    parser
    PUBLIC
    $<$<CXX_COMPILER_ID:MSVC>:/W4$<SEMICOLON>/WX>
    $<$<CXX_COMPILER_ID:GNU>:-Wall$<SEMICOLON>-Wextra$<SEMICOLON>-pedantic$<SEMICOLON>-Werror>
    $<$<CXX_COMPILER_ID:Clang>:-Wall$<SEMICOLON>-Wextra$<SEMICOLON>-pedantic$<SEMICOLON>-Werror>
)
install(TARGETS parser DESTINATION .)

if(MSVC)
    install(FILES $<TARGET_PDB_FILE:parser> DESTINATION . OPTIONAL)
endif(MSVC)
