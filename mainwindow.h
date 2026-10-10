
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QMap>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QSet>
#include <QSize>
#include <QList>

class QComboBox;
class QPushButton;
class QLabel;
class QTableWidget;
class QTextEdit;
class QScrollArea;
class QWidget;
class QGridLayout;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void createFPGA();
    void loadNetlistFile();
    void loadSampleNetlist();
    void analyzeNetlist();
    void runFirstFit();
    void runBestFit();
    void clearAll();

private:
    enum class ResourceType
    {
        CLB, FF, DSP, BRAM, EMPTY
    };

    struct Cell
    {
        ResourceType type = ResourceType::EMPTY;
        QString occupant;
    };

    struct Node
    {
        QString id;
        ResourceType type = ResourceType::CLB;
        int row = -1;
        int col = -1;
    };

    struct Net
    {
        QString name;
        QStringList nodes;
    };

    struct Route
    {
        QString netName;
        QList<QPoint> path;
    };

    struct PlacementResult
    {
        bool success = false;
        int placementCost = 0;
        int totalWireLength = 0;
        int maxNetLength = 0;
        double congestionPercent = 0.0;
        int routedNets = 0;
        int failedNets = 0;
        QMap<QString, QPoint> positions;
        QList<Route> routes;
    };

    // OOP: Abstract placement algorithm
    class PlacementAlgorithm
    {
    public:
        virtual ~PlacementAlgorithm() = default;
        virtual PlacementResult execute(MainWindow &window) = 0;
    };

    class FirstFitAlgorithm : public PlacementAlgorithm
    {
    public:
        PlacementResult execute(MainWindow &window) override;
    };

    class BestFitAlgorithm : public PlacementAlgorithm
    {
    public:
        PlacementResult execute(MainWindow &window) override;
    };

    // UI
    QWidget *page = nullptr;
    QScrollArea *mainScroll = nullptr;

    QComboBox *architectureBox = nullptr;
    QLabel *autoSizeLabel = nullptr;
    QPushButton *createFPGAButton = nullptr;

    QPushButton *loadFileButton = nullptr;
    QPushButton *sampleButton = nullptr;
    QPushButton *analyzeButton = nullptr;
    QTextEdit *netlistEdit = nullptr;

    QLabel *lutCountLabel = nullptr;
    QLabel *ffCountLabel = nullptr;
    QLabel *dspCountLabel = nullptr;
    QLabel *bramCountLabel = nullptr;
    QLabel *netCountLabel = nullptr;

    QScrollArea *floorScroll = nullptr;
    QWidget *floorWidget = nullptr;
    QGridLayout *floorLayout = nullptr;

    QLabel *placementCostLabel = nullptr;
    QLabel *wireLengthLabel = nullptr;
    QLabel *maxNetLabel = nullptr;
    QLabel *congestionLabel = nullptr;
    QLabel *routingLabel = nullptr;
    QLabel *statusLabel = nullptr;

    QPushButton *firstFitButton = nullptr;
    QPushButton *bestFitButton = nullptr;
    QPushButton *clearButton = nullptr;

    QTableWidget *nodeTable = nullptr;
    QTableWidget *netTable = nullptr;

    // FPGA model
    int fpgaRows = 6;
    int fpgaColumns = 6;

    QVector<QVector<Cell>> grid;
    QVector<Node> nodes;
    QVector<Net> nets;

    // UI helpers
    void buildInterface();
    void rebuildFloorplan();
    void updateCell(int r, int c);
    void updateAllCells();
    void updateSummary();
    void setStatus(const QString &text);
    void clearResults();

    // Architecture and automatic sizing
    ResourceType architectureResource(int row, int col) const;
    QString resourceName(ResourceType type) const;

    QSize calculateAutomaticFPGASize() const;
    bool hasEnoughResources(int rows, int columns) const;

    // Netlist
    bool parseNetlist(const QString &text, QString &error);
    ResourceType parseResourceType(const QString &token) const;
    void populateNodeTable();
    void populateNetTable();

    // Placement
    PlacementResult placeFirstFit();
    PlacementResult placeBestFit();

    bool canPlace(const Node &node, int r, int c) const;

    int connectivityScore(
        const Node &node,
        int r,
        int c,
        const QMap<QString, QPoint> &positions) const;

    void applyPlacement(const PlacementResult &result);

    int calculatePlacementCost(
        const QMap<QString, QPoint> &positions) const;

    void clearOccupancy();

    // Routing
    void routePlacement(PlacementResult &result);

    QList<QPoint> shortestRoute(
        const QPoint &source,
        const QSet<QPoint> &targets,
        const QVector<QVector<int>> &edgeRight,
        const QVector<QVector<int>> &edgeDown,
        bool *ok) const;

    int pathLength(const QList<QPoint> &path) const;
    void drawRoutes(const QList<Route> &routes);
    int edgeCapacity() const;

    // Metrics
    int netBoundingLength(
        const Net &net,
        const QMap<QString, QPoint> &positions) const;
};

#endif
