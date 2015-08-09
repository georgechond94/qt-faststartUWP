# qt-faststartUWP
Windows Runtime Component which creates fast-start enabled videos in your UWP Apps.

MP4 files can't be streamed over the web until the whole file is downloaded locally. Qt-faststart adjusts the MP4's metadata to allow for web players or even the UWP MediaElement to play them right away, while it **progressively downloads**. 
Based on [qt-faststart](https://github.com/FFmpeg/FFmpeg/blob/master/tools/qt-faststart.c) tool by [ffmpeg library](https://github.com/FFmpeg/FFmpeg).


##How to use

Simply clone the repo locally, build it and add reference to that from your project.
Then use the QtFaststart class as below:

```
    QtFaststart qt = new QtFaststart();
    qt.EncodeVideoFileFromUri(storageFile.Path); //The path to the video file.

```

And that's it! Your video file is ready to be streamed over the web.
