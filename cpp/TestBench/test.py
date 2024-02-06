import os
import sys
import time

import numpy as np
from PIL import Image

os.chdir(
    os.path.join(
        os.path.dirname(os.path.dirname(sys.argv[0])), "build/TestBench/Release"
    )
)
print(os.getcwd())

data = open("1920.bgra", "rb").read()

img = Image.frombytes("RGBA", (1920, 1080), data, "raw")
img = img.convert("RGB")
start = time.time()
img.save("1920.jpg", quality=100, subsampling="4:2:0")
print(time.time() - start)

# start = time.time()
# ar = np.array(img)
# print(time.time() - start)

# print(ar.shape)

# b = ar[:, :, 0]
# g = ar[:, :, 1]
# r = ar[:, :, 2]

# start = time.time()
# print("size1:", len(zlib.compress(data, 9)), time.time() - start)

# start = time.time()
# sb = len(zlib.compress(b.tobytes(), 9))
# print("size b:", sb, time.time() - start)

# start = time.time()
# sg = len(zlib.compress(g.tobytes(), 9))
# print("size g:", sg, time.time() - start)

# start = time.time()
# sr = len(zlib.compress(r.tobytes(), 9))
# print("size r:", sr, time.time() - start)


# print(sb + sg + sr)

w, h = 1920, 1080

start = time.time()
i1 = img.crop((0, 0, int(w / 2), int(h / 2)))
i2 = img.crop((int(w / 2), 0, w, int(h / 2)))
i3 = img.crop((0, int(h / 2), int(w / 2), h))
i4 = img.crop((int(w / 2), int(h / 2), w, h))
print("crop:", time.time() - start)
i1.save("3840_1.jpg", quality=100, subsampling="4:2:0")
i2.save("3840_2.jpg", quality=100, subsampling="4:2:0")
i3.save("3840_3.jpg", quality=100, subsampling="4:2:0")
i4.save("3840_4.jpg", quality=100, subsampling="4:2:0")
print("save split time:", time.time() - start)


img = Image.frombytes("RGBA", (1920, 1080), data, "raw")
imm = np.array(img)
print(imm.shape)


start = time.time()
im = np.fromfile("1920.bgra", np.dtype("B"))
print("numpy fromfile time:", time.time() - start)

t1 = time.time()
im = im.reshape((1080, 1920, 4))
print("reshape time:", time.time() - t1)

t1 = time.time()
im = im[..., :-1]
print("slice time:", time.time() - t1)
print(im.shape)

# 计算中心点
center_x, center_y = 1920 // 2, 1080 // 2

# 切分图像
t1 = time.time()
LU = im[:center_y, :center_x]  # 左上
RU = im[:center_y, center_x:]  # 右上
LD = im[center_y:, :center_x]  # 左下
RD = im[center_y:, center_x:]  # 右下
print("split time:", time.time() - t1)


# 保存切分后的图像
LU = Image.fromarray(LU, "RGB")
RU = Image.fromarray(RU, "RGB")
LD = Image.fromarray(LD, "RGB")
RD = Image.fromarray(RD, "RGB")

t1 = time.time()
LU.save("LU.jpg", quality=100, subsampling="4:2:0")
RU.save("RU.jpg", quality=100, subsampling="4:2:0")
LD.save("LD.jpg", quality=100, subsampling="4:2:0")
RD.save("RD.jpg", quality=100, subsampling="4:2:0")
print("save time:", time.time() - t1)
print("totlal time:", time.time() - start)