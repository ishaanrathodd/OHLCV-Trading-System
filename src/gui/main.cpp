#include <QtWidgets/QApplication>
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtCore/QDebug>
#include <QtSql/QSqlDatabase>

#include "main_window.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Set application properties
    app.setApplicationName("BSE Trading System");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("Demo Trading Inc");
    app.setApplicationDisplayName("BSE Real-time Trading Dashboard");

    // Parse command line arguments for QuestDB connection and API key
    QString questdbHost = "localhost";
    int questdbPort = 8812;
    QString apiKey = "050HFLSF4NLRGMHW"; // Default fallback

    if (argc >= 3)
    {
        questdbHost = QString(argv[1]);
        questdbPort = QString(argv[2]).toInt();
    }
    if (argc >= 4)
    {
        apiKey = QString(argv[3]);
    }

    // Check for PostgreSQL driver
    if (!QSqlDatabase::drivers().contains("QPSQL"))
    {
        qWarning() << "PostgreSQL driver not available!";
        qWarning() << "Available drivers:" << QSqlDatabase::drivers();
        qWarning() << "Please install Qt PostgreSQL driver for QuestDB connectivity";

        // Continue anyway for demo purposes
    }
    else
    {
        qDebug() << "PostgreSQL driver available - QuestDB connectivity enabled";
    }

    // Create and show main window with full configuration
    MainWindow window(questdbHost, questdbPort, apiKey);
    window.show();

    qDebug() << "BSE Trading System started successfully";
    qDebug() << "QuestDB configuration: " << questdbHost << ":" << questdbPort;
    qDebug() << "API Key: " << apiKey.left(8) << "...";

    return app.exec();
}