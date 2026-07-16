#include "adduserdialog.h"
#include "ui_adduserdialog.h"
#include <QMessageBox>
#include <QDebug>

AddUserDialog::AddUserDialog(UserAuthManager* authManager, QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::AddUserDialog)
    , m_authManager(authManager)
{
    ui->setupUi(this);
    resize(DIALOG_WIDTH, DIALOG_HEIGHT);

    qDebug() << "[AddUserDialog] 构造函数";

    if (!m_authManager) {
        qCritical() << "[AddUserDialog] 错误：认证管理器指针为空";
        QMessageBox::critical(this, "错误", "认证管理器未初始化");
        reject();
        return;
    }

    // 设置权限下拉框的 item data
    ui->permissionCombo->setItemData(0, static_cast<int>(UserPermission::VIEWER));
    ui->permissionCombo->setItemData(1, static_cast<int>(UserPermission::OPERATOR));
    ui->permissionCombo->setItemData(2, static_cast<int>(UserPermission::ADMIN));

    setupConnections();

    qDebug() << "[AddUserDialog] 初始化成功";
}

AddUserDialog::~AddUserDialog()
{
    disconnect(ui->okButton, &QPushButton::clicked, this, &AddUserDialog::onOkClicked);
    disconnect(ui->cancelButton, &QPushButton::clicked, this, &AddUserDialog::onCancelClicked);
    delete ui;
}

void AddUserDialog::setupConnections()
{
    connect(ui->okButton, &QPushButton::clicked, this, &AddUserDialog::onOkClicked);
    connect(ui->cancelButton, &QPushButton::clicked, this, &AddUserDialog::onCancelClicked);
}

void AddUserDialog::onOkClicked()
{
    QString username = ui->usernameEdit->text().trimmed();
    QString password = ui->passwordEdit->text();
    QString confirmPassword = ui->confirmPasswordEdit->text();
    int permValue = ui->permissionCombo->currentData().toInt();

    qDebug() << "[AddUserDialog] 尝试创建用户 - 用户名:" << username << "权限:" << permValue;

    if (username.isEmpty()) {
        QMessageBox::warning(this, "错误", "请输入用户名");
        return;
    }

    if (password.isEmpty()) {
        QMessageBox::warning(this, "错误", "请输入密码");
        return;
    }

    if (password != confirmPassword) {
        QMessageBox::warning(this, "错误", "两次输入的密码不一致");
        return;
    }

    if (permValue < 0 || permValue > 2) {
        QMessageBox::warning(this, "错误", "无效的权限值");
        return;
    }

    UserPermission permission = static_cast<UserPermission>(permValue);

    if (m_authManager->createUser(username, password, permission)) {
        qDebug() << "[AddUserDialog] 用户创建成功:" << username;
        QMessageBox::information(this, "成功", "用户创建成功");
        accept();
    } else {
        qWarning() << "[AddUserDialog] 用户创建失败:" << m_authManager->getLastError();
        QMessageBox::critical(this, "错误", m_authManager->getLastError());
    }
}

void AddUserDialog::onCancelClicked()
{
    qDebug() << "[AddUserDialog] 用户取消操作";
    reject();
}
