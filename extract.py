import lzma
import tarfile
import os

xz_path = r"E:\Android-SDK\NDK-CMake-GStreamer\gstreamer-android.tar.xz"
extract_dir = r"E:\Android-SDK\NDK-CMake-GStreamer"

print("解压xz文件...")
with lzma.open(xz_path) as compressed:
    tar_path = os.path.join(extract_dir, "temp.tar")
    with open(tar_path, "wb") as tar_file:
        while True:
            chunk = compressed.read(1024*1024)
            if not chunk:
                break
            tar_file.write(chunk)
    
print("解压tar文件...")
with tarfile.open(tar_path) as tar:
    tar.extractall(path=extract_dir)

# 删除临时tar文件
os.remove(tar_path)
print("完成!")
