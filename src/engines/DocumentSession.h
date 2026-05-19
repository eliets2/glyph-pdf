#pragma once
#include <QObject>
#include <QString>

class DocumentSession : public QObject {
    Q_OBJECT
public:
    explicit DocumentSession(QObject *parent = nullptr);

    QString path() const;
    void setPath(const QString &path);
    void markReload();
    bool isDirty() const;
    void setClean();

signals:
    void reloadRequested();
    void dirtyChanged(bool dirty);

private:
    QString m_path;
    bool m_dirty = false;
};
