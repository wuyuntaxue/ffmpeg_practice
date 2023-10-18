
文件夹说明
### ch0
第一个ffmepg程序：获取媒体文件的信息并打印
将编译好的ffmpeg引入工程（暂时只有x86_64版本）
使用cmake构建工程

### separate-aac
从媒体文件中分离出aac格式的音频，并单独保存在文件中
假设媒体文件中的音频格式是AAC、采样率48000、双声道

### encoder-h264
读取yuv420格式的原始图像文件，编码成H264文件

### qsv-encoder-h264 vaapi-encoder-h264
使用qsv或vaapi的编码器操作

### parser-h264
H264文件的解码、转码操作。包含H264帧信息解析头文件

### codec-example
编码器、解码器的对象示例（解码器的packet是其他工程的，参考时需要自定义修改）


### mux-ch0
解复用/封装，从中分离出视频流、音频流

### mux-ch1
解封装，将其中的视频流单独保存成文件（保存文件时，帧的PTS总是计算不对，导致输出的视频帧率异常）

### filter-ch0
视频流缩放


```shell
# 运行时的库加载路径
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/xxx/ffmpeg_practice/3rdparty/ffmpeg-n6.0/lib

# libva驱动位置
export LIBVA_DRIVERS_PATH=/usr/lib/x86_64-linux-gnu/dri
```
