#include "fpgaarchitecture.h"

FPGAArchitecture::FPGAArchitecture()
    : rows(0),
      columns(0)
{
}

FPGAArchitecture::FPGAArchitecture(int rows, int columns)
    : rows(0),
      columns(0)
{
    create(rows, columns);
}

void FPGAArchitecture::create(int rows, int columns)
{
    this->rows = rows;
    this->columns = columns;

    grid.clear();
    grid.resize(rows);

    for (int r = 0; r < rows; ++r)
    {
        grid[r].resize(columns);

        for (int c = 0; c < columns; ++c)
        {
            grid[r][c] = ResourceCell(resourceTypeForColumn(c));
        }
    }
}

int FPGAArchitecture::getRows() const
{
    return rows;
}

int FPGAArchitecture::getColumns() const
{
    return columns;
}

ResourceType FPGAArchitecture::resourceTypeForColumn(int column) const
{
    /*
        Example FPGA architecture:

        Column:
        0 = LUT
        1 = LUT
        2 = DSP
        3 = LUT
        4 = BRAM
        5 = LUT

        This gives:

        C C D C B C
        C C D C B C
        C C D C B C
        ...
    */

    switch (column % 6)
    {
    case 0:
    case 1:
    case 3:
    case 5:
        return ResourceType::LUT;

    case 2:
        return ResourceType::DSP;

    case 4:
        return ResourceType::BRAM;

    default:
        return ResourceType::UNKNOWN;
    }
}

ResourceType FPGAArchitecture::getResourceType(int row,
                                                int column) const
{
    if (row < 0 || row >= rows ||
        column < 0 || column >= columns)
    {
        return ResourceType::UNKNOWN;
    }

    return grid[row][column].type;
}

bool FPGAArchitecture::isFree(int row, int column) const
{
    if (row < 0 || row >= rows ||
        column < 0 || column >= columns)
    {
        return false;
    }

    return !grid[row][column].occupied;
}

bool FPGAArchitecture::occupy(const Coordinate& coordinate,
                               QChar symbol)
{
    int r = coordinate.row;
    int c = coordinate.column;

    if (r < 0 || r >= rows ||
        c < 0 || c >= columns)
    {
        return false;
    }

    if (grid[r][c].occupied)
    {
        return false;
    }

    grid[r][c].occupied = true;
    grid[r][c].occupiedBy = symbol;

    return true;
}

void FPGAArchitecture::release(const Coordinate& coordinate)
{
    int r = coordinate.row;
    int c = coordinate.column;

    if (r < 0 || r >= rows ||
        c < 0 || c >= columns)
    {
        return;
    }

    grid[r][c].occupied = false;
    grid[r][c].occupiedBy = '.';
}

void FPGAArchitecture::clearOccupancy()
{
    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < columns; ++c)
        {
            grid[r][c].occupied = false;
            grid[r][c].occupiedBy = '.';
        }
    }
}

int FPGAArchitecture::totalLUTs() const
{
    int count = 0;

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < columns; ++c)
        {
            if (grid[r][c].type == ResourceType::LUT)
                ++count;
        }
    }

    return count;
}

int FPGAArchitecture::totalDSPs() const
{
    int count = 0;

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < columns; ++c)
        {
            if (grid[r][c].type == ResourceType::DSP)
                ++count;
        }
    }

    return count;
}

int FPGAArchitecture::totalBRAMs() const
{
    int count = 0;

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < columns; ++c)
        {
            if (grid[r][c].type == ResourceType::BRAM)
                ++count;
        }
    }

    return count;
}

int FPGAArchitecture::usedLUTs() const
{
    int count = 0;

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < columns; ++c)
        {
            if (grid[r][c].type == ResourceType::LUT &&
                grid[r][c].occupied)
            {
                ++count;
            }
        }
    }

    return count;
}

int FPGAArchitecture::usedDSPs() const
{
    int count = 0;

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < columns; ++c)
        {
            if (grid[r][c].type == ResourceType::DSP &&
                grid[r][c].occupied)
            {
                ++count;
            }
        }
    }

    return count;
}

int FPGAArchitecture::usedBRAMs() const
{
    int count = 0;

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < columns; ++c)
        {
            if (grid[r][c].type == ResourceType::BRAM &&
                grid[r][c].occupied)
            {
                ++count;
            }
        }
    }

    return count;
}

QString FPGAArchitecture::resourceName(ResourceType type) const
{
    switch (type)
    {
    case ResourceType::LUT:
        return "LUT";

    case ResourceType::DSP:
        return "DSP";

    case ResourceType::BRAM:
        return "BRAM";

    default:
        return "Unknown";
    }
}

QChar FPGAArchitecture::resourceSymbol(ResourceType type) const
{
    switch (type)
    {
    case ResourceType::LUT:
        return 'C';

    case ResourceType::DSP:
        return 'D';

    case ResourceType::BRAM:
        return 'B';

    default:
        return '?';
    }
}

const QVector<QVector<ResourceCell>>&
FPGAArchitecture::getGrid() const
{
    return grid;
}