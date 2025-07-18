//
// Created by user on 2/18/24.
//
#include <numeric>

#include "sjpg_bit_stream.h"
#include "sjpg_huffman_table.h"
#include <gmock/gmock.h>

using namespace testing;
using namespace sjpg_codec;
using namespace std;
class AHuffmanTable : public Test {
public:
  std::vector<uint8_t> sym_counts = {0, 3, 1, 1, 1, 1, 1, 1,
                                     1, 1, 1, 0, 0, 0, 0, 0};
  std::vector<uint8_t> symbols = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
};

TEST_F(AHuffmanTable, CanBuildWithCountsAndSymbols) {
  auto htable = HuffmanTable(sym_counts, symbols);

  ASSERT_THAT(htable.getSymbolCounts(), Eq(sym_counts));
  ASSERT_THAT(htable.getSymbols(), Eq(symbols));
}

TEST_F(AHuffmanTable, CanPrintHuffmanCodesAndSymbols) {
  auto htable = HuffmanTable(sym_counts, symbols);

  htable.print();
}

TEST_F(AHuffmanTable, CanCheckIsCantainsCodeOrNot) {
  auto htable = HuffmanTable(sym_counts, symbols);

  ASSERT_TRUE(htable.contains("00"));
  ASSERT_FALSE(htable.contains("111"));
}

TEST_F(AHuffmanTable, CanGetSymbolByCode) {
  auto htable = HuffmanTable(sym_counts, symbols);

  ASSERT_THAT(htable.getSymbol("00"), Eq(0));
  ASSERT_THAT(htable.getSymbol("110"), Eq(3));
}

TEST_F(AHuffmanTable, ThrowsIfCodeIsInvalid) {
  auto htable = HuffmanTable(sym_counts, symbols);

  ASSERT_THROW(htable.getSymbol("111"), std::out_of_range);
}

TEST_F(AHuffmanTable, CanGetSymbolFromBitStream) {
  auto htable = HuffmanTable(sym_counts, symbols);
  BitStream bitStream;
  bitStream.append("0001110"); // 0, 1, 3

  ASSERT_THAT(htable.getSymbol(bitStream), Eq(0));
  ASSERT_THAT(htable.getSymbol(bitStream), Eq(1));
  ASSERT_THAT(htable.getSymbol(bitStream), Eq(3));
}

TEST_F(AHuffmanTable, GetSymbolFromBitStreamIncreaseStreamPosition) {
  auto htable = HuffmanTable(sym_counts, symbols);
  BitStream st;
  st.append("0001110"); // 0, 1, 3

  htable.getSymbol(st);
  ASSERT_THAT(st.getPosition(), Eq(2));
}