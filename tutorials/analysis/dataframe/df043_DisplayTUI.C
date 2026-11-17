/// \file
/// \ingroup tutorial_dataframe
/// \notebook
/// Use the DisplayTUI function to inspect entry values in an interactive ncurses table viewer.
///
/// When run in a terminal, DisplayTUI opens a full-screen table that can be navigated with the
/// arrow keys (PageUp/PageDown and Home/End work as well); press 'q' to quit the viewer.
/// When standard input/output is not a terminal (e.g. when this tutorial runs in the test
/// suite), a plain-text table is printed instead.
///
/// \macro_code
///
/// \date 2026

#include <ROOT/RDF/RDFTUI.hxx>
#include <ROOT/RDataFrame.hxx>

void df043_DisplayTUI()
{
   // Setting up a Dataframe with some data
   ROOT::RDataFrame df(100);
   auto d = df.Define("x", [](ULong64_t e) { return int(e) % 7; })       // A column with ints
              .Define("y", [](int x) { return 2.5 * x; })                  // A column with doubles
              .Define("v", [](ULong64_t e) {                               // A column with a collection
                 return std::vector<int>{int(e), int(e) + 1, int(e) + 2};
              });

   // Show all columns of the first 10 events in the interactive viewer
   ROOT::RDF::DisplayTUI(d, "", 10);

   // Show only some columns of the first 50 events
   ROOT::RDF::DisplayTUI(d, {"x", "y"}, 50);
}
