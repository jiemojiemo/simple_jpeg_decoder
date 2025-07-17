//
// Created by user on 7/17/25.
//

#ifndef SJPG_JPEG_DECODER_H
#define SJPG_JPEG_DECODER_H

#include "sjpg_huffman_table.h"
#include "sjpg_jfif_parser.h"

namespace sjpg_codec {
class JPEGDecoder {
public:
  int decode(JFIFParser& parser) {
    if (!isSupported(parser)) {
      return -1;
    }

    prepare(parser);

    auto sof0 = parser.getSOF0Segment();
    auto mcu_size = getMCUSize(parser);
    auto h_block_size = mcu_size.first;
    auto v_block_size = mcu_size.second;
    if (h_block_size != 8 || v_block_size != 8) {
      LOG_ERROR("JPEG decoder only supports MCU size of 8x8 pixels");
      return -1; // Unsupported MCU size
    }

    auto num_block_in_x_dir = sof0->width / h_block_size;
    auto num_block_in_y_dir = sof0->height / v_block_size;

    int16_t pre_dc_value_y = 0;
    int16_t pre_dc_value_u = 0;
    int16_t pre_dc_value_v = 0;

    auto mcu_count = 0;

    for (auto y = 0; y < num_block_in_y_dir; ++y) {
      for (auto x = 0; x < num_block_in_x_dir; ++x) {
        mcu_count++;

        auto y_data = deHuffman(parser, 0, pre_dc_value_y);
        auto u_data = deHuffman(parser, 1, pre_dc_value_u);
        auto v_data = deHuffman(parser, 2, pre_dc_value_v);

        auto zig_zag_y = deZigZag(y_data);
        auto zig_zag_u = deZigZag(u_data);
        auto zig_zag_v = deZigZag(v_data);

        auto idct_y = idct(zig_zag_y);
        auto idct_u = idct(zig_zag_u);
        auto idct_v = idct(zig_zag_v);

        levelShift(idct_y);
        levelShift(idct_u);
        levelShift(idct_v);

        // fill mcu to decoded data
        auto left_top_x = x * 8;
        auto left_top_y = y * 8;
        for (int j = 0; j < kMCUPixelSize; j++) {
          auto img_pos_x = left_top_x + j % 8;
          auto img_pos_y = left_top_y + j / 8;
          auto img_index = img_pos_y * sof0->width + img_pos_x;
          y_decoded_data_[img_index] = static_cast<uint8_t>(idct_y[j]);
          u_decoded_data_[img_index] = static_cast<uint8_t>(idct_u[j]);
          v_decoded_data_[img_index] = static_cast<uint8_t>(idct_v[j]);
        }
      }
    }

    LOG_INFO("%d mcu decoded", mcu_count);
    return 0;
  }

  // for testing
  static std::unordered_map<uint16_t, HuffmanTable> buildHuffmanTableIndies(JFIFParser& parser) {
    const auto& dht_segments = parser.getDHTSegments();
    std::unordered_map<uint16_t, HuffmanTable> huffman_table_indies;
    for (const auto& dht : dht_segments) {
      uint16_t key = (static_cast<uint16_t>(dht.dc_or_ac) << 8) | dht.table_id;
      auto table = HuffmanTable(dht.symbol_counts, dht.symbols);
      huffman_table_indies.insert({key, table});
    }
    return huffman_table_indies;
  }

  static std::pair<size_t,size_t> getMCUSize(JFIFParser& parser) {
    auto* sof0 = parser.getSOF0Segment();

    auto max_h = 0;
    auto max_v = 0;

    for (auto factor : sof0->sampling_factor) {
      auto h_sampling_factor = factor >> 4;
      auto v_sampling_factor = factor & 0x0F;
      max_h = std::max(max_h, h_sampling_factor);
      max_v = std::max(max_v, v_sampling_factor);
    }

    return {8 * max_h, 8 * max_v}; // MCU size in pixels
  }

  static BitStream buildBitStream(const std::vector<uint8_t>& data) {
    auto bit_stream = BitStream();
    for (auto b : data) {
      std::bitset<8> bits(b);
      bit_stream.append(bits.to_string());
    }
    return bit_stream;
  }

private:
  static bool isSupported(JFIFParser& parser) {
    auto* sof0 = parser.getSOF0Segment();
    for (auto i = 0; i < sof0->num_components; ++i) {
      auto h_sampling_factor = sof0->sampling_factor[i] >> 4;
      auto v_sampling_factor = sof0->sampling_factor[i] & 0x0F;
      if (h_sampling_factor != 1 || v_sampling_factor != 1) {
        LOG_ERROR("JPEG decoder only supports YUV 4:4:4 format");
        return false; // Unsupported sampling factor
      }
    }
    return true;
  }

  std::vector<int16_t> deHuffman(JFIFParser& parser, int component_id, int16_t &pre_dc_value) {
    auto* sos = parser.getSOSSegment();
    auto* sof0 = parser.getSOF0Segment();

    auto htable_ac_id = sos->huffman_table_id_ac[component_id];
    auto htable_dc_id = sos->huffman_table_id_dc[component_id];

    // get dc huffman table
    auto dc_or_ac = 0;
    const auto dc_table_key = (static_cast<uint16_t>(dc_or_ac) << 8) | htable_dc_id;
    const auto& dc_htable = huffman_table_indies_.at(dc_table_key);

    // get ac huffman table
    dc_or_ac = 1;
    const auto ac_table_key = (static_cast<uint16_t>(dc_or_ac) << 8) | htable_ac_id;
    const auto& ac_table = huffman_table_indies_.at(ac_table_key);

    // get quantization table
    const auto qtable_id = sof0->quantization_table_id[component_id];
    const auto* qtable = q_table_refs_[qtable_id];

    // everything ready, let's decode the data
    // dc value always the first
    std::vector<int16_t> decoded_data(kMCUPixelSize, 0);
    auto index = 0;
    auto dc_category = dc_htable.getSymbol(bit_stream_);
    auto dc_value_bits = bit_stream_.getBitN(dc_category);
    auto dc_value = decodeNumber(dc_category, dc_value_bits);
    dc_value += pre_dc_value; // add previous DC value
    pre_dc_value = dc_value;
    decoded_data[index++] = dc_value;

    // decode ac value
    // start from index 1, index 0 is dc value
    for (; index < kMCUPixelSize;) {
      auto rrrr_ssss = ac_table.getSymbol(bit_stream_);
      if (rrrr_ssss == 0) {
        // EOF, no more AC values
        break;
      }
      auto zero_count = rrrr_ssss >> 4; // rrrr is the number of zeros
      auto category = rrrr_ssss & 0x0F;
      auto bits = bit_stream_.getBitN(category);
      auto non_zero_value = decodeNumber(category, bits);

      if (zero_count == 15 && category == 0) {
        index += 16;
        continue;
      }

      index += zero_count;

      decoded_data[index++] = dc_value;
    }

    // dequant
    for (int i = 0; i < kMCUPixelSize; ++i) {
      decoded_data[i] *= qtable->data[i];
    }

    return decoded_data;
  }

  std::vector<int16_t> deZigZag(const std::vector<int16_t> &data) {
    constexpr static int zz_order[kMCUPixelSize] = {
      0,  1,  5,  6,  14, 15, 27, 28, 2,  4,  7,  13, 16, 26, 29, 42,
      3,  8,  12, 17, 25, 30, 41, 43, 9,  11, 18, 24, 31, 40, 44, 53,
      10, 19, 23, 32, 39, 45, 52, 54, 20, 22, 33, 38, 46, 51, 55, 60,
      21, 34, 37, 47, 50, 56, 59, 61, 35, 36, 48, 49, 57, 58, 62, 63};

    std::vector<int16_t> zigzag_data(kMCUPixelSize, 0);
    for (int i = 0; i < kMCUPixelSize; i++) {
      zigzag_data[i] = data[zz_order[i]];
    }
    return zigzag_data;
  }

  std::vector<float> idct(const std::vector<int16_t> &data) {
    static constexpr float SQRT2 = 1.41421356f;

    std::vector<float> result(kMCUPixelSize, 0.0f);
    for (auto y = 0; y < 8; ++y) {
      for (auto x = 0; x < 8; ++x) {
        auto sum = 0.0f;

        for (auto u = 0; u < 8; ++u) {
          for (auto v = 0; v < 8; ++v) {
            float cu = (u == 0) ? 1.0f / SQRT2 : 1.0f;
            float cv = (v == 0) ? 1.0f / SQRT2 : 1.0f;
            float t0 = cu * std::cos((2 * x + 1) * u * M_PI / 16.0);
            float t1 = cv * std::cos((2 * y + 1) * v * M_PI / 16.0);

            // colum major
            auto data_value = data[u * 8 + v];

            sum += (data_value * t0 * t1);
          }
        }

        sum *= 0.25;
        result[x * 8 + y] = sum;
      }
    }
    return result;
  }

  static void levelShift(std::vector<float> &data) {
    for (auto i = 0; i < data.size(); ++i) {
      data[i] += 128;
    }
  }

  void prepare(JFIFParser& parser) {
    huffman_table_indies_ = buildHuffmanTableIndies(parser);
    bit_stream_ = buildBitStream(parser.getEncodedData());
    q_table_refs_ = parser.getQTableRefs();

    // YUV444, all components have the same width and height
    auto* sof0 = parser.getSOF0Segment();
    const auto pixel_count = sof0->width * sof0->height;
    allocateYUVData(pixel_count);
  }

  void allocateYUVData(size_t pixel_count) {
    y_decoded_data_.resize(pixel_count, 0);
    u_decoded_data_.resize(pixel_count, 0);
    v_decoded_data_.resize(pixel_count, 0);
  }

  static int16_t decodeNumber(uint16_t code_length, const std::string &bits) {
    if (bits.empty()) {
      return 0;
    }
    auto l = 1 << (code_length - 1); // 2**(code_length-1)
    auto v = std::bitset<16>(bits).to_ulong();
    if (v >= l) {
      return v;
    } else {
      return v - (2 * l - 1);
    }
  }

  std::vector<uint8_t> y_decoded_data_;
  std::vector<uint8_t> u_decoded_data_;
  std::vector<uint8_t> v_decoded_data_;
  std::unordered_map<uint16_t, HuffmanTable> huffman_table_indies_;
  std::array<segments::QuantizationTable*, 16> q_table_refs_{nullptr}; // 快速访问引用
  BitStream bit_stream_;

  constexpr static int kMCUPixelSize = 64;
};
}

#endif //SJPG_JPEG_DECODER_H
