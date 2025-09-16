#!/usr/bin/env bash
set -euo pipefail

# 项目根目录
ROOT_DIR="/home/sharpa/tsrc/thread_pool"

# 编译参数
CXX=${CXX:-g++}
CXXFLAGS=${CXXFLAGS:--std=c++17 -O2 -Wall -Wextra -Wpedantic}
LDFLAGS=${LDFLAGS:-}
LIBS="-pthread"

# 源文件与输出
SRCS=("${ROOT_DIR}/threadpool.cpp" "${ROOT_DIR}/main.cpp")
OUT="${ROOT_DIR}/main"

echo "[BUILD] CXX=${CXX}"
echo "[BUILD] CXXFLAGS=${CXXFLAGS}"

set -x
${CXX} -o "${OUT}" ${SRCS[@]} ${CXXFLAGS} ${LDFLAGS} ${LIBS}
set +x

echo "[DONE] Built -> ${OUT}" 