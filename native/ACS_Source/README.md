# ACS_Source

Sierra Chart's ACSIL headers (`sierrachart.h` and the headers it includes) go here. They are
Sierra Chart's files, shipped in every Sierra Chart installation under `<SierraChart>\ACS_Source`.

To refresh them, copy the `.h` files from an up-to-date installation:

```powershell
Copy-Item C:\SierraChart\ACS_Source\*.h native\ACS_Source\
```

The DLL records the header version it was built with (`SC_DLL_VERSION` in `sierrachart.h`).
To build against another folder without copying, set `AcsSourceDir` in
`native\AcsilMcp.user.props`.
