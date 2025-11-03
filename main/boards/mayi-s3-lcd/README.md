## 编译命令
```
idf.py @main/boards/mayi-s3-lcd/build.cfg build
```

## 表情定制
在lvgl生成对用表情的.c文件到main/boards/mayi-s3-lcd/components/txp666__otto-emoji-gif-component/src

## 特殊说明
lvgl已更改/main/boards/mayi-s3-lcd/components/lvgl__lvgl/src/libs/gif/gifdec.c
主要解决背景色的问题，现强制为白色透明色
