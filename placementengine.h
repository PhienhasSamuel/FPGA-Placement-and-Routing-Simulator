#ifndef PLACEMENTENGINE_H
#define PLACEMENTENGINE_H

#include <QString>
#include <QVector>

struct Module
{
    QString name;

    int lut;
    int dsp;
    int bram;
    int priority;

    bool placed;

    QVector<QPair<int, int>> occupiedCells;

    Module()
        : lut(0),
          dsp(0),
          bram(0),
          priority(1),
          placed(false)
    {
    }
};

struct FPGAResult
{
    int placementCost;

    int totalLUT;
    int usedLUT;

    int totalDSP;
    int usedDSP;

    int totalBRAM;
    int usedBRAM;

    QString report;

    FPGAResult()
        : placementCost(0),
          totalLUT(0),
          usedLUT(0),
          totalDSP(0),
          usedDSP(0),
          totalBRAM(0),
          usedBRAM(0)
    {
    }
};

class PlacementEngine
{
public:

    PlacementEngine();

    void configureFPGA(
        int rows,
        int columns,
        const QVector<QVector<QString>>& architecture
    );

    void setModules(const QVector<Module>& modules);

    FPGAResult runFirstFit();

    FPGAResult runBestFit();

    void reset();

    const QVector<Module>& getModules() const;

private:

    int rows;
    int columns;

    QVector<QVector<QString>> architecture;
    QVector<QVector<bool>> occupied;

    QVector<Module> modules;

    bool canPlaceModule(
        const Module& module,
        QVector<QPair<int, int>>& selectedCells
    );

    bool findFirstFit(
        const Module& module,
        QVector<QPair<int, int>>& selectedCells
    );

    bool findBestFit(
        const Module& module,
        QVector<QPair<int, int>>& selectedCells,
        int& bestCost
    );

    bool cellCanProvide(
        int row,
        int column,
        const QString& type,
        const Module& module
    ) const;

    int calculatePlacementCost(
        const QVector<QPair<int, int>>& cells
    ) const;

    int calculateDistanceCost(
        const QVector<QPair<int, int>>& cells
    ) const;

    FPGAResult generateResult(
        const QString& algorithmName
    );

    void sortModulesByPriority();
};

#endif