#ifndef USERSTORAGEENGINE_H
#define USERSTORAGEENGINE_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QReadWriteLock>
#include <QSqlDatabase>
#include <QSqlError>
#include "user.h"

class UserStorageEngine : public QObject
{
    Q_OBJECT

public:
    explicit UserStorageEngine(QObject* parent = nullptr);
    ~UserStorageEngine();

    bool initialize(const QString& dbPath = QString());
    bool isInitialized() const;

    bool insertUser(const User& user);
    bool updateUser(const User& user);
    bool deleteUser(int userId);
    bool deleteUser(const QString& username);

    User getUserById(int userId);
    User getUserByUsername(const QString& username);
    QVector<User> getAllUsers();

    bool updateLastLogin(int userId);
    bool changePassword(int userId, const QString& newPasswordHash);
    bool changePassword(const QString& username, const QString& newPasswordHash);

    bool userExists(const QString& username);
    bool isUserActive(const QString& username);

    QString getLastError() const;

    static QString hashPassword(const QString& password);
    static bool verifyPassword(const QString& password, const QString& storedHash);

signals:
    void userInserted(const User& user);
    void userUpdated(const User& user);
    void userDeleted(int userId);
    void errorOccurred(const QString& error);

private:
    bool createTables();
    bool createIndexes();

private:
    QSqlDatabase m_database;
    QString m_dbPath;
    QString m_connectionName;
    bool m_initialized;
    mutable QReadWriteLock m_lock;
    QString m_lastError;
};

#endif // USERSTORAGEENGINE_H
