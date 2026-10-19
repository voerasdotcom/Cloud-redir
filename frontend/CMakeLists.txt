cmake_minimum_required(VERSION 3.16)
project(CloudRedirFrontend LANGUAGES C CXX)

# Set the target platform to Nintendo Switch
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Find required libraries: libnx, curl (for OAuth device flow), mjson (for JSON)
find_package(nx REQUIRED)
find_package(CURL REQUIRED)

# We'll use mjson as a header-only library. Assume it's in include/mjson or we fetch it.
# For simplicity, we'll assume the user has it in their include path or we bundle it.
# We'll include it via target_include_directories.

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the source files
file(GLOB_RECURSE FRONTEND_SOURCES "src/*.cpp" "src/*.c")

# Create the frontend application (nro)
add_executable(${PROJECT_NAME} ${FRONTEND_SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link against libnx and curl
target_link_libraries(${PROJECT_NAME} PRIVATE nx curl::libcurl)

# Set output name and suffix for NRO
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "cloudredir_frontend"
    SUFFIX ".nro"
)

# Install targets
install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}-targets
    RUNTIME DESTINATION .
)

# Install any data files (if needed)
# install(DIRECTORY data/ DESTINATION data)