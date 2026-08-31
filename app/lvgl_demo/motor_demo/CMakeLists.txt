cmake_minimum_required(VERSION 3.8)

project(motor_demo)

if (LVGL_V9)
	MESSAGE(FATAL_ERROR "This app no support LVGL9, please switch to LVGL8")
endif()

include_directories(${PROJECT_SOURCE_DIR}/)

link_directories(./)

set(MOTOR_DEMO_SRCS servo_ui.c)
add_executable(${PROJECT_NAME} ${SRCS} ${MOTOR_DEMO_SRCS})
add_executable(servo_backend
	servo_backend.c
	Rockchip_MADHT1505BA1.c)
add_executable(servo_state_dump
	servo_state_dump.c)
add_executable(cpu_dma_latency_keeper
	cpu_dma_latency_keeper.c)
set_property(TARGET servo_backend PROPERTY LINK_LIBRARIES "")
set_property(TARGET servo_state_dump PROPERTY LINK_LIBRARIES "")
set_property(TARGET cpu_dma_latency_keeper PROPERTY LINK_LIBRARIES "")

set(IGH_ETHERCAT_PREFIX "" CACHE PATH "IgH EtherCAT userspace installation")
set(IGH_ETHERCAT_INCLUDE_DIR "${IGH_ETHERCAT_PREFIX}/include")
set(IGH_ETHERCAT_LIBRARY "${IGH_ETHERCAT_PREFIX}/lib/libethercat.so")
if(NOT EXISTS "${IGH_ETHERCAT_INCLUDE_DIR}/ecrt.h" OR
   NOT EXISTS "${IGH_ETHERCAT_LIBRARY}")
	message(FATAL_ERROR "IgH EtherCAT library not found; set IGH_ETHERCAT_PREFIX")
endif()
target_include_directories(servo_backend PRIVATE ${IGH_ETHERCAT_INCLUDE_DIR})
target_link_libraries(servo_backend PRIVATE ${IGH_ETHERCAT_LIBRARY} pthread)

#This demo only provides ui display. For details about the prototype library, consult RK
#target_link_libraries(${PROJECT_NAME}
#	madht1505ba1 ethercat
#)

install(TARGETS
	${PROJECT_NAME}
	servo_backend
	servo_state_dump
	cpu_dma_latency_keeper
	RUNTIME DESTINATION bin)
install(FILES
	resource/SmileySans-Oblique.ttf
	resource/needle.png
	DESTINATION share/resource)
install(PROGRAMS S10lv_demo DESTINATION /etc/init.d/pre_init RENAME S02lv_demo)
install(PROGRAMS S09servo_backend DESTINATION /etc/init.d/pre_init RENAME S08servo_backend)
