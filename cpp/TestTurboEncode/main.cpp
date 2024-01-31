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

#include "cmdline.h"
#include "lz4.h"
#include "lz4frame.h"
#include "lz4hc.h"
#include "thread_pool.hpp"
#include "turbojpeg.h"

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

void encode(tjhandle handle, FileInfo* info) {
  bool isThread = (handle == nullptr);
  if (!handle) {
    handle = tjInitCompress();
  }
  if (!handle) {
    std::cerr << "tjInitCompress failed" << std::endl;
    return;
  }

  std::string outBuf;
  std::string cpsBuf;
  outBuf.resize(info->width * info->height * 4);
  cpsBuf.resize(info->width * info->height * 4);
  int subsamp = TJSAMP_420;
  int srcSize = info->data.size();
  unsigned char* srcData = (unsigned char*)info->data.c_str();
  unsigned char* outData = (unsigned char*)outBuf.c_str();
  unsigned char* cpsData = (unsigned char*)cpsBuf.c_str();
  unsigned long outSize = tjBufSizeYUV(info->width, info->height, subsamp);

  // 编码文件
  int64_t start = getCurrentTime();
  int ret = -1;

  if (info->ext.find(".jpg") != std::string::npos) {
    ret = tjCompress2(handle, srcData, info->width, 0, info->height, TJPF_BGRA,
                      &outData, &outSize, subsamp, info->quality, info->flag);
  } else {
    ret = tjEncodeYUV2(handle, srcData, info->width, 0, info->height, TJPF_BGRA,
                       outData, subsamp, info->flag);
  }
  if (ret != 0) {
    if (isThread && handle) {
      tjDestroy(handle);
    }
    std::cerr << "Compress failed: " << tjGetErrorStr() << std::endl;
    return;
  }
  int64_t end = getCurrentTime();
  info->etime = (end - start) / 1000.0;

  // 压缩图片
  int cpsSize = LZ4_compress_default((char*)outData, (char*)cpsData, outSize,
                                     LZ4_compressBound(outSize));
  if (cpsSize <= 0) {
    std::cerr << "LZ4 Compress failed" << std::endl;
  }
  info->ctime = (getCurrentTime() - end) / 1000.0;

  info->result =
      "encode time: " + std::to_string(info->etime) +
      "ms\tcompress time: " + std::to_string(info->ctime) +
      "ms\tencode size: " + std::to_string(outSize / 1024.0) +
      "kb\tcompress size: " + std::to_string(cpsSize / 1024.0) +
      "kb\tencode ratio: " + std::to_string(outSize * 1.0 / srcSize * 1.0) +
      "\tcompress ratio: " + std::to_string(cpsSize * 1.0 / srcSize * 1.0) +
      "\t" + info->name + "\n";
  if (isThread && handle) {
    tjDestroy(handle);
  }

  if (info->output.empty()) {
    return;
  }
  // 保存到输出文件
  std::ofstream outFile(info->output, std::ofstream::binary);
  if (!outFile.is_open()) {
    std::cerr << "File can not open:" << info->output << std::endl;
    return;
  }
  outFile.write((char*)outData, outSize);
  outFile.close();
}

int main(int argc, char* argv[]) {
  cmdline::parser p;
  p.add<unsigned int>(
      "threads", 't', "threads", false, 1,
      cmdline::range((unsigned int)1, std::thread::hardware_concurrency() * 4));
  p.add<unsigned int>("quality", 'q', "quality", false, 100,
                      cmdline::range(1, 100));
  p.add<unsigned int>(
      "flag", 'f', "flag", false, TJFLAG_FASTDCT,
      cmdline::oneof(TJFLAG_FASTUPSAMPLE, TJFLAG_NOREALLOC, TJFLAG_FASTDCT,
                     TJFLAG_ACCURATEDCT, TJFLAG_STOPONWARNING,
                     TJFLAG_PROGRESSIVE, TJFLAG_LIMITSCANS));
  p.add<std::string>("ext", 'e', "extension", false, ".jpg",
                     cmdline::oneof(std::string(".jpg"), std::string(".yuv")));
  p.add<std::string>("input", 'i', "input directory", true, "");
  p.add<std::string>("output", 'o', "output directory", false, "");
  p.parse_check(argc, argv);

  const unsigned int threads = p.get<unsigned int>("threads");
  const unsigned int quality = p.get<unsigned int>("quality");
  const unsigned int flag = p.get<unsigned int>("flag");
  const std::string ext = p.get<std::string>("ext");
  const std::string input = p.get<std::string>("input");
  const std::string output = p.get<std::string>("output");

  std::cout << "Threads: " << threads << std::endl;
  std::cout << "Quality: " << quality << std::endl;
  std::cout << "Flag: " << flag << std::endl;
  std::cout << "Extension: " << ext << std::endl;
  std::cout << "Input: " << input << std::endl;
  std::cout << "Output: " << output << std::endl;

  if (!std::filesystem::exists(input)) {
    std::cerr << "Input directory does not exist" << std::endl;
    return 1;
  }
  if (!output.empty()) {
    if (!std::filesystem::exists(output)) {
      std::filesystem::create_directory(std::filesystem::path(output));
    }
  }

  tjhandle handle = tjInitCompress();
  if (!handle) {
    std::cerr << "tjInitCompress failed" << std::endl;
    return 1;
  }

  // 读取所有文件存入数组
  std::vector<FileInfo*> files;
  for (const auto& entry : std::filesystem::directory_iterator(input)) {
    const std::filesystem::path path = entry.path();
    if (std::filesystem::is_regular_file(path)) {
      // 提取文件名中的宽高信息
      std::string name = path.filename().string();
      std::regex regex("_|\\.");
      std::vector<std::string> names(
          std::sregex_token_iterator(name.begin(), name.end(), regex, -1),
          std::sregex_token_iterator());
      if (names.size() != 4) {
        std::cerr << "Invalid file name: " << name << std::endl;
        continue;
      }
      // 读取图片内容
      std::ifstream file(path.string(), std::ifstream::binary);
      if (!file.is_open()) {
        std::cerr << "File can not open:" << path.string() << std::endl;
        continue;
      }
      std::stringstream img;
      img << file.rdbuf();
      file.close();

      FileInfo* info = new FileInfo(
          std::stoi(names[0]), std::stoi(names[1]), std::stoi(names[2]),
          quality, flag, path.filename().string(), ext, path.string(),
          (output.empty() ? "" : output + "/" + path.filename().string() + ext),
          img.str());
      info->print();
      files.emplace_back(info);
    }
  }

  thread_pool::ThreadPool pool(threads);
  std::vector<std::future<void>> results;

  // 遍历目录进行编码和输出
  for (FileInfo* info : files) {
    if (threads == 1) {
      encode(handle, info);
    } else {
      results.emplace_back(pool.Submit(encode, nullptr, info));
    }
  }

  // 等待所有线程结束
  for (auto& result : results) {
    result.wait();
  }
  std::cout << "Waiting for all threads to finish" << std::endl;

  // 输出结果
  for (const FileInfo* info : files) {
    std::cout << info->result;
  }

  // 编码时间
  double ev =
      std::accumulate(files.begin(), files.end(), 0,
                      [](double a, FileInfo* b) { return a + b->etime; }) /
      files.size() * 1.0;
  // 压缩时间
  double cv =
      std::accumulate(files.begin(), files.end(), 0,
                      [](double a, FileInfo* b) { return a + b->ctime; }) /
      files.size() * 1.0;
  // 总时间
  double tv = std::accumulate(files.begin(), files.end(), 0,
                              [](double a, FileInfo* b) {
                                return a + b->etime + b->ctime;
                              }) /
              files.size() * 1.0;
  std::cout << "Average encode time: " << ev << "ms" << std::endl;
  std::cout << "Average decode time: " << cv << "ms" << std::endl;
  std::cout << "Average total time: " << tv << "ms" << std::endl;

  // 释放资源
  tjDestroy(handle);
  for (FileInfo* info : files) {
    delete info;
  }
  return 0;
}