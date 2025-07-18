//
// Created by user on 7/17/25.
//
#include "sjpg_jpeg_decoder.h"

#include <fstream>
#include <gmock/gmock.h>
#include <unordered_map>

using namespace testing;
using namespace sjpg_codec;

class AJEPGDecoder : public Test {
public:
  std::string filepath = "./resources/lenna.jpg";
  JFIFParser parser = JFIFParser();
  std::ifstream inputStream;
  JPEGDecoder decoder;

  void SetUp() override {
    inputStream.open(filepath, std::ios::binary);
    if (!inputStream.is_open()) {
      throw std::runtime_error("Failed to open test file: " + filepath);
    }

    parser.parse(inputStream);
  }
};

TEST_F(AJEPGDecoder, DecodeFailedIfIsNotYUV444) {
  auto* sof0 = parser.getSOF0Segment();
  auto h_sample_factor = 2;
  auto v_sample_factor = 2;
  sof0->sampling_factor[0] = (h_sample_factor << 4) | v_sample_factor;
  sof0->print();

  auto ret = decoder.decode(parser);

  ASSERT_THAT(ret, Not(0));
}

TEST_F(AJEPGDecoder, CanBuildHuffmanTablesIndex) {
  auto huffman_table_indies = decoder.buildHuffmanTableIndies(parser);

  ASSERT_THAT(huffman_table_indies.size(), Eq(4));
}

TEST_F(AJEPGDecoder, CanGetMCUSize) {
  auto mcu_size = JPEGDecoder::getMCUSize(parser);

  ASSERT_THAT(mcu_size.first, Eq(8)); // 8x8 MCU size
  ASSERT_THAT(mcu_size.second, Eq(8)); // 8x8 MCU size
}

TEST_F(AJEPGDecoder, CanBuildBitStream) {
  auto data = std::vector<uint8_t>{0, 1, 2, 3};
  auto bs = JPEGDecoder::buildBitStream(data);

  LOG_INFO("bs: %s", bs.getStringData().c_str());
}

TEST_F(AJEPGDecoder, DecodeSuccessGetYUVData) {
  auto ret = decoder.decode(parser);

  auto y = decoder.getYDecodedData();
  auto u = decoder.getUDecodedData();
  auto v = decoder.getVDecodedData();

  auto expected_size = parser.getSOF0Segment()->width * parser.getSOF0Segment()->height;
  ASSERT_THAT(y.size(), Eq(expected_size));
  ASSERT_THAT(u.size(), Eq(expected_size));
  ASSERT_THAT(v.size(), Eq(expected_size));
}