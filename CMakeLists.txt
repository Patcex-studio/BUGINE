# Root CMake configuration for Historica Universalis
cmake_minimum_required(VERSION 3.20)
project(historica_universalis VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Core engine is always built
add_subdirectory(core)

# Modules are optional
option(BUILD_ASSEMBLY_SYSTEM "Build vehicle assembly system" ON)
option(BUILD_ELECTRICAL_SYSTEM "Build electrical system (power grid simulation)" ON)
option(BUILD_HISTORICAL_SYSTEM "Build historical vehicle data system" ON)
option(BUILD_PROCEDURAL_ARMOR_FACTORY "Build procedural armor factory (tank generation)" ON)
option(BUILD_COMBAT_SYSTEMS "Build advanced combat systems" ON)
option(BUILD_MEDICAL_SYSTEM "Build medical simulation system" ON)
option(BUILD_MODEL_GENERATION_SYSTEM "Build model generation system" ON)
option(BUILD_PTU_SYSTEM "Build PTU (User-Generated Content) system" ON)
option(BUILD_UI_MODULE "Build UI module" ON)

if(BUILD_ASSEMBLY_SYSTEM)
    add_subdirectory(modules/assembly_system)
endif()

if(BUILD_ELECTRICAL_SYSTEM)
    add_subdirectory(modules/electrical_system)
endif()

if(BUILD_PROCEDURAL_ARMOR_FACTORY AND BUILD_HISTORICAL_SYSTEM)
    add_subdirectory(modules/procedural_armor_factory)
endif()

if(BUILD_HISTORICAL_SYSTEM AND BUILD_ASSEMBLY_SYSTEM)
    add_subdirectory(modules/historical_vehicle_system)
endif()

if(BUILD_COMBAT_SYSTEMS)
    add_subdirectory(modules/combat_systems)
endif()

if(BUILD_MEDICAL_SYSTEM)
    add_subdirectory(modules/medical_system)
endif()

if(BUILD_MODEL_GENERATION_SYSTEM)
    add_subdirectory(modules/model_generation_system)
endif()

if(BUILD_PTU_SYSTEM)
    add_subdirectory(modules/ptu_system)
endif()

if(BUILD_UI_MODULE)
    add_subdirectory(modules/ui)
endif()

# Tools are development-only
option(BUILD_TOOLS "Build development tools" OFF)
if(BUILD_TOOLS)
    add_subdirectory(tools)
endif()

# Tests
option(BUILD_TESTS "Build test suite" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
option(BUILD_EXAMPLES "Build example applications" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()