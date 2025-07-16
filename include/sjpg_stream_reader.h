//
// Created by user on 7/16/25.
//

#ifndef SJPG_STREAM_READER_H
#define SJPG_STREAM_READER_H
#include <istream>
namespace sjpg_codec {
class StreamReader {
public:
  static uint8_t readByte(std::istream &stream) {
    uint8_t byte;
    stream.read(reinterpret_cast<char *>(&byte), sizeof(byte));
    if (stream.fail()) {
      return 0x00;
    }
    return byte;
  }

  static uint16_t read2BytesBigEndian(std::istream &stream) {
    uint8_t buf[2];
    stream.read(reinterpret_cast<char *>(buf), 2);
    return (buf[0] << 8) | buf[1];
  }

  static void readTo(std::istream &stream, char *dest, size_t length) {
    stream.read(dest, length);
  }
};
}

#endif //SJPG_STREAM_READER_H
