set(TARGET test-statusbar)
add_executable(${TARGET} main.cpp)
target_link_libraries(${TARGET} PRIVATE common)
target_compile_features(${TARGET} PRIVATE cxx_std_17)

if(LLAMA_TOOLS_INSTALL)
    install(TARGETS ${TARGET} RUNTIME)
endif()
