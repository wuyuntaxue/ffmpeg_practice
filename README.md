
文件夹说明
### ch0
第一个ffmepg程序：获取媒体文件的信息并打印
将编译好的ffmpeg引入工程（暂时只有x86_64版本）
使用cmake构建工程

### separate-aac
从媒体文件中分离出aac格式的音频，并单独保存在文件中
假设媒体文件中的音频格式是AAC、采样率48000、双声道

```shell
# 运行时的库加载路径
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/xxx/ffmpeg_practice/3rdparty/ffmpeg-n6.0/lib
```

//export LIBVA_DRIVERS_PATH=/usr/lib/x86_64-linux-gnu/dri