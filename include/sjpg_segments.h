//
// Created by user on 2/7/24.
//
# pragma once 

#include "sjpg_log.h"
#include "sjpg_markers.h"
#include <vector>
namespace sjpg_codec::segments {

class SOISegment {
public:
  size_t file_pos{0}; // segment start position in file(without marker)
  static const uint8_t marker = JFIF_SOI;

  void print() const {
    LOG_INFO("SOI segment\n");
    LOG_INFO("\tFile position: %zu\n", file_pos);
  }
};

class APP0Segment {
public:
  size_t file_pos{0}; // segment start position in file(without marker)
  int length{0};
  char identifier[5]{};
  int major_version{0};
  int minor_version{0};
  int pixel_units{0};
  int x_density{0};
  int y_density{0};
  int thumbnail_width{0};
  int thumbnail_height{0};
  std::vector<uint8_t> thumbnail_data;
  static const uint8_t marker = JFIF_APP0;

  void print() const {
    LOG_INFO("APP0 segment\n");
    LOG_INFO("\tFile position: %zu\n", file_pos);
    LOG_INFO("\tLength: %d\n", length);
    LOG_INFO("\tIdentifier: %s\n", identifier);
    LOG_INFO("\tVersion: %d.%d\n", major_version, minor_version);
    LOG_INFO("\tPixel units: %d\n", pixel_units);
    LOG_INFO("\tX density: %d\n", x_density);
    LOG_INFO("\tY density: %d\n", y_density);
    LOG_INFO("\tThumbnail width: %d\n", thumbnail_width);
    LOG_INFO("\tThumbnail height: %d\n", thumbnail_height);
    LOG_INFO("\tThumbnail data size: %zu\n", thumbnail_data.size());
  }
};

class COMSegment {
public:
  size_t file_pos{0}; // segment start position in file(without marker)
  int length{0};
  std::vector<uint8_t> comment;
  static const uint8_t marker = JFIF_COM;

  void print() const {
    LOG_INFO("COM segment\n");
    LOG_INFO("\tFile position: %zu\n", file_pos);
    LOG_INFO("\tLength: %d\n", length);
    auto temp = std::string(comment.begin(), comment.end());
    LOG_INFO("\tComment: %s\n", temp.c_str());
  }
};

class QuantizationTable {
public:
  uint8_t id{0};
  uint8_t precision{0}; // 0: 8-bit, 1: 16-bit
  std::vector<uint8_t> data;
  size_t segment_pos{0}; // 在哪个 dqt 段中定义的

  void print() const {
    LOG_INFO("Quantization Table\n");
    LOG_INFO("\tTable ID: %d\n", id);
    LOG_INFO("\tPrecision: %d (%s)\n", precision, precision ? "16-bit" : "8-bit");
    LOG_INFO("\tData size: %zu values\n", data.size());

    // 打印8x8矩阵
    LOG_INFO("\tMatrix:\n");
    for (int i = 0; i < 8; ++i) {
      LOG_INFO("\t\t");
      for (int j = 0; j < 8; ++j) {
        LOG_INFO("%3d ", data[i*8 + j]);
      }
      LOG_INFO("\n");
    }
  }
};

class DQTSegment {
public:
  size_t file_pos{0}; // segment start position in file(without marker)
  uint16_t length{0};
  std::vector<QuantizationTable> tables;
  static const uint8_t marker = JFIF_DQT;

  void print() const {
    LOG_INFO("DQT Segment\n");
    LOG_INFO("\tFile position: %zu\n", file_pos);
    LOG_INFO("\tLength: %d\n", length);
    LOG_INFO("\tNumber of tables: %zu\n", tables.size());

    for (const auto& table : tables) {
      table.print();
    }
  }
};

class SOF0Segment {
public:
  size_t file_pos{0}; // segment start position in file(without marker)
  uint16_t length{0};
  uint8_t bitPerSample{0};
  uint16_t height{0};
  uint16_t width{0};
  uint8_t num_components{0};
  std::vector<uint8_t> component_id;
  std::vector<uint8_t> sampling_factor;
  std::vector<uint8_t> quantization_table_id;
  static const uint8_t marker = JFIF_SOF0;

  void print() const {
    LOG_INFO("SOF0 segment\n");
    LOG_INFO("\tFile position: %zu\n", file_pos);
    LOG_INFO("\tLength: %d\n", length);
    LOG_INFO("\tBitPerSample: %d\n", bitPerSample);
    LOG_INFO("\tHeight: %d\n", height);
    LOG_INFO("\tWidth: %d\n", width);
    LOG_INFO("\tNumber of components: %d\n", num_components);
    for (size_t i = 0; i < num_components; i++) {
      LOG_INFO("\tComponent ID: %d\n", component_id[i]);
      auto h_sampling_factor = sampling_factor[i] >> 4;
      auto v_sampling_factor = sampling_factor[i] & 0x0F;
      LOG_INFO("\tSampling factor: %d:%d\n", h_sampling_factor, v_sampling_factor);
      LOG_INFO("\tQuantization table ID: %d\n", quantization_table_id[i]);
    }
  }
};

class DHTSegment {
public:
  size_t file_pos{0}; // segment start position in file(without marker)
  uint16_t length{0};
  uint8_t dc_or_ac{0}; // 0: DC, 1: AC
  uint8_t table_id{0};
  std::vector<uint8_t> symbol_counts;
  std::vector<uint8_t> symbols;
  static const uint8_t marker = JFIF_DHT;

  void print() const {
    LOG_INFO("DHT segment\n");
    LOG_INFO("\tFile position: %zu\n", file_pos);
    LOG_INFO("\tLength: %d\n", length);
    LOG_INFO("\tDC(0)/AC(1): %d\n", dc_or_ac);
    LOG_INFO("\tTable ID: %d\n", table_id);

    LOG_INFO("\tSymbol counts(%zu): \n", symbol_counts.size());
    for (auto c : symbol_counts) {
      LOG_INFO("%d,", c);
    }
    LOG_INFO("\n");

    LOG_INFO("\tSymbols(%zu): \n", symbols.size());
    for (auto s : symbols) {
      LOG_INFO("%d,", s);
    }
    LOG_INFO("\n");
  }
};

class SOSSegment {
public:
  size_t file_pos{0}; // segment start position in file(without marker)
  uint16_t length{0};
  uint8_t num_components{0};
  std::vector<uint8_t> component_id;
  std::vector<uint8_t> huffman_table_id_dc;
  std::vector<uint8_t> huffman_table_id_ac;
  static const uint8_t marker = JFIF_SOS;

  void print() {
    LOG_INFO("SOS segment\n");
    LOG_INFO("\tFile position: %zu\n", file_pos);
    LOG_INFO("\tLength: %d\n", length);
    LOG_INFO("\tNumber of components: %d\n", num_components);
    for (size_t i = 0; i < num_components; i++) {
      LOG_INFO("\tComponent ID: %d\n", component_id[i]);
      LOG_INFO("\tHuffman table ID DC: %d\n", huffman_table_id_dc[i]);
      LOG_INFO("\tHuffman table ID AC: %d\n", huffman_table_id_ac[i]);
    }
  }
};

class EOISegment {
public:
  size_t file_pos{0}; // segment start position in file(without marker)
  static const uint8_t marker = JFIF_EOI;

  void print() const {
    LOG_INFO("EOI segment\n");
    LOG_INFO("\tFile position: %zu\n", file_pos);
  }
};
} // namespace sjpg_codec::segments

