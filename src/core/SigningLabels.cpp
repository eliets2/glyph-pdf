// SPDX-License-Identifier: Apache-2.0
#include "SigningLabels.h"

namespace gp {
namespace SigningLabels {

QString attainedLevelLabel(PAdESLevel requested, const SignatureOutcomeDetail& detail)
{
    // The label names the HIGHEST standard level whose required pieces are all
    // present given the tracked detail: every level above B-B requires the
    // B-T timestamp token (SEP13 lead 1: a CONFIGURED-but-unreachable TSA
    // degrades the signature to B-B — signingPreflightRefusal only covers the
    // no-TSA-configured case; the in-flight fetch failure surfaces here as
    // detail.timestampMissing). SWEEP-W1 F2: presence of a response body is
    // NOT attainment — the token must have PARSED as an RFC 3161 TS_RESP
    // (detail.timestampTokenValid, set by the engine at embed time) before
    // any level above B-B is claimed, so a garbage/error-page response from a
    // misconfigured or hostile TSA keeps the honest B-B label. B-LT
    // additionally requires the DSS dictionary, B-LTA the archive timestamp
    // on top of that.
    if (requested > PAdESLevel::B_B
        && (detail.timestampMissing || (detail.timestampAttempted && !detail.timestampTokenValid)))
        return QStringLiteral("B-B");
    switch (requested) {
        case PAdESLevel::B_B:  return QStringLiteral("B-B");
        case PAdESLevel::B_T:  return QStringLiteral("B-T");
        case PAdESLevel::B_LT:
            return detail.dssMissing ? QStringLiteral("B-T") : QStringLiteral("B-LT");
        case PAdESLevel::B_LTA:
            if (detail.dssMissing)
                return QStringLiteral("B-T");
            return detail.docTimestampMissing ? QStringLiteral("B-LT") : QStringLiteral("B-LTA");
    }
    return QStringLiteral("B-B");
}

} // namespace SigningLabels
} // namespace gp
