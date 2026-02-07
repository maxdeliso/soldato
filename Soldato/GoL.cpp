#include "GoL.h"
#include "Colors.h"
#include "framework.h"

#include <algorithm>
#include <random>

// Only include intrinsics on supported architectures
#if defined(_M_X64) || defined(_M_IX86)
#include <immintrin.h>
#endif


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
    // Cap cell size at maximum to ensure more cells are displayed on larger canvases
    computedCell = std::min(computedCell, GOL_MAX_CELL_SIZE);

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

  // --- 1. AVX2 Fast Path for the inner grid ---
#if defined(_M_X64) || defined(_M_IX86)
  // We only process the "safe" inner rectangle (from r=1 to H-2, c=1 to W-2)
  // to avoid complex SIMD boundary checks for toroidal wrapping.
  if (m_gridWidth >= 34 && m_gridHeight >= 3) {
    const __m256i vOne = _mm256_set1_epi8(1);
    const __m256i vTwo = _mm256_set1_epi8(2);
    const __m256i vThree = _mm256_set1_epi8(3);

    for (int r = 1; r < m_gridHeight - 1; ++r) {
      size_t rowOffset = static_cast<size_t>(r) * m_gridWidth;
      size_t upOffset = static_cast<size_t>(r - 1) * m_gridWidth;
      size_t dnOffset = static_cast<size_t>(r + 1) * m_gridWidth;

      // Process 32 cells per iteration
      // Start at c=1, end when we have less than 32 cells left before the right edge border (width - 1)
      for (int c = 1; c <= m_gridWidth - 33; c += 32) {
        // Load center row of 32 cells
        __m256i mCol = _mm256_loadu_si256((const __m256i*) & m_grid[rowOffset + c]);

        // Load Neighbors using unaligned loads at offsets -1, 0, +1
        __m256i mUpC = _mm256_loadu_si256((const __m256i*) & m_grid[upOffset + c]);
        __m256i mUpL = _mm256_loadu_si256((const __m256i*) & m_grid[upOffset + c - 1]);
        __m256i mUpR = _mm256_loadu_si256((const __m256i*) & m_grid[upOffset + c + 1]);

        __m256i mDnC = _mm256_loadu_si256((const __m256i*) & m_grid[dnOffset + c]);
        __m256i mDnL = _mm256_loadu_si256((const __m256i*) & m_grid[dnOffset + c - 1]);
        __m256i mDnR = _mm256_loadu_si256((const __m256i*) & m_grid[dnOffset + c + 1]);

        __m256i mColL = _mm256_loadu_si256((const __m256i*) & m_grid[rowOffset + c - 1]);
        __m256i mColR = _mm256_loadu_si256((const __m256i*) & m_grid[rowOffset + c + 1]);

        // Sum neighbors (8-bit adds are sufficient as max sum is 8)
        __m256i neighbors = _mm256_add_epi8(mUpL, mUpC);
        neighbors = _mm256_add_epi8(neighbors, mUpR);
        neighbors = _mm256_add_epi8(neighbors, mColL);
        neighbors = _mm256_add_epi8(neighbors, mColR);
        neighbors = _mm256_add_epi8(neighbors, mDnL);
        neighbors = _mm256_add_epi8(neighbors, mDnC);
        neighbors = _mm256_add_epi8(neighbors, mDnR);

        // Apply Rules using bitwise logic instead of branches:
        // Alive if: (neighbors == 3) OR (wasAlive AND neighbors == 2)

        // _mm256_cmpeq_epi8 returns 0xFF where true, 0x00 where false
        __m256i mask3 = _mm256_cmpeq_epi8(neighbors, vThree);
        __m256i mask2 = _mm256_cmpeq_epi8(neighbors, vTwo);
        __m256i wasAliveAnd2 = _mm256_and_si256(mCol, mask2); // implicit check: mCol must be 0x01 or 0x00 here.
        // if mCol is 0x01, we need to ensure mask2 (0xFF) doesn't result in 0xFF.
        // Actually, if inputs are strictly 0 or 1, mCol & mask2 works if we want result 1 or 0.
        // Let's be safe and use fully saturated masks first, then convert to 1s at the end.

        // Better approach relying on inputs being strictly 0 or 1:
        // (mask3 is 0xFF if 3)
        // (mask2 is 0xFF if 2).
        // We want final result to be 0x01 if alive, 0x00 if dead.

        // If we strictly maintain 0/1 in grid:
        // wasAliveAnd2 will be 0x01 if (alive AND neighbors==2), else 0x00.
        // mask3 needs to be converted to 0x01.
        __m256i neighborsIs3 = _mm256_and_si256(mask3, vOne); // 0x01 if 3, else 0
        __m256i survived = _mm256_and_si256(mask2, mCol);     // 0x01 if (2 AND alive), else 0

        __m256i result = _mm256_or_si256(neighborsIs3, survived);

        _mm256_storeu_si256((__m256i*) & m_nextGrid[rowOffset + c], result);
      }
    }
  }
#endif

  // --- 2. Scalar Fallback for borders and remaining columns ---
  // This ensures full toroidal wrapping still works correctly.
  // It re-processes the whole grid but only actually writes to the edges
  // that AVX skipped.
      for (int r = 0; r < m_gridHeight; ++r) {
  #if defined(_M_X64) || defined(_M_IX86)
        // Optimization: if this is a middle row, only do the edge columns
        bool isMiddleRow = (r > 0 && r < m_gridHeight - 1);
  #endif
    int rUp = (r - 1 + m_gridHeight) % m_gridHeight;
    int rDn = (r + 1) % m_gridHeight;

    for (int c = 0; c < m_gridWidth; ++c) {
      // If we are in the "AVX zone", skip scalar update (only on x86/x64 where AVX ran)
#if defined(_M_X64) || defined(_M_IX86)
      if (isMiddleRow && c >= 1 && c <= m_gridWidth - 33) {
        // Fast-forward to the right edge
        c = (m_gridWidth - 33);
        continue;
      }
#endif

      int cLt = (c - 1 + m_gridWidth) % m_gridWidth;
      int cRt = (c + 1) % m_gridWidth;

      int neighbors = m_grid[index(rUp, cLt)] + m_grid[index(rUp, c)] + m_grid[index(rUp, cRt)] +
        m_grid[index(r, cLt)] + m_grid[index(r, cRt)] +
        m_grid[index(rDn, cLt)] + m_grid[index(rDn, c)] + m_grid[index(rDn, cRt)];

      unsigned char alive = m_grid[index(r, c)];
      m_nextGrid[index(r, c)] = (neighbors == 3 || (alive && neighbors == 2)) ? 1 : 0;
    }
  }

  m_grid.swap(m_nextGrid);
}

void GameOfLife::draw(HDC hdc, int xOffset, int yOffset, COLORREF cellColor, COLORREF bgColor)
{
  if (m_gridWidth <= 0 || m_gridHeight <= 0) return;

  // 1. Optional: Clear background if the grid doesn't strictly fill the canvas
  // (Only necessary if xOffset/yOffset > 0 or if grid * cellSize < canvas size)
  RECT bgRect = { xOffset, yOffset, xOffset + m_lastCanvasW, yOffset + m_lastCanvasH };
  HBRUSH bgBrush = CreateSolidBrush(bgColor);
  FillRect(hdc, &bgRect, bgBrush);
  DeleteObject(bgBrush);

  // 2. Define an 8bpp Bitmap Info header with a 2-color palette
  struct {
    BITMAPINFOHEADER bi;
    RGBQUAD cols[256];
  } dib = { 0 };

  dib.bi.biSize = sizeof(BITMAPINFOHEADER);
  dib.bi.biWidth = m_gridWidth;
  dib.bi.biHeight = -m_gridHeight; // Negative height = top-down image matching your grid
  dib.bi.biPlanes = 1;
  dib.bi.biBitCount = 8;           // 8 bits per pixel (matches your unsigned char)
  dib.bi.biCompression = BI_RGB;

  // Set palette index 0 (dead)
  dib.cols[0].rgbRed = GetRValue(bgColor);
  dib.cols[0].rgbGreen = GetGValue(bgColor);
  dib.cols[0].rgbBlue = GetBValue(bgColor);

  // Set palette index 1 (alive)
  dib.cols[1].rgbRed = GetRValue(cellColor);
  dib.cols[1].rgbGreen = GetGValue(cellColor);
  dib.cols[1].rgbBlue = GetBValue(cellColor);

  // 3. Handle 4-byte row alignment required by GDI
  // If grid width isn't a multiple of 4, we must copy to a padded buffer.
  int rowStride = (m_gridWidth + 3) & ~3;
  const unsigned char* pPixels = m_grid.data();
  std::vector<unsigned char> alignedBuffer;

  if (rowStride != m_gridWidth) {
    alignedBuffer.resize(rowStride * m_gridHeight);
    for (int r = 0; r < m_gridHeight; ++r) {
      // Copy row by row into the strided buffer
      memcpy(&alignedBuffer[r * rowStride], &m_grid[r * m_gridWidth], m_gridWidth);
    }
    pPixels = alignedBuffer.data();
  }

  // 4. Ensure clean scaling without anti-aliasing blur
  int oldMode = SetStretchBltMode(hdc, COLORONCOLOR);

  // 5. The magic single call that draws everything
  StretchDIBits(hdc,
    xOffset, yOffset, m_gridWidth * m_cellSize, m_gridHeight * m_cellSize, // Destination on screen
    0, 0, m_gridWidth, m_gridHeight,                                     // Source from grid
    pPixels, (BITMAPINFO*)&dib, DIB_RGB_COLORS, SRCCOPY);

  SetStretchBltMode(hdc, oldMode);
}
