#ifndef HISTORYLOGMODEL_H
#define HISTORYLOGMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QDateTime>
#include "logstorageengine.h"

class HistoryLogTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum LogRoles { LevelRole = Qt::UserRole + 1 };

    explicit HistoryLogTableModel(QObject* parent = nullptr);
    ~HistoryLogTableModel();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setQueryResults(const QVector<StorageLogEntry>& entries);
    void clear();
    StorageLogEntry getLogEntry(int row) const;

    int getTotalCount() const;
    void setPaginationInfo(int totalCount, int currentPage, int pageSize);
    int getCurrentPage() const;
    int getPageSize() const;
    int getTotalPages() const;

private:
    QVector<StorageLogEntry> m_logs;
    int m_totalCount = 0;
    int m_currentPage = 1;
    int m_pageSize = 100;
};

#endif // HISTORYLOGMODEL_H
