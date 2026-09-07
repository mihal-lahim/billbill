cmake_minimum_required(VERSION 3.20)
project(BillBill LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(billbill_core STATIC Game.cpp CpuPlayer.cpp Settings.cpp)
target_include_directories(billbill_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(EMSCRIPTEN)
    add_executable(billbill_web web/wasm/WebBridge.cpp)
    target_link_libraries(billbill_web PRIVATE billbill_core)
    target_link_options(billbill_web PRIVATE
        "-sEXPORTED_FUNCTIONS=['_bb_create','_bb_start','_bb_select_card','_bb_confirm','_bb_advance','_bb_restart','_bb_state_json','_bb_free']"
        "-sEXPORTED_RUNTIME_METHODS=['ccall','UTF8ToString']"
        "-sMODULARIZE=1"
        "-sEXPORT_ES6=1"
        "-sEXPORT_NAME=createBillBillModule"
        "-sALLOW_MEMORY_GROWTH=1")
    set_target_properties(billbill_web PROPERTIES
        OUTPUT_NAME billbill
        SUFFIX ".js"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/web/client/wasm")
endif()
