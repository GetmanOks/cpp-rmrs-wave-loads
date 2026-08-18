cmake_minimum_required(VERSION 3.16)

# Некоторые зависимости (GLFW, doctest) объявляют старую совместимость с CMake
# На CMake 4.x это ломает конфигурацию, поэтому задаём минимальную версию политик
if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
	set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

project(rmrs_wave_loads LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# На MSVC исходники и строковые литералы в UTF-8 (для корректной кириллицы)
if(MSVC)
	add_compile_options(/utf-8)
endif()

include(FetchContent)

# --- Ядро расчёта (без зависимостей от GUI, полностью тестируемое) -----------
add_library(strength_core STATIC
	src/core/ApplicabilityChecker.cpp
	src/core/WaveLoadCalculator.cpp
)
target_include_directories(strength_core PUBLIC src/core)

# --- Зависимости для GUI и тестов -------------------------------------------
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glfw
	GIT_REPOSITORY https://github.com/glfw/glfw.git
	GIT_TAG        3.4
	GIT_SHALLOW    TRUE
)
FetchContent_Declare(imgui
	GIT_REPOSITORY https://github.com/ocornut/imgui.git
	GIT_TAG        v1.90.9
	GIT_SHALLOW    TRUE
)
FetchContent_Declare(doctest
	GIT_REPOSITORY https://github.com/doctest/doctest.git
	GIT_TAG        v2.4.11
	GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(glfw imgui doctest)

# Dear ImGui поставляется исходниками: собираем как отдельную библиотеку
add_library(imgui STATIC
	${imgui_SOURCE_DIR}/imgui.cpp
	${imgui_SOURCE_DIR}/imgui_draw.cpp
	${imgui_SOURCE_DIR}/imgui_tables.cpp
	${imgui_SOURCE_DIR}/imgui_widgets.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
	${imgui_SOURCE_DIR}
	${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw)

find_package(OpenGL REQUIRED)

# --- Приложение с графическим интерфейсом -----------------------------------
add_executable(rmrs_gui src/app/main.cpp)
target_link_libraries(rmrs_gui PRIVATE strength_core imgui OpenGL::GL)

if(WIN32)
	# Оконное приложение без консоли; точку входа оставляем обычной main()
	# Для подсистемы WINDOWS указываем линковщику mainCRTStartup
	set_target_properties(rmrs_gui PROPERTIES WIN32_EXECUTABLE TRUE)
	target_link_options(rmrs_gui PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/ENTRY:mainCRTStartup>)
endif()

# --- Юнит-тесты ядра --------------------------------------------------------
enable_testing()
add_executable(core_tests tests/core_tests.cpp)
target_link_libraries(core_tests PRIVATE strength_core doctest::doctest)
add_test(NAME core_tests COMMAND core_tests)
