# soffice `-env:` vs `--env:` A/B probes (2026-10-05)

Machine: Windows, LibreOffice at `C:\Program Files\LibreOffice\program\soffice.exe`.
Trigger: full serial gate — TestOfficeImport Timeout at 120.04 s (ctest TIMEOUT 120 ==
convertOfficeToPdf's internal 120000 ms default). Reproduced in isolation.

Controlled A/B (same fresh profile `lo-probe5-profile`, same RTF, seconds apart):

| Arg vector | Result |
|---|---|
| `-env:UserInstallation=file:///C:/Users/User/AppData/Local/Temp/lo-probe5-profile` (SINGLE dash) | **2.6 s, PDF produced** |
| `--env:UserInstallation=file:///C:/Users/User/AppData/Local/Temp/lo-probe5-profile` (DOUBLE dash, else identical) | **hang, killed at 45 s, no PDF** |

Bisect probes (all hung >60 s regardless of the other variables):
fresh mktemp profile + `pdf:writer_pdf_Export` filter + mixed (C:/) paths;
plain-name profile + filter + POSIX paths; plain-name profile + no filter suffix.
The 1-second control runs (lo-probe-profile, warm) used the SINGLE-dash form.
Only the dash count separates hang from success ⇒ `--env:UserInstallation=` is silently
ignored by soffice (bootstrap variables are documented single-dash), which then uses the
shared default profile — currently in a blocking state on this machine.

Conclusion: `ConversionManager::convertOfficeToPdf` never actually used its private
temp profile (`"--env:UserInstallation="` at src/engines/ConversionManager.cpp:655);
yesterday's green gate passed only because the default profile was healthy. Fixed to
`"-env:UserInstallation="` (one token) with an explanatory comment.
