# Contributing

Thanks for your interest. The project is pre-alpha; please open an issue before a large change.

- Contributions must keep the safety boundary in [docs/safety.md](docs/safety.md): no order,
  position, or trade-account code, and every chart mutation a separately reviewed tool.
- Build and test as described in [README.md](README.md) and [AGENTS.md](AGENTS.md). CI must pass,
  including `scripts/check-no-trading.ps1`.
- Keep `native/core` free of `sierrachart.h` so it stays unit-testable.
