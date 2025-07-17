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
  }

  std::vector<uint8_t> getSymbolsByHeight(uint8_t height) {
    assert(height >= 1);

    std::vector<uint8_t> symbols;
    auto count_of_this_height = symbol_counts_[height];
    auto count_sum_of_pre_heights = std::accumulate(
        symbol_counts_.begin(), symbol_counts_.begin() + height, 0);

    for (auto i = count_sum_of_pre_heights;
         i < count_sum_of_pre_heights + count_of_this_height; i++) {
      symbols.push_back(symbols_[i]);
    }

    return symbols;
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
  std::vector<uint8_t> symbol_counts_;
  std::vector<uint8_t> symbols_;
  std::map<std::string, uint16_t> code_symbol_map;
};
} // namespace sjpg_codec

