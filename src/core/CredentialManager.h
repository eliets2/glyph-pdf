#pragma once
#include <QString>
#include <QObject>

class CredentialManager : public QObject {
    Q_OBJECT
public:
    explicit CredentialManager(QObject* parent = nullptr);

    // Service name examples: "Anthropic", "OpenAI", "Gemini"
    // Internally stored as target "GlyphPDF.AI.<service>"
    bool storeKey(const QString& service, const QString& secret);
    QString readKey(const QString& service) const;
    bool deleteKey(const QString& service);
    bool hasKey(const QString& service) const;

    // Format-validates without storing. For "Anthropic": "sk-ant-" prefix + minimum length.
    static bool isPlausibleKey(const QString& service, const QString& key);
};
