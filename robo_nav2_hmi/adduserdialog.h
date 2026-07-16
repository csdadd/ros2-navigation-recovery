#ifndef ADDUSERDIALOG_H
#define ADDUSERDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include "userauthmanager.h"

namespace Ui {
class AddUserDialog;
}

class AddUserDialog : public QDialog
{
    Q_OBJECT

public:
    static const int DIALOG_WIDTH = 350;
    static const int DIALOG_HEIGHT = 300;

    explicit AddUserDialog(UserAuthManager* authManager, QWidget* parent = nullptr);
    ~AddUserDialog();

private slots:
    void onOkClicked();
    void onCancelClicked();

private:
    void setupConnections();

    Ui::AddUserDialog* ui;
    UserAuthManager* m_authManager;
};

#endif // ADDUSERDIALOG_H
