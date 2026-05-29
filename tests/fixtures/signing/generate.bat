@echo off
:: Prefer MSYS2 ucrt64 openssl (3.6.x) over Git-bundled openssl (may be older)
set "PATH=C:\msys64\ucrt64\bin;C:\Program Files\Git\usr\bin;%PATH%"
cd tests\fixtures\signing
:: Generate test_input.pdf via Python for byte-accurate xref offsets
:: (batch echo produces incorrect offsets that cause PoDoFo xref validation failures)
python generate_test_input.py

openssl req -x509 -newkey rsa:2048 -keyout ca.key -out test_ca.pem -days 3650 -nodes -subj "/CN=TestCA"
openssl req -newkey rsa:2048 -keyout signer.key -out signer.csr -nodes -subj "/CN=TestSigner"
echo extendedKeyUsage=emailProtection> ext.cnf
openssl x509 -req -in signer.csr -CA test_ca.pem -CAkey ca.key -CAcreateserial -out signer.crt -days 365 -extfile ext.cnf
openssl pkcs12 -export -in signer.crt -inkey signer.key -certfile test_ca.pem -out test_signer.p12 -passout pass:test

:: ---- Adversarial fixtures for M2-PROMPT-4 ----

:: expired_cert.p12 — cert with NotAfter in the past (already expired)
:: Note: -days -1 is rejected by OpenSSL 3.x; use explicit -not_before/-not_after
openssl genrsa -out expired.key 2048
openssl req -new -key expired.key -out expired.csr -subj "//CN=Expired Signer\O=Test"
openssl x509 -req -in expired.csr -CA test_ca.pem -CAkey ca.key -CAserial test_ca.srl -out expired.crt -sha256 -extfile ext.cnf -not_before 20200101000000Z -not_after 20200102000000Z
openssl pkcs12 -export -in expired.crt -inkey expired.key -certfile test_ca.pem -out expired_cert.p12 -passout pass:test -name "ExpiredSigner"

:: weak_cert_rsa1024.p12 — deliberately weak RSA-1024 key
openssl genrsa -out weak.key 1024
openssl req -new -key weak.key -out weak.csr -subj "//CN=Weak Signer\O=Test"
openssl x509 -req -in weak.csr -CA test_ca.pem -CAkey ca.key -CAserial test_ca.srl -out weak.crt -days 365 -sha256 -extfile ext.cnf
openssl pkcs12 -export -in weak.crt -inkey weak.key -certfile test_ca.pem -out weak_cert_rsa1024.p12 -passout pass:test -name "WeakSigner"

:: revoked_cert.p12 — cert that will be flagged as revoked in a fake OCSP response
openssl genrsa -out revoked.key 2048
openssl req -new -key revoked.key -out revoked.csr -subj "//CN=Revoked Signer\O=Test"
openssl x509 -req -in revoked.csr -CA test_ca.pem -CAkey ca.key -CAserial test_ca.srl -out revoked.crt -days 365 -sha256 -extfile ext.cnf
openssl pkcs12 -export -in revoked.crt -inkey revoked.key -certfile test_ca.pem -out revoked_cert.p12 -passout pass:test -name "RevokedSigner"

:: revoked_ocsp_response.der — minimal DER-encoded OCSP response stub (revoked status)
:: Generated via Python helper for portability across OpenSSL versions
python generate_revoked_ocsp.py
