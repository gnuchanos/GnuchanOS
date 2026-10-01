                         X11 ROOT / DESKTOP
                    ┌──────────────────────────┐
                    │        1366 × 768       │
                    │                          │
                    │   ┌──────────────────┐   │
                    │   │   CONTAINER #1   │   │
                    │   │                  │   │
                    │   │    TERMINAL      │   │
                    │   │    800 × 600     │   │
                    │   │                  │   │
                    │   └──────────────────┘   │
                    │                          │
                    │   ┌──────────────────┐   │
                    │   │   CONTAINER #2   │   │
                    │   │                  │   │
                    │   │     WINE GAME    │   │
                    │   │    1024 × 768    │   │
                    │   │                  │   │
                    │   └────────┬─────────┘   │
                    │            │             │
                    └────────────┼─────────────┘
                                 │
                    Wine requests 1280 × 720
                                 │
                                 ▼
                    ┌─────────────────────────┐
                    │      WM CONTAINER       │
                    │                         │
                    │   resize Container #2   │
                    │        ↓                │
                    │      1280 × 720         │
                    │                         │
                    └─────────────────────────┘

              ROOT / DESKTOP: 1366 × 768  → DEĞİŞMEZ
              CONTAINER #1:    800 × 600  → bağımsız
              CONTAINER #2:   1280 × 720  → bağımsız
              CONTAINER #3:   ...          → bağımsız


        ┌─────────────────────────────────────────────────┐
        │                    WINDOW MANAGER                │
        │                                                 │
        │   X11 Client                                    │
        │       │                                         │
        │       ▼                                         │
        │   ┌───────────────┐                             │
        │   │   Container   │ ← geometry / resize        │
        │   └───────┬───────┘                             │
        │           │                                     │
        │           ▼                                     │
        │      Client Window                              │
        │                                                 │
        │   Her pencere = bağımsız container              │
        │   Container resize ≠ root/desktop resize        │
        └─────────────────────────────────────────────────┘