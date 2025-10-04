#include "main_window.h"
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtCore/QDebug>
#include <QtCore/QDateTime>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>

MainWindow::MainWindow(QWidget *parent)
    : MainWindow("localhost", 8812, "050HFLSF4NLRGMHW", parent)
{
}

MainWindow::MainWindow(const QString &questdbHost, int questdbPort, const QString &apiKey, QWidget *parent)
    : QMainWindow(parent), m_centralWidget(nullptr), m_questdbConnector(std::make_unique<QuestDBConnector>(this)), m_refreshTimer(new QTimer(this)), m_alphaVantageProcess(new QProcess(this)), m_apiKey(apiKey.isEmpty() ? "050HFLSF4NLRGMHW" : apiKey), m_totalRecords(0), m_isConnected(false)
{
    setWindowTitle("BSE Trading System - Real-time Market Data");
    setMinimumSize(1200, 800);
    resize(1400, 900);

    setupUI();
    setupMenuBar();
    setupStatusBar();
    setupStyles();

    // Connect QuestDB signals
    connect(m_questdbConnector.get(), &QuestDBConnector::connectionStatusChanged,
            this, &MainWindow::onConnectionStatusChanged);
    connect(m_questdbConnector.get(), &QuestDBConnector::dataReceived,
            this, &MainWindow::onDataReceived);
    connect(m_questdbConnector.get(), &QuestDBConnector::errorOccurred,
            [this](const QString &error)
            {
                m_logTextEdit->append(QString("[ERROR] %1: %2")
                                          .arg(QDateTime::currentDateTime().toString())
                                          .arg(error));
            });

    // Setup Alpha Vantage process signals
    connect(m_alphaVantageProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onAlphaVantageProcessFinished);
    connect(m_alphaVantageProcess, &QProcess::errorOccurred,
            this, &MainWindow::onAlphaVantageProcessError);

    // Setup refresh timer
    connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::refreshData);
    m_refreshTimer->setInterval(5000); // Refresh every 5 seconds

    // Try to connect to QuestDB with provided configuration
    if (m_questdbConnector->connectToDatabase(questdbHost, questdbPort))
    {
        m_logTextEdit->append(QString("[INFO] %1: Connected to QuestDB at %2:%3 successfully")
                                  .arg(QDateTime::currentDateTime().toString())
                                  .arg(questdbHost)
                                  .arg(questdbPort));
        refreshData();

        // Auto-start Alpha Vantage pipeline
        startAlphaVantagePipeline();
    }
    else
    {
        m_logTextEdit->append(QString("[WARNING] %1: Failed to connect to QuestDB at %2:%3. Please ensure QuestDB is running")
                                  .arg(QDateTime::currentDateTime().toString())
                                  .arg(questdbHost)
                                  .arg(questdbPort));
    }
}

MainWindow::~MainWindow()
{
    // Stop Alpha Vantage pipeline when closing
    stopAlphaVantagePipeline();
}

void MainWindow::setupUI()
{
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    // Main splitter - horizontal split
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);

    // Right splitter - vertical split for charts and controls
    m_rightSplitter = new QSplitter(Qt::Vertical, this);

    setupTableWidget();
    setupChartView();

    // Control Panel
    m_controlGroup = new QGroupBox("Controls", this);
    QVBoxLayout *controlLayout = new QVBoxLayout(m_controlGroup);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_startButton = new QPushButton("Start Pipeline", this);
    m_stopButton = new QPushButton("Stop Pipeline", this);
    m_refreshButton = new QPushButton("Refresh Data", this);

    buttonLayout->addWidget(m_startButton);
    buttonLayout->addWidget(m_stopButton);
    buttonLayout->addWidget(m_refreshButton);
    buttonLayout->addStretch();

    controlLayout->addLayout(buttonLayout);

    // Info Panel
    m_infoGroup = new QGroupBox("System Information", this);
    QVBoxLayout *infoLayout = new QVBoxLayout(m_infoGroup);

    m_connectionStatusLabel = new QLabel("Connection: Disconnected", this);
    m_lastUpdateLabel = new QLabel("Last Update: Never", this);
    m_recordCountLabel = new QLabel("Records: 0", this);

    infoLayout->addWidget(m_connectionStatusLabel);
    infoLayout->addWidget(m_lastUpdateLabel);
    infoLayout->addWidget(m_recordCountLabel);

    m_logTextEdit = new QTextEdit(this);
    m_logTextEdit->setMaximumHeight(150);
    m_logTextEdit->setReadOnly(true);
    infoLayout->addWidget(m_logTextEdit);

    // Layout assembly
    QVBoxLayout *rightLayout = new QVBoxLayout();
    rightLayout->addWidget(m_chartGroup);
    rightLayout->addWidget(m_controlGroup);
    rightLayout->addWidget(m_infoGroup);

    QWidget *rightWidget = new QWidget();
    rightWidget->setLayout(rightLayout);

    m_mainSplitter->addWidget(m_tableGroup);
    m_mainSplitter->addWidget(rightWidget);
    m_mainSplitter->setStretchFactor(0, 2); // Table takes more space
    m_mainSplitter->setStretchFactor(1, 1);

    QHBoxLayout *mainLayout = new QHBoxLayout(m_centralWidget);
    mainLayout->setContentsMargins(5, 5, 5, 5); // Add margins to prevent overlap
    mainLayout->addWidget(m_mainSplitter);

    // Connect button signals
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::startDataPipeline);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::stopDataPipeline);
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::refreshData);
}

void MainWindow::setupTableWidget()
{
    m_tableGroup = new QGroupBox("BSE Market Data", this);
    QVBoxLayout *tableLayout = new QVBoxLayout(m_tableGroup);
    tableLayout->setContentsMargins(8, 20, 8, 8); // Add top margin to prevent title overlap

    m_dataTable = new QTableWidget(this);
    m_dataTable->setColumnCount(7);

    QStringList headers;
    headers << "Symbol" << "Price" << "Volume" << "Side" << "Exchange" << "Timestamp" << "Status";
    m_dataTable->setHorizontalHeaderLabels(headers);

    // Configure table appearance
    m_dataTable->horizontalHeader()->setStretchLastSection(true);
    m_dataTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_dataTable->setAlternatingRowColors(true);
    m_dataTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_dataTable->setSortingEnabled(true);

    tableLayout->addWidget(m_dataTable);
}

void MainWindow::setupChartView()
{
    m_chartGroup = new QGroupBox("Price Charts", this);
    QVBoxLayout *chartLayout = new QVBoxLayout(m_chartGroup);

    // Create chart
    m_chart = new QChart();
    m_chart->setTitle("BSE Stock Prices - Real-time");
    m_chart->setAnimationOptions(QChart::SeriesAnimations);

    // Create series for different stocks
    m_relianceSeries = new QLineSeries();
    m_relianceSeries->setName("RELIANCE.BSE");
    m_relianceSeries->setPen(QPen(QColor(255, 0, 0), 2)); // Red line

    m_tcsSeries = new QLineSeries();
    m_tcsSeries->setName("TCS.BSE");
    m_tcsSeries->setPen(QPen(QColor(0, 150, 0), 2)); // Green line

    m_chart->addSeries(m_relianceSeries);
    m_chart->addSeries(m_tcsSeries);

    // Create axes
    m_axisX = new QDateTimeAxis();
    m_axisX->setFormat("hh:mm:ss");
    m_axisX->setTitleText("Time");
    m_chart->addAxis(m_axisX, Qt::AlignBottom);

    m_axisY = new QValueAxis();
    m_axisY->setTitleText("Price (INR)");
    m_chart->addAxis(m_axisY, Qt::AlignLeft);

    m_relianceSeries->attachAxis(m_axisX);
    m_relianceSeries->attachAxis(m_axisY);
    m_tcsSeries->attachAxis(m_axisX);
    m_tcsSeries->attachAxis(m_axisY);

    // Create chart view
    m_chartView = new QChartView(m_chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setMinimumHeight(300);

    chartLayout->addWidget(m_chartView);
}

void MainWindow::setupMenuBar()
{
    m_exitAction = new QAction("&Exit", this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);

    m_aboutAction = new QAction("&About", this);
    connect(m_aboutAction, &QAction::triggered, [this]()
            { QMessageBox::about(this, "About BSE Trading System",
                                 "BSE Trading System v1.0\n\n"
                                 "Real-time market data from Alpha Vantage\n"
                                 "Powered by QuestDB and Qt6\n\n"
                                 "© 2025 Interview Demo"); });

    QMenu *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction(m_exitAction);

    QMenu *helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction(m_aboutAction);
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage("Ready - Waiting for data...");
}

void MainWindow::setupStyles()
{
    setStyleSheet(R"(
        QMainWindow {
            background-color: #1e1e1e;
            color: #ffffff;
        }
        QWidget {
            background-color: #1e1e1e;
            color: #ffffff;
        }
        QSplitter {
            background-color: #1e1e1e;
            margin-top: 25px;
        }
        QSplitter::handle {
            background-color: #555555;
            width: 2px;
            height: 2px;
        }
        QSplitter::handle:horizontal {
            width: 2px;
            background-color: #555555;
        }
        QSplitter::handle:vertical {
            height: 2px;
            background-color: #555555;
        }
        QGroupBox {
            font-weight: bold;
            border: 2px solid #555555;
            border-radius: 5px;
            margin-top: 12px;
            padding-top: 18px;
            background-color: #2b2b2b;
            color: #ffffff;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 8px;
            padding: 2px 8px 2px 8px;
            color: #ffffff;
            background-color: transparent;
        }
        QLabel {
            background-color: transparent;
            color: #ffffff;
        }
        QPushButton {
            background-color: #4CAF50;
            color: white;
            border: none;
            padding: 8px 16px;
            text-align: center;
            font-size: 14px;
            border-radius: 4px;
        }
        QPushButton:hover {
            background-color: #45a049;
        }
        QPushButton:pressed {
            background-color: #3d8b40;
        }
        QPushButton:disabled {
            background-color: #666666;
            color: #cccccc;
        }
        QTableWidget {
            gridline-color: #555555;
            background-color: #2b2b2b;
            color: #ffffff;
            alternate-background-color: #3d3d3d;
        }
        QTableWidget::item {
            padding: 5px;
            background-color: #2b2b2b;
            color: #ffffff;
        }
        QTableWidget::item:selected {
            background-color: #4CAF50;
            color: #ffffff;
        }
        QHeaderView::section {
            background-color: #1e1e1e;
            color: #ffffff;
            padding: 8px;
            border: 1px solid #555555;
            font-weight: bold;
        }
        QTextEdit {
            background-color: #2b2b2b;
            color: #ffffff;
            font-family: monospace;
            border: 1px solid #555555;
        }
        QMenuBar {
            background-color: #2b2b2b;
            color: #ffffff;
            border-bottom: 1px solid #555555;
        }
        QMenuBar::item {
            background-color: transparent;
            padding: 4px 8px;
        }
        QMenuBar::item:selected {
            background-color: #4CAF50;
        }
        QMenu {
            background-color: #2b2b2b;
            color: #ffffff;
            border: 1px solid #555555;
        }
        QMenu::item:selected {
            background-color: #4CAF50;
        }
        QStatusBar {
            background-color: #2b2b2b;
            color: #ffffff;
            border-top: 1px solid #555555;
        }
    )");
}

void MainWindow::refreshData()
{
    if (!m_questdbConnector->isConnected())
    {
        m_logTextEdit->append(QString("[WARNING] %1: Not connected to QuestDB")
                                  .arg(QDateTime::currentDateTime().toString()));
        return;
    }

    m_questdbConnector->refreshData();
}

void MainWindow::onConnectionStatusChanged(bool connected)
{
    m_isConnected = connected;
    QString status = connected ? "Connected" : "Disconnected";
    QString color = connected ? "green" : "red";

    m_connectionStatusLabel->setText(QString("<span style='color: %1'>Connection: %2</span>")
                                         .arg(color)
                                         .arg(status));

    if (connected)
    {
        statusBar()->showMessage("Connected to QuestDB - Ready for real-time data");
        m_startButton->setEnabled(true);
    }
    else
    {
        statusBar()->showMessage("Disconnected from QuestDB");
        m_startButton->setEnabled(false);
        m_refreshTimer->stop();
    }
}

void MainWindow::onDataReceived(const QList<QVariantMap> &data)
{
    updateTable(data);
    updateChart(data);

    m_totalRecords = data.size();
    m_lastUpdate = QDateTime::currentDateTime();

    m_lastUpdateLabel->setText(QString("Last Update: %1").arg(m_lastUpdate.toString()));
    m_recordCountLabel->setText(QString("Records: %1").arg(m_totalRecords));

    statusBar()->showMessage(QString("Data updated - %1 records at %2")
                                 .arg(m_totalRecords)
                                 .arg(m_lastUpdate.toString("hh:mm:ss")));

    m_logTextEdit->append(QString("[INFO] %1: Updated %2 records")
                              .arg(QDateTime::currentDateTime().toString())
                              .arg(data.size()));
}

void MainWindow::updateTable(const QList<QVariantMap> &data)
{
    m_dataTable->setRowCount(data.size());

    for (int row = 0; row < data.size(); ++row)
    {
        const QVariantMap &record = data[row];

        m_dataTable->setItem(row, 0, new QTableWidgetItem(record["symbol"].toString()));
        m_dataTable->setItem(row, 1, new QTableWidgetItem(QString::number(record["price"].toDouble(), 'f', 2)));
        m_dataTable->setItem(row, 2, new QTableWidgetItem(record["volume"].toString()));

        // Side with color coding
        QString side = record["side"].toString();
        QTableWidgetItem *sideItem = new QTableWidgetItem(side);
        if (side == "B")
        {
            sideItem->setForeground(QColor(0, 150, 0)); // Green for buy
            sideItem->setText("BUY");
        }
        else if (side == "S")
        {
            sideItem->setForeground(QColor(200, 0, 0)); // Red for sell
            sideItem->setText("SELL");
        }
        m_dataTable->setItem(row, 3, sideItem);

        m_dataTable->setItem(row, 4, new QTableWidgetItem(record["exchange"].toString()));
        m_dataTable->setItem(row, 5, new QTableWidgetItem(record["timestamp"].toDateTime().toString()));
        m_dataTable->setItem(row, 6, new QTableWidgetItem("Live"));
    }
}

void MainWindow::updateChart(const QList<QVariantMap> &data)
{
    // Update time series data for charts
    for (const QVariantMap &record : data)
    {
        QString symbol = record["symbol"].toString();
        double price = record["price"].toDouble();
        QDateTime timestamp = record["timestamp"].toDateTime();

        qint64 timeMs = timestamp.toMSecsSinceEpoch();

        if (symbol == "RELIANCE")
        {
            m_relianceSeries->append(timeMs, price);
        }
        else if (symbol == "TCS")
        {
            m_tcsSeries->append(timeMs, price);
        }
    }

    // Keep only last 50 points for performance
    if (m_relianceSeries->count() > 50)
    {
        m_relianceSeries->removePoints(0, m_relianceSeries->count() - 50);
    }
    if (m_tcsSeries->count() > 50)
    {
        m_tcsSeries->removePoints(0, m_tcsSeries->count() - 50);
    }

    // Update axes ranges
    if (!m_relianceSeries->points().isEmpty() || !m_tcsSeries->points().isEmpty())
    {
        QDateTime now = QDateTime::currentDateTime();
        QDateTime startTime = now.addSecs(-300); // Show last 5 minutes

        m_axisX->setRange(startTime, now);

        // Auto-scale Y axis based on data
        double minPrice = std::numeric_limits<double>::max();
        double maxPrice = std::numeric_limits<double>::min();

        for (const QPointF &point : m_relianceSeries->points())
        {
            minPrice = std::min(minPrice, point.y());
            maxPrice = std::max(maxPrice, point.y());
        }
        for (const QPointF &point : m_tcsSeries->points())
        {
            minPrice = std::min(minPrice, point.y());
            maxPrice = std::max(maxPrice, point.y());
        }

        if (minPrice < maxPrice)
        {
            double margin = (maxPrice - minPrice) * 0.1; // 10% margin
            m_axisY->setRange(minPrice - margin, maxPrice + margin);
        }
    }
}

void MainWindow::startDataPipeline()
{
    if (!m_isConnected)
    {
        QMessageBox::warning(this, "Warning", "Please connect to QuestDB first!");
        return;
    }

    m_refreshTimer->start();
    m_startButton->setEnabled(false);
    m_stopButton->setEnabled(true);

    m_logTextEdit->append(QString("[INFO] %1: Data pipeline started - refreshing every 5 seconds")
                              .arg(QDateTime::currentDateTime().toString()));
    statusBar()->showMessage("Data pipeline running - Auto-refresh enabled");
}

void MainWindow::stopDataPipeline()
{
    m_refreshTimer->stop();
    m_startButton->setEnabled(true);
    m_stopButton->setEnabled(false);

    m_logTextEdit->append(QString("[INFO] %1: Data pipeline stopped")
                              .arg(QDateTime::currentDateTime().toString()));
    statusBar()->showMessage("Data pipeline stopped - Manual refresh only");
}

void MainWindow::startAlphaVantagePipeline()
{
    if (m_alphaVantageProcess->state() != QProcess::NotRunning)
    {
        m_logTextEdit->append(QString("[INFO] %1: Alpha Vantage pipeline is already running")
                                  .arg(QDateTime::currentDateTime().toString()));
        return;
    }

    // Use absolute path to the alpha_vantage_pipeline executable
    QString executablePath = "/Users/ishaanrathod/Code/Demo/build/alpha_vantage_pipeline";

    // Debug: Log the path being used
    m_logTextEdit->append(QString("[DEBUG] %1: Looking for Alpha Vantage pipeline at: %2")
                              .arg(QDateTime::currentDateTime().toString())
                              .arg(executablePath));

    QStringList arguments;
    arguments << m_apiKey;

    m_logTextEdit->append(QString("[INFO] %1: Starting Alpha Vantage pipeline...")
                              .arg(QDateTime::currentDateTime().toString()));

    m_alphaVantageProcess->start(executablePath, arguments);

    if (!m_alphaVantageProcess->waitForStarted(3000))
    {
        m_logTextEdit->append(QString("[ERROR] %1: Failed to start Alpha Vantage pipeline: %2")
                                  .arg(QDateTime::currentDateTime().toString())
                                  .arg(m_alphaVantageProcess->errorString()));
    }
    else
    {
        m_logTextEdit->append(QString("[INFO] %1: Alpha Vantage pipeline started successfully (PID: %2)")
                                  .arg(QDateTime::currentDateTime().toString())
                                  .arg(m_alphaVantageProcess->processId()));
        statusBar()->showMessage("Alpha Vantage pipeline running - Fetching live BSE data");
    }
}

void MainWindow::stopAlphaVantagePipeline()
{
    if (m_alphaVantageProcess->state() == QProcess::NotRunning)
    {
        return;
    }

    m_logTextEdit->append(QString("[INFO] %1: Stopping Alpha Vantage pipeline...")
                              .arg(QDateTime::currentDateTime().toString()));

    m_alphaVantageProcess->terminate();

    if (!m_alphaVantageProcess->waitForFinished(5000))
    {
        m_logTextEdit->append(QString("[WARNING] %1: Force killing Alpha Vantage pipeline")
                                  .arg(QDateTime::currentDateTime().toString()));
        m_alphaVantageProcess->kill();
        m_alphaVantageProcess->waitForFinished(3000);
    }

    m_logTextEdit->append(QString("[INFO] %1: Alpha Vantage pipeline stopped")
                              .arg(QDateTime::currentDateTime().toString()));
    statusBar()->showMessage("Alpha Vantage pipeline stopped");
}

void MainWindow::onAlphaVantageProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QString statusStr = (exitStatus == QProcess::NormalExit) ? "Normal" : "Crashed";
    m_logTextEdit->append(QString("[INFO] %1: Alpha Vantage pipeline finished - Exit code: %2, Status: %3")
                              .arg(QDateTime::currentDateTime().toString())
                              .arg(exitCode)
                              .arg(statusStr));

    if (exitStatus == QProcess::CrashExit)
    {
        statusBar()->showMessage("Alpha Vantage pipeline crashed - No live data updates");
    }
}

void MainWindow::onAlphaVantageProcessError(QProcess::ProcessError error)
{
    QString errorStr;
    switch (error)
    {
    case QProcess::FailedToStart:
        errorStr = "Failed to start";
        break;
    case QProcess::Crashed:
        errorStr = "Process crashed";
        break;
    case QProcess::Timedout:
        errorStr = "Process timed out";
        break;
    case QProcess::WriteError:
        errorStr = "Write error";
        break;
    case QProcess::ReadError:
        errorStr = "Read error";
        break;
    default:
        errorStr = "Unknown error";
        break;
    }

    m_logTextEdit->append(QString("[ERROR] %1: Alpha Vantage pipeline error: %2")
                              .arg(QDateTime::currentDateTime().toString())
                              .arg(errorStr));
    statusBar()->showMessage("Alpha Vantage pipeline error - Check logs");
}
