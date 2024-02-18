//
// Created by user on 2/7/24.
//
#include "sjpg_bit_stream.h"
#include "sjpg_huffman_table.h"
#include "sjpg_segments.h"
#include <fstream>

#include <gmock/gmock.h>

using namespace testing;

namespace sjpg_codec {
using namespace segments;

class Decoder {
public:
  bool open(const std::string &filepath) {
    in_file_.open(filepath, std::ios::in | std::ios::binary);
    if (!in_file_.is_open() || !in_file_.good()) {
      LOG_ERROR("Failed to open file %s\n", filepath.c_str());
      return false;
    }

    filepath_ = filepath;
    return true;
  }

  int decodeImageFile() {
    if (!isFileOpened()) {
      LOG_ERROR("File not opened, you cannot decode it\n");
      return -1;
    }

    auto ret = 0;
    for (;;) {
      if (in_file_.eof()) {
        LOG_INFO("End of file\n");
        break;
      }

      auto b = readByte();
      if (b == kJFIFByteFF) {
        b = readByte();
        ret = parseSegment(b);
      }
    }

    if (ret != 0) {
      LOG_ERROR("Failed to parse segment\n");
      return ret;
    }

    // decode scan data
    buildHuffmanTable();

    const auto pixel_count = sof0_.width * sof0_.height;
    y_decoded_data_.resize(pixel_count, 0);
    auto y_index = 0;
    u_decoded_data_.resize(pixel_count, 0);
    auto u_index = 0;
    v_decoded_data_.resize(pixel_count, 0);
    auto v_index = 0;

    const auto mcu_count = pixel_count / kMCUPixelSize;
    int16_t pre_dc_value_y = 0;
    int16_t pre_dc_value_u = 0;
    int16_t pre_dc_value_v = 0;

    for (int i = 0; i < mcu_count; i++) {
      auto y = deHuffman(0, pre_dc_value_y);
      auto u = deHuffman(1, pre_dc_value_u);
      auto v = deHuffman(2, pre_dc_value_v);

      y = deZigZag(y);
      u = deZigZag(u);
      v = deZigZag(v);

      auto idct_y = idct(y);
      auto idct_u = idct(u);
      auto idct_v = idct(v);

      performLevelShift(idct_y);
      performLevelShift(idct_u);
      performLevelShift(idct_v);

      for (int j = 0; j < kMCUPixelSize; j++) {
        y_decoded_data_[y_index++] = y[j];
        u_decoded_data_[u_index++] = u[j];
        v_decoded_data_[v_index++] = v[j];
      }
    }

    return 0;
  }

  const std::string &getFilePath() const { return filepath_; }
  const SOISegment &getSOISegment() const { return soi_; }
  const APP0Segment &getAPP0Segment() const { return app0_; }
  const COMSegment &getCOMSegment() const { return com_; }
  const std::vector<DQTSegment> &getDQTSegments() const { return dqt_; }
  const SOF0Segment &getSOF0Segment() const { return sof0_; }
  const std::vector<DHTSegment> &getDHTSegments() const { return dht_; }
  const SOSSegment &getSOSSegment() const { return sos_; }
  const EOISegment &getEOISegment() const { return eoi_; }

private:
  bool isFileOpened() { return in_file_.is_open(); }
  void performLevelShift(std::vector<float> &data) {
    for (auto i = 0; i < data.size(); ++i) {
      data[i] += 128;
    }
  }
  std::vector<float> idct(const std::vector<int16_t> &data) {

    std::vector<float> result(kMCUPixelSize, 0.0f);
    for (auto y = 0; y < 8; ++y) {
      for (auto x = 0; x < 8; ++x) {
        auto sum = 0.0f;

        for (auto u = 0; u < 8; ++u) {
          for (auto v = 0; v < 8; ++v) {
            float cu = (u == 0) ? 1.0f / std::sqrt(2.0f) : 1.0f;
            float cv = (v == 0) ? 1.0f / std::sqrt(2.0f) : 1.0f;
            auto data_value = data[u * 8 + v];

            sum += cu * cv * data_value *
                   std::cos((2 * x + 1) * u * M_PI / 16.0) *
                   std::cos((2 * y + 1) * v * M_PI / 16.0);
          }
        }

        sum *= 0.25;
        result[y * 8 + x] = sum;
      }
    }

    return result;
  }
  std::vector<int16_t> deZigZag(const std::vector<int16_t> &data) {
    constexpr static int zz_order[kMCUPixelSize] = {
        0,  1,  5,  6,  14, 15, 27, 28, 2,  4,  7,  13, 16, 26, 29, 42,
        3,  8,  12, 17, 25, 30, 41, 43, 9,  11, 18, 24, 31, 40, 44, 53,
        10, 19, 23, 32, 39, 45, 52, 54, 20, 22, 33, 38, 46, 51, 55, 60,
        21, 34, 37, 47, 50, 56, 59, 61, 35, 36, 48, 49, 57, 58, 62, 63};

    std::vector<int16_t> zigzag_data(kMCUPixelSize, 0);
    for (int i = 0; i < kMCUPixelSize; i++) {
      zigzag_data[zz_order[i]] = data[i];
    }
    return zigzag_data;
  }
  std::vector<int16_t> deHuffman(int component_id, int16_t &pre_dc_value) {
    std::vector<int16_t> decoded_data(kMCUPixelSize, 0);
    int index = 0;

    auto ac_or_dc = 0;
    auto htable_ac_id = sos_.huffman_table_id_ac[component_id];
    auto htable_dc_id = sos_.huffman_table_id_dc[component_id];

    ac_or_dc = 0;
    const auto &dc_htable = huffman_table_.at({ac_or_dc, htable_ac_id});
    ac_or_dc = 1;
    const auto &ac_table = huffman_table_.at({ac_or_dc, htable_dc_id});

    const auto qtable_id = sof0_.quantization_table_id[component_id];
    const auto &qtable = dqt_[qtable_id].q_table;

    // decode ac value
    auto dc_code_length = dc_htable.getSymbol(st_);
    auto dc_bits = st_.getBitN(dc_code_length);
    int16_t dc_value = decodeNumber(dc_code_length, dc_bits) + pre_dc_value;
    pre_dc_value = dc_value;

    int16_t dequant_dc_value = dc_value * qtable[0];
    decoded_data[index++] = dequant_dc_value;

    // decode dc value
    for (; index < kMCUPixelSize;) {
      auto rrrr_ssss = ac_table.getSymbol(st_);
      // EOF
      if (rrrr_ssss == 0) {
        break;
      }
      auto run_length = rrrr_ssss >> 4;
      auto ssss = rrrr_ssss & 0x0F; // ssss is the code length
      auto bits = st_.getBitN(ssss);
      int16_t non_zero_value = decodeNumber(ssss, bits);

      if (run_length == 15 && non_zero_value == 0) {
        index += 16;
      } else {
        // insert 0s
        index += run_length;
        // insert non zero value
        auto de_quant_value = non_zero_value * qtable[index];
        decoded_data[index++] = de_quant_value;
      }
    }

    return decoded_data;
  }

  static int16_t decodeNumber(uint16_t code_length, const std::string &bits) {
    auto l = 1 << (code_length - 1); // 2**(code_length-1)
    auto v = std::bitset<16>(bits).to_ulong();
    if (v >= l) {
      return v;
    } else {
      return v - (2 * l - 1);
    }
  };

  void parseSOISegment() {
    soi_.file_pos = in_file_.tellg();
    soi_.print();
  }
  int parseSegment(uint8_t b) {
    auto ret = 0;
    switch (b) {
    case SOISegment::marker:
      parseSOISegment();
      break;
    case APP0Segment::marker:
      parseAPP0Segment();
      break;
    case COMSegment::marker:
      parseCOMSegment();
      break;
    case DQTSegment::marker:
      parseDQTSegment();
      break;
    case SOF0Segment::marker:
      ret = parseSOF0Segment();
      break;
    case DHTSegment::marker:
      parseDHTSegment();
      break;
    case SOSSegment::marker:
      parseSOSSegment();
      scanImageData();
      break;
    default:
      break;
    }

    return ret;
  }
  void parseAPP0Segment() {
    app0_.file_pos = in_file_.tellg();

    auto length = read2BytesBigEndian();
    app0_.length = length;
    in_file_.read(app0_.identifier, 5);

    app0_.major_version = readByte();
    app0_.minor_version = readByte();
    app0_.pixel_units = readByte();
    app0_.x_density = read2BytesBigEndian();
    app0_.y_density = read2BytesBigEndian();
    app0_.thumbnail_width = readByte();
    app0_.thumbnail_height = readByte();

    auto thumbnail_data_length =
        3 * app0_.thumbnail_width * app0_.thumbnail_height;
    if (thumbnail_data_length != 0) {
      app0_.thumbnail_data.resize(thumbnail_data_length);
      in_file_.read(reinterpret_cast<char *>(app0_.thumbnail_data.data()),
                    thumbnail_data_length);
    }

    app0_.print();
  }
  void parseEOISegment() {
    eoi_.file_pos = in_file_.tellg();
    eoi_.print();
  }
  void parseCOMSegment() {
    com_.file_pos = in_file_.tellg();
    auto length = read2BytesBigEndian();
    com_.length = length;
    com_.comment.resize(length - 2);
    in_file_.read(reinterpret_cast<char *>(com_.comment.data()), length - 2);
    com_.print();
  }

  void parseDQTSegment() {
    DQTSegment dqt;
    dqt.file_pos = in_file_.tellg();
    dqt.length = read2BytesBigEndian();
    auto tmp = readByte();
    dqt.precision = tmp >> 4;
    dqt.q_table_id = tmp & 0x0F;

    const static int q_table_size = kMCUPixelSize;
    dqt.q_table.resize(q_table_size);
    in_file_.read(reinterpret_cast<char *>(dqt.q_table.data()), q_table_size);

    dqt_.push_back(dqt);

    dqt.print();
  }

  int parseSOF0Segment() {
    sof0_.file_pos = in_file_.tellg();
    sof0_.length = read2BytesBigEndian();
    sof0_.precision = readByte();
    sof0_.height = read2BytesBigEndian();
    sof0_.width = read2BytesBigEndian();
    sof0_.num_components = readByte();

    bool is_non_sampled = true;
    for (int i = 0; i < sof0_.num_components; i++) {
      auto b0 = readByte();
      auto b1 = readByte();
      auto b2 = readByte();

      sof0_.component_id.push_back(b0);
      sof0_.sampling_factor.push_back(b1);
      sof0_.quantization_table_id.push_back(b2);

      auto h_sampling_factor = b1 >> 4;
      auto v_sampling_factor = b1 & 0x0F;

      if (h_sampling_factor != 1 || v_sampling_factor != 1) {
        is_non_sampled = false;
      }
    }

    sof0_.print();
    if (!is_non_sampled) {
      LOG_INFO("Sampled image is not support ,only support yuv 4:4:4\n");
      return -1;
    }

    return 0;
  }

  void parseDHTSegment() {
    DHTSegment dht;
    dht.file_pos = in_file_.tellg();
    dht.length = read2BytesBigEndian();
    auto b = readByte();
    dht.ac_or_dc = b >> 4;
    dht.table_id = b & 0x0F;

    const static int huffman_table_size = 16;
    dht.symbol_counts.resize(huffman_table_size);
    in_file_.read(reinterpret_cast<char *>(dht.symbol_counts.data()),
                  huffman_table_size);

    auto num_symbols =
        std::accumulate(dht.symbol_counts.begin(), dht.symbol_counts.end(), 0);
    dht.symbols.resize(num_symbols);
    in_file_.read(reinterpret_cast<char *>(dht.symbols.data()), num_symbols);

    dht.print();
    dht_.push_back(dht);
  }

  void parseSOSSegment() {
    sos_.file_pos = in_file_.tellg();
    sos_.length = read2BytesBigEndian();
    sos_.num_components = readByte();
    for (int i = 0; i < sos_.num_components; i++) {
      auto b0 = readByte();
      auto b1 = readByte();
      sos_.component_id.push_back(b0);

      auto huffman_table_id_dc = b1 >> 4;
      auto huffman_table_id_ac = b1 & 0x0F;
      sos_.huffman_table_id_ac.push_back(huffman_table_id_ac);
      sos_.huffman_table_id_dc.push_back(huffman_table_id_dc);
    }

    // skip 3 bytes
    for (int i = 0; i < 3; i++) {
      readByte();
    }
    sos_.print();
  }

  void scanImageData() {
    for (;;) {
      auto b = readByte();

      if (b == kJFIFByteFF) {
        auto pre_b = b;
        auto next_b = readByte();

        if (next_b == EOISegment::marker) {
          parseEOISegment();
          break;
        } else if (next_b == kEmptyByte) {
          std::bitset<8> bits(pre_b);
          st_.append(bits.to_string());
          continue;
        }
      }

      std::bitset<8> bits(b);
      st_.append(bits.to_string());
    }

    LOG_INFO("Scan data size: %zu\n", st_.getSize());
  }

  uint16_t read2BytesBigEndian() {
    uint8_t byte1;
    uint8_t byte2;
    in_file_.read(reinterpret_cast<char *>(&byte1), sizeof(byte1));
    in_file_.read(reinterpret_cast<char *>(&byte2), sizeof(byte2));
    return (byte1 << 8) | byte2;
  }
  uint8_t readByte() {
    uint8_t value;
    in_file_.read(reinterpret_cast<char *>(&value), sizeof(value));
    return value;
  }

  void buildHuffmanTable() {
    for (const auto &dht : dht_) {
      auto id = std::make_pair(dht.ac_or_dc, dht.table_id);
      auto table = HuffmanTable(dht.symbol_counts, dht.symbols);
      huffman_table_.insert({id, table});
    }
  }

  std::ifstream in_file_;
  std::string filepath_;

  // header segments
  SOISegment soi_;
  APP0Segment app0_;
  COMSegment com_;
  std::vector<DQTSegment> dqt_;
  SOF0Segment sof0_;
  std::vector<DHTSegment> dht_;
  std::map<std::pair<uint8_t, uint8_t>, HuffmanTable>
      huffman_table_; // <ac/dc, table_id>
  SOSSegment sos_;
  EOISegment eoi_;

  std::string scan_data_;
  BitStream st_;

  std::vector<uint8_t> y_decoded_data_;
  std::vector<uint8_t> u_decoded_data_;
  std::vector<uint8_t> v_decoded_data_;

  constexpr static int kMCUPixelSize = 64;
};
} // namespace sjpg_codec

class AJPEGDecoder : public Test {
public:
  std::string filepath =
      "/Users/user/Documents/develop/simple_jpeg_codec/resources/lenna.jpg";
  sjpg_codec::Decoder decoder;
};

TEST_F(AJPEGDecoder, OpenFileReturnOKIfFileExists) {
  auto ok = decoder.open(filepath);

  ASSERT_TRUE(ok);
}

TEST_F(AJPEGDecoder, OpenFileReturnFalseIfFileDoesNotExist) {
  auto ok = decoder.open("nonexistent.jpg");

  ASSERT_FALSE(ok);
}

TEST_F(AJPEGDecoder, CanGetCurrentOpenedFile) {
  decoder.open(filepath);
  ASSERT_EQ(filepath, decoder.getFilePath());
}

TEST_F(AJPEGDecoder, DecodeFailedIfFileNotOpened) {
  auto ret = decoder.decodeImageFile();

  ASSERT_THAT(ret, Eq(-1));
}

TEST_F(AJPEGDecoder, CanDecodeFileIfOpened) {
  decoder.open(filepath);
  auto ret = decoder.decodeImageFile();

  ASSERT_THAT(ret, Eq(0));
}

TEST_F(AJPEGDecoder, DecodeFileGetSOIAndEOISegment) {
  decoder.open(filepath);
  decoder.decodeImageFile();

  ASSERT_THAT(decoder.getSOISegment().file_pos, Ne(0));
  ASSERT_THAT(decoder.getEOISegment().file_pos, Ne(0));
}

TEST_F(AJPEGDecoder, DecodeFileGetAPP0Segment) {
  decoder.open(filepath);
  decoder.decodeImageFile();

  ASSERT_THAT(decoder.getAPP0Segment().file_pos, Ne(0));
}

TEST_F(AJPEGDecoder, DecodeFileGetCOMSegment) {
  decoder.open(filepath);
  decoder.decodeImageFile();

  ASSERT_THAT(decoder.getCOMSegment().file_pos, Ne(0));
}

TEST_F(AJPEGDecoder, DecodeFileGetDQTSegment) {
  decoder.open(filepath);
  decoder.decodeImageFile();

  auto dqt_count = decoder.getDQTSegments().size();

  ASSERT_THAT(dqt_count, Gt(0));
}

TEST_F(AJPEGDecoder, DecodeFileGetSOF0Segment) {
  decoder.open(filepath);
  decoder.decodeImageFile();

  auto sof0_count = decoder.getSOF0Segment();

  ASSERT_THAT(sof0_count.file_pos, Gt(0));
}

TEST_F(AJPEGDecoder, DecodeFileGetDHTSegment) {
  decoder.open(filepath);
  decoder.decodeImageFile();

  auto dht_segments = decoder.getDHTSegments();

  ASSERT_THAT(dht_segments.size(), Gt(0));
  ASSERT_THAT(dht_segments[0].file_pos, Gt(0));
}

TEST_F(AJPEGDecoder, DecodeFileGetSOSSegment) {
  decoder.open(filepath);
  decoder.decodeImageFile();

  auto sos = decoder.getSOSSegment();

  ASSERT_THAT(sos.file_pos, Gt(0));
}