//
// Created by user on 2/18/24.
//
# pragma once
#include "sjpg_log.h"
#include "sjpg_bit_stream.h"
#include <bitset>
#include <map>
#include <numeric>
#include <vector>
#include <cstdint>
#include <cassert>

namespace sjpg_codec {
class HuffmanTable {
public:
  explicit HuffmanTable(std::vector<uint8_t> symbol_counts,
                        std::vector<uint8_t> symbols)
      : symbol_counts_(std::move(symbol_counts)), symbols_(std::move(symbols)) {
    buildHuffmanTree();

    code_symbol_map = buildHuffmanTable(symbol_counts_, symbols_);
  }

  bool contains(const std::string &code) const {
    return code_symbol_map.find(code) != code_symbol_map.end();
  }

  uint16_t getSymbol(const std::string &code) const {
    if (!contains(code)) {
      throw std::out_of_range("Code not found");
    }
    return code_symbol_map.at(code);
  }

  uint16_t getSymbol(BitStream &st) const {
    std::string code;
    while (true) {
      code.push_back(st.getBit());
      if (contains(code)) {
        return getSymbol(code);
      }
    }
  }

  void print() const {
    for (auto const &[key, val] : code_symbol_map) {
      LOG_INFO("Code: %s Symbol: %d\n", key.c_str(), val);
    }
  }

  const std::vector<uint8_t> &getSymbolCounts() const { return symbol_counts_; }

  const std::vector<uint8_t> &getSymbols() const { return symbols_; }

private:
  void buildHuffmanTree() {
    assert(symbol_counts_.size() == 16);

    constexpr static auto kTreeHeight = 16;

    uint16_t code = 0;
    int symbol_index = 0;
    for (auto height = 1; height <= kTreeHeight; height++) {
      auto count_in_this_height = symbol_counts_[height - 1];
      for (auto i = 0; i < count_in_this_height; i++) {
        auto symbol = symbols_[symbol_index++];
        auto code_str = std::bitset<kTreeHeight>(code).to_string();
        auto code_str_trimmed = code_str.substr(kTreeHeight - height);
        code_symbol_map[code_str_trimmed] = symbol;

        code += 1;
      }
      code *= 2;
    }
  }

  static std::map<std::string, uint16_t> buildHuffmanTable(
    const std::vector<uint8_t>& counts, // counts[16]
    const std::vector<uint8_t>& symbols // n个
) {
    constexpr static auto kMaxHuffmanCodeLength = 16;
    assert(counts.size() == kMaxHuffmanCodeLength);

    std::map<std::string, uint16_t> table;
    int code = 0;
    int symInd = 0;
    // length: 1 ~ 16 (JPEG哈夫曼表的码长范围)
    for (int length = 1; length <= kMaxHuffmanCodeLength; ++length) {
      int numCodes = counts[length - 1];
      for (int i = 0; i < numCodes; ++i) {
        // 生成指定位长的二进制字符串
        std::string codeStr = std::bitset<16>(code).to_string().substr(16 - length, length);
        table[codeStr] = symbols[symInd++];
        code++;
      }
      code <<= 1;
    }
    return table;
  }


  std::vector<uint8_t> symbol_counts_;
  std::vector<uint8_t> symbols_;
  std::map<std::string, uint16_t> code_symbol_map;
};
} // namespace sjpg_codec

