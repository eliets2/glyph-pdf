// SPDX-License-Identifier: Apache-2.0
#ifndef SIGNATUREDIALOG_H
#define SIGNATUREDIALOG_H

#include <QDialog>

class QLineEdit;

class SignatureDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SignatureDialog(QWidget *parent = nullptr);

    QString certificatePath() const;
    QString password() const;
    QString reason() const;
    QString location() const;

private slots:
    void browseCertificate();

private:
    QLineEdit *m_certPathEdit;
    QLineEdit *m_passwordEdit;
    QLineEdit *m_reasonEdit;
    QLineEdit *m_locationEdit;
};

#endif // SIGNATUREDIALOG_H
