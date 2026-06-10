"""
generate_revoked_ocsp.py - emit a real, properly-encoded OCSP response that
reports REVOKED for the revoked_cert.p12 signer certificate.

Unlike the earlier hand-rolled DER stub (which hardcoded serial=1 and all-zero
issuer hashes, so the SignatureManager certID match never fired), this builds
the response with pyca/cryptography from the actual CA + revoked certificate.
The CertID therefore carries the revoked cert's real serial number, which is
what SignatureManager.extractOcspFromDss matches against (serial-number compare,
ASN1_INTEGER_cmp). The response is signed by the CA key.

The signature does not need to pass OCSP_basic_verify for the revoked-status
path (that path only reads the singleResponse status after a serial match), but
we sign it properly anyway so the fixture is a faithful OCSP response.

Run:  python generate_revoked_ocsp.py
Deps: cryptography (pip install cryptography)
"""
import os
import datetime
from cryptography import x509
from cryptography.x509 import ocsp
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.serialization import pkcs12

HERE = os.path.dirname(os.path.abspath(__file__))


def _load_pem_cert(name):
    with open(os.path.join(HERE, name), "rb") as f:
        return x509.load_pem_x509_certificate(f.read())


def _load_pem_key(name):
    with open(os.path.join(HERE, name), "rb") as f:
        return serialization.load_pem_private_key(f.read(), password=None)


def main():
    # CA cert + key (the OCSP responder, signing the response).
    ca_cert = _load_pem_cert("test_ca.pem")
    ca_key = _load_pem_key("ca.key")

    # The revoked end-entity certificate (its serial drives the CertID match).
    revoked_cert = _load_pem_cert("revoked.crt")

    now = datetime.datetime.now(datetime.timezone.utc)

    builder = ocsp.OCSPResponseBuilder()
    builder = builder.add_response(
        cert=revoked_cert,
        issuer=ca_cert,
        algorithm=hashes.SHA1(),  # CertID hash algorithm (standard for OCSP)
        cert_status=ocsp.OCSPCertStatus.REVOKED,
        this_update=now - datetime.timedelta(hours=1),
        next_update=now + datetime.timedelta(days=7),
        revocation_time=now - datetime.timedelta(days=1),
        revocation_reason=x509.ReasonFlags.key_compromise,
    ).responder_id(ocsp.OCSPResponderEncoding.NAME, ca_cert)

    response = builder.sign(ca_key, hashes.SHA256())

    out_path = os.path.join(HERE, "revoked_ocsp_response.der")
    with open(out_path, "wb") as f:
        f.write(response.public_bytes(serialization.Encoding.DER))

    der_len = os.path.getsize(out_path)
    serial = revoked_cert.serial_number
    print(f"Written {der_len} bytes to {out_path}")
    print(f"CertID serial = {serial} (0x{serial:x}) - matches revoked.crt")
    print("Status = REVOKED, signed by test CA.")


if __name__ == "__main__":
    main()
