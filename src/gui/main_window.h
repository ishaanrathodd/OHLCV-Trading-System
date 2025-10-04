#pragma once

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHeaderView>
#include <QtCore/QTimer>
#include <QtCore/QProcess>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>
#include <QtCore/QDateTime>
#include <memory>

#include "questdb_connector.h"

QT_BEGIN_NAMESPACE
class QAction;
class QMenu;
class QMenuBar;
class QStatusBar;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    explicit MainWindow(const QString &questdbHost, int questdbPort, const QString &apiKey = "", QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void refreshData();
    void onConnectionStatusChanged(bool connected);
    void onDataReceived(const QList<QVariantMap> &data);
    void startDataPipeline();
    void stopDataPipeline();
    void onAlphaVantageProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onAlphaVantageProcessError(QProcess::ProcessError error);

private:
    void setupUI();
    void setupMenuBar();
    void setupStatusBar();
    void startAlphaVantagePipeline();
    void stopAlphaVantagePipeline();
    void setupTableWidget();
    void setupChartView();
    void updateTable(const QList<QVariantMap> &data);
    void updateChart(const QList<QVariantMap> &data);
    void setupStyles();

    // UI Components
    QWidget *m_centralWidget;
    QSplitter *m_mainSplitter;
    QSplitter *m_rightSplitter;

    // Data Table
    QGroupBox *m_tableGroup;
    QTableWidget *m_dataTable;

    // Charts
    QGroupBox *m_chartGroup;
    QChartView *m_chartView;
    QChart *m_chart;
    QLineSeries *m_relianceSeries;
    QLineSeries *m_tcsSeries;
    QDateTimeAxis *m_axisX;
    QValueAxis *m_axisY;

    // Controls
    QGroupBox *m_controlGroup;
    QPushButton *m_startButton;
    QPushButton *m_stopButton;
    QPushButton *m_refreshButton;

    // Info Panel
    QGroupBox *m_infoGroup;
    QTextEdit *m_logTextEdit;
    QLabel *m_connectionStatusLabel;
    QLabel *m_lastUpdateLabel;
    QLabel *m_recordCountLabel;

    // Menu and Status
    QMenuBar *m_menuBar;
    QStatusBar *m_statusBar;
    QAction *m_exitAction;
    QAction *m_aboutAction;

    // Data handling
    std::unique_ptr<QuestDBConnector> m_questdbConnector;
    QTimer *m_refreshTimer;

    // Alpha Vantage Pipeline Process
    QProcess *m_alphaVantageProcess;
    QString m_apiKey;

    // Data tracking
    int m_totalRecords;
    QDateTime m_lastUpdate;
    bool m_isConnected;
};
