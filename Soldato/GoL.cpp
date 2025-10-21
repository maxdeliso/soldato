#include "GoL.h"
#include "Colors.h"

#include <algorithm>
#include <random>

GameOfLife::GameOfLife()
    : m_cellSize(0)
    , m_gridWidth(0)
    , m_gridHeight(0)
    , m_lastCanvasW(0)
    , m_lastCanvasH(0)
{
}

void GameOfLife::Resize(int width, int height, int minCellSize)
{
    if (width <= 0 || height <= 0) {
        return;
    }

    m_lastCanvasW = width;
    m_lastCanvasH = height;

    int minEdge = std::min(width, height);
    // Simplified heuristic: aim ~15 cells across min dimension, but respect minCellSize
    int targetCellSize = std::max(1, minEdge / 15);
    int computedCell = std::max(minCellSize, targetCellSize);

    int newGridWidth = std::max(1, width / computedCell);
    int newGridHeight = std::max(1, height / computedCell);

    bool cellChanged = (computedCell != m_cellSize);
    bool dimsChanged = (newGridWidth != m_gridWidth) || (newGridHeight != m_gridHeight);

    if (cellChanged || dimsChanged) {
        m_cellSize = computedCell;
        m_gridWidth = newGridWidth;
        m_gridHeight = newGridHeight;

        size_t total = static_cast<size_t>(m_gridWidth) * static_cast<size_t>(m_gridHeight);
        m_grid.assign(total, 0);
        m_nextGrid.assign(total, 0);
        initializeRandom();
    }
}

void GameOfLife::initializeRandom()
{
    if (m_gridWidth <= 0 || m_gridHeight <= 0) return;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    const float density = 0.18f; // initial live cell probability
    for (int r = 0; r < m_gridHeight; ++r) {
        for (int c = 0; c < m_gridWidth; ++c) {
            m_grid[index(r, c)] = (dist(gen) < density) ? 1 : 0;
        }
    }
}

void GameOfLife::update()
{
    if (m_gridWidth <= 0 || m_gridHeight <= 0) return;

    for (int r = 0; r < m_gridHeight; ++r) {
        int rUp = (r - 1 + m_gridHeight) % m_gridHeight;
        int rDn = (r + 1) % m_gridHeight;
        for (int c = 0; c < m_gridWidth; ++c) {
            int cLt = (c - 1 + m_gridWidth) % m_gridWidth;
            int cRt = (c + 1) % m_gridWidth;

            int neighbors = 0;
            neighbors += m_grid[index(rUp, cLt)];
            neighbors += m_grid[index(rUp, c)];
            neighbors += m_grid[index(rUp, cRt)];
            neighbors += m_grid[index(r, cLt)];
            neighbors += m_grid[index(r, cRt)];
            neighbors += m_grid[index(rDn, cLt)];
            neighbors += m_grid[index(rDn, c)];
            neighbors += m_grid[index(rDn, cRt)];

            unsigned char alive = m_grid[index(r, c)];
            unsigned char nextAlive = 0;
            if (alive) {
                nextAlive = (neighbors == 2 || neighbors == 3) ? 1 : 0;
            } else {
                nextAlive = (neighbors == 3) ? 1 : 0;
            }
            m_nextGrid[index(r, c)] = nextAlive;
        }
    }

    m_grid.swap(m_nextGrid);
}

void GameOfLife::draw(HDC hdc, int xOffset, int yOffset, HBRUSH cellBrush, HBRUSH backgroundBrush) const
{
    if (m_gridWidth <= 0 || m_gridHeight <= 0) return;

    RECT bgRect = { xOffset, yOffset, xOffset + m_lastCanvasW, yOffset + m_lastCanvasH };
    FillRect(hdc, &bgRect, backgroundBrush);

    for (int r = 0; r < m_gridHeight; ++r) {
        for (int c = 0; c < m_gridWidth; ++c) {
            if (m_grid[index(r, c)]) {
                int left = xOffset + c * m_cellSize;
                int top = yOffset + r * m_cellSize;
                RECT cellRect = { left, top, left + m_cellSize, top + m_cellSize };
                FillRect(hdc, &cellRect, cellBrush);
            }
        }
    }
}
