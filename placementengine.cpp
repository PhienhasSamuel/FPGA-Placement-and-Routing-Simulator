#include "placementengine.h"

#include <algorithm>
#include <cmath>
#include <limits>

PlacementEngine::PlacementEngine()
    : rows(0),
      columns(0)
{
}

void PlacementEngine::configureFPGA(
    int newRows,
    int newColumns,
    const QVector<QVector<QString>>& newArchitecture)
{
    rows = newRows;
    columns = newColumns;

    architecture = newArchitecture;

    occupied = QVector<QVector<bool>>(
        rows,
        QVector<bool>(columns, false)
    );
}

void PlacementEngine::setModules(const QVector<Module>& newModules)
{
    modules = newModules;

    for (Module& module : modules)
    {
        module.placed = false;
        module.occupiedCells.clear();
    }
}

void PlacementEngine::reset()
{
    occupied = QVector<QVector<bool>>(
        rows,
        QVector<bool>(columns, false)
    );

    for (Module& module : modules)
    {
        module.placed = false;
        module.occupiedCells.clear();
    }
}

const QVector<Module>& PlacementEngine::getModules() const
{
    return modules;
}

bool PlacementEngine::cellCanProvide(
    int row,
    int column,
    const QString& type,
    const Module& module) const
{
    Q_UNUSED(module);

    if (row < 0 || row >= rows ||
        column < 0 || column >= columns)
    {
        return false;
    }

    if (occupied[row][column])
    {
        return false;
    }

    return architecture[row][column] == type;
}

bool PlacementEngine::canPlaceModule(
    const Module& module,
    QVector<QPair<int, int>>& selectedCells)
{
    return findFirstFit(module, selectedCells);
}

bool PlacementEngine::findFirstFit(
    const Module& module,
    QVector<QPair<int, int>>& selectedCells)
{
    selectedCells.clear();

    QVector<QPair<int, int>> lutCells;
    QVector<QPair<int, int>> dspCells;
    QVector<QPair<int, int>> bramCells;

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < columns; ++c)
        {
            if (!occupied[r][c])
            {
                QString type = architecture[r][c];

                if (type == "CLB")
                    lutCells.append({r, c});

                else if (type == "DSP")
                    dspCells.append({r, c});

                else if (type == "BRAM")
                    bramCells.append({r, c});
            }
        }
    }

    if (lutCells.size() < module.lut ||
        dspCells.size() < module.dsp ||
        bramCells.size() < module.bram)
    {
        return false;
    }

    for (int i = 0; i < module.lut; ++i)
        selectedCells.append(lutCells[i]);

    for (int i = 0; i < module.dsp; ++i)
        selectedCells.append(dspCells[i]);

    for (int i = 0; i < module.bram; ++i)
        selectedCells.append(bramCells[i]);

    return true;
}

bool PlacementEngine::findBestFit(
    const Module& module,
    QVector<QPair<int, int>>& selectedCells,
    int& bestCost)
{
    selectedCells.clear();
    bestCost = std::numeric_limits<int>::max();

    QVector<QPair<int, int>> availableLUT;
    QVector<QPair<int, int>> availableDSP;
    QVector<QPair<int, int>> availableBRAM;

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < columns; ++c)
        {
            if (occupied[r][c])
                continue;

            QString type = architecture[r][c];

            if (type == "CLB")
                availableLUT.append({r, c});

            else if (type == "DSP")
                availableDSP.append({r, c});

            else if (type == "BRAM")
                availableBRAM.append({r, c});
        }
    }

    if (availableLUT.size() < module.lut ||
        availableDSP.size() < module.dsp ||
        availableBRAM.size() < module.bram)
    {
        return false;
    }

    /*
     * Level 2 abstraction:
     *
     * A hardware module consumes a number of LUT, DSP and BRAM
     * resources. The simulator chooses available physical resource
     * locations and calculates a placement cost based on how spread
     * out those locations are.
     */

    QVector<QPair<int, int>> candidate;

    for (int lutStart = 0;
         lutStart <= availableLUT.size() - module.lut;
         ++lutStart)
    {
        candidate.clear();

        for (int i = 0; i < module.lut; ++i)
            candidate.append(availableLUT[lutStart + i]);

        for (int i = 0; i < module.dsp; ++i)
            candidate.append(availableDSP[i]);

        for (int i = 0; i < module.bram; ++i)
            candidate.append(availableBRAM[i]);

        int cost = calculateDistanceCost(candidate);

        if (cost < bestCost)
        {
            bestCost = cost;
            selectedCells = candidate;
        }
    }

    return !selectedCells.isEmpty();
}

int PlacementEngine::calculateDistanceCost(
    const QVector<QPair<int, int>>& cells) const
{
    if (cells.size() <= 1)
        return 0;

    int minRow = cells[0].first;
    int maxRow = cells[0].first;

    int minColumn = cells[0].second;
    int maxColumn = cells[0].second;

    for (const auto& cell : cells)
    {
        minRow = std::min(minRow, cell.first);
        maxRow = std::max(maxRow, cell.first);

        minColumn = std::min(minColumn, cell.second);
        maxColumn = std::max(maxColumn, cell.second);
    }

    /*
     * Bounding-box based placement cost.
     *
     * Smaller spread => smaller cost.
     */

    return (maxRow - minRow + 1) *
           (maxColumn - minColumn + 1);
}

int PlacementEngine::calculatePlacementCost(
    const QVector<QPair<int, int>>& cells) const
{
    return calculateDistanceCost(cells);
}

void PlacementEngine::sortModulesByPriority()
{
    std::sort(
        modules.begin(),
        modules.end(),
        [](const Module& a, const Module& b)
        {
            return a.priority < b.priority;
        }
    );
}

FPGAResult PlacementEngine::runFirstFit()
{
    reset();

    sortModulesByPriority();

    for (Module& module : modules)
    {
        QVector<QPair<int, int>> selected;

        if (findFirstFit(module, selected))
        {
            module.placed = true;
            module.occupiedCells = selected;

            for (const auto& cell : selected)
            {
                occupied[cell.first][cell.second] = true;
            }
        }
        else
        {
            module.placed = false;
            module.occupiedCells.clear();
        }
    }

    return generateResult("First-Fit");
}

FPGAResult PlacementEngine::runBestFit()
{
    reset();

    sortModulesByPriority();

    for (Module& module : modules)
    {
        QVector<QPair<int, int>> selected;
        int cost = 0;

        if (findBestFit(module, selected, cost))
        {
            module.placed = true;
            module.occupiedCells = selected;

            for (const auto& cell : selected)
            {
                occupied[cell.first][cell.second] = true;
            }
        }
        else
        {
            module.placed = false;
            module.occupiedCells.clear();
        }
    }

    return generateResult("Best-Fit");
}

FPGAResult PlacementEngine::generateResult(
    const QString& algorithmName)
{
    FPGAResult result;

    result.report +=
        "========================================\n";

    result.report +=
        " FPGA PLACEMENT SIMULATION RESULT\n";

    result.report +=
        "========================================\n\n";

    result.report +=
        "Algorithm: " + algorithmName + "\n\n";

    int totalLUT = 0;
    int totalDSP = 0;
    int totalBRAM = 0;

    int usedLUT = 0;
    int usedDSP = 0;
    int usedBRAM = 0;

    int totalCost = 0;

    for (const Module& module : modules)
    {
        totalLUT += module.lut;
        totalDSP += module.dsp;
        totalBRAM += module.bram;

        result.report +=
            "Module: " + module.name + "\n";

        result.report +=
            "  LUT  : " +
            QString::number(module.lut) + "\n";

        result.report +=
            "  DSP  : " +
            QString::number(module.dsp) + "\n";

        result.report +=
            "  BRAM : " +
            QString::number(module.bram) + "\n";

        result.report +=
            "  Priority: " +
            QString::number(module.priority) + "\n";

        if (module.placed)
        {
            usedLUT += module.lut;
            usedDSP += module.dsp;
            usedBRAM += module.bram;

            int moduleCost =
                calculatePlacementCost(module.occupiedCells);

            totalCost += moduleCost;

            result.report +=
                "  Status: PLACED\n";

            result.report +=
                "  Placement cost: " +
                QString::number(moduleCost) + "\n";

            result.report +=
                "  Locations: ";

            for (const auto& cell : module.occupiedCells)
            {
                result.report +=
                    "(" +
                    QString::number(cell.first) +
                    "," +
                    QString::number(cell.second) +
                    ") ";
            }

            result.report += "\n";
        }
        else
        {
            result.report +=
                "  Status: FAILED\n";

            if (module.lut > 0)
                result.report +=
                    "  Required LUT resources could not be allocated.\n";

            if (module.dsp > 0)
                result.report +=
                    "  Required DSP resources could not be allocated.\n";

            if (module.bram > 0)
                result.report +=
                    "  Required BRAM resources could not be allocated.\n";
        }

        result.report += "\n";
    }

    /*
     * FPGA capacity is calculated from the architecture.
     */

    int capacityLUT = 0;
    int capacityDSP = 0;
    int capacityBRAM = 0;

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < columns; ++c)
        {
            if (architecture[r][c] == "CLB")
                ++capacityLUT;

            else if (architecture[r][c] == "DSP")
                ++capacityDSP;

            else if (architecture[r][c] == "BRAM")
                ++capacityBRAM;
        }
    }

    result.placementCost = totalCost;

    result.totalLUT = capacityLUT;
    result.usedLUT = usedLUT;

    result.totalDSP = capacityDSP;
    result.usedDSP = usedDSP;

    result.totalBRAM = capacityBRAM;
    result.usedBRAM = usedBRAM;

    result.report +=
        "----------------------------------------\n";

    result.report += "SUMMARY\n\n";

    result.report +=
        "Placement Cost: " +
        QString::number(totalCost) + "\n";

    result.report +=
        "LUT Utilization: " +
        QString::number(usedLUT) +
        "/" +
        QString::number(capacityLUT) +
        "\n";

    result.report +=
        "DSP Utilization: " +
        QString::number(usedDSP) +
        "/" +
        QString::number(capacityDSP) +
        "\n";

    result.report +=
        "BRAM Utilization: " +
        QString::number(usedBRAM) +
        "/" +
        QString::number(capacityBRAM) +
        "\n";

    return result;
}