FROM centos:8

# ---- 修复 CentOS 8 EOL 仓库到 vault，保证 dnf 可用 ----
RUN sed -i 's/mirrorlist/#mirrorlist/g' /etc/yum.repos.d/CentOS-* \
     && sed -i 's|#baseurl=http://mirror.centos.org|baseurl=http://vault.centos.org|g' /etc/yum.repos.d/CentOS-* \
     && dnf -y clean all && dnf -y makecache

# ---- 基础开发环境 ----
RUN dnf -y update && dnf -y install \
     gcc gcc-c++ make cmake git pkgconfig which boost-devel \
     perl python3 curl ca-certificates tar xz bzip2 unzip \
     && dnf clean all

# ---- 启用 PowerTools（某些包如 clang-tools-extra 可能需要）----
RUN (dnf config-manager --set-enabled PowerTools || dnf config-manager --set-enabled powertools) || true

# ---- 安装 clang/clangd（clangd 在 clang-tools-extra 包内）----
RUN dnf -y install clang clang-tools-extra \
     && dnf clean all \
     && ln -sf /usr/bin/clangd /usr/local/bin/clangd

# 让 pkg-config / 运行时能找到我们安装到 /usr/local 的库
ENV PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig:/usr/local/lib/pkgconfig
ENV LD_LIBRARY_PATH=/usr/local/lib64:/usr/local/lib

# ---- 安装 fmt 9.1.0（C++11 友好）----
RUN git clone --depth 1 --branch 9.1.0 https://github.com/fmtlib/fmt.git /tmp/fmt \
     && cmake -S /tmp/fmt -B /tmp/fmt/build \
     -DFMT_TEST=OFF -DFMT_DOC=OFF -DFMT_INSTALL=ON -DBUILD_SHARED_LIBS=ON \
     && cmake --build /tmp/fmt/build -j"$(nproc)" \
     && cmake --install /tmp/fmt/build \
     && rm -rf /tmp/fmt

# ---- 安装 GoogleTest 1.10.0（C++11）----
RUN git clone --depth 1 --branch release-1.10.0 https://github.com/google/googletest.git /tmp/googletest \
     && cmake -S /tmp/googletest -B /tmp/googletest/build \
     -DBUILD_GMOCK=OFF -DBUILD_GTEST=ON -DCMAKE_CXX_STANDARD=11 -DBUILD_SHARED_LIBS=ON \
     && cmake --build /tmp/googletest/build -j"$(nproc)" \
     && cmake --install /tmp/googletest/build \
     && rm -rf /tmp/googletest

# ---- 安装 Google Benchmark v1.8.3（仍以 C++11 构建）----
# 关闭其自测/依赖，避免不必要的 GTest/LibPFM 约束；安装共享库与 CMake config
RUN git clone --depth 1 --branch v1.8.3 https://github.com/google/benchmark.git /tmp/benchmark \
     && cmake -S /tmp/benchmark -B /tmp/benchmark/build \
     -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=11 \
     -DBENCHMARK_ENABLE_TESTING=OFF -DBENCHMARK_ENABLE_GTEST_TESTS=OFF \
     -DBENCHMARK_ENABLE_LIBPFM=OFF -DBUILD_SHARED_LIBS=ON \
     && cmake --build /tmp/benchmark/build -j"$(nproc)" \
     && cmake --install /tmp/benchmark/build \
     && rm -rf /tmp/benchmark

# ---- 安装 lcov（覆盖率工具，Perl 脚本，轻量）----
RUN git clone --depth 1 --branch v1.15 https://github.com/linux-test-project/lcov.git /tmp/lcov \
     && make -C /tmp/lcov install \
     && rm -rf /tmp/lcov

CMD ["/bin/bash"]
