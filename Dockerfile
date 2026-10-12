# Build stage : compiles finton and downloads ONNX Runtime
FROM ubuntu:24.04 AS build
RUN apt-get update \
 && apt-get install -y --no-install-recommends g++ cmake make ca-certificates \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY CMakeLists.txt ./
COPY src ./src
COPY tests ./tests
COPY tools ./tools
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DFINTON_TSAN=OFF \
 && cmake --build build --target finton loadgen -j"$(nproc)"
RUN mkdir -p /out/lib \
 && cp build/finton build/loadgen /out/ \
 && cp -P build/_deps/onnxruntime-src/lib/libonnxruntime.so* /out/lib/

# Runtime stage : the two binaries, the ONNX Runtime library and the example model
FROM ubuntu:24.04
RUN useradd --system --no-create-home finton
WORKDIR /opt/finton
COPY --from=build /out/ ./
COPY models/mnist ./models/mnist
ENV LD_LIBRARY_PATH=/opt/finton/lib
USER finton
EXPOSE 3001
ENTRYPOINT ["/opt/finton/finton"]
