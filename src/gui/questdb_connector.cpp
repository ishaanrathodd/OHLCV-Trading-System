#include "questdb_connector.h"
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QtSql/QSqlRecord>
#include <QtCore/QDebug>
#include <QtCore/QDateTime>
#include <QtNetwork/QTcpSocket>

QuestDBConnector::QuestDBConnector(QObject *parent)
    : QObject(parent), m_testSocket(new QTcpSocket(this)), m_hostName("localhost"), m_port(8812), m_connected(false)
{
    connect(m_testSocket, &QTcpSocket::connected, this, &QuestDBConnector::onSocketConnected);
    connect(m_testSocket, &QTcpSocket::disconnected, this, &QuestDBConnector::onSocketDisconnected);
    connect(m_testSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &QuestDBConnector::onSocketError);
}

QuestDBConnector::~QuestDBConnector()
{
    disconnectFromDatabase();
}

bool QuestDBConnector::connectToDatabase(const QString &host, int port)
{
    m_hostName = host;
    m_port = port;

    // Test connection first
    m_testSocket->connectToHost(host, port);
    if (!m_testSocket->waitForConnected(5000))
    {
        logError(QString("Cannot connect to QuestDB at %1:%2 - %3")
                     .arg(host)
                     .arg(port)
                     .arg(m_testSocket->errorString()));
        return false;
    }
    m_testSocket->disconnectFromHost();

    return setupDatabase();
}

bool QuestDBConnector::setupDatabase()
{
    // Remove existing connection if it exists
    if (QSqlDatabase::contains("questdb_connection"))
    {
        QSqlDatabase::removeDatabase("questdb_connection");
    }

    // Create PostgreSQL connection to QuestDB
    m_database = QSqlDatabase::addDatabase("QPSQL", "questdb_connection");
    m_database.setHostName(m_hostName);
    m_database.setPort(m_port);
    m_database.setDatabaseName("qdb"); // QuestDB default database
    m_database.setUserName("admin");
    m_database.setPassword("quest");

    if (!m_database.open())
    {
        logError(QString("Failed to open database: %1").arg(m_database.lastError().text()));
        m_connected = false;
        emit connectionStatusChanged(false);
        return false;
    }

    // Test the connection with a simple query
    QSqlQuery testQuery(m_database);
    if (!testQuery.exec("SELECT 1"))
    {
        logError(QString("Database connection test failed: %1").arg(testQuery.lastError().text()));
        m_database.close();
        m_connected = false;
        emit connectionStatusChanged(false);
        return false;
    }

    qDebug() << "Successfully connected to QuestDB";
    m_connected = true;
    emit connectionStatusChanged(true);
    return true;
}

void QuestDBConnector::disconnectFromDatabase()
{
    if (m_database.isOpen())
    {
        m_database.close();
    }
    m_connected = false;
    emit connectionStatusChanged(false);
}

bool QuestDBConnector::isConnected() const
{
    return m_connected && m_database.isOpen();
}

QList<QVariantMap> QuestDBConnector::queryBSEData(const QString &symbol, int limit)
{
    if (!isConnected())
    {
        logError("Not connected to database");
        return QList<QVariantMap>();
    }

    QString queryStr = "SELECT * FROM trades WHERE exchange = 'BSE'";
    if (!symbol.isEmpty())
    {
        queryStr += QString(" AND symbol = '%1'").arg(symbol);
    }
    queryStr += QString(" ORDER BY timestamp DESC LIMIT %1").arg(limit);

    QSqlQuery query(m_database);
    if (!query.exec(queryStr))
    {
        logError(QString("Query failed: %1").arg(query.lastError().text()));
        return QList<QVariantMap>();
    }

    return processQueryResult(query);
}

QList<QVariantMap> QuestDBConnector::queryLatestPrices()
{
    if (!isConnected())
    {
        logError("Not connected to database");
        return QList<QVariantMap>();
    }

    // Simple query to get recent BSE data - QuestDB specific
    QString queryStr = R"(
        SELECT 
            symbol, 
            price, 
            quantity as volume, 
            timestamp,
            side,
            exchange
        FROM trades 
        WHERE exchange = 'BSE' 
        ORDER BY timestamp DESC 
        LIMIT 10
    )";

    QSqlQuery query(m_database);
    if (!query.exec(queryStr))
    {
        logError(QString("Latest prices query failed: %1").arg(query.lastError().text()));
        return QList<QVariantMap>();
    }

    return processQueryResult(query);
}

QList<QVariantMap> QuestDBConnector::queryTimeSeriesData(const QString &symbol, int minutes)
{
    if (!isConnected())
    {
        logError("Not connected to database");
        return QList<QVariantMap>();
    }

    QString queryStr = QString(R"(
        SELECT 
            timestamp,
            symbol,
            price,
            quantity as volume,
            side
        FROM trades 
        WHERE exchange = 'BSE'
        AND symbol = '%1' 
        AND timestamp > dateadd('m', -%2, now())
        ORDER BY timestamp ASC
    )")
                           .arg(symbol)
                           .arg(minutes);

    QSqlQuery query(m_database);
    if (!query.exec(queryStr))
    {
        logError(QString("Time series query failed: %1").arg(query.lastError().text()));
        return QList<QVariantMap>();
    }

    return processQueryResult(query);
}

bool QuestDBConnector::executeQuery(const QString &queryStr)
{
    if (!isConnected())
    {
        logError("Not connected to database");
        return false;
    }

    QSqlQuery query(m_database);
    if (!query.exec(queryStr))
    {
        logError(QString("Query execution failed: %1").arg(query.lastError().text()));
        return false;
    }

    return true;
}

QList<QVariantMap> QuestDBConnector::processQueryResult(QSqlQuery &query)
{
    QList<QVariantMap> results;

    while (query.next())
    {
        QVariantMap row;
        QSqlRecord record = query.record();

        for (int i = 0; i < record.count(); ++i)
        {
            QString fieldName = record.fieldName(i);
            QVariant value = query.value(i);
            row[fieldName] = value;
        }

        results.append(row);
    }

    qDebug() << QString("Query returned %1 rows").arg(results.size());
    return results;
}

void QuestDBConnector::testConnection()
{
    m_testSocket->connectToHost(m_hostName, m_port);
}

void QuestDBConnector::refreshData()
{
    QList<QVariantMap> data = queryLatestPrices();
    emit dataReceived(data);
}

void QuestDBConnector::onSocketConnected()
{
    qDebug() << "Socket connected to QuestDB";
}

void QuestDBConnector::onSocketDisconnected()
{
    qDebug() << "Socket disconnected from QuestDB";
}

void QuestDBConnector::onSocketError()
{
    logError(QString("Socket error: %1").arg(m_testSocket->errorString()));
}

void QuestDBConnector::logError(const QString &message)
{
    m_lastError = message;
    qDebug() << "QuestDBConnector Error:" << message;
    emit errorOccurred(message);
}
