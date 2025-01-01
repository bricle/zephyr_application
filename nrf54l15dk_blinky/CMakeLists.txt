# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nrf54l15dk_blinky)

target_sources(app PRIVATE src/main.c)
target_sources_ifdef(CONFIG_MYFUNCTION app PRIVATE src/myFunc.c)