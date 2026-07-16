#ifndef USER_H
#define USER_H

#include <QString>
#include <QDateTime>
#include <QMetaType>

enum class UserPermission {
    VIEWER = 0,
    OPERATOR = 1,
    ADMIN = 2
};

class User {
public:
    User() : id(0), permission(UserPermission::VIEWER), active(true) {}
    User(int userId, const QString& name, const QString& hash, UserPermission perm)
        : id(userId), username(name), passwordHash(hash), permission(perm), active(true) {}

    // Getter方法
    int getId() const { return id; }
    QString getUsername() const { return username; }
    QString getPasswordHash() const { return passwordHash; }
    UserPermission getPermission() const { return permission; }
    QDateTime getCreatedAt() const { return createdAt; }
    QDateTime getLastLogin() const { return lastLogin; }
    bool isActive() const { return active; }

    // Setter方法
    void setId(int value) { id = value; }
    void setUsername(const QString& value) { username = value; }
    void setPasswordHash(const QString& value) { passwordHash = value; }
    void setPermission(UserPermission value) { permission = value; }
    void setCreatedAt(const QDateTime& value) { createdAt = value; }
    void setLastLogin(const QDateTime& value) { lastLogin = value; }
    void setActive(bool value) { active = value; }

    static QString permissionToString(UserPermission permission);
    static UserPermission stringToPermission(const QString& str);

private:
    int id;
    QString username;
    QString passwordHash;
    UserPermission permission;
    QDateTime createdAt;
    QDateTime lastLogin;
    bool active;
};

inline QString User::permissionToString(UserPermission permission)
{
    switch (permission) {
        case UserPermission::VIEWER:
            return "VIEWER";
        case UserPermission::OPERATOR:
            return "OPERATOR";
        case UserPermission::ADMIN:
            return "ADMIN";
        default:
            return "VIEWER";
    }
}

inline UserPermission User::stringToPermission(const QString& str)
{
    bool ok = false;
    int value = str.toInt(&ok);
    
    if (ok && value >= 0 && value <= 2) {
        return static_cast<UserPermission>(value);
    }
    
    if (str == "VIEWER") {
        return UserPermission::VIEWER;
    } else if (str == "OPERATOR") {
        return UserPermission::OPERATOR;
    } else if (str == "ADMIN") {
        return UserPermission::ADMIN;
    }
    return UserPermission::VIEWER;
}

Q_DECLARE_METATYPE(User)

#endif // USER_H
