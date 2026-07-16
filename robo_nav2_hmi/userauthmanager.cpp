#include "userauthmanager.h"
#include "userstorageengine.h"
#include <QDebug>

UserAuthManager::UserAuthManager(QObject* parent)
    : QObject(parent)
    , m_storageEngine(nullptr)
    , m_initialized(false)
    , m_loggedIn(false)
{
}

UserAuthManager::~UserAuthManager()
{
    if (m_loggedIn) {
        m_currentUser = User();
        m_loggedIn = false;
    }
}

bool UserAuthManager::initialize(UserStorageEngine* storageEngine)
{
    if (m_initialized) {
        qWarning() << "[UserAuthManager] 警告：initialize()被重复调用，认证管理器已经初始化";
        return true;
    }

    if (!storageEngine) {
        m_lastError = "存储引擎为空";
        emit errorOccurred(m_lastError);
        return false;
    }

    if (!storageEngine->isInitialized()) {
        m_lastError = "存储引擎未初始化";
        emit errorOccurred(m_lastError);
        return false;
    }

    m_storageEngine = storageEngine;
    m_initialized = true;
    qDebug() << "[UserAuthManager] 认证管理器初始化完成";
    return true;
}

bool UserAuthManager::isInitialized() const
{
    return m_initialized;
}

QString UserAuthManager::hashPassword(const QString& password)
{
    return UserStorageEngine::hashPassword(password);
}

bool UserAuthManager::verifyPassword(const QString& password, const QString& passwordHash)
{
    return UserStorageEngine::verifyPassword(password, passwordHash);
}

bool UserAuthManager::login(const QString& username, const QString& password)
{
    QMutexLocker locker(&m_mutex);

    if (!m_initialized) {
        m_lastError = "认证管理器未初始化";
        emit loginFailed(m_lastError);
        return false;
    }

    if (!validateUsername(username)) {
        m_lastError = "用户名格式无效";
        emit loginFailed(m_lastError);
        return false;
    }

    if (!validatePassword(password)) {
        m_lastError = "密码格式无效";
        emit loginFailed(m_lastError);
        return false;
    }

    LoginAttempt& attempt = m_loginAttempts[username];

    if (attempt.locked) {
        if (QDateTime::currentDateTime() < attempt.lockUntil) {
            int remainingMinutes = QDateTime::currentDateTime().secsTo(attempt.lockUntil) / 60;
            m_lastError = QString("账户已锁定，请%1分钟后再试").arg(remainingMinutes);
            emit loginFailed(m_lastError);
            return false;
        } else {
            attempt.locked = false;
            attempt.count = 0;
        }
    }

    User user = m_storageEngine->getUserByUsername(username);

    if (user.getId() == 0 || !user.isActive()) {
        m_lastError = "用户名或密码错误";
        emit loginFailed(m_lastError);
        return false;
    }

    if (!verifyPassword(password, user.getPasswordHash())) {
        attempt.count++;
        attempt.lastAttempt = QDateTime::currentDateTime();

        if (attempt.count >= MAX_LOGIN_ATTEMPTS) {
            attempt.locked = true;
            attempt.lockUntil = QDateTime::currentDateTime().addSecs(LOCK_DURATION_MINUTES * 60);
            m_lastError = QString("登录失败次数过多，账户已被锁定%1分钟").arg(LOCK_DURATION_MINUTES);
        } else {
            int remainingAttempts = MAX_LOGIN_ATTEMPTS - attempt.count;
            m_lastError = QString("用户名或密码错误（剩余尝试次数：%1）").arg(remainingAttempts);
        }

        emit loginFailed(m_lastError);
        return false;
    }

    attempt.count = 0;
    attempt.locked = false;

    m_currentUser = user;
    m_loggedIn = true;

    if (!m_storageEngine->updateLastLogin(user.getId())) {
        qWarning() << "[UserAuthManager] 警告：更新最后登录时间失败，但登录仍成功";
    }

    emit loginSuccess(m_currentUser);
    return true;
}

bool UserAuthManager::logout()
{
    QMutexLocker locker(&m_mutex);

    if (!m_loggedIn) {
        return true;
    }

    m_currentUser = User();
    m_loggedIn = false;

    emit logoutSuccess();
    return true;
}

bool UserAuthManager::isLoggedIn() const
{
    QMutexLocker locker(&m_mutex);
    return m_loggedIn;
}

User UserAuthManager::getCurrentUser() const
{
    QMutexLocker locker(&m_mutex);
    return getCurrentUserImpl();
}

QString UserAuthManager::getCurrentUsername() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentUser.getUsername();
}

UserPermission UserAuthManager::getCurrentPermission() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentUser.getPermission();
}

bool UserAuthManager::hasPermission(UserPermission requiredPermission) const
{
    QMutexLocker locker(&m_mutex);
    return hasPermissionImpl(requiredPermission);
}

bool UserAuthManager::canView() const
{
    return hasPermission(UserPermission::VIEWER);
}

bool UserAuthManager::canOperate() const
{
    return hasPermission(UserPermission::OPERATOR);
}

bool UserAuthManager::canAdmin() const
{
    return hasPermission(UserPermission::ADMIN);
}

bool UserAuthManager::changePassword(const QString& oldPassword, const QString& newPassword)
{
    QMutexLocker locker(&m_mutex);

    if (!m_loggedIn) {
        m_lastError = "未登录";
        emit errorOccurred(m_lastError);
        return false;
    }

    if (!verifyPassword(oldPassword, m_currentUser.getPasswordHash())) {
        m_lastError = "旧密码不正确";
        emit errorOccurred(m_lastError);
        return false;
    }

    if (!validatePassword(newPassword)) {
        m_lastError = "新密码不符合要求";
        emit errorOccurred(m_lastError);
        return false;
    }

    QString newPasswordHash = UserStorageEngine::hashPassword(newPassword);

    if (!m_storageEngine->changePassword(m_currentUser.getId(), newPasswordHash)) {
        m_lastError = m_storageEngine->getLastError();
        emit errorOccurred(m_lastError);
        return false;
    }

    m_currentUser.setPasswordHash(newPasswordHash);
    emit passwordChanged();
    return true;
}

bool UserAuthManager::resetPassword(const QString& username, const QString& newPassword)
{
    QMutexLocker locker(&m_mutex);

    if (!hasPermissionImpl(UserPermission::ADMIN)) {
        m_lastError = "权限不足";
        emit errorOccurred(m_lastError);
        return false;
    }

    if (!validatePassword(newPassword)) {
        m_lastError = "新密码不符合要求";
        emit errorOccurred(m_lastError);
        return false;
    }

    if (!m_storageEngine->userExists(username)) {
        m_lastError = "用户不存在";
        emit errorOccurred(m_lastError);
        return false;
    }

    QString newPasswordHash = UserStorageEngine::hashPassword(newPassword);

    if (!m_storageEngine->changePassword(username, newPasswordHash)) {
        m_lastError = m_storageEngine->getLastError();
        emit errorOccurred(m_lastError);
        return false;
    }

    emit passwordChanged();
    return true;
}

bool UserAuthManager::createUser(const QString& username, const QString& password, UserPermission permission)
{
    QMutexLocker locker(&m_mutex);

    if (!hasPermissionImpl(UserPermission::ADMIN)) {
        m_lastError = "权限不足";
        emit errorOccurred(m_lastError);
        return false;
    }

    if (!validateUsername(username)) {
        m_lastError = "用户名格式无效";
        emit errorOccurred(m_lastError);
        return false;
    }

    if (!validatePassword(password)) {
        m_lastError = "密码格式无效";
        emit errorOccurred(m_lastError);
        return false;
    }

    if (m_storageEngine->userExists(username)) {
        m_lastError = "用户已存在";
        emit errorOccurred(m_lastError);
        return false;
    }

    User newUser;
    newUser.setUsername(username);
    newUser.setPasswordHash(UserStorageEngine::hashPassword(password));
    newUser.setPermission(permission);
    newUser.setCreatedAt(QDateTime::currentDateTime());
    newUser.setActive(true);

    if (!m_storageEngine->insertUser(newUser)) {
        m_lastError = m_storageEngine->getLastError();
        emit errorOccurred(m_lastError);
        return false;
    }

    emit userCreated(newUser);
    return true;
}

bool UserAuthManager::deleteUser(const QString& username)
{
    QMutexLocker locker(&m_mutex);

    if (!hasPermissionImpl(UserPermission::ADMIN)) {
        m_lastError = "权限不足";
        emit errorOccurred(m_lastError);
        return false;
    }

    if (username == m_currentUser.getUsername()) {
        m_lastError = "不能删除自己的账户";
        emit errorOccurred(m_lastError);
        return false;
    }

    if (!m_storageEngine->userExists(username)) {
        m_lastError = "用户不存在";
        emit errorOccurred(m_lastError);
        return false;
    }

    if (!m_storageEngine->deleteUser(username)) {
        m_lastError = m_storageEngine->getLastError();
        emit errorOccurred(m_lastError);
        return false;
    }

    emit userDeleted(username);
    return true;
}

bool UserAuthManager::updateUserPermission(const QString& username, UserPermission newPermission)
{
    QMutexLocker locker(&m_mutex);

    if (!hasPermissionImpl(UserPermission::ADMIN)) {
        m_lastError = "权限不足";
        emit errorOccurred(m_lastError);
        return false;
    }

    if (username == m_currentUser.getUsername()) {
        m_lastError = "不能修改自己的权限";
        emit errorOccurred(m_lastError);
        return false;
    }

    User user = m_storageEngine->getUserByUsername(username);

    if (user.getId() == 0) {
        m_lastError = "用户不存在";
        emit errorOccurred(m_lastError);
        return false;
    }

    user.setPermission(newPermission);

    if (!m_storageEngine->updateUser(user)) {
        m_lastError = m_storageEngine->getLastError();
        emit errorOccurred(m_lastError);
        return false;
    }

    emit permissionChanged(username, newPermission);
    return true;
}

QVector<User> UserAuthManager::getAllUsers()
{
    QMutexLocker locker(&m_mutex);

    if (!m_storageEngine) {
        m_lastError = "存储引擎未初始化";
        emit errorOccurred(m_lastError);
        return QVector<User>();
    }

    if (!hasPermissionImpl(UserPermission::ADMIN)) {
        m_lastError = "权限不足";
        emit errorOccurred(m_lastError);
        return QVector<User>();
    }

    return m_storageEngine->getAllUsers();
}

void UserAuthManager::setTestAdminMode()
{
    QMutexLocker locker(&m_mutex);

    User testAdmin(1, "TestAdmin", "", UserPermission::ADMIN);
    testAdmin.setActive(true);
    testAdmin.setCreatedAt(QDateTime::currentDateTime());
    testAdmin.setLastLogin(QDateTime::currentDateTime());

    m_currentUser = testAdmin;
    m_loggedIn = true;

    emit loginSuccess(m_currentUser);

    qDebug() << "[UserAuthManager] 测试模式：以管理员身份登录";
}

QString UserAuthManager::getLastError() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastError;
}

bool UserAuthManager::validatePassword(const QString& password)
{
    if (password.length() < MIN_PASSWORD_LENGTH) {
        return false;
    }

    if (password.length() > MAX_PASSWORD_LENGTH) {
        return false;
    }

    bool hasUpper = false;
    bool hasLower = false;
    bool hasDigit = false;

    for (const QChar& c : password) {
        if (c.isUpper()) {
            hasUpper = true;
        } else if (c.isLower()) {
            hasLower = true;
        } else if (c.isDigit()) {
            hasDigit = true;
        }
    }

    return hasUpper && hasLower && hasDigit;
}

bool UserAuthManager::validateUsername(const QString& username)
{
    if (username.length() < MIN_USERNAME_LENGTH) {
        return false;
    }

    if (username.length() > MAX_USERNAME_LENGTH) {
        return false;
    }

    for (const QChar& c : username) {
        if (!c.isLetterOrNumber() && c != '_') {
            return false;
        }
    }

    return true;
}

bool UserAuthManager::hasPermissionImpl(UserPermission requiredPermission) const
{
    if (!m_loggedIn) {
        return false;
    }

    UserPermission currentPermission = m_currentUser.getPermission();

    switch (requiredPermission) {
        case UserPermission::VIEWER:
            return true;
        case UserPermission::OPERATOR:
            return currentPermission == UserPermission::OPERATOR ||
                   currentPermission == UserPermission::ADMIN;
        case UserPermission::ADMIN:
            return currentPermission == UserPermission::ADMIN;
        default:
            qWarning() << "[UserAuthManager] 警告：无效的权限值";
            return false;
    }
}

User UserAuthManager::getCurrentUserImpl() const
{
    return m_currentUser;
}
