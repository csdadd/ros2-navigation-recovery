#include "logindialog.h"
#include "ui_logindialog.h"
#include <QDebug>
#include <QtConcurrent>
#include <QShowEvent>

LoginDialog::LoginDialog(UserAuthManager* authManager, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
    , m_authManager(authManager)
    , m_loginWatcher(nullptr)
{
    ui->setupUi(this);
    resize(DIALOG_WIDTH, DIALOG_HEIGHT);
    ui->messageLabel->hide();
    setupConnections();
}

LoginDialog::~LoginDialog()
{
    disconnect(m_authManager, &UserAuthManager::loginSuccess, this, &LoginDialog::onLoginSuccess);
    disconnect(m_authManager, &UserAuthManager::loginFailed, this, &LoginDialog::onLoginFailed);

    if (m_loginWatcher) {
        m_loginWatcher->waitForFinished();
        delete m_loginWatcher;
    }

    delete ui;
}

User LoginDialog::getCurrentUser() const
{
    return m_currentUser;
}

void LoginDialog::setupConnections()
{
    connect(ui->loginButton, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(ui->cancelButton, &QPushButton::clicked, this, &LoginDialog::onCancelClicked);
    connect(m_authManager, &UserAuthManager::loginSuccess, this, &LoginDialog::onLoginSuccess, Qt::QueuedConnection);
    connect(m_authManager, &UserAuthManager::loginFailed, this, &LoginDialog::onLoginFailed, Qt::QueuedConnection);

    connect(ui->usernameEdit, &QLineEdit::returnPressed, this, &LoginDialog::onLoginClicked);
    connect(ui->passwordEdit, &QLineEdit::returnPressed, this, &LoginDialog::onLoginClicked);
}

void LoginDialog::setUiEnabled(bool enabled)
{
    ui->loginButton->setEnabled(enabled);
    ui->cancelButton->setEnabled(enabled);
    ui->usernameEdit->setEnabled(enabled);
    ui->passwordEdit->setEnabled(enabled);
}

void LoginDialog::resetDialog()
{
    setUiEnabled(true);
    ui->usernameEdit->clear();
    ui->passwordEdit->clear();
    ui->messageLabel->clear();
    ui->messageLabel->hide();
    m_currentUser = User();
    ui->usernameEdit->setFocus();
}

void LoginDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    resetDialog();
}

void LoginDialog::onLoginClicked()
{
    QString username = ui->usernameEdit->text().trimmed();
    QString password = ui->passwordEdit->text();

    if (username.isEmpty()) {
        ui->messageLabel->setText("请输入用户名");
        ui->messageLabel->show();
        return;
    }

    if (password.isEmpty()) {
        ui->messageLabel->setText("请输入密码");
        ui->messageLabel->show();
        return;
    }

    ui->messageLabel->hide();
    setUiEnabled(false);

    // 使用QtConcurrent实现异步登录，避免UI阻塞
    if (!m_loginWatcher) {
        m_loginWatcher = new QFutureWatcher<bool>(this);
        connect(m_loginWatcher, &QFutureWatcher<bool>::finished, this, &LoginDialog::onLoginFinished);
    }

    QFuture<bool> future = QtConcurrent::run([this, username, password]() {
        return m_authManager->login(username, password);
    });

    m_loginWatcher->setFuture(future);
}

void LoginDialog::onLoginFinished()
{
    bool success = m_loginWatcher->result();

    if (!success) {
        // 登录失败，恢复UI
        setUiEnabled(true);
        ui->messageLabel->setText(m_authManager->getLastError());
        ui->messageLabel->show();
        qDebug() << "[LoginDialog] Login failed:" << m_authManager->getLastError();
    }
}

void LoginDialog::onCancelClicked()
{
    reject();
}

void LoginDialog::onLoginSuccess(const User& user)
{
    m_currentUser = user;
    setUiEnabled(true);
    qDebug() << "[LoginDialog] Login successful for user:" << user.getUsername();
    accept();
}

void LoginDialog::onLoginFailed(const QString& reason)
{
    ui->messageLabel->setText(reason);
    ui->messageLabel->show();
    setUiEnabled(true);
    qDebug() << "[LoginDialog] Login failed:" << reason;
}
