cmake_minimum_required(VERSION 3.8)
project(offboard_rl)

# Default to C99
if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 99)
endif()

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 14)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(px4_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem thread)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(eigen3_cmake_module REQUIRED)
find_package(Eigen3 REQUIRED)

# Header files inside:
# offboard_rl/include/
include_directories(include)


add_executable(
  go_to_point
  src/go_to_point.cpp
)

ament_target_dependencies(
  go_to_point
  rclcpp
  px4_msgs
  Boost
  geometry_msgs
  Eigen3
)

# ============================================================
# rajectory planner
# ============================================================

add_executable(
  trajectory_planner
  src/trajectory_planner.cpp
)

ament_target_dependencies(
  trajectory_planner
  rclcpp
  px4_msgs
  Boost
  geometry_msgs
  Eigen3
)

# ============================================================
# Install ROS2 executables
# ============================================================

install(
  TARGETS
    go_to_point
    trajectory_planner
  DESTINATION lib/${PROJECT_NAME}
)

# ============================================================
# Testing / linting
# ============================================================

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)

  # Skip copyright check
  set(ament_cmake_copyright_FOUND TRUE)

  # Skip cpplint
  set(ament_cmake_cpplint_FOUND TRUE)

  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
