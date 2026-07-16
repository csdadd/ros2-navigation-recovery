#ifndef USERMANAGEMENTDIALOG_H
#define USERMANAGEMENTDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include "userauthmanager.h"
#include "user.h"

namespace Ui {
class UserManagementDialog;
}

class UserManagementDialog : public QDialog
{
    Q_OBJECT

public:
    static const int DIALOG_WIDTH = 700;
    static const int DIALOG_HEIGHT = 500;

    explicit UserManagementDialog(UserAuthManager* authManager, QWidget* parent = nullptr);
    ~UserManagementDialog();

private slots:
    void onAddUserClicked();
    void onDeleteUserClicked();
    void onChangePasswordClicked();
    void onRefreshClicked();
    void onCloseClicked();
    void onUserCreated(const User& user);
    void onUserDeleted(const QString& username);
    void onPermissionChanged(const QString& username, UserPermission newPermission);
    void onPasswordChanged();
    void onErrorOccurred(const QString& error);

private:
    void setupConnections();
    void loadUsers();
    void addUserToTable(const User& user);

    Ui::UserManagementDialog* ui;
    UserAuthManager* m_authManager;
};

#endif // USERMANAGEMENTDIALOG_H
