#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <vector>
#include <memory>

// Conway's Game of Life implementation with dynamic sizing
class GameOfLife {
public:
  GameOfLife();

  void Resize(int width, int height, int minCellSize);
  void initializeRandom();
  void update();
  void draw(HDC hdc, int xOffset, int yOffset, COLORREF cellColor, COLORREF bgColor);
  int getCellSize() const { return m_cellSize; }
  int getGridWidth() const { return m_gridWidth; }
  int getGridHeight() const { return m_gridHeight; }

private:
  int m_cellSize;
  int m_gridWidth;
  int m_gridHeight;

  // Grid is stored row-major: m_grid[r][c]
  std::vector<unsigned char> m_grid;
  std::vector<unsigned char> m_nextGrid;

  // Cached last canvas size used for Resize
  int m_lastCanvasW;
  int m_lastCanvasH;

  int index(int r, int c) const { return r * m_gridWidth + c; }
};
