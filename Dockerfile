FROM almalinux:8@sha256:4a87d2615a770506e204c27d6248ac97f4df67f4e41e2e9c47c81f0ed0be98cb

# AlmaLinux 8 keeps the EL8 ABI and GCC 8.5 compatibility baseline without
# depending on the retired CentOS 8 vault repositories.
RUN dnf -y clean all && dnf -y makecache

# ---- 基础 Linux/GCC 开发环境 ----
RUN dnf -y update && dnf -y install \
     gcc gcc-c++ make cmake git pkgconfig which boost-devel \
     perl python3 curl ca-certificates tar xz bzip2 unzip \
     && dnf clean all

# ---- 启用 PowerTools/CRB（某些包如 clang-tools-extra 可能需要）----
RUN (dnf config-manager --set-enabled powertools || dnf config-manager --set-enabled crb) || true

# ---- 安装 clang/clangd（开发辅助工具；默认构建仍使用 GCC/g++）----
RUN dnf -y install clang clang-tools-extra \
     && dnf clean all \
     && ln -sf /usr/bin/clangd /usr/local/bin/clangd

# 让 pkg-config / 运行时能找到我们安装到 /usr/local 的库
ENV PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig:/usr/local/lib/pkgconfig
ENV LD_LIBRARY_PATH=/usr/local/lib64:/usr/local/lib

# ---- 安装 fmt 9.1.0（C++11 友好）----
ARG FMT_COMMIT=a33701196adfad74917046096bf5a2aa0ab0bb50
RUN git clone --depth 1 --branch 9.1.0 https://github.com/fmtlib/fmt.git /tmp/fmt \
     && test "$(git -C /tmp/fmt rev-parse HEAD)" = "$FMT_COMMIT" \
     && cmake -S /tmp/fmt -B /tmp/fmt/build \
     -DFMT_TEST=OFF -DFMT_DOC=OFF -DFMT_INSTALL=ON -DBUILD_SHARED_LIBS=ON \
     && cmake --build /tmp/fmt/build -j"$(nproc)" \
     && cmake --install /tmp/fmt/build \
     && rm -rf /tmp/fmt

# ---- 安装 GoogleTest 1.10.0（C++11）----
ARG GOOGLETEST_COMMIT=703bd9caab50b139428cea1aaff9974ebee5742e
RUN git clone --depth 1 --branch release-1.10.0 https://github.com/google/googletest.git /tmp/googletest \
     && test "$(git -C /tmp/googletest rev-parse HEAD)" = "$GOOGLETEST_COMMIT" \
     && cmake -S /tmp/googletest -B /tmp/googletest/build \
     -DBUILD_GMOCK=OFF -DBUILD_GTEST=ON -DCMAKE_CXX_STANDARD=11 -DBUILD_SHARED_LIBS=ON \
     && cmake --build /tmp/googletest/build -j"$(nproc)" \
     && cmake --install /tmp/googletest/build \
     && rm -rf /tmp/googletest

# ---- 安装 Google Benchmark v1.9.5（benchmark 工具链使用 C++17；库本体仍以 C++11 验证）----
# 关闭其自测/依赖，避免不必要的 GTest/LibPFM 约束；安装共享库与 CMake config
ARG BENCHMARK_COMMIT=192ef10025eb2c4cdd392bc502f0c852196baa48
RUN git clone --depth 1 --branch v1.9.5 https://github.com/google/benchmark.git /tmp/benchmark \
     && test "$(git -C /tmp/benchmark rev-parse HEAD)" = "$BENCHMARK_COMMIT" \
     && cmake -S /tmp/benchmark -B /tmp/benchmark/build \
     -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 \
     -DBENCHMARK_ENABLE_TESTING=OFF -DBENCHMARK_ENABLE_GTEST_TESTS=OFF \
     -DBENCHMARK_ENABLE_LIBPFM=OFF -DBUILD_SHARED_LIBS=ON \
     && cmake --build /tmp/benchmark/build -j"$(nproc)" \
     && cmake --install /tmp/benchmark/build \
     && rm -rf /tmp/benchmark

# ---- 安装 lcov（覆盖率工具，Perl 脚本，轻量）----
ARG LCOV_COMMIT=d100e6cdd4c67cbe5322fa26b2ee8aa34ea7ebcf
RUN git clone --depth 1 --branch v1.15 https://github.com/linux-test-project/lcov.git /tmp/lcov \
     && test "$(git -C /tmp/lcov rev-parse HEAD)" = "$LCOV_COMMIT" \
     && make -C /tmp/lcov install \
     && rm -rf /tmp/lcov

CMD ["/bin/bash"]
