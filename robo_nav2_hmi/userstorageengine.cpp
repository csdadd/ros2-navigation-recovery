#include "userstorageengine.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <QCryptographicHash>
#include <QDateTime>

// QSqlQuery对象在超出作用域时会自动关闭,无需手动调用close()

UserStorageEngine::UserStorageEngine(QObject* parent)
    : QObject(parent)
    , m_initialized(false)
    , m_connectionName(QString("user_connection_%1").arg(reinterpret_cast<quintptr>(this), 0, 16))
{
}

UserStorageEngine::~UserStorageEngine()
{
    if (m_database.isOpen()) {
        m_database.close();
    }
    QSqlDatabase::removeDatabase(m_database.connectionName());
}

bool UserStorageEngine::initialize(const QString& dbPath)
{
    QWriteLocker locker(&m_lock);

    if (m_initialized) {
        qWarning() << "[UserStorageEngine] 警告：initialize()被重复调用，数据库已经初始化";
        return true;
    }

    qDebug() << "[UserStorageEngine] 正在初始化数据库...";

    if (dbPath.isEmpty()) {
        QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

        QDir dir(dataDir);
        if (!dir.exists()) {
            if (!dir.mkpath(dataDir)) {
                qWarning() << "[UserStorageEngine] 警告：无法创建目录" << dataDir;
            }
        }
        m_dbPath = dataDir + "/users.db";
    } else {
        m_dbPath = dbPath;
    }

    if (QSqlDatabase::contains(m_connectionName)) {
        m_database = QSqlDatabase::database(m_connectionName);
        // 如果数据库已经存在连接,尝试关闭它以重新初始化
        if (m_database.isOpen()) {
            m_database.close();
        }
    } else {
        m_database = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    }

    m_database.setDatabaseName(m_dbPath);

    if (!m_database.open()) {
        m_lastError = QString("Failed to open database: %1").arg(m_database.lastError().text());
        qCritical() << "[UserStorageEngine]" << m_lastError;
        emit errorOccurred(m_lastError);
        return false;
    }

    if (!createTables()) {
        qCritical() << "[UserStorageEngine] 创建表失败";
        m_database.close();
        return false;
    }

    if (!createIndexes()) {
        qCritical() << "[UserStorageEngine] 创建索引失败";
        m_database.close();
        return false;
    }

    QSqlQuery checkQuery(m_database);
    checkQuery.prepare("SELECT COUNT(*) FROM users WHERE username = ?");
    checkQuery.addBindValue("admin");
    bool adminExists = false;
    if (checkQuery.exec() && checkQuery.next()) {
        adminExists = checkQuery.value(0).toInt() > 0;
    }

    if (!adminExists) {
        QSqlQuery insertQuery(m_database);
        insertQuery.prepare(R"(
            INSERT INTO users (username, password_hash, permission, created_at, last_login, active)
            VALUES (?, ?, ?, ?, ?, ?)
        )");

        User defaultAdmin;
        defaultAdmin.setUsername("admin");
        defaultAdmin.setPasswordHash(hashPassword("Admin123"));
        defaultAdmin.setPermission(UserPermission::ADMIN);
        defaultAdmin.setCreatedAt(QDateTime::currentDateTime());
        defaultAdmin.setActive(true);

        insertQuery.addBindValue(defaultAdmin.getUsername());
        insertQuery.addBindValue(defaultAdmin.getPasswordHash());
        insertQuery.addBindValue(static_cast<int>(defaultAdmin.getPermission()));
        insertQuery.addBindValue(defaultAdmin.getCreatedAt().toMSecsSinceEpoch());
        insertQuery.addBindValue(QVariant());
        insertQuery.addBindValue(defaultAdmin.isActive() ? 1 : 0);

        if (!insertQuery.exec()) {
            qWarning() << "[UserStorageEngine] 创建默认管理员账户失败:" << insertQuery.lastError().text();
        }
    }

    m_initialized = true;
    qDebug() << "[UserStorageEngine] 数据库初始化完成:" << m_dbPath;
    return true;
}

bool UserStorageEngine::isInitialized() const
{
    QReadLocker locker(&m_lock);
    return m_initialized;
}

bool UserStorageEngine::createTables()
{
    QSqlQuery query(m_database);

    QString createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            permission INTEGER NOT NULL,
            created_at INTEGER NOT NULL,
            last_login INTEGER,
            active INTEGER NOT NULL DEFAULT 1
        )
    )";

    if (!query.exec(createTableSQL)) {
        m_lastError = QString("Failed to create users table: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    return true;
}

bool UserStorageEngine::createIndexes()
{
    QSqlQuery query(m_database);

    QString createUsernameIndex = "CREATE INDEX IF NOT EXISTS idx_username ON users(username)";
    if (!query.exec(createUsernameIndex)) {
        m_lastError = QString("Failed to create username index: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    return true;
}

bool UserStorageEngine::insertUser(const User& user)
{
    User insertedUser;
    bool success = false;
    QString error;

    {
        QWriteLocker locker(&m_lock);

        if (!m_initialized || !m_database.isOpen()) {
            m_lastError = "Database not initialized";
            return false;
        }

        QSqlQuery query(m_database);
        query.prepare(R"(
            INSERT INTO users (username, password_hash, permission, created_at, last_login, active)
            VALUES (?, ?, ?, ?, ?, ?)
        )");

        query.addBindValue(user.getUsername());
        query.addBindValue(user.getPasswordHash());
        query.addBindValue(static_cast<int>(user.getPermission()));
        query.addBindValue(user.getCreatedAt().toMSecsSinceEpoch());
        query.addBindValue(user.getLastLogin().isValid() ? user.getLastLogin().toMSecsSinceEpoch() : QVariant());
        query.addBindValue(user.isActive() ? 1 : 0);

        if (!query.exec()) {
            m_lastError = QString("Failed to insert user: %1").arg(query.lastError().text());
            emit errorOccurred(m_lastError);
            return false;
        }

        insertedUser = user;
        success = true;
    }

    if (success) {
        emit userInserted(insertedUser);
    }

    return success;
}

bool UserStorageEngine::updateUser(const User& user)
{
    User updatedUser;
    bool success = false;

    {
        QWriteLocker locker(&m_lock);

        if (!m_initialized || !m_database.isOpen()) {
            m_lastError = "Database not initialized";
            return false;
        }

        QSqlQuery query(m_database);
        query.prepare(R"(
            UPDATE users SET
                username = ?,
                password_hash = ?,
                permission = ?,
                last_login = ?,
                active = ?
            WHERE id = ?
        )");

        query.addBindValue(user.getUsername());
        query.addBindValue(user.getPasswordHash());
        query.addBindValue(static_cast<int>(user.getPermission()));
        query.addBindValue(user.getLastLogin().isValid() ? user.getLastLogin().toMSecsSinceEpoch() : QVariant());
        query.addBindValue(user.isActive() ? 1 : 0);
        query.addBindValue(user.getId());

        if (!query.exec()) {
            m_lastError = QString("Failed to update user: %1").arg(query.lastError().text());
            emit errorOccurred(m_lastError);
            return false;
        }

        updatedUser = user;
        success = true;
    }

    if (success) {
        emit userUpdated(updatedUser);
    }

    return success;
}

bool UserStorageEngine::deleteUser(int userId)
{
    QWriteLocker locker(&m_lock);

    if (!m_initialized || !m_database.isOpen()) {
        m_lastError = "Database not initialized";
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("DELETE FROM users WHERE id = ?");
    query.addBindValue(userId);

    if (!query.exec()) {
        m_lastError = QString("Failed to delete user: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    emit userDeleted(userId);
    return true;
}

bool UserStorageEngine::deleteUser(const QString& username)
{
    if (!m_initialized || !m_database.isOpen()) {
        m_lastError = "Database not initialized";
        return false;
    }

    User user = getUserByUsername(username);
    if (user.getId() == 0) {
        m_lastError = "User not found";
        return false;
    }

    bool success = false;

    {
        QWriteLocker locker(&m_lock);

        QSqlQuery query(m_database);
        query.prepare("DELETE FROM users WHERE username = ?");
        query.addBindValue(username);

        if (!query.exec()) {
            m_lastError = QString("Failed to delete user: %1").arg(query.lastError().text());
            emit errorOccurred(m_lastError);
            return false;
        }

        success = true;
    }

    if (success) {
        emit userDeleted(user.getId());
    }

    return success;
}

User UserStorageEngine::getUserById(int userId)
{
    QReadLocker locker(&m_lock);

    User user;

    if (!m_initialized || !m_database.isOpen()) {
        m_lastError = "Database not initialized";
        return user;
    }

    QSqlQuery query(m_database);
    query.prepare("SELECT id, username, password_hash, permission, created_at, last_login, active FROM users WHERE id = ?");
    query.addBindValue(userId);

    if (!query.exec()) {
        m_lastError = QString("Failed to get user: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return user;
    }

    if (query.next()) {
        user.setId(query.value(0).toInt());
        user.setUsername(query.value(1).toString());
        user.setPasswordHash(query.value(2).toString());
        user.setPermission(User::stringToPermission(query.value(3).toString()));
        
        bool timestampValid = true;
        qint64 timestamp = query.value(4).toLongLong(&timestampValid);
        user.setCreatedAt(QDateTime::fromMSecsSinceEpoch(timestamp));
        if (!timestampValid || !user.getCreatedAt().isValid()) {
            qWarning() << "[UserStorageEngine] 警告：无效的created_at时间戳:" << timestamp;
            user.setCreatedAt(QDateTime::currentDateTime());
        }
        
        if (!query.value(5).isNull()) {
            timestampValid = true;
            timestamp = query.value(5).toLongLong(&timestampValid);
            user.setLastLogin(QDateTime::fromMSecsSinceEpoch(timestamp));
            if (!timestampValid || !user.getLastLogin().isValid()) {
                qWarning() << "[UserStorageEngine] 警告：无效的last_login时间戳:" << timestamp;
                user.setLastLogin(QDateTime());
            }
        }
        user.setActive(query.value(6).toInt() == 1);
    }

    return user;
}

User UserStorageEngine::getUserByUsername(const QString& username)
{
    QReadLocker locker(&m_lock);

    User user;

    if (!m_initialized || !m_database.isOpen()) {
        m_lastError = "Database not initialized";
        return user;
    }

    QSqlQuery query(m_database);
    query.prepare("SELECT id, username, password_hash, permission, created_at, last_login, active FROM users WHERE username = ?");
    query.addBindValue(username);

    if (!query.exec()) {
        m_lastError = QString("Failed to get user: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return user;
    }

    if (query.next()) {
        user.setId(query.value(0).toInt());
        user.setUsername(query.value(1).toString());
        user.setPasswordHash(query.value(2).toString());
        user.setPermission(User::stringToPermission(query.value(3).toString()));
        
        bool timestampValid = true;
        qint64 timestamp = query.value(4).toLongLong(&timestampValid);
        user.setCreatedAt(QDateTime::fromMSecsSinceEpoch(timestamp));
        if (!timestampValid || !user.getCreatedAt().isValid()) {
            qWarning() << "[UserStorageEngine] 警告：无效的created_at时间戳:" << timestamp;
            user.setCreatedAt(QDateTime::currentDateTime());
        }
        
        if (!query.value(5).isNull()) {
            timestampValid = true;
            timestamp = query.value(5).toLongLong(&timestampValid);
            user.setLastLogin(QDateTime::fromMSecsSinceEpoch(timestamp));
            if (!timestampValid || !user.getLastLogin().isValid()) {
                qWarning() << "[UserStorageEngine] 警告：无效的last_login时间戳:" << timestamp;
                user.setLastLogin(QDateTime());
            }
        }
        user.setActive(query.value(6).toInt() == 1);
    }

    return user;
}

QVector<User> UserStorageEngine::getAllUsers()
{
    QReadLocker locker(&m_lock);

    QVector<User> users;

    if (!m_initialized || !m_database.isOpen()) {
        m_lastError = "Database not initialized";
        return users;
    }

    QSqlQuery query(m_database);
    query.prepare("SELECT id, username, password_hash, permission, created_at, last_login, active FROM users ORDER BY id");

    if (!query.exec()) {
        m_lastError = QString("Failed to get all users: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return users;
    }

    while (query.next()) {
        User user;
        user.setId(query.value(0).toInt());
        user.setUsername(query.value(1).toString());
        user.setPasswordHash(query.value(2).toString());
        user.setPermission(User::stringToPermission(query.value(3).toString()));

        bool timestampValid = true;
        qint64 timestamp = query.value(4).toLongLong(&timestampValid);
        user.setCreatedAt(QDateTime::fromMSecsSinceEpoch(timestamp));
        if (!timestampValid || !user.getCreatedAt().isValid()) {
            qWarning() << "[UserStorageEngine] 警告：无效的created_at时间戳:" << timestamp;
            user.setCreatedAt(QDateTime::currentDateTime());
        }

        if (!query.value(5).isNull()) {
            timestampValid = true;
            timestamp = query.value(5).toLongLong(&timestampValid);
            user.setLastLogin(QDateTime::fromMSecsSinceEpoch(timestamp));
            if (!timestampValid || !user.getLastLogin().isValid()) {
                qWarning() << "[UserStorageEngine] 警告：无效的last_login时间戳:" << timestamp;
                user.setLastLogin(QDateTime());
            }
        }
        user.setActive(query.value(6).toInt() == 1);
        users.append(user);
    }

    return users;
}

bool UserStorageEngine::updateLastLogin(int userId)
{
    QWriteLocker locker(&m_lock);

    if (!m_initialized || !m_database.isOpen()) {
        m_lastError = "Database not initialized";
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("UPDATE users SET last_login = ? WHERE id = ?");
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    query.addBindValue(userId);

    if (!query.exec()) {
        m_lastError = QString("Failed to update last login: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    return true;
}

bool UserStorageEngine::changePassword(int userId, const QString& newPasswordHash)
{
    QWriteLocker locker(&m_lock);

    if (!m_initialized || !m_database.isOpen()) {
        m_lastError = "Database not initialized";
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("UPDATE users SET password_hash = ? WHERE id = ?");
    query.addBindValue(newPasswordHash);
    query.addBindValue(userId);

    if (!query.exec()) {
        m_lastError = QString("Failed to change password: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    return true;
}

bool UserStorageEngine::changePassword(const QString& username, const QString& newPasswordHash)
{
    QWriteLocker locker(&m_lock);

    if (!m_initialized || !m_database.isOpen()) {
        m_lastError = "Database not initialized";
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("UPDATE users SET password_hash = ? WHERE username = ?");
    query.addBindValue(newPasswordHash);
    query.addBindValue(username);

    if (!query.exec()) {
        m_lastError = QString("Failed to change password: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    return true;
}

bool UserStorageEngine::userExists(const QString& username)
{
    QReadLocker locker(&m_lock);

    if (!m_initialized || !m_database.isOpen()) {
        m_lastError = "Database not initialized";
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("SELECT COUNT(*) FROM users WHERE username = ?");
    query.addBindValue(username);

    if (!query.exec()) {
        m_lastError = QString("Failed to check user existence: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    if (query.next()) {
        return query.value(0).toInt() > 0;
    }

    return false;
}

bool UserStorageEngine::isUserActive(const QString& username)
{
    QReadLocker locker(&m_lock);

    if (!m_initialized || !m_database.isOpen()) {
        m_lastError = "Database not initialized";
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("SELECT active FROM users WHERE username = ?");
    query.addBindValue(username);

    if (!query.exec()) {
        m_lastError = QString("Failed to check user active status: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    if (query.next()) {
        return query.value(0).toInt() == 1;
    }

    return false;
}

QString UserStorageEngine::getLastError() const
{
    QReadLocker locker(&m_lock);
    return m_lastError;
}

QString UserStorageEngine::hashPassword(const QString& password)
{
    QByteArray salt = QCryptographicHash::hash(
        QByteArray::number(QDateTime::currentMSecsSinceEpoch()),
        QCryptographicHash::Sha256
    );

    QByteArray hash = QCryptographicHash::hash(
        password.toUtf8() + salt,
        QCryptographicHash::Sha256
    );

    return salt.toHex() + ":" + hash.toHex();
}

bool UserStorageEngine::verifyPassword(const QString& password, const QString& storedHash)
{
    QStringList parts = storedHash.split(':');
    if (parts.size() != 2) {
        return false;
    }

    QByteArray salt = QByteArray::fromHex(parts[0].toUtf8());
    QByteArray expectedHash = QByteArray::fromHex(parts[1].toUtf8());

    QByteArray actualHash = QCryptographicHash::hash(
        password.toUtf8() + salt,
        QCryptographicHash::Sha256
    );

    if (actualHash.size() != expectedHash.size()) {
        return false;
    }

    int result = 0;
    for (int i = 0; i < actualHash.size(); ++i) {
        result |= actualHash[i] ^ expectedHash[i];
    }

    return result == 0;
}
