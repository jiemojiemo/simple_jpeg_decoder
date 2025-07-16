//
// Created by user on 7/16/25.
//

#ifndef SJPG_MARKERS_H
#define SJPG_MARKERS_H
#include <cstdint>

namespace sjpg_codec {
// Null byte
const uint8_t JFIF_BYTE_0    = 0x00;

// JPEG-JFIF File Markers
//
// Refer to ITU-T.81 (09/92), page 32
const uint8_t JFIF_BYTE_FF    = 0xFF; // All markers start with this as the MSB
const uint8_t JFIF_SOF0       = 0xC0; // Start of Frame 0, Baseline DCT
const uint8_t JFIF_SOF1       = 0xC1; // Start of Frame 1, Extended Sequential DCT
const uint8_t JFIF_SOF2       = 0xC2; // Start of Frame 2, Progressive DCT
const uint8_t JFIF_SOF3       = 0xC3; // Start of Frame 3, Lossless (Sequential)
const uint8_t JFIF_DHT        = 0xC4; // Define Huffman Table
const uint8_t JFIF_SOF5       = 0xC5; // Start of Frame 5, Differential Sequential DCT
const uint8_t JFIF_SOF6       = 0xC6; // Start of Frame 6, Differential Progressive DCT
const uint8_t JFIF_SOF7       = 0xC7; // Start of Frame 7, Differential Loessless (Sequential)
const uint8_t JFIF_SOF9       = 0xC9; // Extended Sequential DCT, Arithmetic Coding
const uint8_t JFIF_SOF10      = 0xCA; // Progressive DCT, Arithmetic Coding
const uint8_t JFIF_SOF11      = 0xCB; // Lossless (Sequential), Arithmetic Coding
const uint8_t JFIF_SOF13      = 0xCD; // Differential Sequential DCT, Arithmetic Coding
const uint8_t JFIF_SOF14      = 0xCE; // Differential Progressive DCT, Arithmetic Coding
const uint8_t JFIF_SOF15      = 0xCF; // Differential Lossless (Sequential), Arithmetic Coding
const uint8_t JFIF_SOI        = 0xD8; // Start of Image
const uint8_t JFIF_EOI        = 0xD9; // End of Image
const uint8_t JFIF_SOS        = 0xDA; // Start of Scan
const uint8_t JFIF_DQT        = 0xDB; // Define Quantization Table
const uint8_t JFIF_APP0       = 0xE0; // Application Segment 0, JPEG-JFIF Image
const uint8_t JFIF_COM        = 0xFE; // Comment
}

#endif //SJPG_MARKERS_H
