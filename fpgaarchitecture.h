#ifndef FPGAARCHITECTURE_H
#define FPGAARCHITECTURE_H

#include <QVector>
#include <QChar>
#include <QString>

enum class ResourceType
{
    LUT,
    DSP,
    BRAM,
    UNKNOWN
};

struct ResourceCell
{
    ResourceType type;
    QChar occupiedBy;
    bool occupied;

    ResourceCell()
        : type(ResourceType::UNKNOWN),
          occupiedBy('.'),
          occupied(false)
    {
    }

    ResourceCell(ResourceType type)
        : type(type),
          occupiedBy('.'),
          occupied(false)
    {
    }
};

struct Coordinate
{
    int row;
    int column;

    Coordinate(int r = 0, int c = 0)
        : row(r), column(c)
    {
    }
};

class FPGAArchitecture
{
public:

    FPGAArchitecture();
    FPGAArchitecture(int rows, int columns);

    void create(int rows, int columns);

    int getRows() const;
    int getColumns() const;

    ResourceType getResourceType(int row, int column) const;

    bool isFree(int row, int column) const;

    bool occupy(const Coordinate& coordinate, QChar symbol);

    void release(const Coordinate& coordinate);

    void clearOccupancy();

    int totalLUTs() const;
    int totalDSPs() const;
    int totalBRAMs() const;

    int usedLUTs() const;
    int usedDSPs() const;
    int usedBRAMs() const;

    QString resourceName(ResourceType type) const;
    QChar resourceSymbol(ResourceType type) const;

    const QVector<QVector<ResourceCell>>& getGrid() const;

private:

    int rows;
    int columns;

    QVector<QVector<ResourceCell>> grid;

    ResourceType resourceTypeForColumn(int column) const;
};

#endif // FPGAARCHITECTURE_H