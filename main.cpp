#include "sjpg_decoder.h"
#include "sjpg_jpeg_decoder.h"

#include <iostream>

using namespace sjpg_codec;
void saveRGBTOPPM(uint8_t *data, int linesize, int width, int height,
                  const char *output_name) {
  FILE *pFile;

  // Open file
  pFile = fopen(output_name, "wb");
  if (pFile == NULL) {
    return;
  }

  // Write header
  fprintf(pFile, "P6\n%d %d\n255\n", width, height);

  // Write pixel data
  const int kBytesPerPixel = 3; // R(8) G(8) B(8)
  for (int y = 0; y < height; y++) {
    uint8_t *img_row = data + y * linesize;
    fwrite(img_row, 1, width * kBytesPerPixel, pFile);
  }

  // Close file
  fclose(pFile);
}

auto convertYUVToRGB(const std::vector<uint8_t> &y_data,
                     const std::vector<uint8_t> &u_data,
                     const std::vector<uint8_t> &v_data, int img_width,
                     int img_height) -> std::vector<uint8_t> {
  const auto total_pixel = img_width * img_height;
  std::vector<uint8_t> rgb;

  for (int i = 0; i < total_pixel; ++i) {
    auto y = y_data[i];
    auto u = u_data[i];
    auto v = v_data[i];

    auto r = y + 1.402 * (v - 128);
    auto g = y - 0.344136 * (u - 128) - 0.714136 * (v - 128);
    auto b = y + 1.772 * (u - 128);

    r = std::max(0, std::min(255, (int)r));
    g = std::max(0, std::min(255, (int)g));
    b = std::max(0, std::min(255, (int)b));

    rgb.push_back(r);
    rgb.push_back(g);
    rgb.push_back(b);
  }

  return rgb;
};

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: %s <jpg_file_path>\n", argv[0]);
    return -1;
  }
  auto file_path = argv[1];
  auto file_stream = std::ifstream(file_path, std::ios::binary);
  if (!file_stream.is_open()) {
    LOG_ERROR("Failed to open file: %s", file_path);
    return -1;
  }

  JFIFParser parser;
  auto ret = parser.parse(file_stream);
  if (ret != 0) {
    printf("Failed to parse file: %s\n", file_path);
    return -1;
  }

  JPEGDecoder decoder;
  ret = decoder.decode(parser);
  if (ret != 0) {
    printf("Failed to decode file: %s\n", file_path);
    return -1;
  }


  auto img_width = parser.getSOF0Segment()->width;
  auto img_height = parser.getSOF0Segment()->height;
  auto y_data = decoder.getYDecodedData();
  auto u_data = decoder.getUDecodedData();
  auto v_data = decoder.getVDecodedData();
  auto x = convertYUVToRGB(y_data, u_data, v_data, img_width, img_height);

  auto rgb_stride = img_width * 3;
  saveRGBTOPPM(x.data(), rgb_stride, img_width, img_height, "output.ppm");
}
