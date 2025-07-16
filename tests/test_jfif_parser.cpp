//
// Created by user on 7/16/25.
//
#include "sjpg_markers.h"
#include "sjpg_segments.h"
#include "sjpg_stream_reader.h"
#include <fstream>
#include <gmock/gmock.h>

namespace sjpg_codec {
class JFIFParser {
public:
  enum ParseResult {
    Success = 0,
    StreamInvalid = -1,
    StreamEmpty = -2,
    NoSOIMark = -3
  };

  int parse(std::istream &stream) {
    if (stream.fail()) {
      soi_.reset();
      return StreamInvalid;
    }

    if (stream.peek() == EOF) {
      soi_.reset();
      return StreamEmpty;
    }

    if (!hasSOIMark(stream)) {
      soi_.reset();
      return NoSOIMark;
    }
    // 解析成功，创建 SOISegment，并设置 file_pos
    soi_ = buildSOISegment(stream);

    for (;;) {
      if (stream.eof()) {
        LOG_INFO("End of file reached\n");
        break;
      }

      auto b = StreamReader::readByte(stream);
      if (b == JFIF_BYTE_FF) {
        b = StreamReader::readByte(stream);
        if (b == JFIF_APP0) {
          parseAPP0Segment(stream);
        }else if (b == JFIF_COM) {
          parseCOMSegment(stream);
        }
      }

    }

    return Success;
  }

  const segments::SOISegment *getSOISegment() const { return soi_.get(); }
  const segments::APP0Segment *getAPP0Segment() const { return app0_.get(); }
  const segments::COMSegment* getCOMSegment() const {
    return com_.get();
  }

private:
  static bool hasSOIMark(std::istream &stream) {
    // 记录当前位置
    unsigned char soi[2];
    stream.read(reinterpret_cast<char *>(soi), 2);
    bool result =
        (stream.gcount() == 2 && soi[0] == JFIF_BYTE_FF && soi[1] == JFIF_SOI);
    return result;
  }

  static std::unique_ptr<segments::SOISegment>
  buildSOISegment(std::istream &stream) {
    auto soi = std::make_unique<segments::SOISegment>();
    soi->file_pos = static_cast<unsigned long>(stream.tellg());
    return soi;
  }

  void parseAPP0Segment(std::istream &stream) {
    app0_ = std::make_unique<segments::APP0Segment>();
    app0_->file_pos = stream.tellg();
    app0_->length = StreamReader::read2BytesBigEndian(stream);
    StreamReader::readTo(stream, app0_->identifier, 5);
    app0_->major_version = StreamReader::readByte(stream);
    app0_->minor_version = StreamReader::readByte(stream);
    app0_->pixel_units = StreamReader::readByte(stream);
    app0_->x_density = StreamReader::read2BytesBigEndian(stream);
    app0_->y_density = StreamReader::read2BytesBigEndian(stream);
    app0_->thumbnail_width = StreamReader::readByte(stream);
    app0_->thumbnail_height = StreamReader::readByte(stream);

    static constexpr int kRGBChannels = 3;
    static constexpr int kRGBBits = 8; // 每个RGB通道8位
    static constexpr int kRGBBytesPerPixel = kRGBChannels * (kRGBBits / 8);

    const auto thumbnail_image_pixel_count = app0_->thumbnail_width * app0_->thumbnail_height;
    const auto thumbnail_data_length = kRGBBytesPerPixel * thumbnail_image_pixel_count;
    app0_->thumbnail_data.resize(thumbnail_data_length);
    if (thumbnail_data_length > 0) {
      StreamReader::readTo(stream, reinterpret_cast<char *>(app0_->thumbnail_data.data()), thumbnail_data_length);
    }

    app0_->print();
  }

  void parseCOMSegment(std::istream &stream) {
    com_ = std::make_unique<segments::COMSegment>();
    com_->file_pos = stream.tellg();
    com_->length = StreamReader::read2BytesBigEndian(stream);
    auto commentSize = com_->length - 2; // 减去长度字段的2个字节
    com_->comment.resize(commentSize); // 减去长度字段的2个字节
    StreamReader::readTo(stream, reinterpret_cast<char *>(com_->comment.data()), commentSize);

    com_->print();
  }

  std::unique_ptr<segments::SOISegment> soi_;
  std::unique_ptr<segments::APP0Segment> app0_;
  std::unique_ptr<segments::COMSegment> com_;
};
} // namespace sjpg_codec

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