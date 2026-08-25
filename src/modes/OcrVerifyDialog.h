// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QDialog>
#include <QImage>
#include <QString>
#include <QList>

class QLabel;
class QLineEdit;
class QToolButton;

namespace gp {

/**
 * B3 (FineReader verify spec): the Verify Text dialog.
 *
 * A floating tool-window that steps through every low-confidence word one
 * at a time: magnified image crop of the current word on top, reason label,
 * editable correction field, Confirm / Skip actions. The dialog owns no
 * document state — OCRMode feeds it items and applies its signals.
 */
class OcrVerifyDialog : public QDialog {
    Q_OBJECT
public:
    explicit OcrVerifyDialog(QWidget* parent = nullptr);

    /// One flagged word as shown by the dialog.
    struct Item {
        int     wordIndex  = -1;   // index into OCRMode's full word list
        QString text;          // recognized text
        int     confidence     = 0;
        QImage  crop;             // magnified crop (may be null)
    };

    /// Load the ordered list of flagged words and start at the first.
    void setItems(const QList<Item> &items);
    /// Number of items still queued (unvisited tail from the cursor on).
    int  remaining() const;
    int  currentIndex() const { return m_cursor; }

signals:
    /// User confirmed (possibly corrected) text for the word at wordIndex.
    void confirmRequested(int wordIndex, const QString &correctedText);
    /// User skipped the word without changes.
    void skipRequested(int wordIndex);
    /// B10: add the current word to the user dictionary.
    void addToDictionaryRequested(const QString &word);
    /// The queue was exhausted (all items visited).
    void finished();

private slots:
    void onConfirm();
    void onSkip();
    void onAddToDictionary();

private:
    void showItem();

    QList<Item> m_items;
    int m_cursor = -1;

    QLabel*      m_cropLabel   = nullptr;
    QLabel*      m_reasonLabel = nullptr;
    QLabel*      m_posLabel    = nullptr;
    QLineEdit*   m_edit        = nullptr;
    QToolButton* m_btnConfirm  = nullptr;
    QToolButton* m_btnSkip     = nullptr;
    QToolButton* m_btnAddDict  = nullptr;
};

} // namespace gp
