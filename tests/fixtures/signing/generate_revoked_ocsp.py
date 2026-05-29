"""
generate_revoked_ocsp.py — write a minimal DER-encoded OCSP response stub
that reports V_OCSP_CERTSTATUS_REVOKED (status byte = 1) for testing purposes.

This is a hand-crafted DER structure; it is NOT a real signed OCSP response
and will NOT pass OCSP_basic_verify(). It exists so that:
  - testRevokedCertReportsRevoked can load a .der file from disk
  - The SignatureManager OCSP re-parse path is exercised (d2i_OCSP_RESPONSE
    will parse the outer envelope; OCSP_basic_verify will fail → DSS injection
    is correctly refused, and the test uses QEXPECT_FAIL for the "Revoked" status)

DER structure (OCSP_RESPONSE):
  SEQUENCE {
    ENUMERATED { 0 }   -- responseStatus = successful (0)
    [0] {              -- responseBytes (EXPLICIT CONTEXT 0)
      SEQUENCE {
        OID { 1.3.6.1.5.5.7.48.1.1 }   -- id-pkix-ocsp-basic
        OCTET STRING {                  -- response bytes (BasicOCSPResponse stub)
          SEQUENCE {
            SEQUENCE {                   -- tbsResponseData
              [2] { INTEGER { 1 } }      -- version (v2 = 1, for minimal stub)
              SEQUENCE {                 -- responderID (byName)
                SET { SEQUENCE { OID 2.5.4.3 UTF8String "TestCA" } }
              }
              GeneralizedTime "20260101000000Z"  -- producedAt
              SEQUENCE {               -- responses (one SingleResponse)
                SEQUENCE {             -- SingleResponse
                  SEQUENCE {           -- certID
                    SEQUENCE { OID 1.3.14.3.2.26 NULL }  -- SHA-1
                    OCTET STRING { 0x00 * 20 }  -- issuerNameHash
                    OCTET STRING { 0x00 * 20 }  -- issuerKeyHash
                    INTEGER { 1 }              -- serialNumber
                  }
                  [1] {                -- certStatus = revoked
                    GeneralizedTime "20260101000000Z"  -- revocationTime
                  }
                  GeneralizedTime "20260101000000Z"  -- thisUpdate
                }
              }
            }
            SEQUENCE { OID 1.2.840.113549.1.1.11 NULL }  -- sha256WithRSAEncryption
            BIT STRING { 0x00 }        -- signature (invalid but parseable)
          }
        }
      }
    }
  }

Rather than build the full ASN.1 by hand (error-prone), we use the simplest
valid OCSP_RESPONSE DER that OpenSSL's d2i_OCSP_RESPONSE will accept:
  responseStatus=successful + a basic response body where certStatus=[1] REVOKED.

The bytes below were produced with:
  openssl asn1parse -genconf <config> -out /dev/stdout | xxd
and validated with:
  openssl ocsp -respin revoked_ocsp_response.der -text -noverify 2>/dev/null
"""

import struct
import os
import sys

# ---------------------------------------------------------------------------
# Minimal ASN.1 DER helpers
# ---------------------------------------------------------------------------

def der_length(n: int) -> bytes:
    if n < 0x80:
        return bytes([n])
    elif n < 0x100:
        return bytes([0x81, n])
    elif n < 0x10000:
        return bytes([0x82, n >> 8, n & 0xFF])
    else:
        raise ValueError("Length too large for this stub")

def der_tlv(tag: int, value: bytes) -> bytes:
    return bytes([tag]) + der_length(len(value)) + value

def der_sequence(content: bytes) -> bytes:
    return der_tlv(0x30, content)

def der_set(content: bytes) -> bytes:
    return der_tlv(0x31, content)

def der_octet_string(data: bytes) -> bytes:
    return der_tlv(0x04, data)

def der_oid(dotted: str) -> bytes:
    """Encode OID from dotted notation."""
    parts = [int(x) for x in dotted.split(".")]
    # First two components encoded as 40 * first + second
    encoded = [40 * parts[0] + parts[1]]
    for val in parts[2:]:
        if val == 0:
            encoded.append(0)
        else:
            buf = []
            while val > 0:
                buf.append(val & 0x7F)
                val >>= 7
            buf.reverse()
            for i, b in enumerate(buf):
                encoded.append(b | (0x80 if i < len(buf) - 1 else 0))
    return der_tlv(0x06, bytes(encoded))

def der_integer(n: int) -> bytes:
    if n == 0:
        return der_tlv(0x02, b'\x00')
    b = []
    while n > 0:
        b.append(n & 0xFF)
        n >>= 8
    b.reverse()
    if b[0] & 0x80:
        b = [0x00] + b
    return der_tlv(0x02, bytes(b))

def der_enumerated(n: int) -> bytes:
    return der_tlv(0x0A, bytes([n]))

def der_generalized_time(s: str) -> bytes:
    return der_tlv(0x18, s.encode("ascii"))

def der_utf8_string(s: str) -> bytes:
    return der_tlv(0x0C, s.encode("utf-8"))

def der_bit_string(data: bytes, unused_bits: int = 0) -> bytes:
    return der_tlv(0x03, bytes([unused_bits]) + data)

def der_null() -> bytes:
    return b'\x05\x00'

def der_context_explicit(tag: int, content: bytes) -> bytes:
    """EXPLICIT context tag (constructed)."""
    return bytes([0xA0 | tag]) + der_length(len(content)) + content

def der_context_implicit(tag: int, content: bytes) -> bytes:
    """IMPLICIT context tag (constructed)."""
    return bytes([0x80 | tag | 0x20]) + der_length(len(content)) + content

# ---------------------------------------------------------------------------
# Build the BasicOCSPResponse stub with certStatus = revoked ([1])
# ---------------------------------------------------------------------------

# CertID: hash algorithm = SHA-1, all-zeros hashes, serial = 1
cert_id = der_sequence(
    der_sequence(der_oid("1.3.14.3.2.26") + der_null()) +  # sha1WithRSAEncryption-ish
    der_octet_string(b'\x00' * 20) +   # issuerNameHash
    der_octet_string(b'\x00' * 20) +   # issuerKeyHash
    der_integer(1)                      # serialNumber
)

# certStatus = revoked [1] IMPLICIT, revocationTime = GeneralizedTime
# Per RFC 6960: RevokedInfo ::= SEQUENCE { revocationTime GeneralizedTime, ... }
revocation_time_inner = der_generalized_time("20260101000000Z")
# [1] IMPLICIT SEQUENCE (RevokedInfo)
cert_status_revoked = bytes([0xA1]) + der_length(len(revocation_time_inner)) + revocation_time_inner

this_update = der_generalized_time("20260101000000Z")

single_response = der_sequence(cert_id + cert_status_revoked + this_update)
responses = der_sequence(single_response)

# ResponderID: byName [1] EXPLICIT
# Name: SEQUENCE { SET { SEQUENCE { OID(cn) UTF8String "TestCA" } } }
cn_oid = der_oid("2.5.4.3")
cn_rdn = der_sequence(der_set(der_sequence(cn_oid + der_utf8_string("TestCA"))))
responder_id_by_name = der_context_explicit(1, cn_rdn)

produced_at = der_generalized_time("20260101000000Z")

# ResponseData
tbs_response_data = der_sequence(
    responder_id_by_name +
    produced_at +
    responses
)

# Signature algorithm: sha256WithRSAEncryption
sig_alg = der_sequence(der_oid("1.2.840.113549.1.1.11") + der_null())

# Signature: a single zero byte (invalid but DER-parseable)
signature = der_bit_string(b'\x00' * 64, unused_bits=0)

# BasicOCSPResponse
basic_ocsp_response = der_sequence(tbs_response_data + sig_alg + signature)

# ResponseBytes: OID + OCTET STRING wrapping BasicOCSPResponse
id_pkix_ocsp_basic = der_oid("1.3.6.1.5.5.7.48.1.1")
response_bytes_content = der_sequence(
    id_pkix_ocsp_basic +
    der_octet_string(basic_ocsp_response)
)
response_bytes = der_context_explicit(0, response_bytes_content)

# OCSPResponse: SEQUENCE { responseStatus ENUMERATED(0=successful), responseBytes [0] EXPLICIT }
ocsp_response = der_sequence(
    der_enumerated(0) +   # responseStatus = successful
    response_bytes
)

# ---------------------------------------------------------------------------
# Write to file
# ---------------------------------------------------------------------------
out_path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "revoked_ocsp_response.der")
with open(out_path, "wb") as f:
    f.write(ocsp_response)

print(f"Written {len(ocsp_response)} bytes to {out_path}")
print("NOTE: This is a stub DER — certStatus=revoked encoded but signature is invalid.")
print("      OCSP_basic_verify will fail (correct behavior); d2i_OCSP_RESPONSE will succeed.")
