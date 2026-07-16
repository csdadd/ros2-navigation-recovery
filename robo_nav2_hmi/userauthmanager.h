#ifndef USERAUTHMANAGER_H
#define USERAUTHMANAGER_H

#include <QObject>
#include <QString>
#include <QCryptographicHash>
#include <QDateTime>
#include <QMutex>
#include <QHash>

#include "user.h"
#include "userstorageengine.h"

class UserAuthManager : public QObject
{
    Q_OBJECT

public:
    explicit UserAuthManager(QObject* parent = nullptr);
    ~UserAuthManager();

    /**
     * @brief 初始化认证管理器
     * @param storageEngine 存储引擎指针(所有权归调用者,AuthManager不负责释放)
     * @return 初始化是否成功
     * @note storageEngine指针必须在AuthManager整个生命周期内有效
     */
    bool initialize(UserStorageEngine* storageEngine);
    bool isInitialized() const;

    QString hashPassword(const QString& password);
    bool verifyPassword(const QString& password, const QString& passwordHash);

    bool login(const QString& username, const QString& password);
    bool logout();

    bool isLoggedIn() const;
    User getCurrentUser() const;
    QString getCurrentUsername() const;
    UserPermission getCurrentPermission() const;

    bool hasPermission(UserPermission requiredPermission) const;
    bool canView() const;
    bool canOperate() const;
    bool canAdmin() const;

    bool changePassword(const QString& oldPassword, const QString& newPassword);
    bool resetPassword(const QString& username, const QString& newPassword);

    bool createUser(const QString& username, const QString& password, UserPermission permission);
    bool deleteUser(const QString& username);
    bool updateUserPermission(const QString& username, UserPermission newPermission);

    QVector<User> getAllUsers();

    // 测试模式：直接以管理员身份登录（仅用于测试）
    void setTestAdminMode();

    QString getLastError() const;

signals:
    void loginSuccess(const User& user);
    void loginFailed(const QString& reason);
    void logoutSuccess();
    void passwordChanged();
    void userCreated(const User& user);
    void userDeleted(const QString& username);
    void permissionChanged(const QString& username, UserPermission newPermission);
    void errorOccurred(const QString& error);

private:
    bool validatePassword(const QString& password);
    bool validateUsername(const QString& username);

    // 内部实现函数（不加锁版本，供其他内部函数调用）
    bool hasPermissionImpl(UserPermission requiredPermission) const;
    User getCurrentUserImpl() const;

private:
    struct LoginAttempt {
        int count = 0;
        QDateTime lastAttempt;
        bool locked = false;
        QDateTime lockUntil;
    };

private:
    UserStorageEngine* m_storageEngine;
    User m_currentUser;
    bool m_initialized;
    bool m_loggedIn;
    QString m_lastError;
    static const int MIN_PASSWORD_LENGTH = 8;
    static const int MAX_PASSWORD_LENGTH = 32;
    static const int MIN_USERNAME_LENGTH = 1;
    static const int MAX_USERNAME_LENGTH = 20;
    static const int MAX_LOGIN_ATTEMPTS = 5;
    static const int LOCK_DURATION_MINUTES = 30;

    mutable QMutex m_mutex;
    QHash<QString, LoginAttempt> m_loginAttempts;
};

#endif // USERAUTHMANAGER_H
