#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QFutureWatcher>
#include "userauthmanager.h"
#include "user.h"

QT_BEGIN_NAMESPACE
namespace Ui { class LoginDialog; }
QT_END_NAMESPACE

/**
 * @brief 登录对话框类
 * @details 支持异步登录，避免UI阻塞
 */
class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(UserAuthManager* authManager, QWidget *parent = nullptr);
    ~LoginDialog();

    User getCurrentUser() const;

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void onLoginClicked();
    void onCancelClicked();
    void onLoginSuccess(const User& user);
    void onLoginFailed(const QString& reason);
    void onLoginFinished();

private:
    void setupConnections();
    void setUiEnabled(bool enabled);
    void resetDialog();

private:
    Ui::LoginDialog *ui;
    UserAuthManager* m_authManager;
    User m_currentUser;
    QFutureWatcher<bool>* m_loginWatcher;
    static const int DIALOG_WIDTH = 400;
    static const int DIALOG_HEIGHT = 300;
};

#endif // LOGINDIALOG_H
