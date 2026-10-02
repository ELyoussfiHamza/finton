

We imagine In the first version a simulation of GPU work 

Our goal is to build a system that can optimize the best tradeoffs and make the users ping pong between the traedoffs depending on his system

(See The photos of arch)

                    ┌─────────────────┐
Request ───────────►│  HTTP/gRPC API  │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │ Request Queue   │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │    Scheduler    │
                    └───────┬─────────┘
                            │
                 ┌──────────┼──────────┐
                 ▼          ▼          ▼
              Worker 0   Worker 1   Worker N
                 │          │          │
                 └──────────┼──────────┘
                            ▼
                    ┌─────────────────┐
                    │ Model Runtime   │
                    │ ONNX Runtime    │
                    └─────────────────┘

