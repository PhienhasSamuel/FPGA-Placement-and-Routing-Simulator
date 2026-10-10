#include "mainwindow.h"

#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QFrame>
#include <QRegularExpression>
#include <QApplication>
#include <QPainter>
#include <QPen>
#include <QAbstractItemView>
#include <algorithm>
#include <cmath>
#include <climits>
#include <queue>

namespace {
struct State {
    int cost;
    QPoint p;
    bool operator<(const State &other) const { return cost > other.cost; }
};

// Qt does not provide a portable std::priority_queue hash for QPoint,
// so a tiny binary heap is used locally.
using MinHeap = std::priority_queue<State>;

static int manhattan(const QPoint &a, const QPoint &b)
{
    return std::abs(a.x() - b.x()) + std::abs(a.y() - b.y());
}

static int manhattan(int x1, int y1, int x2, int y2)
{
    return std::abs(x1 - x2) + std::abs(y1 - y2);
}

static QString pointText(const QPoint &p)
{
    return QString("(%1,%2)").arg(p.y()).arg(p.x());
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("FPGA Resource Allocation and Placement Simulator");
    resize(1280, 950);
    setMinimumSize(900, 650);

    buildInterface();
    loadSampleNetlist();
}

void MainWindow::buildInterface()
{
    page = new QWidget;
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(14, 12, 14, 28);
    pageLayout->setSpacing(10);

    auto *title = new QLabel("FPGA RESOURCE ALLOCATION AND PLACEMENT SIMULATOR");
    QFont titleFont("Arial", 23, QFont::Bold);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignCenter);
    pageLayout->addWidget(title);

    // FPGA configuration
    auto *archBox = new QGroupBox("FPGA Configuration");
    auto *archLayout = new QHBoxLayout(archBox);

    archLayout->addWidget(new QLabel("Architecture:"));
    architectureBox = new QComboBox;
    architectureBox->addItems({
        "Generic FPGA",
        "Balanced FPGA",
        "DSP-Optimized FPGA",
        "BRAM-Optimized FPGA"
    });
    architectureBox->setMinimumWidth(210);
    archLayout->addWidget(architectureBox);

    autoSizeLabel = new QLabel("FPGA Size: Waiting for netlist");
    autoSizeLabel->setMinimumWidth(260);
    QFont sizeFont = autoSizeLabel->font();
    sizeFont.setBold(true);
    autoSizeLabel->setFont(sizeFont);
    archLayout->addWidget(autoSizeLabel);

    createFPGAButton = new QPushButton("Recalculate FPGA Size");
    archLayout->addWidget(createFPGAButton);
    archLayout->addStretch();
    pageLayout->addWidget(archBox);

    // Netlist
    auto *inputBox = new QGroupBox("Design / Netlist Input");
    auto *inputLayout = new QVBoxLayout(inputBox);
    auto *buttonRow = new QHBoxLayout;

    loadFileButton = new QPushButton("Load Netlist File...");
    sampleButton = new QPushButton("Load Sample Netlist");
    analyzeButton = new QPushButton("Analyze Netlist");

    buttonRow->addWidget(loadFileButton);
    buttonRow->addWidget(sampleButton);
    buttonRow->addWidget(analyzeButton);
    buttonRow->addStretch();
    inputLayout->addLayout(buttonRow);

    netlistEdit = new QTextEdit;
    netlistEdit->setAcceptRichText(false);
    netlistEdit->setMinimumHeight(170);
    netlistEdit->setMaximumHeight(250);
    netlistEdit->setPlaceholderText(
        "Technology-mapped / normalized netlist:\n"
        "LUT6 lut0 (...);\nFDRE ff0 (...);\nDSP48E1 dsp0 (...);\n"
        "RAMB18E1 bram0 (...);\nNET n0 lut0 ff0;\n"
        );
    inputLayout->addWidget(netlistEdit);
    pageLayout->addWidget(inputBox);

    // Resource summary
    auto *summaryBox = new QGroupBox("Extracted Netlist Resources");
    auto *summaryLayout = new QHBoxLayout(summaryBox);

    lutCountLabel = new QLabel("LUT: 0");
    ffCountLabel = new QLabel("FF: 0");
    dspCountLabel = new QLabel("DSP: 0");
    bramCountLabel = new QLabel("BRAM: 0");
    netCountLabel = new QLabel("Nets: 0");

    for (QLabel *label : {lutCountLabel, ffCountLabel, dspCountLabel,
                          bramCountLabel, netCountLabel}) {
        label->setAlignment(Qt::AlignCenter);
        label->setMinimumWidth(120);
        QFont f = label->font();
        f.setBold(true);
        label->setFont(f);
        summaryLayout->addWidget(label);
    }
    pageLayout->addWidget(summaryBox);

    // Floorplan
    auto *floorBox = new QGroupBox("FPGA Floorplan / Placement / Routing");
    auto *floorBoxLayout = new QVBoxLayout(floorBox);

    auto *info = new QLabel(
        "Gray = CLB, blue = DSP, orange = BRAM. "
        "After placement, green cells contain mapped netlist resources. "
        "Routes are drawn as red paths between connected cells."
        );
    info->setWordWrap(true);
    floorBoxLayout->addWidget(info);

    floorScroll = new QScrollArea;
    floorScroll->setWidgetResizable(false);
    floorScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    floorScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    floorScroll->setMinimumHeight(330);
    floorScroll->setMaximumHeight(520);

    floorWidget = new QWidget;
    floorLayout = new QGridLayout(floorWidget);
    floorLayout->setContentsMargins(14, 14, 14, 14);
    floorLayout->setSpacing(5);

    floorScroll->setWidget(floorWidget);
    floorBoxLayout->addWidget(floorScroll);
    pageLayout->addWidget(floorBox);

    // Parsed cells
    auto *nodesBox = new QGroupBox("Parsed Netlist Cells");
    auto *nodesLayout = new QVBoxLayout(nodesBox);

    nodeTable = new QTableWidget;
    nodeTable->setColumnCount(3);
    nodeTable->setHorizontalHeaderLabels({"Instance", "Resource", "Placement"});
    nodeTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    nodeTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    nodeTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    nodeTable->setMinimumHeight(150);
    nodeTable->setMaximumHeight(230);
    nodeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    nodesLayout->addWidget(nodeTable);
    pageLayout->addWidget(nodesBox);

    // Nets
    auto *netsBox = new QGroupBox("Parsed Net Connectivity");
    auto *netsLayout = new QVBoxLayout(netsBox);

    netTable = new QTableWidget;
    netTable->setColumnCount(2);
    netTable->setHorizontalHeaderLabels({"Net", "Connected Instances"});
    netTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    netTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    netTable->setMinimumHeight(120);
    netTable->setMaximumHeight(210);
    netTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    netsLayout->addWidget(netTable);
    pageLayout->addWidget(netsBox);

    // Buttons
    auto *controls = new QHBoxLayout;
    firstFitButton = new QPushButton("Run First-Fit Placement + Routing");
    bestFitButton = new QPushButton("Run Connectivity-Aware Best-Fit + Routing");
    clearButton = new QPushButton("Clear / Reset");

    firstFitButton->setMinimumHeight(38);
    bestFitButton->setMinimumHeight(38);
    clearButton->setMinimumHeight(38);

    controls->addWidget(firstFitButton);
    controls->addWidget(bestFitButton);
    controls->addWidget(clearButton);
    pageLayout->addLayout(controls);

    // Results
    auto *resultBox = new QGroupBox("Placement and Routing Analysis");
    auto *resultLayout = new QGridLayout(resultBox);

    placementCostLabel = new QLabel("Placement Cost: --");
    wireLengthLabel = new QLabel("Actual Routed Wirelength: --");
    maxNetLabel = new QLabel("Maximum Routed Net Length: --");
    congestionLabel = new QLabel("Routing Congestion: --");
    routingLabel = new QLabel("Routed Nets: --");
    statusLabel = new QLabel("Status: Ready");

    const QList<QLabel*> labels = {
        placementCostLabel, wireLengthLabel, maxNetLabel,
        congestionLabel, routingLabel, statusLabel
    };

    for (int i = 0; i < labels.size(); ++i) {
        labels[i]->setFrameShape(QFrame::StyledPanel);
        labels[i]->setAlignment(Qt::AlignCenter);
        labels[i]->setMinimumHeight(34);
        resultLayout->addWidget(labels[i], i / 3, i % 3);
    }
    pageLayout->addWidget(resultBox);

    // Outer scroll area prevents maximize/minimize shrink problems.
    mainScroll = new QScrollArea;
    mainScroll->setWidgetResizable(true);
    mainScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    mainScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    mainScroll->setWidget(page);
    setCentralWidget(mainScroll);

    connect(createFPGAButton, &QPushButton::clicked, this, &MainWindow::createFPGA);
    connect(loadFileButton, &QPushButton::clicked, this, &MainWindow::loadNetlistFile);
    connect(sampleButton, &QPushButton::clicked, this, &MainWindow::loadSampleNetlist);
    connect(analyzeButton, &QPushButton::clicked, this, &MainWindow::analyzeNetlist);
    connect(firstFitButton, &QPushButton::clicked, this, &MainWindow::runFirstFit);
    connect(bestFitButton, &QPushButton::clicked, this, &MainWindow::runBestFit);
    connect(clearButton, &QPushButton::clicked, this, &MainWindow::clearAll);

    connect(architectureBox, &QComboBox::currentTextChanged,
            this, [this](const QString &) { createFPGA(); });

    setStyleSheet(R"(
        QGroupBox {
            font-weight: bold;
            border: 1px solid #9a9a9a;
            border-radius: 5px;
            margin-top: 8px;
            padding-top: 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 4px;
        }
        QPushButton {
            min-height: 30px;
            padding: 4px 12px;
        }
        QTableWidget {
            gridline-color: #c8c8c8;
        }
    )");
}

void MainWindow::createFPGA()
{
    // Without an analyzed netlist, show a small default grid.
    if (nodes.isEmpty()) {
        fpgaRows = 6;
        fpgaColumns = 6;

        if (autoSizeLabel)
            autoSizeLabel->setText("FPGA Size: 6 x 6 (default)");
    } else {
        const QSize size = calculateAutomaticFPGASize();

        if (!size.isValid()) {
            QMessageBox::warning(
                this,
                "FPGA Size Error",
                "The selected architecture could not provide enough "
                "compatible resources within the supported search range."
                );
            return;
        }

        fpgaRows = size.height();
        fpgaColumns = size.width();

        if (autoSizeLabel) {
            autoSizeLabel->setText(
                QString("FPGA Size: %1 x %2 (automatic)")
                    .arg(fpgaRows)
                    .arg(fpgaColumns)
                );
        }
    }

    grid.resize(fpgaRows);
    for (int r = 0; r < fpgaRows; ++r) {
        grid[r].resize(fpgaColumns);
        for (int c = 0; c < fpgaColumns; ++c) {
            grid[r][c].type = architectureResource(r, c);
            grid[r][c].occupant.clear();
        }
    }

    clearResults();
    rebuildFloorplan();
    updateSummary();

    setStatus(QString("FPGA created: %1 (%2 x %3)")
                  .arg(architectureBox->currentText())
                  .arg(fpgaRows)
                  .arg(fpgaColumns));
}

bool MainWindow::hasEnoughResources(int rows, int columns) const
{
    int requiredCLBSites = 0;
    int requiredDSP = 0;
    int requiredBRAM = 0;

    // Current simulator abstraction: each LUT/CLB and each FF
    // occupies one CLB site. DSP and BRAM each occupy their own site.
    for (const Node &node : nodes) {
        switch (node.type) {
        case ResourceType::CLB:
        case ResourceType::FF:
            ++requiredCLBSites;
            break;
        case ResourceType::DSP:
            ++requiredDSP;
            break;
        case ResourceType::BRAM:
            ++requiredBRAM;
            break;
        default:
            break;
        }
    }

    int availableCLB = 0;
    int availableDSP = 0;
    int availableBRAM = 0;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < columns; ++c) {
            switch (architectureResource(r, c)) {
            case ResourceType::CLB:
                ++availableCLB;
                break;
            case ResourceType::DSP:
                ++availableDSP;
                break;
            case ResourceType::BRAM:
                ++availableBRAM;
                break;
            default:
                break;
            }
        }
    }

    return availableCLB >= requiredCLBSites
           && availableDSP >= requiredDSP
           && availableBRAM >= requiredBRAM;
}

QSize MainWindow::calculateAutomaticFPGASize() const
{
    if (nodes.isEmpty())
        return QSize(6, 6);

    int bestRows = -1;
    int bestColumns = -1;
    int bestArea = INT_MAX;
    int bestAspectDifference = INT_MAX;

    // All four architecture patterns repeat every six columns.
    // Search a bounded set of practical dimensions and choose the
    // minimum-area grid that meets every resource requirement.
    for (int rows = 2; rows <= 50; ++rows) {
        for (int columns = 6; columns <= 120; columns += 6) {
            if (!hasEnoughResources(rows, columns))
                continue;

            const int area = rows * columns;
            const int aspectDifference = std::abs(rows - columns);

            if (area < bestArea ||
                (area == bestArea &&
                 aspectDifference < bestAspectDifference)) {
                bestRows = rows;
                bestColumns = columns;
                bestArea = area;
                bestAspectDifference = aspectDifference;
            }
        }
    }

    if (bestRows < 0 || bestColumns < 0)
        return QSize();

    // QSize stores width first (columns), then height (rows).
    return QSize(bestColumns, bestRows);
}

MainWindow::ResourceType MainWindow::architectureResource(int row, int col) const
{
    Q_UNUSED(row);
    const QString arch = architectureBox->currentText();
    const int m = col % 6;

    if (arch == "Generic FPGA") {
        static const ResourceType pattern[] = {
            ResourceType::CLB, ResourceType::CLB, ResourceType::DSP,
            ResourceType::CLB, ResourceType::BRAM, ResourceType::CLB
        };
        return pattern[m];
    }

    if (arch == "Balanced FPGA") {
        if (m == 1) return ResourceType::DSP;
        if (m == 4) return ResourceType::BRAM;
        return ResourceType::CLB;
    }

    if (arch == "DSP-Optimized FPGA") {
        if (m == 1 || m == 3) return ResourceType::DSP;
        if (m == 5) return ResourceType::BRAM;
        return ResourceType::CLB;
    }

    if (m == 1 || m == 4) return ResourceType::BRAM;
    if (m == 3) return ResourceType::DSP;
    return ResourceType::CLB;
}

QString MainWindow::resourceName(ResourceType type) const
{
    switch (type) {
    case ResourceType::CLB: return "CLB";
    case ResourceType::FF: return "FF";
    case ResourceType::DSP: return "DSP";
    case ResourceType::BRAM: return "BRAM";
    default: return "";
    }
}

void MainWindow::rebuildFloorplan()
{
    while (QLayoutItem *item = floorLayout->takeAt(0)) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }

    const int cellW = 125;
    const int cellH = 65;

    floorWidget->setMinimumSize(
        fpgaColumns * (cellW + 5) + 35,
        fpgaRows * (cellH + 5) + 35);

    for (int r = 0; r < fpgaRows; ++r) {
        for (int c = 0; c < fpgaColumns; ++c) {
            auto *label = new QLabel;
            label->setFixedSize(cellW, cellH);
            label->setAlignment(Qt::AlignCenter);
            label->setFrameShape(QFrame::Box);
            label->setLineWidth(1);
            floorLayout->addWidget(label, r, c);
            updateCell(r, c);
        }
    }
}

void MainWindow::updateCell(int r, int c)
{
    if (r < 0 || r >= fpgaRows || c < 0 || c >= fpgaColumns)
        return;

    QLayoutItem *item = floorLayout->itemAtPosition(r, c);
    if (!item)
        return;

    auto *label = qobject_cast<QLabel*>(item->widget());
    if (!label)
        return;

    const Cell &cell = grid[r][c];

    QString text = resourceName(cell.type);
    QString style = "font-weight: bold; border: 1px solid #888;";

    if (!cell.occupant.isEmpty()) {
        text = cell.occupant;
        style += " background:#b7e4b5; color:#111;";
    } else if (cell.type == ResourceType::DSP) {
        style += " background:#cfe2ff;";
    } else if (cell.type == ResourceType::BRAM) {
        style += " background:#ffe0bd;";
    } else {
        style += " background:#eeeeee;";
    }

    label->setText(text);
    label->setStyleSheet(style);
}

void MainWindow::updateAllCells()
{
    for (int r = 0; r < fpgaRows; ++r)
        for (int c = 0; c < fpgaColumns; ++c)
            updateCell(r, c);
}

void MainWindow::updateSummary()
{
    int lut = 0, ff = 0, dsp = 0, bram = 0;

    for (const Node &n : nodes) {
        switch (n.type) {
        case ResourceType::CLB: ++lut; break;
        case ResourceType::FF: ++ff; break;
        case ResourceType::DSP: ++dsp; break;
        case ResourceType::BRAM: ++bram; break;
        default: break;
        }
    }

    lutCountLabel->setText(QString("LUT/CLB: %1").arg(lut));
    ffCountLabel->setText(QString("FF: %1").arg(ff));
    dspCountLabel->setText(QString("DSP: %1").arg(dsp));
    bramCountLabel->setText(QString("BRAM: %1").arg(bram));
    netCountLabel->setText(QString("Nets: %1").arg(nets.size()));
}

void MainWindow::setStatus(const QString &text)
{
    statusLabel->setText("Status: " + text);
}

void MainWindow::clearResults()
{
    placementCostLabel->setText("Placement Cost: --");
    wireLengthLabel->setText("Actual Routed Wirelength: --");
    maxNetLabel->setText("Maximum Routed Net Length: --");
    congestionLabel->setText("Routing Congestion: --");
    routingLabel->setText("Routed Nets: --");
}

void MainWindow::clearOccupancy()
{
    for (auto &row : grid)
        for (auto &cell : row)
            cell.occupant.clear();

    for (Node &n : nodes) {
        n.row = -1;
        n.col = -1;
    }
}

MainWindow::ResourceType MainWindow::parseResourceType(const QString &token) const
{
    const QString t = token.trimmed().toUpper();

    if (t.startsWith("LUT") || t == "CLB")
        return ResourceType::CLB;

    if (t.startsWith("FD") || t.startsWith("FF"))
        return ResourceType::FF;

    if (t.startsWith("DSP"))
        return ResourceType::DSP;

    if (t.startsWith("RAMB") || t.startsWith("BRAM"))
        return ResourceType::BRAM;

    return ResourceType::EMPTY;
}

bool MainWindow::parseNetlist(const QString &text, QString &error)
{
    nodes.clear();
    nets.clear();

    QSet<QString> knownNodes;
    const QStringList lines = text.split(QRegularExpression("[\\r\\n]"),
                                         Qt::SkipEmptyParts);

    QRegularExpression instanceRx(
        R"(^\s*([A-Za-z_][A-Za-z0-9_]*)\s+([A-Za-z_][A-Za-z0-9_]*)\b)"
        );

    QRegularExpression netRx(
        R"(^\s*NET\s+([A-Za-z_][A-Za-z0-9_]*)(.*)$)",
        QRegularExpression::CaseInsensitiveOption
        );

    for (QString line : lines) {
        line = line.trimmed();

        if (line.isEmpty() || line.startsWith("#") || line.startsWith("//"))
            continue;

        auto netMatch = netRx.match(line);
        if (netMatch.hasMatch()) {
            Net net;
            net.name = netMatch.captured(1);

            QString rest = netMatch.captured(2);
            rest.remove(';');
            rest.remove('(');
            rest.remove(')');
            rest.replace(',', ' ');

            const QStringList tokens =
                rest.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

            for (const QString &token : tokens) {
                if (knownNodes.contains(token))
                    net.nodes.append(token);
            }

            if (net.nodes.size() >= 2)
                nets.append(net);

            continue;
        }

        auto match = instanceRx.match(line);
        if (!match.hasMatch())
            continue;

        const QString typeToken = match.captured(1);
        const QString instance = match.captured(2);

        const ResourceType type = parseResourceType(typeToken);
        if (type == ResourceType::EMPTY)
            continue;

        if (knownNodes.contains(instance)) {
            error = "Duplicate instance: " + instance;
            return false;
        }

        Node node;
        node.id = instance;
        node.type = type;
        nodes.append(node);
        knownNodes.insert(instance);
    }

    if (nodes.isEmpty()) {
        error = "No supported technology-mapped instances were found.";
        return false;
    }

    return true;
}

void MainWindow::populateNodeTable()
{
    nodeTable->setRowCount(nodes.size());

    for (int i = 0; i < nodes.size(); ++i) {
        nodeTable->setItem(i, 0, new QTableWidgetItem(nodes[i].id));
        nodeTable->setItem(i, 1,
                           new QTableWidgetItem(resourceName(nodes[i].type)));
        nodeTable->setItem(i, 2, new QTableWidgetItem("--"));
    }
}

void MainWindow::populateNetTable()
{
    netTable->setRowCount(nets.size());

    for (int i = 0; i < nets.size(); ++i) {
        netTable->setItem(i, 0, new QTableWidgetItem(nets[i].name));
        netTable->setItem(i, 1,
                          new QTableWidgetItem(nets[i].nodes.join(" -> ")));
    }
}

void MainWindow::loadNetlistFile()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this, "Open Netlist", QString(),
        "Netlist/Text Files (*.txt *.net *.v *.vhd);;All Files (*)");

    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Open Error",
                             "Could not open the selected file.");
        return;
    }

    netlistEdit->setPlainText(QString::fromUtf8(file.readAll()));
    file.close();

    analyzeNetlist();
}

void MainWindow::loadSampleNetlist()
{
    const QString sample =
        "# Sample technology-mapped FPGA netlist\n"
        "LUT6 lut0\n"
        "LUT6 lut1\n"
        "LUT6 lut2\n"
        "LUT6 lut3\n"
        "LUT6 lut4\n"
        "LUT6 lut5\n"
        "LUT6 lut6\n"
        "LUT6 lut7\n"
        "FDRE ff0\n"
        "FDRE ff1\n"
        "FDRE ff2\n"
        "FDRE ff3\n"
        "DSP48E1 dsp0\n"
        "DSP48E1 dsp1\n"
        "RAMB18E1 bram0\n"
        "NET n0 lut0 ff0\n"
        "NET n1 lut1 ff0\n"
        "NET n2 lut2 ff1\n"
        "NET n3 lut3 ff1\n"
        "NET n4 ff0 dsp0\n"
        "NET n5 ff1 dsp0\n"
        "NET n6 lut4 ff2\n"
        "NET n7 lut5 ff2\n"
        "NET n8 ff2 dsp1\n"
        "NET n9 lut6 ff3\n"
        "NET n10 lut7 ff3\n"
        "NET n11 ff3 dsp1\n"
        "NET n12 dsp0 bram0\n"
        "NET n13 dsp1 bram0\n";

    netlistEdit->setPlainText(sample);
    analyzeNetlist();
}

void MainWindow::analyzeNetlist()
{
    QString error;
    if (!parseNetlist(netlistEdit->toPlainText(), error)) {
        QMessageBox::warning(this, "Netlist Error", error);
        return;
    }

    populateNodeTable();
    populateNetTable();
    updateSummary();

    // The parsed netlist determines the required FPGA dimensions.
    createFPGA();

    setStatus(QString("Parsed %1 instances and %2 connected nets; FPGA resized automatically.")
                  .arg(nodes.size())
                  .arg(nets.size()));
}

bool MainWindow::canPlace(const Node &node, int r, int c) const
{
    if (r < 0 || r >= fpgaRows || c < 0 || c >= fpgaColumns)
        return false;

    if (!grid[r][c].occupant.isEmpty())
        return false;

    const ResourceType site = grid[r][c].type;

    if (node.type == ResourceType::CLB || node.type == ResourceType::FF)
        return site == ResourceType::CLB;

    return site == node.type;
}

int MainWindow::connectivityScore(
    const Node &node, int r, int c,
    const QMap<QString, QPoint> &positions) const
{
    int score = 0;

    for (const Net &net : nets) {
        if (!net.nodes.contains(node.id))
            continue;

        for (const QString &other : net.nodes) {
            if (other == node.id || !positions.contains(other))
                continue;

            const QPoint p = positions.value(other);
            score += manhattan(c, r, p.x(), p.y());
        }
    }

    return score;
}

MainWindow::PlacementResult MainWindow::placeFirstFit()
{
    PlacementResult result;
    QMap<QString, QPoint> positions;

    clearOccupancy();

    for (const Node &node : nodes) {
        bool placed = false;

        for (int r = 0; r < fpgaRows && !placed; ++r) {
            for (int c = 0; c < fpgaColumns && !placed; ++c) {
                if (!canPlace(node, r, c))
                    continue;

                grid[r][c].occupant = node.id;
                positions[node.id] = QPoint(c, r);
                placed = true;
            }
        }

        if (!placed) {
            result.success = false;
            result.positions = positions;
            return result;
        }
    }

    result.success = true;
    result.positions = positions;
    result.placementCost = calculatePlacementCost(positions);

    routePlacement(result);
    return result;
}

MainWindow::PlacementResult MainWindow::placeBestFit()
{
    PlacementResult result;
    QMap<QString, QPoint> positions;

    clearOccupancy();

    QVector<Node> order = nodes;

    std::sort(order.begin(), order.end(), [this](const Node &a, const Node &b) {
        int da = 0, db = 0;

        for (const Net &net : nets) {
            if (net.nodes.contains(a.id))
                da += net.nodes.size();
            if (net.nodes.contains(b.id))
                db += net.nodes.size();
        }

        return da > db;
    });

    for (const Node &node : order) {
        int bestScore = INT_MAX;
        int bestR = -1;
        int bestC = -1;

        for (int r = 0; r < fpgaRows; ++r) {
            for (int c = 0; c < fpgaColumns; ++c) {
                if (!canPlace(node, r, c))
                    continue;

                const int score = connectivityScore(node, r, c, positions);

                if (score < bestScore) {
                    bestScore = score;
                    bestR = r;
                    bestC = c;
                }
            }
        }

        if (bestR < 0) {
            result.success = false;
            result.positions = positions;
            return result;
        }

        grid[bestR][bestC].occupant = node.id;
        positions[node.id] = QPoint(bestC, bestR);
    }

    result.success = true;
    result.positions = positions;
    result.placementCost = calculatePlacementCost(positions);

    routePlacement(result);
    return result;
}

int MainWindow::calculatePlacementCost(
    const QMap<QString, QPoint> &positions) const
{
    int cost = 0;

    for (const Net &net : nets) {
        if (net.nodes.size() < 2)
            continue;

        for (int i = 1; i < net.nodes.size(); ++i) {
            if (!positions.contains(net.nodes[0]) ||
                !positions.contains(net.nodes[i]))
                continue;

            cost += manhattan(positions.value(net.nodes[0]),
                              positions.value(net.nodes[i]));
        }
    }

    return cost;
}

int MainWindow::netBoundingLength(
    const Net &net,
    const QMap<QString, QPoint> &positions) const
{
    if (net.nodes.size() < 2)
        return 0;

    int minX = INT_MAX, maxX = INT_MIN;
    int minY = INT_MAX, maxY = INT_MIN;

    for (const QString &id : net.nodes) {
        if (!positions.contains(id))
            continue;

        const QPoint p = positions.value(id);
        minX = std::min(minX, p.x());
        maxX = std::max(maxX, p.x());
        minY = std::min(minY, p.y());
        maxY = std::max(maxY, p.y());
    }

    if (minX == INT_MAX)
        return 0;

    return (maxX - minX) + (maxY - minY);
}

int MainWindow::edgeCapacity() const
{
    return 1;
}

QList<QPoint> MainWindow::shortestRoute(
    const QPoint &source,
    const QSet<QPoint> &targets,
    const QVector<QVector<int>> &edgeRight,
    const QVector<QVector<int>> &edgeDown,
    bool *ok) const
{
    *ok = false;

    if (targets.contains(source)) {
        *ok = true;
        return {source};
    }

    const int INF = INT_MAX / 4;
    QVector<QVector<int>> dist(fpgaRows,
                               QVector<int>(fpgaColumns, INF));
    QVector<QVector<QPoint>> prev(fpgaRows,
                                  QVector<QPoint>(fpgaColumns, QPoint(-1, -1)));

    MinHeap heap;
    dist[source.y()][source.x()] = 0;
    heap.push({0, source});

    const QPoint directions[] = {
        QPoint(1,0), QPoint(-1,0), QPoint(0,1), QPoint(0,-1)
};

while (!heap.empty()) {
    State current = heap.top();
    heap.pop();

    if (current.cost != dist[current.p.y()][current.p.x()])
        continue;

    if (targets.contains(current.p)) {
        QList<QPoint> path;
        QPoint p = current.p;

        while (p.x() >= 0 && p.y() >= 0) {
            path.prepend(p);
            if (p == source)
                break;
            p = prev[p.y()][p.x()];
        }

        *ok = !path.isEmpty();
        return path;
    }

    for (const QPoint &d : directions) {
        QPoint np = current.p + d;

        if (np.x() < 0 || np.x() >= fpgaColumns ||
            np.y() < 0 || np.y() >= fpgaRows)
            continue;

        int congestion = 0;

        if (d == QPoint(1,0))
            congestion = edgeRight[current.p.y()][current.p.x()];
        else if (d == QPoint(-1,0))
            congestion = edgeRight[current.p.y()][np.x()];
        else if (d == QPoint(0,1))
            congestion = edgeDown[current.p.y()][current.p.x()];
        else if (d == QPoint(0,-1))
            congestion = edgeDown[np.y()][current.p.x()];

        const int stepCost = 1 + congestion * 8;
        const int newCost = current.cost + stepCost;

        if (newCost < dist[np.y()][np.x()]) {
            dist[np.y()][np.x()] = newCost;
            prev[np.y()][np.x()] = current.p;
            heap.push({newCost, np});
        }
    }
}

return {};
}

void MainWindow::routePlacement(PlacementResult &result)
{
    result.routes.clear();
    result.routedNets = 0;
    result.failedNets = 0;
    result.totalWireLength = 0;
    result.maxNetLength = 0;
    result.congestionPercent = 0.0;

    QVector<QVector<int>> edgeRight(
        fpgaRows, QVector<int>(std::max(0, fpgaColumns - 1), 0));
    QVector<QVector<int>> edgeDown(
        std::max(0, fpgaRows - 1), QVector<int>(fpgaColumns, 0));

    for (const Net &net : nets) {
        if (net.nodes.size() < 2)
            continue;

        if (!result.positions.contains(net.nodes[0])) {
            ++result.failedNets;
            continue;
        }

        QSet<QPoint> tree;
        tree.insert(result.positions.value(net.nodes[0]));

        bool netFailed = false;
        QList<QPoint> fullPath;

        for (int i = 1; i < net.nodes.size(); ++i) {
            if (!result.positions.contains(net.nodes[i])) {
                netFailed = true;
                break;
            }

            QSet<QPoint> targets;
            targets.insert(result.positions.value(net.nodes[i]));

            bool ok = false;
            QList<QPoint> segment =
                shortestRoute(targets.isEmpty()
                                  ? QPoint()
                                  : result.positions.value(net.nodes[0]),
                              targets, edgeRight, edgeDown, &ok);

            // Try from any current tree point if the direct root route did not
            // connect to the existing tree efficiently.
            if (!ok) {
                for (const QPoint &treePoint : tree) {
                    segment = shortestRoute(treePoint, targets,
                                            edgeRight, edgeDown, &ok);
                    if (ok)
                        break;
                }
            }

            if (!ok || segment.size() < 1) {
                netFailed = true;
                break;
            }

            for (int j = 0; j < segment.size(); ++j) {
                const QPoint &p = segment[j];

                if (j > 0) {
                    const QPoint prev = segment[j - 1];

                    if (p.x() != prev.x()) {
                        const int y = p.y();
                        const int x = std::min(p.x(), prev.x());
                        if (y >= 0 && y < edgeRight.size() &&
                            x >= 0 && x < edgeRight[y].size())
                            edgeRight[y][x]++;
                    } else if (p.y() != prev.y()) {
                        const int y = std::min(p.y(), prev.y());
                        const int x = p.x();
                        if (y >= 0 && y < edgeDown.size() &&
                            x >= 0 && x < edgeDown[y].size())
                            edgeDown[y][x]++;
                    }
                }

                tree.insert(p);
                fullPath.append(p);
            }
        }

        if (netFailed) {
            ++result.failedNets;
            continue;
        }

        Route route;
        route.netName = net.name;
        route.path = fullPath;
        result.routes.append(route);

        const int len = pathLength(fullPath);
        result.totalWireLength += len;
        result.maxNetLength = std::max(result.maxNetLength, len);
        ++result.routedNets;
    }

    int usedEdges = 0;
    int congestedEdges = 0;
    const int capacity = edgeCapacity();

    for (const auto &row : edgeRight) {
        for (int usage : row) {
            if (usage > 0)
                ++usedEdges;
            if (usage > capacity)
                ++congestedEdges;
        }
    }

    for (const auto &row : edgeDown) {
        for (int usage : row) {
            if (usage > 0)
                ++usedEdges;
            if (usage > capacity)
                ++congestedEdges;
        }
    }

    const int totalEdges =
        fpgaRows * std::max(0, fpgaColumns - 1) +
        std::max(0, fpgaRows - 1) * fpgaColumns;

    if (totalEdges > 0)
        result.congestionPercent =
            100.0 * static_cast<double>(congestedEdges) /
            static_cast<double>(totalEdges);
}

int MainWindow::pathLength(const QList<QPoint> &path) const
{
    if (path.size() < 2)
        return 0;

    int length = 0;
    for (int i = 1; i < path.size(); ++i)
        length += manhattan(path[i - 1], path[i]);

    return length;
}

void MainWindow::applyPlacement(const PlacementResult &result)
{
    clearOccupancy();

    for (auto it = result.positions.cbegin();
         it != result.positions.cend(); ++it) {
        const QPoint p = it.value();

        if (p.y() >= 0 && p.y() < fpgaRows &&
            p.x() >= 0 && p.x() < fpgaColumns) {
            grid[p.y()][p.x()].occupant = it.key();
        }
    }

    for (Node &node : nodes) {
        if (result.positions.contains(node.id)) {
            const QPoint p = result.positions.value(node.id);
            node.col = p.x();
            node.row = p.y();
        }
    }

    for (int r = 0; r < nodeTable->rowCount(); ++r) {
        const QString id = nodeTable->item(r, 0)->text();
        const auto pos = result.positions.value(id, QPoint(-1, -1));

        nodeTable->setItem(
            r, 2,
            new QTableWidgetItem(
                pos.x() < 0 ? "--"
                            : QString("(%1,%2)").arg(pos.y()).arg(pos.x())));
    }

    placementCostLabel->setText(
        QString("Placement Cost: %1").arg(result.placementCost));
    wireLengthLabel->setText(
        QString("Actual Routed Wirelength: %1").arg(result.totalWireLength));
    maxNetLabel->setText(
        QString("Maximum Routed Net Length: %1").arg(result.maxNetLength));
    congestionLabel->setText(
        QString("Routing Congestion: %1%").arg(
            QString::number(result.congestionPercent, 'f', 2)));
    routingLabel->setText(
        QString("Routed Nets: %1 / %2 | Failed: %3")
            .arg(result.routedNets)
            .arg(nets.size())
            .arg(result.failedNets));

    updateAllCells();
    drawRoutes(result.routes);
}

void MainWindow::drawRoutes(const QList<Route> &routes)
{
    if (routes.isEmpty())
        return;

    // Routes are drawn on a transparent overlay so the resource cells remain
    // readable. The overlay is intentionally lightweight and educational.
    auto *overlay = new QWidget(floorWidget);
    overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    overlay->setGeometry(floorWidget->rect());
    overlay->raise();

    const int cellW = 125;
    const int cellH = 65;
    const int spacing = 5;
    const int margin = 14;

    overlay->installEventFilter(this);

    // Use a dynamic property containing route data. The paint is performed
    // immediately through a small lambda-backed QObject event filter below.
    overlay->setProperty("routeCount", routes.size());

    // For simplicity, use a child QLabel for every route segment. This gives
    // a visible routing trace without requiring custom widget plumbing.
    int colorIndex = 0;
    const QStringList routeColors = {
        "#d62728", "#1f77b4", "#2ca02c", "#9467bd", "#ff7f0e"
    };

    for (const Route &route : routes) {
        Q_UNUSED(route);
        ++colorIndex;
    }

    // Repaint via a local custom widget is not possible with a lambda-only
    // QWidget. Instead, put route information into the status bar and use
    // cell highlighting for routed cells below.
    overlay->deleteLater();

    QSet<QPoint> routePoints;
    for (const Route &route : routes)
        for (const QPoint &p : route.path)
            routePoints.insert(p);

    for (const QPoint &p : routePoints) {
        if (p.y() < 0 || p.y() >= fpgaRows ||
            p.x() < 0 || p.x() >= fpgaColumns)
            continue;

        QLayoutItem *item = floorLayout->itemAtPosition(p.y(), p.x());
        auto *label = item ? qobject_cast<QLabel*>(item->widget()) : nullptr;
        if (!label)
            continue;

        const Cell &cell = grid[p.y()][p.x()];

        if (!cell.occupant.isEmpty()) {
            label->setStyleSheet(
                "font-weight:bold; border:3px solid #d62728;"
                " background:#b7e4b5; color:#111;");
        } else if (cell.type == ResourceType::DSP) {
            label->setStyleSheet(
                "font-weight:bold; border:3px solid #d62728;"
                " background:#cfe2ff;");
        } else if (cell.type == ResourceType::BRAM) {
            label->setStyleSheet(
                "font-weight:bold; border:3px solid #d62728;"
                " background:#ffe0bd;");
        } else {
            label->setStyleSheet(
                "font-weight:bold; border:3px solid #d62728;"
                " background:#eeeeee;");
        }
    }

    Q_UNUSED(cellW);
    Q_UNUSED(cellH);
    Q_UNUSED(spacing);
    Q_UNUSED(margin);
}

void MainWindow::runFirstFit()
{
    if (nodes.isEmpty()) {
        QMessageBox::information(this, "No Netlist",
                                 "Load and analyze a netlist first.");
        return;
    }

    FirstFitAlgorithm firstFit;
    PlacementAlgorithm &algorithm = firstFit;
    PlacementResult result = algorithm.execute(*this);

    if (!result.success) {
        clearResults();
        setStatus("First-Fit failed: insufficient compatible FPGA resources.");
        QMessageBox::warning(
            this, "Placement Failed",
            "First-Fit could not place all netlist resources on this FPGA.");
        return;
    }

    applyPlacement(result);
    setStatus(QString("First-Fit completed: %1 nets routed, %2 failed.")
                  .arg(result.routedNets)
                  .arg(result.failedNets));
}

void MainWindow::runBestFit()
{
    if (nodes.isEmpty()) {
        QMessageBox::information(this, "No Netlist",
                                 "Load and analyze a netlist first.");
        return;
    }

    BestFitAlgorithm bestFit;
    PlacementAlgorithm &algorithm = bestFit;
    PlacementResult result = algorithm.execute(*this);

    if (!result.success) {
        clearResults();
        setStatus("Best-Fit failed: insufficient compatible FPGA resources.");
        QMessageBox::warning(
            this, "Placement Failed",
            "Connectivity-aware Best-Fit could not place all resources.");
        return;
    }

    applyPlacement(result);
    setStatus(QString("Connectivity-aware Best-Fit completed: %1 nets routed, %2 failed.")
                  .arg(result.routedNets)
                  .arg(result.failedNets));
}

MainWindow::PlacementResult
MainWindow::FirstFitAlgorithm::execute(MainWindow &window)
{
    return window.placeFirstFit();
}

MainWindow::PlacementResult
MainWindow::BestFitAlgorithm::execute(MainWindow &window)
{
    return window.placeBestFit();
}

void MainWindow::clearAll()
{
    netlistEdit->clear();
    nodes.clear();
    nets.clear();

    nodeTable->setRowCount(0);
    netTable->setRowCount(0);

    clearOccupancy();
    updateSummary();
    clearResults();
    rebuildFloorplan();

    if (autoSizeLabel)
        autoSizeLabel->setText("FPGA Size: 6 x 6 (default)");

    setStatus("Reset complete.");
}
