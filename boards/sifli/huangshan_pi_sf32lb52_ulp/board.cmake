# SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
# SPDX-License-Identifier: Apache-2.0

# keep first
board_runner_args(sftool "--chip=SF32LB52")

# keep first
include(${ZEPHYR_BASE}/boards/common/sftool.board.cmake)
