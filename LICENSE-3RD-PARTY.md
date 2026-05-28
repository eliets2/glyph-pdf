# Third-Party Licenses

This document lists the third-party dependencies used in GlyphPDF, along with their versions, licenses, and compatibility notes.

| Library | Version | License | Compatible | Compatibility Notes / Rationale |
| :--- | :--- | :--- | :---: | :--- |
| **Qt 6** | 6.11.0 (MSYS2 ucrt64) | LGPL-3.0 / commercial | **Yes** | Permitted under LGPL-3.0 dynamic linking rules. |
| **PoDoFo** | 0.10.4 (MSYS2 ucrt64) | LGPL-2.0+ OR MPL-2.0 | **Yes** | Permitted under LGPL/MPL dynamic linking rules. |
| **OpenSSL** | 3.6.2 (MSYS2 ucrt64) | Apache-2.0 | **Yes** | Permitted, Apache-2.0 license compatible. |
| **PDFium** | ANY (prebuilt) | BSD-3-Clause | **Yes** | Permitted, BSD-3-Clause is extremely permissive. |
| **qpdf** | 12.3.2 (MSYS2 ucrt64) | Apache-2.0 | **Yes** | Permitted, Apache-2.0 license compatible. |
| **Tesseract** | 5.5.2 (MSYS2 ucrt64) | Apache-2.0 | **Yes** | Permitted, Apache-2.0 license compatible. |
| **Leptonica** | 1.87.0 (MSYS2 ucrt64) | BSD-2-Clause | **Yes** | Permitted, BSD-2-Clause is extremely permissive. |
| **OpenXLSX** | 2025-07-14 | BSD-3-Clause | **Yes** | Permitted, BSD-3-Clause is extremely permissive. |
| **DuckX** | 1.2.2 | MIT | **Yes** | Permitted, MIT is extremely permissive. |
| **ONNX Runtime** | 1.17.3 | MIT | **Yes** | Permitted, MIT is extremely permissive. |
| **MuPDF** | ANY | **AGPL-3.0** | **FORBIDDEN** | **Incompatible.** Linking forces entire application to AGPL-3.0. |
| **Poppler** | ANY | **GPL-2.0+** | **FORBIDDEN** | **Incompatible.** Linking forces entire application to GPL. |
