# M2-PROMPT-4 Source Analysis B: Fixtures + Mock + CMakeLists

## 1. generate.bat: Full OpenSSL Command Sequence

**File:** `tests/fixtures/signing/generate.bat`

### PDF Fixture Creation (Lines 1-24)
Creates a minimal valid PDF-1.4 with proper xref table and trailer:
- **PDF structure:** Minimal 3-object PDF with Catalog → Pages → Page hierarchy
- **Method:** Pure batch echo to build PDF spec-compliant structure
- **Output:** `test_input.pdf` (357 bytes)

### OpenSSL CA Generation (Line 26)
```batch
openssl req -x509 -newkey rsa:2048 -keyout ca.key -out test_ca.pem -days 3650 -nodes -subj "/CN=TestCA"
```
- **Self-signed root CA**
- **RSA 2048-bit key**
- **Validity:** 10 years (3650 days)
- **No password on CA key** (`-nodes`)
- **Outputs:** `ca.key` (1704 bytes), `test_ca.pem` (1107 bytes)

### OpenSSL Signer Certificate Request (Line 27)
```batch
openssl req -newkey rsa:2048 -keyout signer.key -out signer.csr -nodes -subj "/CN=TestSigner"
```
- **RSA 2048-bit key**
- **Outputs:** `signer.key` (1704 bytes), `signer.csr` (891 bytes)

### Extension Config for Email Protection (Line 28)
```batch
echo extendedKeyUsage=emailProtection> ext.cnf
```
- **Extension file:** `ext.cnf` (34 bytes)
- **Purpose:** Marks certificate for email signature (PAdES compatibility)

### CA-Signed Signer Certificate (Line 29)
```batch
openssl x509 -req -in signer.csr -CA test_ca.pem -CAkey ca.key -CAcreateserial -out signer.crt -days 365 -extfile ext.cnf
```
- **Signs CSR with CA**
- **Validity:** 1 year (365 days)
- **Applies email protection extension**
- **Auto-creates serial file:** `test_ca.srl` (41 bytes)
- **Output:** `signer.crt` (1115 bytes)

### PKCS#12 Bundle Creation (Line 30)
```batch
openssl pkcs12 -export -in signer.crt -inkey signer.key -certfile test_ca.pem -out test_signer.p12 -passout pass:test
```
- **Bundles:** Signer cert + signer key + CA cert chain
- **Format:** PKCS#12 (.p12 / .pfx)
- **Password:** `"test"` (hardcoded, passed via `-passout`)
- **Output:** `test_signer.p12` (3363 bytes)

**Artifacts generated:**
- `ca.key`, `signer.key`, `signer.csr`, `signer.crt`, `ext.cnf`, `test_ca.srl` — intermediate files
- **Tracked:** `test_ca.pem`, `test_input.pdf`, `test_signer.p12` — core fixtures

---

## 2. .gitignore: Fixture Binary Patterns

**File:** `.gitignore` lines 66–71

```
# Signing fixtures generated binaries (script regenerates)
tests/fixtures/signing/*.p12
tests/fixtures/signing/*.key
tests/fixtures/signing/*.crt
tests/fixtures/signing/*.csr
tests/fixtures/signing/*.srl
```

### Status
- ✅ **Already gitignored:** All intermediate + final binaries
- ✅ **Pattern covers:** `.p12`, `.key`, `.crt`, `.csr`, `.srl`
- ✅ **Only `test_ca.pem` + `test_input.pdf` are tracked** (plain text / spec-compliant)

### For D1 fixture generation:
- New `.p12` + intermediate files **will auto-ignore** when added to `tests/fixtures/signing/`
- No need to modify `.gitignore`

---

## 3. How to Generate: Expired Certificate

**Goal:** `-days -1` or startdate in past (expired immediately upon creation)

### Option A: OpenSSL `-not_after` in past (direct, 3.x+)
```bash
openssl x509 -req -in signer.csr \
  -CA test_ca.pem -CAkey ca.key \
  -out expired_cert.crt \
  -set_serial 2 \
  -days -1
  # OR: -not_after "2020-01-01T00:00:00Z"
```

### Option B: Modify system date (not practical)
Not portable, avoided.

### Option C: Post-generate (use Python + OpenSSL.crypto)
```python
from OpenSSL import crypto
cert = crypto.load_certificate(crypto.FILETYPE_PEM, open('signer.crt').read())
cert.set_notAfter(b'20200101000000Z')  # Expired date
crypto.dump_certificate(crypto.FILETYPE_PEM, cert)
```

### Recommended for D1:
Use **Option A** with `-days -1`, or invoke `openssl.exe` in CMakeLists to generate at configure time.

---

## 4. How to Generate: Revoked Certificate + OCSP Response

### Revoked Cert (CRL Approach)
1. Create CA Certificate Revocation List (CRL):
```bash
openssl ca -config ca.cnf -gencrl -out revoked.crl
```

2. Revoke a cert (requires CA index):
```bash
openssl ca -config ca.cnf -revoke signer.crt
openssl ca -config ca.cnf -gencrl -out revoked.crl
```

### OCSP Response (Fake, Non-Standard)
OCSP responder requires a full OCSP-responder cert and signed response. For **test fixtures**:

**Simplest approach:** Embed revoked status in test code
```cpp
// In TestSignatureRealCrypto::testRevokedCertificate()
// Fake the OCSP response; don't actually generate it
SignatureInfo sig = manager->validateSignatures("revoked.pdf");
EXPECT_EQ(sig.status, ValidationStatus::REVOKED);
```

**If real OCSP fixture needed:**
```bash
openssl ocsp -no_nonce \
  -issuer test_ca.pem \
  -cert signer.crt \
  -CAfile test_ca.pem \
  -responder_key ca.key \
  -responder_cert responder.crt \
  -out ocsp_response.der
```
(Requires `responder.crt` signed by CA with OCSP signing purpose.)

---

## 5. How to Generate: Weak RSA-1024 Certificate

```bash
# Generate 1024-bit signer key
openssl req -newkey rsa:1024 -keyout weak_signer.key -out weak_signer.csr -nodes \
  -subj "/CN=WeakSigner"

# Sign with CA (same extension logic as line 29)
echo extendedKeyUsage=emailProtection > ext.cnf
openssl x509 -req -in weak_signer.csr \
  -CA test_ca.pem -CAkey ca.key \
  -out weak_signer.crt \
  -days 365 \
  -extfile ext.cnf

# Bundle into PKCS#12
openssl pkcs12 -export -in weak_signer.crt -inkey weak_signer.key \
  -certfile test_ca.pem \
  -out weak_cert_rsa1024.p12 \
  -passout pass:test
```

**Output:** `weak_cert_rsa1024.p12` (3KB, gitignored)

---

## 6. How to Generate: Tampered PDF

**Goal:** Sign a PDF, then flip 1 byte in the signature value (invalidate signature without breaking PDF structure)

### Approach 1: Python binary patch
```python
import hashlib

# Sign the PDF with SignatureManager
manager.signDocument('test_input.pdf', 'signed.pdf', 'test_signer.p12', 'test', 'reason', 'location')

# Load signed PDF, find /V (signature value), flip 1 byte
with open('signed.pdf', 'rb') as f:
    data = bytearray(f.read())

# Find hex string "/V <" and flip 1 byte in the value
idx = data.find(b'/V ')
if idx > 0:
    # Find the < and corrupt a byte
    v_start = data.find(b'<', idx)
    if v_start > 0:
        data[v_start + 10] ^= 0xFF  # Flip byte 10 positions after '<'

with open('tampered.pdf', 'wb') as f:
    f.write(data)
```

### Approach 2: Use `signpdf` + manual hex edit
Less portable; Python approach preferred.

---

## 7. How to Generate: Multi-Signature PDF

**Goal:** Sign once, then sign the same PDF again with a different signature

```cpp
// In C++ test or fixture builder:
SignatureManager manager;

// First signature
manager.signDocument("test_input.pdf", "once_signed.pdf", "test_signer.p12", "test", "Signer1", "Location1");

// Second signature (sign the already-signed PDF)
manager.signDocument("once_signed.pdf", "multi_sig.pdf", "test_signer.p12", "test", "Signer2", "Location2");

// Result: PDF with 2 independent signature dictionaries
```

**Or during test:**
```cpp
QCOMPARE(manager.validateSignatures("multi_sig.pdf").size(), 2);
```

---

## 8. MockSignatureManager: Interface Methods

**File:** `tests/mocks/MockSignatureManager.h` (44 lines)

### Inherited from `ISignatureManager`

| Method | Parameters | Return | Mocked Behavior |
|--------|-----------|--------|-----------------|
| `setTsaUrl` | `const QString &url` | `void` | Stores `m_tsaUrl`; no validation |
| `setSignatureLevel` | `PAdESLevel level` | `void` | Stores `m_level` (default: `B_T`) |
| `signDocument` | `inputPath`, `outputPath`, `certPath`, `password`, `reason`, `location` | `bool` | Captures all params in `m_last*` fields; increments `m_signCalls`; returns `m_signResult` |
| `validateSignatures` | `const QString &filePath` | `QList<SignatureInfo>` | Captures `filePath` in `m_lastInputPath`; increments `m_validateCalls`; returns `m_signatures` |

### State + Call Tracking

| Field | Type | Default | Purpose |
|-------|------|---------|---------|
| `m_signResult` | `bool` | `true` | Controllable return from `signDocument()` |
| `m_signatures` | `QList<SignatureInfo>` | empty | Controllable return from `validateSignatures()` |
| `m_level` | `PAdESLevel` | `B_T` | Last set signature level |
| `m_signCalls` | `int` | 0 | Count of `signDocument()` invocations |
| `m_validateCalls` | `int` | 0 | Count of `validateSignatures()` invocations |
| `m_lastInputPath` | `QString` | — | Last path passed to `signDocument()` or `validateSignatures()` |
| `m_lastOutputPath` | `QString` | — | Last output path in `signDocument()` |
| `m_lastCertPath` | `QString` | — | Last cert path in `signDocument()` |
| `m_lastPassword` | `QString` | — | Last password in `signDocument()` |
| `m_lastReason` | `QString` | — | Last reason in `signDocument()` |
| `m_lastLocation` | `QString` | — | Last location in `signDocument()` |
| `m_tsaUrl` | `QString` | — | Last TSA URL set |

### Usage Pattern (from tests)
```cpp
MockSignatureManager mock;

// Set return values
mock.m_signResult = false;  // Simulate sign failure
mock.m_signatures = { SignatureInfo{/* ... */} };

// Call methods
mock.signDocument("input.pdf", "output.pdf", "cert.p12", "pass", "Reason", "Location");

// Verify calls
EXPECT_EQ(mock.m_signCalls, 1);
EXPECT_EQ(mock.m_lastCertPath, "cert.p12");
EXPECT_EQ(mock.validateSignatures("test.pdf").size(), 1);
```

**No dependencies:** MockSignatureManager has **no includes beyond `ISignatureManager.h` + Qt headers** — highly portable for unit tests.

---

## 9. CMakeLists.txt: Test Registration Blocks

### TestSignatureValidation (Lines 474–489)

```cmake
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSignatureValidation.cpp")
    add_executable(TestSignatureValidation
        tests/TestSignatureValidation.cpp
    )
    target_include_directories(TestSignatureValidation PRIVATE src tests)
    target_link_libraries(TestSignatureValidation PRIVATE
        pdfws_core
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_test(NAME TestSignatureValidation COMMAND TestSignatureValidation)
    set_tests_properties(TestSignatureValidation PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;security;signatures;qt;headless"
    )
endif()
```

**Key points:**
- **Unit test only** (core interface, no crypto)
- **Libs:** `pdfws_core`, Qt6 test + base libs
- **No OpenSSL, no podofo**
- **Timeout:** 60s
- **Labels:** unit, security, signatures, qt, headless

---

### TestSignatureRealCrypto (Lines 491–510)

```cmake
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSignatureRealCrypto.cpp")
    add_executable(TestSignatureRealCrypto
        tests/TestSignatureRealCrypto.cpp
    )
    target_include_directories(TestSignatureRealCrypto PRIVATE src tests)
    target_link_libraries(TestSignatureRealCrypto PRIVATE
        pdfws_engines pdfws_core
        OpenSSL::SSL OpenSSL::Crypto
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets podofo::podofo
    )
    if(WIN32)
        target_link_libraries(TestSignatureRealCrypto PRIVATE Crypt32)
    endif()
    add_test(NAME TestSignatureRealCrypto COMMAND TestSignatureRealCrypto)
    set_tests_properties(TestSignatureRealCrypto PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "integration;security;signatures;crypto;qt;headless"
    )
endif()
```

**Key points:**
- **Integration test** (real signing engine)
- **Libs:** `pdfws_engines`, `pdfws_core`, OpenSSL, podofo
- **Windows-specific:** Crypt32 (Windows certificate store)
- **Timeout:** 120s (longer; actual crypto ops)
- **Labels:** integration, security, signatures, crypto, qt, headless

---

## 10. Plan for D1 (Fixture Generation) + D3 (Mock Migration)

### D1: Fixture Generation
1. **Add new generate script** (e.g., `generate_extended.bat` or extend existing `generate.bat`)
   - Expired cert: `openssl x509 -req ... -days -1 ... -out expired_cert.crt`
   - Weak RSA-1024: `openssl req -newkey rsa:1024 ... && openssl pkcs12 -export ... -out weak_cert_rsa1024.p12`
   - Multi-signature PDF: Two sequential calls to `signDocument()` in fixture builder
   - Tampered PDF: Python post-processing of signed PDF (flip 1 byte in `/V` value)

2. **All outputs in `tests/fixtures/signing/`** — auto-gitignored by existing patterns

3. **Update CMakeLists** (optional): Add custom target to regenerate at configure time
   ```cmake
   add_custom_target(regenerate_signing_fixtures
       COMMAND generate_extended.bat
       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
   )
   ```

### D3: Mock Migration
1. **Create `TestSignatureValidationMock` target** in CMakeLists (copy of TestSignatureValidation but uses MockSignatureManager)
   ```cmake
   if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSignatureValidationMock.cpp")
       add_executable(TestSignatureValidationMock
           tests/TestSignatureValidationMock.cpp
       )
       # Same libs, same config as TestSignatureValidation
       # Test file uses #include "mocks/MockSignatureManager.h"
   endif()
   ```

2. **Rename current `TestSignatureValidation.cpp`** → `TestSignatureRealValidation.cpp` (or add new mock variant)

3. **Both variants coexist:**
   - `TestSignatureValidationMock` — lightweight unit test (MockSignatureManager)
   - `TestSignatureRealCrypto` — integration test (real signing engine + fixtures)
   - Remove or retire old `TestSignatureValidation` or repurpose with mock

### Fixture Path for Tests
```cpp
const QString FIXTURE_DIR = QStringLiteral("tests/fixtures/signing");
const QString TEST_SIGNER_P12 = FIXTURE_DIR + "/test_signer.p12";
const QString TEST_CA_PEM = FIXTURE_DIR + "/test_ca.pem";
const QString EXPIRED_CERT_P12 = FIXTURE_DIR + "/expired_cert.p12";
const QString WEAK_CERT_P12 = FIXTURE_DIR + "/weak_cert_rsa1024.p12";
```

---

## Summary: Ready for Implementation

| Artifact | Status | Location |
|----------|--------|----------|
| **generate.bat** | ✅ Analyzed; OpenSSL commands clear | `tests/fixtures/signing/generate.bat` |
| **Expired cert** | 📋 Plan: `-days -1` or OpenSSL 3.x `-not_after` | To be generated |
| **Weak RSA-1024** | 📋 Plan: `openssl req -newkey rsa:1024` | To be generated |
| **Tampered PDF** | 📋 Plan: Python binary patch post-sign | To be generated |
| **Multi-sig PDF** | 📋 Plan: Sequential sign calls | To be generated |
| **MockSignatureManager** | ✅ Complete interface, 8 methods + state fields | `tests/mocks/MockSignatureManager.h` |
| **.gitignore** | ✅ Already covers `.p12`, `.key`, `.crt`, `.csr`, `.srl` | `.gitignore` lines 66–71 |
| **CMakeLists** | ✅ TestSignatureValidation (unit) + TestSignatureRealCrypto (integration) | `CMakeLists.txt` lines 474–510 |
| **TestSignatureValidationMock target** | 📋 Plan: New target, copy of TestSignatureValidation with mock | To be added to CMakeLists |

