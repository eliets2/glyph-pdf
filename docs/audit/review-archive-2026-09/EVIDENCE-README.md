# Audit evidence

Repository: C:/Users/User/Projects/pdf
Commit: 703fa34ece32733ea3b2093da94fa1aed94e1afc

The probes use synthetic documents only. `probe_engine.cpp` intentionally exercises a same-file form save that can destroy its generated `engine-probe-data/form.pdf`. It never edits repository inputs or user PDFs. These are diagnostic probes: exit 0 means the probe completed, not that the product behavior was correct.

Included:

- `configure.log`, `build.log`: fresh build evidence.
- `ctest-final.log`: final 84/84 target run.
- `probe_preprocess.log`: image polarity, deskew, and pure endpoint-validation results. No network request is made.
- `probe_engine.log`: readable source text versus exported bytes, appended-page comparison, and form-save failure.
- `probe_engine_elevated.log`: independent confirmation outside sandbox file virtualization; its earlier version also attempted an additional form setup and exited after that setup failed. The completed probe is in `probe_engine.log`.
- C++ probe sources and Python helpers.

To reproduce on the audited machine, extract these files into a scratch directory. Put C:/msys64/ucrt64/bin first on PATH. Configure and build the repository into `build-current` beneath that directory:

```powershell
cmake -S C:/Users/User/Projects/pdf -B build-current -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-current --parallel 4
```

For the two Djot tests, copy C:/Users/User/Projects/pdf/third_party/djot to `third_party/djot` beside `build-current`. This supplies the sibling fixture directory assumed by those tests. Run `python run_tests.py` with access to the test namespaces in Windows settings. The helper normalizes environment-variable key casing before invoking CTest.

Run `python compile_probe.py` for the engine probe. It derives linker libraries from the fresh Ninja build rather than substituting implementations.

For the OCR and endpoint probe:

```powershell
$auditFlags = ((pkg-config --cflags --libs Qt6Gui Qt6Concurrent Qt6Network lept) -split ' ') | Where-Object { $_ }
g++ -std=c++17 -DHAS_TESSERACT -IC:/Users/User/Projects/pdf/src probe_preprocess.cpp C:/Users/User/Projects/pdf/src/engines/ocr/OcrPreprocessor.cpp -o probe_preprocess.exe @auditFlags
$env:QT_QPA_PLATFORM='offscreen'
.\probe_preprocess.exe
```

`run_tests.py` and `compile_probe.py` resolve their scratch directory from their own location. The recorded logs came from the original audit workspace. The full report distinguishes runtime reproductions from code-traced findings.
