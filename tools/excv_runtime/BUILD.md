# MGO2MT EXCV build

This isolated source set is the converter runtime. Historical analysis-only command entry points are omitted; the rest of the project tools are not replaced.

Tested with 64-bit official Python 3.14, including Tcl/Tk. From the repository root:

```powershell
python -m venv .excv-build-env
.excv-build-env/Scripts/python.exe -m pip install -r tools/excv_runtime/requirements-build.txt
.excv-build-env/Scripts/python.exe tools/build_excv.py --output outputs/excv-build --build-version v0.01-20260921232941
.excv-build-env/Scripts/python.exe tools/package_excv.py outputs/excv-software --build outputs/excv-build
```

No original game data is needed or accepted by the software build. The converter reads originals only when the user selects them. `source-manifest.json` verifies the public source inventory before freezing. Licenses are taken from the actual build interpreter and dependencies, with OpenSSL notices in `tools/excv_licenses`.

Original source assets, communication data, and original machine-code headers are not part of this source set. Resource recipes contain native settings, format metadata, selectors and hashes, not model/image/audio/font payloads. The frozen GUI can be tested with `MGO2MTEXCV.exe --smoke-gui report.json`.
