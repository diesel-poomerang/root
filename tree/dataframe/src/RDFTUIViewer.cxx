/*************************************************************************
 * Copyright (C) 1995-2026, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include "ROOT/RDF/RTUIViewer.hxx"

#include "TError.h"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef ROOT_RDF_TUI_CURSES_ENABLED
#include <curses.h>
#include <unistd.h>
#endif

namespace ROOT {
namespace Internal {
namespace RDF {

RTUITable BuildRTUITable(const ROOT::RDF::RDisplay &display)
{
   RTUITable table;
   const auto &elements = display.GetTable();
   if (elements.empty())
      return table;

   const auto &widths = display.GetColumnWidths();
   table.fColumnWidths.assign(widths.begin(), widths.end());

   // The first row of the RDisplay table holds the column names
   table.fColumnNames.reserve(elements.front().size());
   for (const auto &element : elements.front())
      table.fColumnNames.push_back(element.GetRepresentation());

   // The remaining rows hold the data. Rows left completely empty (e.g. ragged collection rows) are skipped.
   for (size_t row = 1; row < elements.size(); ++row) {
      std::vector<std::string> cells;
      cells.reserve(elements[row].size());
      bool isEmptyRow = true;
      for (const auto &element : elements[row]) {
         std::string cell;
         if (element.IsDot())
            cell = "...";
         else if (element.IsPrint())
            cell = element.GetRepresentation();
         // Elements flagged as "ignored" are rendered as empty cells
         if (!cell.empty())
            isEmptyRow = false;
         cells.push_back(std::move(cell));
      }
      if (!isEmptyRow)
         table.fCells.push_back(std::move(cells));
   }

   // Make sure the column names always fit their column
   for (size_t col = 0; col < table.fColumnNames.size() && col < table.fColumnWidths.size(); ++col)
      table.fColumnWidths[col] = std::max(table.fColumnWidths[col], table.fColumnNames[col].size());

   return table;
}

std::string RenderRTUITableText(const RTUITable &table)
{
   std::stringstream ss;
   const size_t nCols = table.fColumnNames.size();
   if (nCols == 0)
      return ss.str();

   auto dashesLine = [&]() {
      std::string line = "+";
      for (size_t col = 0; col < nCols; ++col)
         line += std::string(table.fColumnWidths[col] + 2, '-') + "+";
      return line + "\n";
   };
   auto rowLine = [&](const std::vector<std::string> &cells) {
      std::string line = "|";
      for (size_t col = 0; col < nCols; ++col) {
         std::string cell = col < cells.size() ? cells[col] : "";
         if (cell.size() > table.fColumnWidths[col])
            cell.resize(table.fColumnWidths[col]);
         line += " " + cell + std::string(table.fColumnWidths[col] - cell.size(), ' ') + " |";
      }
      return line + "\n";
   };

   ss << dashesLine() << rowLine(table.fColumnNames) << dashesLine();
   for (const auto &cells : table.fCells)
      ss << rowLine(cells);
   ss << dashesLine();
   return ss.str();
}

#ifdef ROOT_RDF_TUI_CURSES_ENABLED
namespace {

constexpr int kTitleRow = 0;
constexpr int kHeaderRow = 1;
constexpr int kSeparatorRow = 2;
constexpr int kFirstDataRow = 3;
constexpr int kMinLines = kFirstDataRow + 2;
constexpr int kMinCols = 10;

struct RTUIViewerState {
   size_t fCursorRow = 0; ///< Currently selected data row
   size_t fCursorCol = 0; ///< Currently selected column
   size_t fRowOffset = 0; ///< First visible data row
   size_t fColOffset = 0; ///< First visible column
};

/// Full-screen ncurses viewer for an RTUITable, navigable with the arrow keys
class RTUICursesViewer {
public:
   RTUICursesViewer()
   {
      initscr();
      cbreak();
      noecho();
      keypad(stdscr, TRUE);
      curs_set(0);
      fHasColors = has_colors();
      if (fHasColors) {
         start_color();
         init_pair(1, COLOR_WHITE, COLOR_BLUE); // header
         init_pair(2, COLOR_BLACK, COLOR_CYAN); // selected cell
      }
   }

   ~RTUICursesViewer() { endwin(); }

   RTUICursesViewer(const RTUICursesViewer &) = delete;
   RTUICursesViewer &operator=(const RTUICursesViewer &) = delete;

   void Run(const RTUITable &table, const std::string &title)
   {
      fTable = &table;
      fTitle = title;
      Draw();
      int ch = 0;
      while ((ch = getch()) != 'q' && ch != 'Q' && ch != 27 && ch != ERR) {
         HandleKey(ch);
         Draw();
      }
   }

private:
   const RTUITable *fTable = nullptr;
   std::string fTitle;
   RTUIViewerState fState;
   bool fHasColors = false;

   attr_t HeaderAttr() const { return A_BOLD | (fHasColors ? COLOR_PAIR(1) : 0); }
   attr_t SelectedAttr() const { return A_REVERSE | (fHasColors ? COLOR_PAIR(2) : 0); }

   static std::string PadRight(std::string text, size_t width)
   {
      if (text.size() > width)
         text.resize(width);
      text.append(width - text.size(), ' ');
      return text;
   }

   /// x coordinate of the '|' preceding column `col`, given that column fState.fColOffset starts at x = 1
   int ColumnX(size_t col) const
   {
      int x = 1;
      for (size_t c = fState.fColOffset; c < col; ++c)
         x += int(fTable->fColumnWidths[c]) + 3;
      return x;
   }

   /// Draw `text` at (y, x), clipped so that no character is written past `maxX` nor into the lower-right corner
   static void DrawText(int y, int x, const std::string &text, int maxX, attr_t attr)
   {
      if (x >= maxX || x >= COLS)
         return;
      int avail = maxX - x;
      if (y == LINES - 1)
         avail = std::min(avail, COLS - 1 - x);
      if (avail <= 0)
         return;
      attron(attr);
      mvaddnstr(y, x, text.c_str(), std::min<int>(int(text.size()), avail));
      attroff(attr);
   }

   void EnsureRowOffset()
   {
      const size_t nRows = fTable->fCells.size();
      const int visibleRows = LINES - 1 - kFirstDataRow; // the last line is reserved for the status
      if (nRows == 0 || visibleRows < 1) {
         fState.fCursorRow = 0;
         fState.fRowOffset = 0;
         return;
      }
      if (fState.fCursorRow >= nRows)
         fState.fCursorRow = nRows - 1;
      if (fState.fCursorRow < fState.fRowOffset)
         fState.fRowOffset = fState.fCursorRow;
      else if (fState.fCursorRow >= fState.fRowOffset + size_t(visibleRows))
         fState.fRowOffset = fState.fCursorRow - size_t(visibleRows) + 1;
   }

   void EnsureColOffset()
   {
      const size_t nCols = fTable->fColumnNames.size();
      if (nCols == 0)
         return;
      if (fState.fCursorCol >= nCols)
         fState.fCursorCol = nCols - 1;
      if (fState.fCursorCol < fState.fColOffset)
         fState.fColOffset = fState.fCursorCol;
      // Scroll to the right until the selected column fits on screen
      while (fState.fColOffset < fState.fCursorCol) {
         const int x = ColumnX(fState.fCursorCol);
         const int w = int(fTable->fColumnWidths[fState.fCursorCol]) + 3;
         if (x + w <= COLS - 1)
            break;
         ++fState.fColOffset;
      }
   }

   void HandleKey(int ch)
   {
      const size_t nRows = fTable->fCells.size();
      const size_t nCols = fTable->fColumnNames.size();
      const int visibleRows = LINES - 1 - kFirstDataRow;
      const size_t page = visibleRows > 0 ? size_t(visibleRows) : 1;
      switch (ch) {
      case KEY_UP:
         if (fState.fCursorRow > 0)
            --fState.fCursorRow;
         break;
      case KEY_DOWN:
         if (fState.fCursorRow + 1 < nRows)
            ++fState.fCursorRow;
         break;
      case KEY_LEFT:
         if (fState.fCursorCol > 0)
            --fState.fCursorCol;
         break;
      case KEY_RIGHT:
         if (fState.fCursorCol + 1 < nCols)
            ++fState.fCursorCol;
         break;
      case KEY_PPAGE: fState.fCursorRow = fState.fCursorRow > page ? fState.fCursorRow - page : 0; break;
      case KEY_NPAGE:
         fState.fCursorRow = nRows > 0 ? std::min(nRows - 1, fState.fCursorRow + page) : 0;
         break;
      case KEY_HOME: fState.fCursorRow = 0; break;
      case KEY_END: fState.fCursorRow = nRows > 0 ? nRows - 1 : 0; break;
      default: break; // includes KEY_RESIZE: just redraw
      }
   }

   void DrawStatus(size_t nRows, size_t nCols)
   {
      std::stringstream ss;
      ss << " Row " << (nRows > 0 ? fState.fCursorRow + 1 : 0) << "/" << nRows;
      if (nCols > 0)
         ss << "  Col \"" << fTable->fColumnNames[fState.fCursorCol] << "\" (" << fState.fCursorCol + 1 << "/" << nCols
            << ")";
      ss << "   |   arrows: move   PgUp/PgDn: page   Home/End: jump   q: quit";
      DrawText(LINES - 1, 0, ss.str(), COLS, A_NORMAL);
   }

   void Draw()
   {
      const size_t nCols = fTable->fColumnNames.size();
      const size_t nRows = fTable->fCells.size();

      erase();

      if (LINES < kMinLines || COLS < kMinCols) {
         mvaddstr(0, 0, "Terminal too small for the TUI viewer: enlarge it or press q to quit");
         refresh();
         return;
      }

      DrawText(kTitleRow, 0, fTitle + "  [q: quit]", COLS, A_BOLD);

      if (nCols == 0) {
         mvaddstr(kHeaderRow, 0, "(no columns to display)");
         DrawStatus(nRows, nCols);
         refresh();
         return;
      }

      EnsureRowOffset();
      EnsureColOffset();

      // Figure out which columns fit on screen, starting from the first visible one
      const size_t firstCol = fState.fColOffset;
      size_t lastCol = firstCol;
      int x = 1;
      while (lastCol < nCols && x + int(fTable->fColumnWidths[lastCol]) + 3 <= COLS) {
         x += int(fTable->fColumnWidths[lastCol]) + 3;
         ++lastCol;
      }
      if (lastCol == firstCol)
         ++lastCol; // always show at least one column
      const bool moreToTheRight = lastCol < nCols;
      const bool moreToTheLeft = firstCol > 0;
      const int maxX = COLS - (moreToTheRight ? 2 : 1); // rightmost x usable for cell content

      // Header row
      for (size_t c = firstCol; c < lastCol; ++c) {
         const int cx = ColumnX(c);
         if (cx > maxX)
            break;
         mvaddch(kHeaderRow, cx, '|');
         DrawText(kHeaderRow, cx + 1, PadRight(fTable->fColumnNames[c], fTable->fColumnWidths[c]), maxX, HeaderAttr());
      }
      if (x < COLS - 1)
         mvaddch(kHeaderRow, x, '|');

      // Separator row
      for (size_t c = firstCol; c < lastCol; ++c) {
         const int cx = ColumnX(c);
         if (cx > maxX)
            break;
         mvaddch(kSeparatorRow, cx, '+');
         DrawText(kSeparatorRow, cx + 1, std::string(fTable->fColumnWidths[c] + 2, '-'), maxX, A_NORMAL);
      }
      if (x < COLS - 1)
         mvaddch(kSeparatorRow, x, '+');

      // Data rows
      const int visibleRows = LINES - 1 - kFirstDataRow;
      for (int i = 0; i < visibleRows; ++i) {
         const size_t row = fState.fRowOffset + size_t(i);
         if (row >= nRows)
            break;
         const int y = kFirstDataRow + i;
         for (size_t c = firstCol; c < lastCol; ++c) {
            const int cx = ColumnX(c);
            if (cx > maxX)
               break;
            const bool isSelected = row == fState.fCursorRow && c == fState.fCursorCol;
            mvaddch(y, cx, '|');
            const std::string cell = c < fTable->fCells[row].size() ? fTable->fCells[row][c] : "";
            DrawText(y, cx + 1, PadRight(cell, fTable->fColumnWidths[c]), maxX, isSelected ? SelectedAttr() : A_NORMAL);
         }
         if (x < COLS - 1)
            mvaddch(y, x, '|');
         if (moreToTheRight)
            mvaddch(y, COLS - 1, '>');
         if (moreToTheLeft)
            mvaddch(y, 0, '<');
      }

      // Column indicators on header and separator rows as well
      if (moreToTheRight) {
         mvaddch(kHeaderRow, COLS - 1, '>');
         mvaddch(kSeparatorRow, COLS - 1, '>');
      }
      if (moreToTheLeft) {
         mvaddch(kHeaderRow, 0, '<');
         mvaddch(kSeparatorRow, 0, '<');
      }

      DrawStatus(nRows, nCols);
      refresh();
   }
};

} // namespace
#endif // ROOT_RDF_TUI_CURSES_ENABLED

void LaunchRTUIViewer(const RTUITable &table, const std::string &title)
{
#ifdef ROOT_RDF_TUI_CURSES_ENABLED
   if (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO)) {
      RTUICursesViewer viewer;
      viewer.Run(table, title);
      return;
   }
   Info("DisplayTUI", "Standard input/output is not a terminal: printing a plain-text table instead.\n");
#else
   Info("DisplayTUI", "ROOT was built without ncurses support (-Drdf_tui=ON): printing a plain-text table instead.\n");
#endif
   std::cout << title << "\n" << RenderRTUITableText(table) << std::flush;
}

} // namespace RDF
} // namespace Internal
} // namespace ROOT
