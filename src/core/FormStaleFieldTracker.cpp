// SPDX-License-Identifier: Apache-2.0
#include "core/FormStaleFieldTracker.h"

#include "core/interfaces/IFormManager.h"

namespace gp {

void FormStaleFieldTracker::applyCascadeOutcome(const QString& docPath,
                                                const QList<FormJsFailure>& failures)
{
    if (docPath.isEmpty()) return;
    QMutexLocker lock(&m_mutex);
    QMap<QString, Entry>& doc = m_byDoc[docPath];
    doc.clear();
    for (const FormJsFailure& f : failures) {
        if (f.fieldName.isEmpty()) continue; // engine-level entry — no field to flag
        doc.insert(f.fieldName, Entry{ f.kind, f.reason });
    }
    if (doc.isEmpty())
        m_byDoc.remove(docPath); // everything recomputed — no warnings kept
}

bool FormStaleFieldTracker::isStale(const QString& docPath, const QString& fieldName) const
{
    QMutexLocker lock(&m_mutex);
    const auto it = m_byDoc.constFind(docPath);
    return it != m_byDoc.constEnd() && it->contains(fieldName);
}

QString FormStaleFieldTracker::staleReason(const QString& docPath, const QString& fieldName) const
{
    QMutexLocker lock(&m_mutex);
    const auto it = m_byDoc.constFind(docPath);
    if (it == m_byDoc.constEnd()) return {};
    const auto entry = it->constFind(fieldName);
    return entry == it->constEnd() ? QString() : entry->reason;
}

QStringList FormStaleFieldTracker::staleFields(const QString& docPath) const
{
    QMutexLocker lock(&m_mutex);
    const auto it = m_byDoc.constFind(docPath);
    return it == m_byDoc.constEnd() ? QStringList() : it->keys();
}

void FormStaleFieldTracker::acknowledge(const QString& docPath, const QString& fieldName)
{
    QMutexLocker lock(&m_mutex);
    const auto it = m_byDoc.find(docPath);
    if (it == m_byDoc.end()) return;
    it->remove(fieldName);
    if (it->isEmpty()) m_byDoc.erase(it);
}

void FormStaleFieldTracker::clearDocument(const QString& docPath)
{
    QMutexLocker lock(&m_mutex);
    m_byDoc.remove(docPath);
}

} // namespace gp
