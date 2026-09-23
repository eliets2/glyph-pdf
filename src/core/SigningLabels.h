// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>

#include "core/interfaces/ISignatureManager.h"

namespace gp {
namespace SigningLabels {

// R19c (SWEEP-W3 move 1, audit SWEEP-W3-ARCHITECT-2026-09-20 §5): pure
// attained-level label — the single home of the PAdES attained-level naming.
// Body moved verbatim from SecurityController::attainedLevelLabel (which now
// delegates here) so core code (SigningRequestRunner) makes a legal downward
// call instead of reaching up into shell/controllers.
QString attainedLevelLabel(PAdESLevel requested, const SignatureOutcomeDetail& detail);

} // namespace SigningLabels
} // namespace gp
