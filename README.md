# FPGA Placement and Routing Simulator

A C++ and Qt-based simulator for exploring FPGA architecture and logic-module placement.

## Overview

This project provides a software framework for representing FPGA hardware resources and developing placement algorithms. It uses a Qt graphical user interface to organize and visualize the simulator's functionality.

## Features

- **FPGA Architecture Representation:** Model the FPGA architecture and its hardware resources.
- **Hardware Module Representation:** Represent hardware modules used in the placement process.
- **Placement Engine:** Implement and explore placement algorithms for FPGA modules.
- **Graphical User Interface:** Interact with the simulator through a Qt-based GUI.
- **Sample Netlist:** Use the included sample netlist as input for experimentation.

## Project Structure

| File | Description |
|---|---|
| `main.cpp` | Application entry point |
| `mainwindow.cpp/.h` | Main window implementation |
| `mainwindow.ui` | Qt Designer user interface |
| `fpgaarchitecture.cpp/.h` | FPGA architecture representation |
| `hardwaremodule.cpp/.h` | Hardware module representation |
| `placementengine.cpp/.h` | Placement engine implementation |
| `sample_netlist.txt` | Sample netlist input |
| `CMakeLists.txt` | CMake build configuration |

## Technologies Used

- C++
- Qt Framework
- CMake
- Git and GitHub

## Getting Started

### Prerequisites

- A C++ compiler compatible with your Qt installation
- Qt development libraries
- CMake
- Qt Creator or another compatible development environment

### Build and Run

1. Clone the repository:

   ```bash
   git clone https://github.com/PhienhasSamuel/FPGA-Placement-and-Routing-Simulator.git
   ```

2. Open the project in Qt Creator using its `CMakeLists.txt` file.

3. Configure the project with a compatible Qt kit and C++ compiler.

4. Build and run the application from Qt Creator.

> **Note:** The exact Qt version and compiler depend on the development environment used to configure the project.

## Future Improvements

- Add additional placement algorithms and compare their results.
- Extend routing functionality and routing-resource visualization.
- Add placement quality metrics and resource utilization analysis.
- Improve result visualization and reporting.

## Author

PhienhasSamuel

## License

No license has been specified yet.
