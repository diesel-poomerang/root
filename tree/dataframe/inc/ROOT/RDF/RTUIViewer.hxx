/*************************************************************************
 * Copyright (C) 1995-2026, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#ifndef ROOT_RDF_RTUIVIEWER
#define ROOT_RDF_RTUIVIEWER

#include "ROOT/RDF/RDisplay.hxx"

#include <cstddef>
#include <string>
#include <vector>

namespace ROOT {
namespace Internal {
namespace RDF {

/// A rendering-agnostic snapshot of the tabular content buffered by an RDisplay: column names, column widths and
/// the string representation of each cell. The first entry of fColumnNames is the entry index column ("Row").
struct RTUITable {
   std::vector<std::string> fColumnNames;        ///< Name of each column, including the leading "Row" column
   std::vector<size_t> fColumnWidths;            ///< Width in characters of each column
   std::vector<std::vector<std::string>> fCells; ///< String representation of the cells, one vector per row
};

/// Extract a rendering-agnostic table from the content buffered by an RDisplay.
RTUITable BuildRTUITable(const ROOT::RDF::RDisplay &display);

/// Render an RTUITable as a plain-text (markdown-like) table.
/// Used as fallback output when no interactive terminal is available.
std::string RenderRTUITableText(const RTUITable &table);

/// Show an RTUITable in an interactive full-screen ncurses table that can be navigated with the arrow keys.
/// Blocks until the user quits the viewer. If ROOT was built without curses support or standard input/output is
/// not a terminal, falls back to printing RenderRTUITableText() to standard output.
void LaunchRTUIViewer(const RTUITable &table, const std::string &title);

} // namespace RDF
} // namespace Internal
} // namespace ROOT

#endif
