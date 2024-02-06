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
#include "qoixx.hpp"
#include "turbojpeg.h"
#include "zstd.h"

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

static void test_lz4(const char* srcData, int srcSize, int width, int height,
                     const std::string& prefix = "") {
  std::vector<int> levels = {1, 3, 6, 9, 10, 12};

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

static void test_lz4_hc(const char* srcData, int srcSize, int width, int height,
                        const std::string& prefix = "") {
  std::vector<int> levels = {1, 3, 6, 9, 10, 12};

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

static void test_zstd(const char* srcData, int srcSize, int width, int height,
                      const std::string& prefix = "") {
  encBuf.resize(ZSTD_compressBound(srcSize));
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

static void test_zstd_thread(const char* srcData, int srcSize, int width,
                             int height, const std::string& prefix = "") {
  return;
  int threads = 2;
  encBuf.resize(ZSTD_compressBound(srcSize));
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

static void test_lz4_gpu(const char* srcData, int srcSize, int width,
                         int height, const std::string& prefix = "") {}

static void test_zstd_gpu(const char* srcData, int srcSize, int width,
                          int height, const std::string& prefix = "") {}

static void test_jpeg(const char* srcData, int srcSize, int width, int height,
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
  unsigned long outSize = 0;
  unsigned char* outData = (unsigned char*)encBuf.c_str();

  fmt::println("{}test jpeg", prefix);
  for (int quality : qualitys) {
    for (size_t i = 0; i < flags.size(); i++) {
      int flag = flags[i];
      std::string name = flagNames[i];
      int64_t start = getCurrentTime();
      int ret =
          tjCompress2(handle, (const unsigned char*)srcData, width, 0, height,
                      TJPF_BGRA, &outData, &outSize, subsamp, quality, flag);
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
        test_lz4((const char*)outData, outSize, width, height, "\t");
        test_zstd((const char*)outData, outSize, width, height, "\t");
      }
    }
  }

  if (handle) {
    tjDestroy(handle);
  }
}

static void test_jpeg_yuv(const char* srcData, int srcSize, int width,
                          int height, const std::string& prefix = "",
                          bool compress = true) {
  tjhandle handle = tjInitCompress();
  if (!handle) {
    fmt::println(stderr, "tjInitCompress Init failed: {}", tjGetErrorStr());
    return;
  }

  std::vector<int> flags = {TJFLAG_FASTDCT, TJFLAG_ACCURATEDCT};
  std::vector<std::string> flagNames = {"TJFLAG_FASTDCT", "TJFLAG_ACCURATEDCT"};

  int subsamp = TJSAMP_420;
  unsigned long outSize = tjBufSizeYUV(width, height, subsamp);
  encBuf.resize(outSize);
  unsigned char* outData = (unsigned char*)encBuf.c_str();

  fmt::println("{}test yuv", prefix);
  for (size_t i = 0; i < flags.size(); i++) {
    int flag = flags[i];
    std::string name = flagNames[i];
    int64_t start = getCurrentTime();
    int ret = ret = tjEncodeYUV2(handle, (unsigned char*)srcData, width, 0,
                                 height, TJPF_BGRA, outData, subsamp, flag);
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
      test_lz4((const char*)outData, outSize, width, height, "\t");
      test_zstd((const char*)outData, outSize, width, height, "\t");
    }
  }

  if (handle) {
    tjDestroy(handle);
  }
}

static void test_qoi(const char* srcData, int srcSize, int width, int height,
                     const std::string& prefix = "") {
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

static void test_xarray(const char* srcData, int srcSize, int width, int height,
                        const std::string& prefix = "") {
  int64_t start = getCurrentTime();
  std::vector<int> shape = {height, width, 4};
  xt::xarray<uint8_t> a =
      xt::adapt((const uint8_t*)srcData, srcSize, xt::no_ownership(), shape);
  fmt::println("{}test xarray", prefix);
  fmt::println("    init time: {}", (getCurrentTime() - start) / 1000.0);

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
  int cx = width / 2;
  int cy = height / 2;
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

  // encode to yuv
  test_jpeg_yuv((const char*)LU.data(), LU.size(), cx, cy, "\t", false);
  test_jpeg_yuv((const char*)RU.data(), RU.size(), cx, cy, "\t", false);
  test_jpeg_yuv((const char*)LD.data(), LD.size(), cx, cy, "\t", false);
  test_jpeg_yuv((const char*)RD.data(), RD.size(), cx, cy, "\t", false);

  // // encode to jpg
  test_jpeg((const char*)LU.data(), LU.size(), cx, cy, "\t", false);
  test_jpeg((const char*)RU.data(), RU.size(), cx, cy, "\t", false);
  test_jpeg((const char*)LD.data(), LD.size(), cx, cy, "\t", false);
  test_jpeg((const char*)RD.data(), RD.size(), cx, cy, "\t", false);
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

  // 测试压缩
  // test_lz4(imgData, imgSize, width, height);
  // test_lz4_hc(imgData, imgSize, width, height);
  // test_zstd(imgData, imgSize, width, height);
  // test_zstd_thread(imgData, imgSize, width, height);
  // test_lz4_gpu(imgData, imgSize, width, height);
  // test_zstd_gpu(imgData, imgSize, width, height);
  test_jpeg(imgData, imgSize, width, height, "", false);
  test_jpeg_yuv(imgData, imgSize, width, height, "", false);
  // test_qoi(imgData, imgSize, width, height);
  test_xarray(imgData, imgSize, width, height);

  return 0;
}