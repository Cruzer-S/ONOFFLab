cmake_minimum_require(VERSION 3.13)

include(pico_sdk_import.cmake)

project(test_project)

pico_sdk_init()

add_executable(test
	test.c
)

pico_enable+stdo
