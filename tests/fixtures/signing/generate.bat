@echo off
set "PATH=C:\Program Files\Git\usr\bin;%PATH%"
cd tests\fixtures\signing
echo %%PDF-1.4> test_input.pdf
echo 1 0 obj>> test_input.pdf
echo ^<^< /Type /Catalog /Pages 2 0 R ^>^>>> test_input.pdf
echo endobj>> test_input.pdf
echo 2 0 obj>> test_input.pdf
echo ^<^< /Type /Pages /Kids [3 0 R] /Count 1 ^>^>>> test_input.pdf
echo endobj>> test_input.pdf
echo 3 0 obj>> test_input.pdf
echo ^<^< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources ^<^<^>^> ^>^>>> test_input.pdf
echo endobj>> test_input.pdf
echo xref>> test_input.pdf
echo 0 4>> test_input.pdf
echo 0000000000 65535 f>> test_input.pdf
echo 0000000009 00000 n>> test_input.pdf
echo 0000000058 00000 n>> test_input.pdf
echo 0000000115 00000 n>> test_input.pdf
echo trailer>> test_input.pdf
echo ^<^< /Size 4 /Root 1 0 R ^>^>>> test_input.pdf
echo startxref>> test_input.pdf
echo 200>> test_input.pdf
echo %%%%EOF>> test_input.pdf

openssl req -x509 -newkey rsa:2048 -keyout ca.key -out test_ca.pem -days 3650 -nodes -subj "/CN=TestCA"
openssl req -newkey rsa:2048 -keyout signer.key -out signer.csr -nodes -subj "/CN=TestSigner"
echo extendedKeyUsage=emailProtection> ext.cnf
openssl x509 -req -in signer.csr -CA test_ca.pem -CAkey ca.key -CAcreateserial -out signer.crt -days 365 -extfile ext.cnf
openssl pkcs12 -export -in signer.crt -inkey signer.key -certfile test_ca.pem -out test_signer.p12 -passout pass:test
