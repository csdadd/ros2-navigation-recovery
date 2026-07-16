#include "changepassworddialog.h"
#include "ui_changepassworddialog.h"
#include <QMessageBox>
#include <QDebug>

ChangePasswordDialog::ChangePasswordDialog(Mode mode, UserAuthManager* authManager, const QString& username, QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::ChangePasswordDialog)
    , m_mode(mode)
    , m_authManager(authManager)
    , m_username(username)
{
    ui->setupUi(this);

    qDebug() << "[ChangePasswordDialog] 构造函数 - 模式:" << (m_mode == Mode::AdminReset ? "管理员重置" : "用户自改");

    if (!m_authManager) {
        qCritical() << "[ChangePasswordDialog] 错误：认证管理器指针为空";
        QMessageBox::critical(this, "错误", "认证管理器未初始化");
        reject();
        return;
    }

    setupUI();
    setupConnections();

    qDebug() << "[ChangePasswordDialog] 初始化成功";
}

ChangePasswordDialog::~ChangePasswordDialog()
{
    disconnect(ui->okButton, &QPushButton::clicked, this, &ChangePasswordDialog::onOkClicked);
    disconnect(ui->cancelButton, &QPushButton::clicked, this, &ChangePasswordDialog::onCancelClicked);
    delete ui;
}

void ChangePasswordDialog::setupUI()
{
    if (m_mode == Mode::AdminReset) {
        ui->oldPasswordLabel->hide();
        ui->oldPasswordEdit->hide();
        resize(DIALOG_WIDTH, DIALOG_HEIGHT_WITHOUT_OLD_PASSWORD);
        qDebug() << "[ChangePasswordDialog] 设置为管理员重置模式";
    } else {
        resize(DIALOG_WIDTH, DIALOG_HEIGHT_WITH_OLD_PASSWORD);
        qDebug() << "[ChangePasswordDialog] 设置为用户自改模式";
    }
}

void ChangePasswordDialog::setupConnections()
{
    connect(ui->okButton, &QPushButton::clicked, this, &ChangePasswordDialog::onOkClicked);
    connect(ui->cancelButton, &QPushButton::clicked, this, &ChangePasswordDialog::onCancelClicked);
}

void ChangePasswordDialog::onOkClicked()
{
    QString newPassword = ui->newPasswordEdit->text();
    QString confirmPassword = ui->confirmPasswordEdit->text();

    qDebug() << "[ChangePasswordDialog] 尝试修改密码";

    if (newPassword.isEmpty()) {
        QMessageBox::warning(this, "错误", "请输入新密码");
        return;
    }

    if (newPassword != confirmPassword) {
        QMessageBox::warning(this, "错误", "两次输入的密码不一致");
        return;
    }

    if (m_mode == Mode::SelfChange) {
        QString oldPassword = ui->oldPasswordEdit->text();
        if (oldPassword.isEmpty()) {
            QMessageBox::warning(this, "错误", "请输入旧密码");
            return;
        }

        qDebug() << "[ChangePasswordDialog] 用户自改密码模式";
        if (m_authManager->changePassword(oldPassword, newPassword)) {
            qDebug() << "[ChangePasswordDialog] 密码修改成功";
            QMessageBox::information(this, "成功", "密码修改成功");
            accept();
        } else {
            qWarning() << "[ChangePasswordDialog] 密码修改失败:" << m_authManager->getLastError();
            QMessageBox::critical(this, "错误", m_authManager->getLastError());
        }
    } else {
        qDebug() << "[ChangePasswordDialog] 管理员重置密码模式 - 用户:" << m_username;
        if (m_authManager->resetPassword(m_username, newPassword)) {
            qDebug() << "[ChangePasswordDialog] 密码重置成功";
            QMessageBox::information(this, "成功", "密码修改成功");
            accept();
        } else {
            qWarning() << "[ChangePasswordDialog] 密码重置失败:" << m_authManager->getLastError();
            QMessageBox::critical(this, "错误", m_authManager->getLastError());
        }
    }
}

void ChangePasswordDialog::onCancelClicked()
{
    qDebug() << "[ChangePasswordDialog] 用户取消操作";
    reject();
}
