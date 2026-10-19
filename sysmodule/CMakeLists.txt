cmake_minimum_required(VERSION 3.16)
project(CloudRedirSysmodule LANGUAGES C CXX)

# Set the target platform to Nintendo Switch
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Use devkitPro toolchain (assuming it's in the environment)
# We'll let the user set the toolchain file via CMAKE_TOOLCHAIN_FILE when invoking cmake
# Alternatively, we can try to find it, but for simplicity we rely on the user setting it.

# Find required libraries: libnx, curl, mbedtls (for HTTPS), etc.
# Note: For a sysmodule, we must be very careful about the size.
# We'll use the static versions of the libraries to avoid dynamic loading overhead.

# Find libnx (the switch SDK)
find_package(nx REQUIRED)

# We'll use libcurl for HTTP, but note that we might need to use a minimal version.
# Alternatively, we can use the nx http service if available and lightweight.
# For now, we'll look for curl.
find_package(CURL REQUIRED)

# For JSON parsing (if needed) we can use a lightweight library like jsmn or mjson.
# We'll use mjson for simplicity.
# We'll assume we have a copy of mjson in the project or we can fetch it.
# For now, we'll just include it as a header-only library.

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the source files
file(GLOB_RECURSE SYSMODULE_SOURCES "src/*.cpp" "src/*.c")

# Create the sysmodule (nso)
add_library(${PROJECT_NAME} STATIC ${SYSMODULE_SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link against libnx and curl
target_link_libraries(${PROJECT_name} PRIVATE nx curl::libcurl)

# Set sysmodule specific flags
# We need to output an .nso file and set the appropriate attributes.
# We'll use the nx_add_sysmodule function from the nx CMake package if available.
# Alternatively, we can set the output suffix and flags manually.
# Since we don't have the nx CMake package details, we'll assume the user sets the linker flags.

# We'll set the output name and suffix
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "cloudredir"
    SUFFIX ".nso"
)

# We might need to set the entry point and other attributes for the sysmodule.
# This is highly platform-specific and would require the nx toolchain's ld flags.
# For now, we leave it to the user to adjust the linker script or use the nx provided macros.

# Install targets
install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include)