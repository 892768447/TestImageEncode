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

#ifdef _WIN32
#include <direct.h>
#include <windows.h>

#else
#include <unistd.h>
#endif

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

static void test_jpeg(const std::string& image, int width, int height) {}

static void test_qoi(const std::string& image, int width, int height) {}

static void test_lz4(const std::string& image, int width, int height) {}

static void test_zstd(const std::string& image, int width, int height) {}

static void test_zstd_thread(const std::string& image, int width, int height) {}

static void test_lz4_gpu(const std::string& image, int width, int height) {}

static void test_zstd_gpu(const std::string& image, int width, int height) {}

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
  }
  if (argc > 2) {
    width = std::stoi(argv[2]);
  }
  if (argc > 3) {
    height = std::stoi(argv[3]);
  }

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
  std::cout << "Input Image size: " << image.size() << ", Width: " << width
            << ", Height: " << height << std::endl;

  // 测试压缩
  test_jpeg(image, width, height);

  return 0;
}