# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(dwm3001cdk_conn)

target_sources(app PRIVATE src/main.c)
set(BOARD_FLASH_RUNNER jlink)