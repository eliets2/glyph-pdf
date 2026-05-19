#ifndef FINDBAR_H
#define FINDBAR_H

#include <QWidget>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QPushButton;
class QLabel;
class QCheckBox;
class QKeyEvent;
class QShowEvent;
QT_END_NAMESPACE

class FindBar : public QWidget
{
    Q_OBJECT

public:
    explicit FindBar(QWidget *parent = nullptr);

signals:
    void searchRequested(const QString &text, bool forward, bool matchCase, bool wholeWords);
    void redactAllRequested(const QString &text, bool matchCase, bool wholeWords);
    void closeRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void updateButtonStates();

private:
    QLineEdit *searchInput;
    QPushButton *btnNext;
    QPushButton *btnPrev;
    QPushButton *btnRedactAll;
    QPushButton *btnClose;
    QCheckBox *caseCheckBox;
    QCheckBox *wordsCheckBox;
    QLabel *resultLabel;
};

#endif // FINDBAR_H
