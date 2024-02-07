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

#include "fmt/core.h"
#include "lz4.h"
#include "lz4frame.h"
#include "lz4hc.h"
#include "thread_pool.hpp"
#include "turbojpeg.h"
#include "zstd.h"

#define ANKERL_NANOBENCH_IMPLEMENT
#include "nanobench.h"

#define XTENSOR_USE_XSIMD 1
#include "xtensor/xadapt.hpp"
#include "xtensor/xarray.hpp"
#include "xtensor/xio.hpp"
#include "xtensor/xstrided_view.hpp"

#ifdef _WIN32
#include <direct.h>
#include <windows.h>

#else
#include <unistd.h>
#endif

#define IN_CHUNK_SIZE 16384  // 16 * 1024
#undef max

static std::string encBuf;  // 编码输出缓冲区
static std::string cpsBuf;  // 压缩输出缓冲区

struct ImageInfo;
typedef void (*EncFunc)(ImageInfo*);
typedef void (*CpsFunc)(ImageInfo*);

struct ImageInfo {
  int index;
  int width;
  int height;
  int quality;
  int flag;
  int srcSize;
  int encSize;
  int cpsSize;
  int level;            // 压缩等级 0为不压缩
  double encTime;       // 编码时间
  double cpsTime;       // 压缩时间
  const char* srcData;  // 原始数据
  const char* encData;  // 编码数据
  const char* cpsData;  // 压缩数据
  EncFunc encFunc;      // 编码函数
  CpsFunc cpsFunc;      // 压缩函数

  ImageInfo(int index, int width, int height, int quality, int srcSize,
            const char* srcData, const char* encData, const char* cpsData,
            int level = 0, int flag = 1024)
      : index(index),
        width(width),
        height(height),
        quality(quality),
        flag(flag),
        encTime(0),
        cpsTime(0),
        srcSize(srcSize),
        level(level),
        srcData(srcData),
        encData(encData),
        cpsData(cpsData) {}

  double getEncRatio() const { return 100.0 * (1 - 1.0 * encSize / srcSize); }

  double getCpsRatio() const { return 100.0 * (1 - 1.0 * cpsSize / srcSize); }
};

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

static void compress_lz4(ImageInfo& info, bool enc = false) {
  int64_t start = getCurrentTime();
  info.cpsSize = LZ4_compress_fast(
      enc ? info.encData : info.srcData, (char*)info.cpsData,
      enc ? info.encSize : info.srcSize,
      LZ4_compressBound(enc ? info.encSize : info.srcSize), info.level);
  info.cpsTime = (getCurrentTime() - start) / 1000.0;
}

static void compress_lz4_hc(ImageInfo& info) {
  int64_t start = getCurrentTime();
  info.cpsSize =
      LZ4_compress_HC(info.srcData, (char*)info.cpsData, info.srcSize,
                      LZ4_compressBound(info.srcSize), info.level);
  info.cpsTime = (getCurrentTime() - start) / 1000.0;
}

static void compress_zstd(ImageInfo& info, bool enc = false) {
  int64_t start = getCurrentTime();
  info.cpsSize =
      ZSTD_compress((char*)info.cpsData,
                    ZSTD_compressBound(enc ? info.encSize : info.srcSize),
                    enc ? info.encData : info.srcData,
                    enc ? info.encSize : info.srcSize, info.level);
  info.cpsTime = (getCurrentTime() - start) / 1000.0;
}

static void encode_jpeg(ImageInfo& info) {
  int subsamp = TJSAMP_420;

  tjhandle handle = tjInitCompress();
  if (!handle) {
    info.encTime = 999;
    fmt::println(stderr, "tjInitCompress Init failed: {}", tjGetErrorStr());
    return;
  }

  int64_t start = getCurrentTime();
  int ret = tjCompress2(
      handle, (const unsigned char*)info.srcData, info.width, 0, info.height,
      TJPF_BGRA, (unsigned char**)(&info.encData),
      (unsigned long*)(&info.encSize), subsamp, info.quality, info.flag);
  info.encTime = (getCurrentTime() - start) / 1000.0;

  if (ret != 0) {
    fmt::println(stderr, "tjInitCompress Compress failed: {}", tjGetErrorStr());
  }

  if (handle) {
    tjDestroy(handle);
  }
}

static void encode_yuv(ImageInfo& info) {
  int subsamp = TJSAMP_420;
  tjhandle handle = tjInitCompress();
  if (!handle) {
    info.encTime = 999;
    fmt::println(stderr, "tjInitCompress Init failed: {}", tjGetErrorStr());
    return;
  }

  int64_t start = getCurrentTime();
  info.encSize = tjBufSizeYUV(info.width, info.height, subsamp);
  int ret = tjEncodeYUV2(handle, (unsigned char*)info.srcData, info.width, 0,
                         info.height, TJPF_BGRA, (unsigned char*)info.encData,
                         subsamp, info.flag);
  info.encTime = (getCurrentTime() - start) / 1000.0;

  if (ret != 0) {
    fmt::println(stderr, "tjInitCompress Compress failed: {}", tjGetErrorStr());
  }

  if (handle) {
    tjDestroy(handle);
  }
}

static void test_lz4(ImageInfo& info) {
  std::vector<int> levels = {1, 3, 6, 9, 10, 12};

  fmt::println("test lz4");
  for (int level : levels) {
    info.level = level;
    compress_lz4(info);
    fmt::println(
        "    level: {:>2}  ratio: {:>6.3f}  compress time: {:>4.1f} ms \t "
        "({:^4} => {:^4}) kb",
        level, info.getCpsRatio(), info.cpsTime, info.srcSize / 1024,
        info.cpsSize / 1024);
  }

  fmt::println("");
}

static void test_lz4_hc(ImageInfo& info) {
  std::vector<int> levels = {1, 3, 6, 9, 10, 12};

  fmt::println("test lz4hc");
  for (int level : levels) {
    info.level = level;
    compress_lz4_hc(info);
    fmt::println(
        "    level: {:>2}  ratio: {:>6.3f}  compress time: {:>4.1f} ms \t "
        "({:^4} => {:^4}) kb",
        level, info.getCpsRatio(), info.cpsTime, info.srcSize / 1024,
        info.cpsSize / 1024);
  }

  fmt::println("");
}

static void test_zstd(ImageInfo& info) {
  fmt::println("test zstd");
  for (int level = 0; level <= 4; level++) {
    info.level = level;
    compress_zstd(info);
    fmt::println(
        "    level: {:>2}  ratio: {:>6.3f}  compress time: {:>4.1f} ms \t "
        "({:^4} => {:^4}) kb",
        level, info.getCpsRatio(), info.cpsTime, info.srcSize / 1024,
        info.cpsSize / 1024);
  }

  fmt::println("");
}

static void test_jpeg(ImageInfo& info) {
  std::vector<int> qualities = {70, 75, 80, 85, 90, 95, 100};
  std::vector<int> flags = {TJFLAG_FASTDCT, TJFLAG_ACCURATEDCT};
  std::vector<std::string> flagNames = {"TJFLAG_FASTDCT", "TJFLAG_ACCURATEDCT"};

  fmt::println("test jpeg");
  for (int quality : qualities) {
    for (size_t i = 0; i < flags.size(); i++) {
      info.quality = quality;
      info.flag = flags[i];
      encode_jpeg(info);

      info.level = 1;
      compress_lz4(info, true);
      double lz4Time = info.cpsTime;
      int lz4Size = info.cpsSize;
      info.level = 3;
      compress_zstd(info, true);
      double zstdTime = info.cpsTime;
      int zstdSize = info.cpsSize;

      fmt::println(
          "enc ratio: {:>6.3f}  cps ratio: {:>6.3f}  quality: {:<3}  encode "
          "time: {:>4.1f} ms  lz4 time: {:>4.1f} ms  zstd time: {:>4.1f} ms "
          " flag: {:<18} \t ({:^4} => {:^4} => [{:^4}|{:^4}] [lz4/zstd]) kb",
          info.getEncRatio(), info.getCpsRatio(), quality, info.encTime,
          lz4Time, zstdTime, flagNames[i], info.srcSize / 1024,
          info.encSize / 1024, lz4Size / 1024, zstdSize / 1024);
    }
  }

  fmt::println("");
}

static void test_jpeg_yuv(ImageInfo& info) {
  std::vector<int> flags = {TJFLAG_FASTDCT, TJFLAG_ACCURATEDCT};
  std::vector<std::string> flagNames = {"TJFLAG_FASTDCT", "TJFLAG_ACCURATEDCT"};

  fmt::println("test yuv");
  for (size_t i = 0; i < flags.size(); i++) {
    info.flag = flags[i];
    encode_yuv(info);
    info.level = 1;
    compress_lz4(info, true);
    double lz4Time = info.cpsTime;
    int lz4Size = info.cpsSize;
    info.level = 3;
    compress_zstd(info, true);
    double zstdTime = info.cpsTime;
    int zstdSize = info.cpsSize;

    fmt::println(
        "enc ratio: {:>6.3f}  cps ratio: {:>6.3f}  encode time: {:>4.1f} ms  "
        "lz4 time: {:>4.1f} ms  zstd time: {:>4.1f} ms  flag: {:<18} \t "
        "({:^4} => {:^4} => [{:^4}|{:^4}] [lz4/zstd]) kb",
        info.getEncRatio(), info.getCpsRatio(), info.encTime, lz4Time, zstdTime,
        flagNames[i], info.srcSize / 1024, info.encSize / 1024, lz4Size / 1024,
        zstdSize / 1024);
  }

  fmt::println("");
}

static void test_xarray(ImageInfo& info) {
  int64_t start = getCurrentTime();
  std::vector<int> shape = {info.height, info.width, 4};
  xt::xarray<uint8_t> a = xt::adapt((const uint8_t*)info.srcData, info.srcSize,
                                    xt::no_ownership(), shape);
  fmt::println("test xarray");
  fmt::println("    init time: {} ms", (getCurrentTime() - start) / 1000.0);

  // bgra to bgr
  int64_t time1 = getCurrentTime();
  auto v2 =
      xt::strided_view(a, {xt::ellipsis(), xt::range(xt::placeholders::_, -1)});
  fmt::println("    to rgb time: {}", (getCurrentTime() - time1) / 1000.0);

  // split data
  /*
  LU = im[:cy, :cx]  # 左上
  RU = im[:cy, cx:]  # 右上
  LD = im[cy:, :cx]  # 左下
  RD = im[cy:, cx:]  # 右下
  */
  int cx = info.width / 2;
  int cy = info.height / 2;
  time1 = getCurrentTime();
  auto LU = xt::strided_view(v2, {xt::range(xt::placeholders::_, cy),
                                  xt::range(xt::placeholders::_, cx)});
  auto RU = xt::strided_view(v2, {xt::range(xt::placeholders::_, cy),
                                  xt::range(cx, xt::placeholders::_)});
  auto LD = xt::strided_view(v2, {xt::range(cy, xt::placeholders::_),
                                  xt::range(xt::placeholders::_, cx)});
  auto RD = xt::strided_view(v2, {xt::range(cy, xt::placeholders::_),
                                  xt::range(cx, xt::placeholders::_)});
  fmt::println("    split time: {}", (getCurrentTime() - time1) / 1000.0);
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
  int imgSize = image.size();
  const char* imgData = image.c_str();
  fmt::println("Input Image size: {}, Width: {}, Height: {}", imgSize, width,
               height);

  ImageInfo info(0, width, height, 100, imgSize, imgData, encBuf.c_str(),
                 cpsBuf.c_str(), 1, TJFLAG_FASTDCT);

  // 测试压缩
  test_lz4(info);
  // test_lz4_hc(info);
  // test_zstd(info);
  // test_jpeg(info);
  // test_jpeg_yuv(info);
  // test_xarray(info);

  return 0;
}