#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <ostream>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#define QOI_IMPLEMENTATION

#include "cmdline.h"
#include "lz4.h"
#include "lz4frame.h"
#include "lz4hc.h"
#include "qoi/qoi.h"
#include "thread_pool.hpp"

struct FileInfo {
  int index;
  int width;
  int height;
  int quality;
  int flag;
  double etime;  // 编码时间
  double ctime;  // 压缩时间
  std::string name;
  std::string ext;
  std::string input;
  std::string output;
  std::string data;
  std::string result;

  FileInfo(int index, int width, int height, int quality, int flag,
           const std::string& name, const std::string& ext,
           const std::string& input, const std::string& output,
           const std::string& data)
      : index(index),
        width(width),
        height(height),
        quality(quality),
        flag(flag),
        etime(0),
        ctime(0),
        name(name),
        ext(ext),
        input(input),
        output(output),
        data(data),
        result("") {}

  void print() {
    std::cout << "Index: " << index << ", Width: " << width
              << ", Height: " << height << ", Quality: " << quality
              << ", Flag: " << flag << ", Name: " << name
              << ", Input: " << input << ", Output: " << output
              << ", Size:" << data.size() << std::endl;
  }
};

static int64_t getCurrentTime() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

int main(int argc, char* argv[]) {
  // 读取图片内容
  const std::string path =
      "G:/Workspace/TestImageEncode/images/raws/1920/0_1920_1080.rgb";
  //   const std::string
  //   path="G:/Workspace/TestImageEncode/images/raws/3840/0_3840_2160.rgb";
  std::ifstream file(path, std::ifstream::binary);
  if (!file.is_open()) {
    std::cerr << "File can not open:" << path << std::endl;
    return -1;
  }
  std::stringstream img;
  img << file.rdbuf();
  file.close();
  const std::string data = img.str();
  int srcSize = data.size();

  int64_t start = getCurrentTime();
  int outSize;
  qoi_desc desc;
  desc.width = 1920;
  desc.height = 1080;
  desc.channels = 3;
  desc.colorspace = QOI_SRGB;

  void* encoded = qoi_encode(data.data(), &desc, &outSize);

  int64_t end = getCurrentTime();
  std::cout << "Time: " << (end - start) / 1000.0 << "ms" << std::endl;
  std::cout << outSize << " " << ((double)outSize / (double)srcSize / 1.0)
            << std::endl;

  start = getCurrentTime();
  void *decode = qoi_decode(encoded, outSize, &desc, 3);
  end = getCurrentTime();
  std::cout << "Time: " << (end - start) / 1000.0 << "ms" << std::endl;
  std::cout << outSize << " " << ((double)outSize / (double)srcSize / 1.0)
            << std::endl;

  if (encoded) {
    free(encoded);
  }
   if (decode) {
    free(decode);
  }
  return 0;
}