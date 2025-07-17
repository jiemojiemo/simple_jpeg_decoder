//
// Created by user on 7/16/25.
//

#include <fstream>
#include <gmock/gmock.h>
#include "sjpg_jfif_parser.h"

using namespace testing;
using namespace sjpg_codec;
class AJFIFParser : public Test {
public:
  std::string filepath = "./resources/lenna.jpg";
  JFIFParser parser = JFIFParser();
  std::ifstream inputStream;

  void SetUp() override {
    inputStream.open(filepath, std::ios::binary);
    if (!inputStream.is_open()) {
      throw std::runtime_error("Failed to open test file: " + filepath);
    }
  }
};

TEST_F(AJFIFParser, ParserWithFileStream) {
  auto ret = parser.parse(inputStream);

  ASSERT_THAT(ret, Eq(JFIFParser::Success));
}

TEST_F(AJFIFParser, ParserFailedIfInputStreamInvalid) {
  auto fileStream = std::ifstream("nonexistent.jpg", std::ios::binary);

  auto ret = parser.parse(fileStream);

  ASSERT_THAT(ret, Not(JFIFParser::Success));
}

TEST_F(AJFIFParser, ParserFailedWithEmptyStream) {
  std::istringstream emptyStream;
  auto ret = parser.parse(emptyStream);

  ASSERT_THAT(ret, Not(JFIFParser::Success));
}

TEST_F(AJFIFParser, ParserFailedIfFileWithoutSOIMark) {
  std::istringstream invalidStream("hello world", std::ios::binary);

  auto ret = parser.parse(invalidStream);

  ASSERT_THAT(ret, Not(JFIFParser::Success));
}

TEST_F(AJFIFParser, ParserOkGetSOISegmentNotNull) {
  auto ret = parser.parse(inputStream);

  auto *segment = parser.getSOISegment();
  ASSERT_THAT(ret, Eq(JFIFParser::Success));
  ASSERT_THAT(segment, NotNull());
  ASSERT_THAT(segment->file_pos, Ne(0));
}

TEST_F(AJFIFParser, ParserOKGetAPP0SegmentNotNull) {
  auto ret = parser.parse(inputStream);

  auto *segment = parser.getAPP0Segment();
  ASSERT_THAT(ret, Eq(JFIFParser::Success));
  ASSERT_THAT(segment, NotNull());
}

TEST_F(AJFIFParser, ParserOKGetCOMSegmentIfExists) {
  auto ret = parser.parse(inputStream);

  auto *segment = parser.getCOMSegment();
  ASSERT_THAT(ret, Eq(JFIFParser::Success));
  ASSERT_THAT(segment, NotNull());
}

TEST_F(AJFIFParser, ParserOKGetDQTSegment) {
  auto ret = parser.parse(inputStream);

  auto segments = parser.getDQTSegments();
  ASSERT_THAT(ret, Eq(JFIFParser::Success));
  ASSERT_THAT(segments.size(), Eq(2));
}

TEST_F(AJFIFParser, ParserOKGetQTableRefs) {
  auto ret = parser.parse(inputStream);

  auto table_refs = parser.getQTableRefs();
  ASSERT_THAT(table_refs[0], NotNull());
  ASSERT_THAT(table_refs[0]->data.size(), Eq(64));
  ASSERT_THAT(table_refs[1], NotNull());
  ASSERT_THAT(table_refs[1]->data.size(), Eq(64));
}

TEST_F(AJFIFParser, ParserOKGetSOFOSegment) {
  auto ret = parser.parse(inputStream);

  auto *segment = parser.getSOF0Segment();
  ASSERT_THAT(ret, Eq(JFIFParser::Success));
  ASSERT_THAT(segment, NotNull());
}

TEST_F(AJFIFParser, ParsreOKGetDHTSegment) {
  parser.parse(inputStream);

  auto segments = parser.getDHTSegments();
  ASSERT_FALSE(segments.empty());
}

TEST_F(AJFIFParser, ParseOKGetSOSSegment) {
  parser.parse(inputStream);

  auto *segment = parser.getSOSSegment();
  ASSERT_THAT(segment, NotNull());
}

TEST_F(AJFIFParser, ParseOKGetEncodedData) {
  parser.parse(inputStream);

  auto& data = parser.getEncodedData();

  ASSERT_THAT(data.size(), Not(0));
}

TEST_F(AJFIFParser, ParseOKGetEOISegment) {
  parser.parse(inputStream);

  auto *segment = parser.getEOISegment();
  ASSERT_THAT(segment, NotNull());
}