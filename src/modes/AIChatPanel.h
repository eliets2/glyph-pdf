#pragma once
#include <QFrame>
#include <memory>

class CredentialManager;

namespace gp {

class AIChatPanel : public QFrame {
    Q_OBJECT
public:
    explicit AIChatPanel(QWidget* parent = nullptr);
    ~AIChatPanel() override;

private:
    std::unique_ptr<CredentialManager> m_credentialManager;
};

} // namespace gp
