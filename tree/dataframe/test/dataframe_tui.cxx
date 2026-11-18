/****** Tests for the rendering-agnostic part of the RDataFrame TUI viewer *******/
#include <gtest/gtest.h>
#include <ROOT/RDataFrame.hxx>
#include <ROOT/RDF/RDisplay.hxx>
#include <ROOT/RDF/RTUIViewer.hxx>

#include <string>
#include <vector>

using namespace ROOT;
using ROOT::Internal::RDF::BuildRTUITable;
using ROOT::Internal::RDF::RenderRTUITableText;

TEST(RDataFrameTUI, BuildTableFromDisplay)
{
   RDataFrame df(4);
   auto dfXY = df.Define("x", [](ULong64_t e) { return int(e); }, {"rdfentry_"})
                  .Define("y", [](int x) { return 2.5 * x; }, {"x"});
   auto display = dfXY.Display({"x", "y"}, 4);
   const auto table = BuildRTUITable(*display.GetPtr());

   EXPECT_EQ(table.fColumnNames, (std::vector<std::string>{"Row", "x", "y"}));
   ASSERT_EQ(table.fCells.size(), 4u);
   EXPECT_EQ(table.fCells[0], (std::vector<std::string>{"0", "0", "0.000000"}));
   EXPECT_EQ(table.fCells[3], (std::vector<std::string>{"3", "3", "7.500000"}));
   ASSERT_EQ(table.fColumnWidths.size(), 3u);
   EXPECT_EQ(table.fColumnWidths[0], 3u); // "Row"
   EXPECT_EQ(table.fColumnWidths[2], 8u); // "7.500000"
}

TEST(RDataFrameTUI, CollectionsAndDots)
{
   RDataFrame df(2);
   // Show at most one collection element per row: the second one is rendered as "..."
   auto dd = df.Define("v", [](ULong64_t e) { return std::vector<int>{int(e), int(e) + 10}; }, {"rdfentry_"})
                .Display({"v"}, 2, 1);
   const auto table = BuildRTUITable(*dd.GetPtr());

   EXPECT_EQ(table.fColumnNames, (std::vector<std::string>{"Row", "v"}));
   // Each of the two events expands to two rows: the element and the "..."
   ASSERT_EQ(table.fCells.size(), 4u);
   EXPECT_EQ(table.fCells[0], (std::vector<std::string>{"0", "0"}));
   EXPECT_EQ(table.fCells[1], (std::vector<std::string>{"", "..."}));
   EXPECT_EQ(table.fCells[2], (std::vector<std::string>{"1", "1"}));
   EXPECT_EQ(table.fCells[3], (std::vector<std::string>{"", "..."}));
}

TEST(RDataFrameTUI, RenderTextTable)
{
   RDataFrame df(2);
   auto dd = df.Define("x", [](ULong64_t e) { return int(e); }, {"rdfentry_"}).Display({"x"}, 2);
   const auto table = BuildRTUITable(*dd.GetPtr());
   const std::string text = RenderRTUITableText(table);

   EXPECT_EQ(text, "+-----+---+\n"
                   "| Row | x |\n"
                   "+-----+---+\n"
                   "| 0   | 0 |\n"
                   "| 1   | 1 |\n"
                   "+-----+---+\n");
}

TEST(RDataFrameTUI, EmptyDataFrame)
{
   RDataFrame df(0);
   auto dd = df.Define("x", [](ULong64_t e) { return int(e); }, {"rdfentry_"}).Display({"x"}, 5);
   const auto table = BuildRTUITable(*dd.GetPtr());

   EXPECT_EQ(table.fColumnNames, (std::vector<std::string>{"Row", "x"}));
   EXPECT_TRUE(table.fCells.empty());
   EXPECT_EQ(RenderRTUITableText(table), "+-----+---+\n"
                                         "| Row | x |\n"
                                         "+-----+---+\n"
                                         "+-----+---+\n");
}
