# finton

A small inference server core in C++20, inspired by NVIDIA Triton Inference Server and aimed at ordinary CPUs.

finton takes requests from many clients, groups them into batches, and runs each batch on a pool of worker threads. It exists to study one question: how do batch size and waiting time trade latency against throughput?

This is a learning project. The core (queue, scheduler, workers) is written by hand. The build files, the load generator and the documentation were written with AI assistance.

## Status

Working today:

- a thread-safe, bounded request queue
- a scheduler that builds batches from two settings, a maximum batch size and a maximum delay
- a pool of worker threads that pull batches from the scheduler
- a response delivered to each client through a promise and a future
- a graceful shutdown that finishes every accepted request
- a load generator that reports throughput and latency percentiles

Not built yet:

- the HTTP front end (requests come from threads inside the program)
- a real model backend (the backend is simulated with a sleep)
- support for several models

## Build

You need CMake 3.22 or newer and a C++20 compiler. GCC 13 is recommended; see the note on ThreadSanitizer below.

There are two configurations. Use the first to check correctness and the second to measure.

```bash
# correctness: ThreadSanitizer on (the default)
cmake -S . -B build -DCMAKE_CXX_COMPILER=g++-13
cmake --build build

# timing: optimised, ThreadSanitizer off
cmake -S . -B build-release -DCMAKE_CXX_COMPILER=g++-13 -DCMAKE_BUILD_TYPE=Release -DFINTON_TSAN=OFF
cmake --build build-release
```

Timings from the first configuration mean nothing: the sanitizer makes the program 5 to 15 times slower.

### ThreadSanitizer and GCC 11

With GCC 11 the sanitized build does not work for this project. It can stop at startup with `unexpected memory mapping`, and it reports data races on the queue that are not real. Its runtime does not follow the mutex through a timed wait on a condition variable. GCC 13 does not have either problem.

## Run

```bash
./build-release/finton [workers] [batch_size] [delay_ms] [clients] [requests_per_client] [gap_ms]
```

| Argument | Default | Meaning |
|---|---|---|
| `workers` | 2 | number of worker threads |
| `batch_size` | 4 | maximum number of requests in a batch |
| `delay_ms` | 5 | longest time a request is held back to fill a batch |
| `clients` | 4 | number of client threads |
| `requests_per_client` | 3 | requests each client sends, one after the other |
| `gap_ms` | 0 | pause between two requests of the same client |

Each client sends a request, waits for its response, then sends the next one. A batch can therefore never be larger than the number of clients.

Example:

```
$ ./build-release/finton 2 8 5 16 100

--- settings
workers 2 | batch size 8 | delay 5 ms | clients 16 | requests/client 100 | gap 0 ms
--- results
completed  : 1600/1600  (rejected 0, dropped 0, wrong id 0)
wall time  : 1.90262 s
throughput : 840.943 requests/s
latency ms : p50 18.4806 | p95 21.5577 | p99 23.8739 | max 26.4779
```

The numbers depend on the simulated backend, which costs 10 ms per batch plus 1 ms per request. They show how the scheduler behaves, not how fast a real model would run.

The program exits with code 0 when every request was answered with the right id.

## Layout

```
src/header/    class declarations
  request.hpp    a request, its response, and the promise that links them
  queue.hpp      thread-safe bounded queue
  scheduler.hpp  batching policy
  workers.hpp    worker thread pool
src/lib/       implementations
src/main.cpp   load generator
design/        design document and diagram
```

## Design

[design/arch.md](design/arch.md) explains the life of a request and the reasons behind each design decision.

## Roadmap

1. HTTP front end
2. Real backend with ONNX Runtime
3. Measurements on a real model
4. Several models, each with its own scheduler
