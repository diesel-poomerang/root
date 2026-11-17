/*************************************************************************
 * Copyright (C) 1995-2026, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#ifndef ROOT_RDF_RDFTUI
#define ROOT_RDF_RDFTUI

#include "ROOT/RDF/RInterface.hxx"
#include "ROOT/RDF/RTUIViewer.hxx"

#include <cstddef>
#include <string>

namespace ROOT {
namespace RDF {

////////////////////////////////////////////////////////////////////////////////////////////////
/// \brief Shows the content of the dataset in an interactive terminal (ncurses) table viewer.
/// \tparam Proxied Proxied type of the RInterface, e.g. RLoopManager for ROOT::RDataFrame.
/// \param[in] df The RDataFrame (or a derived RInterface, e.g. after Filter) to inspect.
/// \param[in] columnList Names of the columns to be shown.
/// \param[in] nRows Number of events for each column to be shown.
/// \param[in] nMaxCollectionElements Maximum number of collection elements shown per row.
///
/// The first \p nRows events are collected just like with RInterface::Display(), then shown in a full-screen
/// ncurses table that can be navigated with the arrow keys (PageUp/PageDown and Home/End work as well).
/// Press 'q' to quit the viewer and return to the prompt. If standard input/output is not a terminal or ROOT
/// was built without curses support (-Drdf_tui=OFF), a plain-text table is printed instead.
///
/// Example usage:
/// ~~~{.cpp}
/// ROOT::RDF::DisplayTUI(rdf, {"x", "y"}, 100); // show columns x and y of the first 100 events
/// ROOT::RDF::DisplayTUI(rdf, "");              // show all columns of the first 100 events
/// ~~~
template <typename Proxied>
void DisplayTUI(RInterface<Proxied> &df, const ColumnNames_t &columnList, size_t nRows = 100,
                size_t nMaxCollectionElements = 10)
{
   auto display = df.Display(columnList, nRows, nMaxCollectionElements);
   ROOT::Internal::RDF::LaunchRTUIViewer(ROOT::Internal::RDF::BuildRTUITable(*display.GetPtr()),
                                          "RDataFrame TUI viewer");
}

////////////////////////////////////////////////////////////////////////////////////////////////
/// \brief Shows the content of the dataset in an interactive terminal (ncurses) table viewer.
/// \tparam Proxied Proxied type of the RInterface, e.g. RLoopManager for ROOT::RDataFrame.
/// \param[in] df The RDataFrame (or a derived RInterface, e.g. after Filter) to inspect.
/// \param[in] columnNameRegexp A regular expression to select the columns. If empty, all columns are selected.
/// \param[in] nRows Number of events for each column to be shown.
/// \param[in] nMaxCollectionElements Maximum number of collection elements shown per row.
///
/// See the previous overload for further details.
template <typename Proxied>
void DisplayTUI(RInterface<Proxied> &df, const char *columnNameRegexp = "", size_t nRows = 100,
                size_t nMaxCollectionElements = 10)
{
   const auto columnNames = df.GetColumnNames();
   const auto selectedColumns = ROOT::Internal::RDF::ConvertRegexToColumns(columnNames, columnNameRegexp, "DisplayTUI");
   DisplayTUI(df, selectedColumns, nRows, nMaxCollectionElements);
}

} // namespace RDF
} // namespace ROOT

#endif
