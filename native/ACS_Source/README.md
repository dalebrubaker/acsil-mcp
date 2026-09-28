# ACS_Source

A verbatim copy of Sierra Chart's `<SierraChart>\ACS_Source` folder: the ACSIL headers
(`sierrachart.h` and the headers it includes) and the sample studies and trading systems
(`*.cpp`). They are Sierra Chart's files, shipped in every Sierra Chart installation. Do not
modify them.

Only the headers are used by the build. The `.cpp` files are reference material for how ACSIL is
used. They include order-entry calls, which acsil-mcp itself must never make (`docs/safety.md`);
`scripts/check-no-trading.ps1` scans only `native/core` and `native/adapter`.

To refresh, copy the whole folder from an up-to-date installation:

```powershell
Copy-Item C:\SierraChart\ACS_Source\* native\ACS_Source\ -Recurse -Force
```

The DLL records the header version it was built with (`SC_DLL_VERSION` in `sierrachart.h`).
To build against another folder without copying, set `AcsSourceDir` in
`native\AcsilMcp.user.props`.
