#pragma once

#include <QtCore/QObject>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QtCore/QVariantMap>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtNetwork/QTcpSocket>

class QuestDBConnector : public QObject
{
    Q_OBJECT

public:
    explicit QuestDBConnector(QObject *parent = nullptr);
    ~QuestDBConnector();

    bool connectToDatabase(const QString &host = "localhost", int port = 8812);
    void disconnectFromDatabase();
    bool isConnected() const;

    QList<QVariantMap> queryBSEData(const QString &symbol = "", int limit = 100);
    QList<QVariantMap> queryLatestPrices();
    QList<QVariantMap> queryTimeSeriesData(const QString &symbol, int minutes = 60);
    bool executeQuery(const QString &query);

public slots:
    void testConnection();
    void refreshData();

signals:
    void connectionStatusChanged(bool connected);
    void dataReceived(const QList<QVariantMap> &data);
    void errorOccurred(const QString &error);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError();

private:
    bool setupDatabase();
    QList<QVariantMap> processQueryResult(QSqlQuery &query);
    void logError(const QString &message);

    QSqlDatabase m_database;
    QTcpSocket *m_testSocket;
    QString m_hostName;
    int m_port;
    bool m_connected;
    QString m_lastError;
};
