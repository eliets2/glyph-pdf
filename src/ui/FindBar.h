#ifndef FINDBAR_H
#define FINDBAR_H

#include <QWidget>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QPushButton;
class QLabel;
class QCheckBox;
class QComboBox;
class QKeyEvent;
class QShowEvent;
QT_END_NAMESPACE

class FindBar : public QWidget
{
    Q_OBJECT

public:
    enum SearchScope { ScopeDocumentText, ScopeComments, ScopeBookmarks, ScopeAll };
    Q_ENUM(SearchScope)

    explicit FindBar(QWidget *parent = nullptr);

    void setMatchCount(int current, int total);

signals:
    void searchRequested(const QString &text, bool forward, bool matchCase,
                         bool wholeWords, bool useRegex, int scope);
    void replaceRequested(const QString &searchText, const QString &replaceText,
                          bool matchCase, bool wholeWords, bool useRegex);
    void replaceAllRequested(const QString &searchText, const QString &replaceText,
                             bool matchCase, bool wholeWords, bool useRegex);
    void redactAllRequested(const QString &text, bool matchCase, bool wholeWords);
    void closeRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void updateButtonStates();
    void onNext();
    void onPrev();

private:
    QLineEdit *searchInput;
    QLineEdit *replaceInput;
    QPushButton *btnNext;
    QPushButton *btnPrev;
    QPushButton *btnReplace;
    QPushButton *btnReplaceAll;
    QPushButton *btnRedactAll;
    QPushButton *btnClose;
    QCheckBox *caseCheckBox;
    QCheckBox *wordsCheckBox;
    QCheckBox *regexCheckBox;
    QComboBox *scopeCombo;
    QLabel *resultLabel;
};

#endif // FINDBAR_H
