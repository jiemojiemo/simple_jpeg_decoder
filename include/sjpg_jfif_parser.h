//
// Created by user on 7/17/25.
//

#ifndef SJPG_JFIF_PARSER_H
#define SJPG_JFIF_PARSER_H
#include "sjpg_markers.h"
#include "sjpg_segments.h"
#include "sjpg_stream_reader.h"
#include <numeric>


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
        } else if (b == JFIF_COM) {
          parseCOMSegment(stream);
        } else if (b == JFIF_DQT) {
          parseDQTSegment(stream);
        } else if (b == JFIF_SOF0) {
          parseSOF0Segment(stream);
        } else if (b == JFIF_DHT) {
          parseDHTSegment(stream);
        } else if (b == JFIF_SOS) {
          parseSOSSegment(stream);
          scanImageDataAndEOI(stream);
        }
      }
    }

    return Success;
  }

  const segments::SOISegment *getSOISegment() const { return soi_.get(); }
  const segments::APP0Segment *getAPP0Segment() const { return app0_.get(); }
  const segments::COMSegment *getCOMSegment() const { return com_.get(); }
  const std::vector<segments::DQTSegment> &getDQTSegments() const {
    return dqt_segments_;
  }
  const std::array<segments::QuantizationTable *, 16> &getQTableRefs() const {
    return q_table_refs;
  }
  const segments::SOF0Segment *getSOF0Segment() const { return sof0_.get(); }
  const std::vector<segments::DHTSegment> &getDHTSegments() const {
    return dht_segments_;
  }

  const segments::SOSSegment *getSOSSegment() const { return sos_.get(); }
  const segments::EOISegment* getEOISegment() const {
    return eoi_.get();
  }
  const std::vector<uint8_t>& getEncodedData() const {
    return encoded_data_;
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

    const auto thumbnail_image_pixel_count =
        app0_->thumbnail_width * app0_->thumbnail_height;
    const auto thumbnail_data_length =
        kRGBBytesPerPixel * thumbnail_image_pixel_count;
    app0_->thumbnail_data.resize(thumbnail_data_length);
    if (thumbnail_data_length > 0) {
      StreamReader::readTo(
          stream, reinterpret_cast<char *>(app0_->thumbnail_data.data()),
          thumbnail_data_length);
    }

    app0_->print();
  }

  void parseCOMSegment(std::istream &stream) {
    com_ = std::make_unique<segments::COMSegment>();
    com_->file_pos = stream.tellg();
    com_->length = StreamReader::read2BytesBigEndian(stream);
    auto commentSize = com_->length - 2; // 减去长度字段的2个字节
    com_->comment.resize(commentSize);   // 减去长度字段的2个字节
    StreamReader::readTo(stream, reinterpret_cast<char *>(com_->comment.data()),
                         commentSize);

    com_->print();
  }

  void parseDQTSegment(std::istream &stream) {
    // TODO: multiple table in a dqt segment
    segments::DQTSegment dqt;
    dqt.file_pos = static_cast<unsigned long>(stream.tellg());
    dqt.length = StreamReader::read2BytesBigEndian(stream);
    auto tmp = StreamReader::readByte(stream);

    segments::QuantizationTable table;
    table.precision = tmp >> 4;
    table.id = tmp & 0x0F;

    constexpr static int kQuantizationTableSize = 64; // 8x8 matrix
    table.data.resize(kQuantizationTableSize);
    StreamReader::readTo(stream, reinterpret_cast<char *>(table.data.data()),
                         kQuantizationTableSize);
    dqt.tables.emplace_back(table);
    dqt.print();

    dqt_segments_.emplace_back(dqt);

    auto &last_segment = dqt_segments_.back();
    for (auto &t : last_segment.tables) {
      q_table_refs[t.id] = &table;
    }
  }

  void parseSOF0Segment(std::istream &stream) {
    sof0_ = std::make_unique<segments::SOF0Segment>();
    sof0_->file_pos = stream.tellg();
    sof0_->length = StreamReader::read2BytesBigEndian(stream);
    sof0_->bitPerSample = StreamReader::readByte(stream);
    sof0_->height = StreamReader::read2BytesBigEndian(stream);
    sof0_->width = StreamReader::read2BytesBigEndian(stream);
    sof0_->num_components = StreamReader::readByte(stream);

    for (int i = 0; i < sof0_->num_components; i++) {
      auto b0 = StreamReader::readByte(stream);
      auto b1 = StreamReader::readByte(stream);
      auto b2 = StreamReader::readByte(stream);

      sof0_->component_id.push_back(b0);
      sof0_->sampling_factor.push_back(b1);
      sof0_->quantization_table_id.push_back(b2);
    }

    sof0_->print();
  }

  void parseSOSSegment(std::istream &stream) {
    sos_ = std::make_unique<segments::SOSSegment>();
    sos_->file_pos = static_cast<unsigned long>(stream.tellg());
    sos_->length = StreamReader::read2BytesBigEndian(stream);
    sos_->num_components = StreamReader::readByte(stream);

    for (int i = 0; i < sos_->num_components; i++) {
      auto b0 = StreamReader::readByte(stream);
      auto b1 = StreamReader::readByte(stream);
      sos_->component_id.push_back(b0);

      auto huffman_table_id_dc = b1 >> 4;
      auto huffman_table_id_ac = b1 & 0x0F;
      sos_->huffman_table_id_ac.push_back(huffman_table_id_ac);
      sos_->huffman_table_id_dc.push_back(huffman_table_id_dc);
    }

    // skip 3 bytes
    for (int j = 0; j < 3; j++) {
      StreamReader::readByte(stream);
    }
    sos_->print();
  }

  void scanImageDataAndEOI(std::istream &stream) {
    for (;;) {
      auto b = StreamReader::readByte(stream);
      if (b == JFIF_BYTE_FF) {
        auto next_b = StreamReader::readByte(stream);
        if (next_b == JFIF_EOI) {
          // build eoi segment
          eoi_ = std::make_unique<segments::EOISegment>();
          eoi_->file_pos = static_cast<unsigned long>(stream.tellg());
          return;
        }else {
          // it is encoded data
          encoded_data_.push_back(b);
        }
      }else {
        encoded_data_.push_back(b);
      }
    }
  }

  void parseDHTSegment(std::istream &stream) {
    auto dht = segments::DHTSegment();
    dht.file_pos = static_cast<unsigned long>(stream.tellg());
    dht.length = StreamReader::read2BytesBigEndian(stream);
    auto b = StreamReader::readByte(stream);
    dht.ac_or_dc = b >> 4;
    dht.table_id = b & 0x0F;

    static constexpr int kHuffmanTableSize = 16;
    dht.symbol_counts.resize(kHuffmanTableSize);
    StreamReader::readTo(stream,
                         reinterpret_cast<char *>(dht.symbol_counts.data()),
                         kHuffmanTableSize);

    auto num_total_symbols =
        std::accumulate(dht.symbol_counts.begin(), dht.symbol_counts.end(), 0);
    dht.symbols.resize(num_total_symbols);
    StreamReader::readTo(stream, reinterpret_cast<char *>(dht.symbols.data()),
                         num_total_symbols);
    dht.print();

    dht_segments_.emplace_back(dht);
  }

  std::unique_ptr<segments::SOISegment> soi_;
  std::unique_ptr<segments::APP0Segment> app0_;
  std::unique_ptr<segments::COMSegment> com_;
  std::vector<segments::DQTSegment> dqt_segments_;
  std::array<segments::QuantizationTable *, 16> q_table_refs{
      nullptr}; // 快速访问引用
  std::unique_ptr<segments::SOF0Segment> sof0_;
  std::vector<segments::DHTSegment> dht_segments_;
  std::unique_ptr<segments::SOSSegment> sos_;
  std::vector<uint8_t> encoded_data_; // 存储编码后的数据
  std::unique_ptr<segments::EOISegment> eoi_;
};
} // namespace sjpg_codec

#endif //SJPG_JFIF_PARSER_H
