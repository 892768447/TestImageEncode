#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <ostream>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "NumCpp.hpp"
#include "fmt/core.h"
#include "lz4.h"
#include "lz4frame.h"
#include "lz4hc.h"
#include "qoixx.hpp"
#include "turbojpeg.h"
#include "zstd.h"

#ifdef _WIN32
#include <direct.h>
#include <windows.h>

#else
#include <unistd.h>
#endif

#undef max

static std::string encBuf;  // 编码输出缓冲区
static std::string cpsBuf;  // 压缩输出缓冲区

static int64_t getCurrentTime() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

static std::string getCurrentDirectory() {
  std::string path;
  size_t pos = 0;
#ifdef _WIN32
  char buf[MAX_PATH + 1] = "";
  if (::GetModuleFileName(NULL, buf, MAX_PATH) != 0) path = buf;
  pos = path.rfind("\\");
#elif defined(__linux__) || defined(__linux) || defined(linux) || \
    defined(__gnu_linux__)
  char buf[PATH_MAX + 1] = "";
  if (readlink("/proc/self/exe", buf, PATH_MAX) != -1 && ('\0' != buf[0]))
    path = buf;
  pos = path.rfind("/");
#else
#endif
  return path.substr(0, pos + 1);
}

static void test_lz4(const std::string& image, int width, int height,
                     const std::string& prefix = "") {
  std::vector<int> levels = {1, 3, 6, 9, 10, 12};

  int srcSize = image.size();
  unsigned char* srcData = (unsigned char*)image.c_str();
  unsigned char* outData = (unsigned char*)encBuf.c_str();

  fmt::println("{}test lz4", prefix);
  for (int level : levels) {
    int64_t start = getCurrentTime();
    int outSize = LZ4_compress_fast((char*)srcData, (char*)outData, srcSize,
                                    LZ4_compressBound(srcSize), level);
    int64_t end = getCurrentTime();
    fmt::println(
        "{}\tencode time: {:>10.3f} ms \t ({:^8} kb => {:^8} kb) \t encode "
        "ratio: {:>6.3f}% \t level: {:>2}",
        prefix, (end - start) / 1000.0, srcSize / 1024, outSize / 1024,
        100.0 * (1 - 1.0 * outSize / srcSize), level);
  }
}

static void test_lz4_hc(const std::string& image, int width, int height,
                        const std::string& prefix = "") {
  std::vector<int> levels = {1, 3, 6, 9, 10, 12};

  int srcSize = image.size();

  unsigned char* srcData = (unsigned char*)image.c_str();
  unsigned char* outData = (unsigned char*)encBuf.c_str();

  fmt::println("{}test lz4hc", prefix);
  for (int level : levels) {
    int64_t start = getCurrentTime();
    int outSize = LZ4_compress_HC((char*)srcData, (char*)outData, srcSize,
                                  LZ4_compressBound(srcSize), level);
    int64_t end = getCurrentTime();
    fmt::println(
        "{}\tencode time: {:>10.3f} ms \t ({:^8} kb => {:^8} kb) \t encode "
        "ratio: {:>6.3f}% \t level: {:>2}",
        prefix, (end - start) / 1000.0, srcSize / 1024, outSize / 1024,
        100.0 * (1 - 1.0 * outSize / srcSize), level);
  }
}

static void test_zstd(const std::string& image, int width, int height,
                      const std::string& prefix = "") {
  int srcSize = image.size();

  encBuf.resize(ZSTD_compressBound(srcSize));
  unsigned char* srcData = (unsigned char*)image.c_str();
  unsigned char* outData = (unsigned char*)encBuf.c_str();

  fmt::println("{}test zstd", prefix);
  for (int level = 0; level <= ZSTD_maxCLevel(); level++) {
    int64_t start = getCurrentTime();
    int outSize = ZSTD_compress(outData, encBuf.size(), srcData, srcSize, 1);
    int64_t end = getCurrentTime();
    fmt::println(
        "{}\tencode time: {:>10.3f} ms \t ({:^8} kb => {:^8} kb) \t encode "
        "ratio: {:>6.3f}% \t level: {:>2}",
        prefix, (end - start) / 1000.0, srcSize / 1024, outSize / 1024,
        100.0 * (1 - 1.0 * outSize / srcSize), level);
  }
}

static void test_zstd_thread(const std::string& image, int width, int height,
                             const std::string& prefix = "") {
  return;
  int srcSize = image.size();
  int threads = 2;

  encBuf.resize(ZSTD_compressBound(srcSize));
  unsigned char* srcData = (unsigned char*)image.c_str();
  unsigned char* outData = (unsigned char*)encBuf.c_str();

  fmt::println("test zstd thread");
  int level = 19;
  auto ctx = ZSTD_createCCtx();
  ZSTD_CCtx_setParameter(ctx, ZSTD_c_nbWorkers, threads);
  // for (int level = 0; level <= ZSTD_maxCLevel(); level++) {
  ZSTD_CCtx_setParameter(ctx, ZSTD_c_compressionLevel, level);
  int64_t start = getCurrentTime();
  int outSize = ZSTD_compress2(ctx, outData, encBuf.size(), srcData, srcSize);
  int64_t end = getCurrentTime();

  // if (ZSTD_isError(outSize)) {
  //   fmt::println(stderr, "ZSTD_compress2 failed: {}",
  //                ZSTD_getErrorName(outSize));
  //   continue;
  // }
  fmt::println(
      "\tencode time: {:>10.3f} ms \t ({:^8} kb => {:^8} kb) \t encode "
      "ratio: {:>6.3f}% \t level: {:>2} \t threads: {:>2}",
      (end - start) / 1000.0, srcSize / 1024, outSize / 1024,
      100.0 * (1 - 1.0 * outSize / srcSize), level, threads);
  // }
  ZSTD_freeCCtx(ctx);
}

static void test_lz4_gpu(const std::string& image, int width, int height,
                         const std::string& prefix = "") {}

static void test_zstd_gpu(const std::string& image, int width, int height,
                          const std::string& prefix = "") {}

static void test_jpeg(const std::string& image, int width, int height,
                      const std::string& prefix = "", bool compress = true) {
  tjhandle handle = tjInitCompress();
  if (!handle) {
    fmt::println(stderr, "tjInitCompress Init failed: {}", tjGetErrorStr());
    return;
  }

  std::vector<int> qualitys = {70, 75, 80, 85, 90, 95, 100};
  std::vector<int> flags = {TJFLAG_FASTDCT, TJFLAG_ACCURATEDCT};
  std::vector<std::string> flagNames = {"TJFLAG_FASTDCT", "TJFLAG_ACCURATEDCT"};

  encBuf.resize(width * height * 4);
  int subsamp = TJSAMP_420;
  int srcSize = image.size();
  unsigned long outSize = 0;
  unsigned char* srcData = (unsigned char*)image.c_str();
  unsigned char* outData = (unsigned char*)encBuf.c_str();

  fmt::println("{}test jpeg", prefix);
  for (int quality : qualitys) {
    for (size_t i = 0; i < flags.size(); i++) {
      int flag = flags[i];
      std::string name = flagNames[i];
      int64_t start = getCurrentTime();
      int ret = tjCompress2(handle, srcData, width, 0, height, TJPF_BGRA,
                            &outData, &outSize, subsamp, quality, flag);
      int64_t end = getCurrentTime();

      if (ret != 0) {
        fmt::println(stderr, "tjInitCompress Compress failed: {}",
                     tjGetErrorStr());
        continue;
      }
      fmt::println(
          "{}\tencode time: {:>10.3f} ms \t ({:^8} kb => {:^8} kb) \t encode "
          "ratio: {:>6.3f}% \t quality: {:>3} \t flag: {:<25}",
          prefix, (end - start) / 1000.0, srcSize / 1024, outSize / 1024,
          100.0 * (1 - 1.0 * outSize / srcSize), quality, name);

      if (compress) {
        const std::string cData = std::string((char*)outData, outSize);
        test_lz4(cData, width, height, "\t");
        test_zstd(cData, width, height, "\t");
      }
    }
  }

  if (handle) {
    tjDestroy(handle);
  }
}

static void test_jpeg_yuv(const std::string& image, int width, int height,
                          const std::string& prefix = "",
                          bool compress = true) {
  tjhandle handle = tjInitCompress();
  if (!handle) {
    fmt::println(stderr, "tjInitCompress Init failed: {}", tjGetErrorStr());
    return;
  }

  std::vector<int> flags = {TJFLAG_FASTDCT, TJFLAG_ACCURATEDCT};
  std::vector<std::string> flagNames = {"TJFLAG_FASTDCT", "TJFLAG_ACCURATEDCT"};

  int subsamp = TJSAMP_420;
  int srcSize = image.size();
  unsigned long outSize = tjBufSizeYUV(width, height, subsamp);
  encBuf.resize(outSize);
  unsigned char* srcData = (unsigned char*)image.c_str();
  unsigned char* outData = (unsigned char*)encBuf.c_str();

  fmt::println("{}test yuv", prefix);
  for (size_t i = 0; i < flags.size(); i++) {
    int flag = flags[i];
    std::string name = flagNames[i];
    int64_t start = getCurrentTime();
    int ret = ret = tjEncodeYUV2(handle, srcData, width, 0, height, TJPF_BGRA,
                                 outData, subsamp, flag);
    int64_t end = getCurrentTime();

    if (ret != 0) {
      fmt::println(stderr, "tjInitCompress Compress failed: {}",
                   tjGetErrorStr());
      continue;
    }
    fmt::println(
        "{}\tencode time: {:>10.3f} ms \t ({:^8} kb => {:^8} kb) \t encode "
        "ratio: {:>6.3f}% \t flag: {:<25}",
        prefix, (end - start) / 1000.0, srcSize / 1024, outSize / 1024,
        100.0 * (1 - 1.0 * outSize / srcSize), name);

    if (compress) {
      const std::string cData = std::string((char*)outData, outSize);
      test_lz4(cData, width, height, "\t");
      test_zstd(cData, width, height, "\t");
    }
  }

  if (handle) {
    tjDestroy(handle);
  }
}

static void test_qoi(const std::string& image, int width, int height,
                     const std::string& prefix = "") {
  int srcSize = image.size();
  unsigned char* srcData = (unsigned char*)image.c_str();

  qoixx::qoi::desc desc;
  desc.width = width;
  desc.height = height;
  desc.channels = 3;
  desc.colorspace = qoixx::qoi::colorspace::srgb;

  fmt::println("{}test qoi", prefix);
  int64_t start = getCurrentTime();
  const auto data =
      qoixx::qoi::encode<std::vector<std::byte>>(srcData, srcSize, desc);
  int outSize = data.size();
  int64_t end = getCurrentTime();

  fmt::println(
      "{}\tencode time: {:>10.3f} ms \t ({:^8} kb => {:^8} kb) \t encode "
      "ratio: {:>6.3f}%",
      prefix, (end - start) / 1000.0, srcSize / 1024, outSize / 1024,
      100.0 * (1 - 1.0 * outSize / srcSize));
}

static void test_numpy_split(const std::string& image, int width, int height,
                             const std::string& prefix = "") {
  nc::NdArray array = nc::frombuffer<uint32_t>(image.data(), image.size());
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
  _chdir(getCurrentDirectory().c_str());
#else
  chdir(getCurrentDirectory().c_str());
#endif

  int width = 1920;
  int height = 1080;
  std::string path = "1920.bgra";
  if (argc > 1) {
    path = argv[1];
    if (argc < 4) {
      fmt::println(stderr, "Usage: {} <image> [width] [height]", argv[0]);
      return 1;
    }
  }
  if (argc > 2) {
    width = std::stoi(argv[2]);
  }
  if (argc > 3) {
    height = std::stoi(argv[3]);
  }

  // 申请缓存
  encBuf.resize(width * height * 4);
  cpsBuf.resize(width * height * 4);

  // 读取图片内容
  std::ifstream file(path, std::ifstream::binary);
  if (!file.is_open()) {
    std::cerr << "File can not open:" << path << std::endl;
    return 1;
  }
  std::stringstream img;
  img << file.rdbuf();
  file.close();
  const std::string image = img.str();
  fmt::println("Input Image size: {}, Width: {}, Height: {}", image.size(),
               width, height);

  // 测试压缩
  test_lz4(image, width, height);
  test_lz4_hc(image, width, height);
  test_zstd(image, width, height);
  test_zstd_thread(image, width, height);
  test_lz4_gpu(image, width, height);
  test_zstd_gpu(image, width, height);
  test_jpeg(image, width, height, "", false);
  test_jpeg_yuv(image, width, height, "", false);
  test_qoi(image, width, height);
  test_numpy_split(image, width, height);

  return 0;
}