
cmake_minimum_required(VERSION 3.16)

project(FPGA_Placement_GUI VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(FPGA_Placement_GUI
    main.cpp
    mainwindow.h
    mainwindow.cpp
    sample_netlist.txt
)

target_link_libraries(FPGA_Placement_GUI PRIVATE Qt6::Widgets)
