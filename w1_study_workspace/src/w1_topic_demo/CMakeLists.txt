cmake_minimum_required(VERSION 3.8)
project(w1_topic_demo)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)



add_executable(topic_publisher_node  src/topic_publisher_demo.cpp)
ament_target_dependencies(topic_publisher_node rclcpp std_msgs)
add_executable(topic_subscriber_node  src/w1_subscriber_demo.cpp)
ament_target_dependencies(topic_subscriber_node rclcpp std_msgs)


install(TARGETS 
topic_publisher_node
topic_subscriber_node
DESTINATION lib/${PROJECT_NAME}
)
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
