# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(dwm3001cdk_conn_data_ex)

target_sources(app PRIVATE src/main.c src/my_lbs.c)
zephyr_library_include_directories(.)
set(BOARD_FLASH_RUNNER jlink)