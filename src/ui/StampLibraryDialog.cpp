// SPDX-License-Identifier: Apache-2.0
#include "ui/StampLibraryDialog.h"
#include "core/StampLibrary.h"

#include <QListWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QSettings>
#include <QUuid>

StampLibraryDialog::StampLibraryDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Stamp Library"));
    setModal(false);

    auto* col = new QVBoxLayout(this);

    col->addWidget(new QLabel(tr("Stamps"), this));

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("stampList"));
    col->addWidget(m_list, 1);

    m_preview = new QLabel(this);
    m_preview->setObjectName(QStringLiteral("stampPreviewLabel"));
    m_preview->setWordWrap(true);
    col->addWidget(m_preview);

    m_disclosure = new QLabel(
        tr("Placeholders: %1\nValues are substituted when the stamp is placed; "
           "the saved stamp always shows the concrete author and date.")
            .arg(StampLibrary::placeholderHelp()), this);
    m_disclosure->setObjectName(QStringLiteral("stampDisclosure"));
    m_disclosure->setWordWrap(true);
    col->addWidget(m_disclosure);

    auto* authorRow = new QHBoxLayout;
    authorRow->addWidget(new QLabel(tr("Default author:"), this));
    m_author = new QLineEdit(this);
    m_author->setObjectName(QStringLiteral("stampAuthorEdit"));
    m_author->setPlaceholderText(tr("Your name"));
    m_author->setText(QSettings().value(QStringLiteral("stamps/author")).toString());
    authorRow->addWidget(m_author, 1);
    col->addLayout(authorRow);

    auto* btnRow = new QHBoxLayout;
    m_placeBtn = new QPushButton(tr("Place Stamp"), this);
    m_placeBtn->setObjectName(QStringLiteral("stampPlaceButton"));
    m_newBtn = new QPushButton(tr("New Stamp\u2026"), this);
    m_newBtn->setObjectName(QStringLiteral("stampNewButton"));
    // Row 18: import a picture as a reusable stamp (the image places through
    // the existing signature-Upload placement + /Stamp image-appearance
    // writer, so the placed annotation is a standard PDF object).
    m_importBtn = new QPushButton(tr("Import Image\u2026"), this);
    m_importBtn->setObjectName(QStringLiteral("stampImportButton"));
    m_deleteBtn = new QPushButton(tr("Delete"), this);
    m_deleteBtn->setObjectName(QStringLiteral("stampDeleteButton"));
    m_closeBtn = new QPushButton(tr("Close"), this);
    btnRow->addWidget(m_placeBtn);
    btnRow->addWidget(m_newBtn);
    btnRow->addWidget(m_importBtn);
    btnRow->addWidget(m_deleteBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_closeBtn);
    col->addLayout(btnRow);

    connect(m_list, &QListWidget::currentRowChanged, this, [this](int) {
        refreshPreview();
        const auto tmpl = StampLibrary::findById(selectedTemplateId());
        m_deleteBtn->setEnabled(tmpl.has_value()
                                && tmpl->id.startsWith(QStringLiteral("custom:")));
    });
    connect(m_author, &QLineEdit::textChanged, this, [this](const QString& text) {
        QSettings().setValue(QStringLiteral("stamps/author"), text);
        refreshPreview();
    });
    connect(m_placeBtn, &QPushButton::clicked, this, [this]() {
        const QString id = selectedTemplateId();
        if (!id.isEmpty()) {
            emit placeRequested(id);
            accept();
        }
    });
    connect(m_newBtn, &QPushButton::clicked, this, [this]() {
        // Small modal authoring flow; programmatic tests use addCustomStamp.
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("New Stamp"),
                                                   tr("Stamp name:"), QLineEdit::Normal,
                                                   QString(), &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        const QString tmpl = QInputDialog::getText(this, tr("New Stamp"),
                                                   tr("Text template (%1):")
                                                       .arg(StampLibrary::placeholderHelp()),
                                                   QLineEdit::Normal,
                                                   QStringLiteral("%1 | ${date}").arg(name.trimmed()),
                                                   &ok);
        if (!ok) return;
        if (!addCustomStamp(name.trimmed(), tmpl))
            QMessageBox::warning(this, tr("New Stamp"),
                                 tr("Could not save the custom stamp."));
    });
    connect(m_importBtn, &QPushButton::clicked, this, [this]() {
        // Row 18 flow: pick image → name it → it lands in the library.
        // A cancelled picker or cancelled/empty name is not an error; an
        // UNUSABLE image is refused with its typed reason (never a silent
        // accept — the catalog only ever gains a fully-decoded copy).
        const QString file = QFileDialog::getOpenFileName(
            this, tr("Import Image Stamp"), QString(),
            tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tif *.tiff)"));
        if (file.isEmpty()) return;
        bool ok = false;
        const QString name = QInputDialog::getText(
            this, tr("Import Image Stamp"), tr("Stamp name:"),
            QLineEdit::Normal, QFileInfo(file).completeBaseName(), &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        QString error;
        if (!addCustomImageStamp(name, file, &error)) {
            QMessageBox::warning(this, tr("Import Image Stamp"),
                                 error.isEmpty()
                                     ? tr("Could not import the image as a stamp.")
                                     : error);
        }
    });
    connect(m_deleteBtn, &QPushButton::clicked, this, [this]() {
        deleteSelectedCustom();
    });
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::close);

    reload();
}

void StampLibraryDialog::reload() {
    m_list->clear();
    for (const auto& t : StampLibrary::all()) {
        const QString detail = t.imagePath.isEmpty()
            ? t.textTemplate
            : tr("(image stamp)");
        auto* item = new QListWidgetItem(QStringLiteral("%1 — %2").arg(t.name, detail),
                                         m_list);
        item->setData(Qt::UserRole, t.id);
    }
    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
    refreshPreview();
}

bool StampLibraryDialog::addCustomStamp(const QString& name, const QString& textTemplate) {
    if (name.trimmed().isEmpty() || textTemplate.trimmed().isEmpty()) return false;
    auto stamps = StampLibrary::custom();
    StampTemplate t;
    t.id = QStringLiteral("custom:")
           + QUuid::createUuid().toString(QUuid::WithoutBraces);
    t.name = name.trimmed();
    t.textTemplate = textTemplate;
    stamps.append(t);
    if (!StampLibrary::saveCustom(stamps)) return false;
    reload();
    // Select the newly added stamp.
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->data(Qt::UserRole).toString() == t.id) {
            m_list->setCurrentRow(i);
            break;
        }
    }
    return true;
}

bool StampLibraryDialog::addCustomImageStamp(const QString& name,
                                             const QString& sourceImagePath,
                                             QString* error) {
    const auto t = StampLibrary::addImageStampTo(StampLibrary::defaultCustomPath(),
                                                 name, sourceImagePath, error);
    if (!t) return false;
    reload();
    // Select the newly added stamp.
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->data(Qt::UserRole).toString() == t->id) {
            m_list->setCurrentRow(i);
            break;
        }
    }
    return true;
}

bool StampLibraryDialog::deleteSelectedCustom() {
    const QString id = selectedTemplateId();
    if (!id.startsWith(QStringLiteral("custom:"))) return false;
    auto stamps = StampLibrary::custom();
    QString imagePath;
    for (int i = 0; i < stamps.size(); ++i) {
        if (stamps[i].id == id) {
            imagePath = stamps[i].imagePath;
            stamps.removeAt(i);
            break;
        }
    }
    const bool ok = StampLibrary::saveCustom(stamps);
    if (ok) {
        // Row 18: deleting an image stamp also deletes the catalog's OWN
        // copy — only files inside the managed stamp-images folder are ever
        // removed (imageAbsolutePath refuses anything else).
        if (!imagePath.isEmpty()) {
            const QString jsonPath = StampLibrary::defaultCustomPath();
            const QString abs = StampLibrary::imageAbsolutePath(jsonPath, imagePath);
            if (!abs.isEmpty()
                && abs.startsWith(StampLibrary::imageStampsDirFor(jsonPath)
                                      + QStringLiteral("/"))) {
                QFile::remove(abs);
            }
        }
        reload();
    }
    return ok;
}

QString StampLibraryDialog::selectedTemplateId() const {
    QListWidgetItem* item = m_list->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

QString StampLibraryDialog::selectedPreviewText() const {
    QListWidgetItem* item = m_list->currentItem();
    if (!item) return QString();
    const auto tmpl = StampLibrary::findById(item->data(Qt::UserRole).toString());
    if (!tmpl || !tmpl->imagePath.isEmpty()) return QString();
    return StampLibrary::resolveText(tmpl->textTemplate, authorName(),
                                     QDateTime::currentDateTime());
}

QImage StampLibraryDialog::selectedStampImage() const {
    QListWidgetItem* item = m_list->currentItem();
    if (!item) return QImage();
    const auto tmpl = StampLibrary::findById(item->data(Qt::UserRole).toString());
    if (!tmpl || tmpl->imagePath.isEmpty()) return QImage();
    return StampLibrary::loadStampImage(StampLibrary::defaultCustomPath(), *tmpl);
}

QString StampLibraryDialog::authorName() const {
    return m_author ? m_author->text() : QString();
}

void StampLibraryDialog::setAuthorName(const QString& name) {
    if (m_author) m_author->setText(name);
}

void StampLibraryDialog::refreshPreview() {
    if (!m_preview) return;
    const auto tmpl = StampLibrary::findById(selectedTemplateId());
    if (!tmpl) {
        m_preview->setPixmap(QPixmap());
        m_preview->setText(tr("Select a stamp to preview its resolved text."));
        return;
    }
    // Row 18: an image stamp previews its picture (scaled, letterboxed by the
    // label) — a missing file is stated, never shown as a blank success.
    if (!tmpl->imagePath.isEmpty()) {
        const QImage img = StampLibrary::loadStampImage(
            StampLibrary::defaultCustomPath(), *tmpl);
        if (img.isNull()) {
            m_preview->setPixmap(QPixmap());
            m_preview->setText(tr("Image stamp — the image file is missing or unreadable."));
        } else {
            m_preview->setText(QString());
            m_preview->setPixmap(QPixmap::fromImage(img).scaled(
                160, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        return;
    }
    m_preview->setPixmap(QPixmap());
    const QString text = selectedPreviewText();
    m_preview->setText(text.isEmpty()
                           ? tr("Select a stamp to preview its resolved text.")
                           : tr("Placed text would read: %1").arg(text));
}
