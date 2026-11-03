# Справочник контейнеров, кодеков и полей MediaInfo

Источник: MediaInfoLib, снимок `b47ab462f9df8a89e971af1ef8753d802c71de8a`. [Исходный код](https://github.com/MediaArea/MediaInfoLib/tree/b47ab462f9df8a89e971af1ef8753d802c71de8a); [документация полей](https://mediaarea.net/en/MediaInfo/Support/Fields); [поддерживаемые форматы](https://mediaarea.net/en/MediaInfo/Support/Formats).

## Как читать справочник

В первой таблице перечислены форматы из базы MediaInfo: контейнеры, кодеки и другие форматы. Далее приведены полные каталоги именованных полей по видам потоков, с точными именами и оригинальными англоязычными описаниями разработчика. Если описание отсутствует в источнике, это отмечено.

**Ограничение:** это единый словарь результатов MediaInfo, а не таблица совместимости каждого поля с каждым контейнером или кодеком. Наличие названия в словаре не означает, что поле хранится в конкретном формате, доступно для записи через FFmpeg или отображается проигрывателем. Некоторые значения вычисляются программой, а имена MediaInfo могут отличаться от имен полей в спецификации. Форматы и внутренние обозначения даны в точности как в базе, включая устаревшие записи.

## База форматов

| Имя формата | Тип в базе | Описание в базе | Расширения |
|---|---|---|---|
| AAF | M | — | aaf |
| AIFF | M | Apple/SGI | aiff aifc aif |
| AMV | M | Chinese hack of AVI | amv |
| AVI | M | Audio Video Interleave | avi |
| BDAV | M | Blu-ray Video | m2ts mts ssif |
| Blu-ray Clip info | M | — | clpi |
| Blu-ray Index | M | — | bdmv bdm |
| Blu-ray Movie object | M | — | bdmv bdm |
| Blu-ray Playlist | M | — | mpls |
| CDDA | M | — | cda |
| CDXA | M | — | dat |
| DASH MPD | M | — | mpd |
| DV | M | — | dv dif |
| DivX | M | Hack of AVI | divx |
| DPG | M | Nintendo DS | dpg |
| DVD Video | M | — | ifo |
| Flash Video | M | — | flv |
| GXF | M | SMPTE 360M | gxf |
| HDS F4M | M | Flash Media Manifest | f4m |
| HEIF | M | — | heif avci avcs heic heics avif avis |
| HLS | M | — | m3u8 |
| Google Video | M | Hack of AVI | gvi |
| ICC |  | — | icc |
| ISM | M | Internet Streaming Media | ism |
| IVF | M | — | ivf |
| LXF | M | — | lxf |
| Matroska | M | — | mkv mk3d mka mks |
| MPEG-PS | M | — | mpeg mpg m2p vob vro pss evo |
| MPEG-TS | M | — | ts m2t m2s m4t m4s tmf ts tp trp ty |
| MPEG-4 | M | — | braw mov mp4 m4v m4a m4b m4p m4r 3ga 3gpa 3gpp 3gp 3gpp2 3g2 k3g jpm jpx mqv ismv isma ismt f4a f4b f4v |
| MTV | M | Chinese hack of MPEG-1 layer 3 | mtv |
| MXF | M | — | mxf |
| NSV | M | Nullsoft Streaming Video | nsv |
| NUT | M | — | nut |
| Ogg | M | — | oga ogg ogm ogv ogx opus spx |
| PMP | M | Playstation Portable | pmp |
| PTX | M | — | ptx |
| QuickTime | M | Original Apple specifications | braw mov qt |
| RealMedia | M | — | rm rmvb ra |
| RIFF-MMP | M | RIFF Multimedia Movie | — |
| ShockWave | M | — | swf |
| SKM | M | Sky Korean Mobilephone | skm |
| WebM | M | — | webm |
| Windows Media | M | — | asf dvr-ms wma wmv |
| WTV | M | — | wtv |
| APV | V | Advanced Professional Video | apv |
| AV1 | V | AOMedia Video 1 | — |
| AV2 | V | AOMedia Video 2 | — |
| AVC | V | Advanced Video Codec | avc h264 264 |
| AVS Video | V | Audio Video Standard, Video part | — |
| AVS3 Video | V | Audio Video Standard 3, Video part | avs3 |
| Cinegy Daniel | V | — | — |
| Cinegy Daniel2 | V | — | — |
| Dirac | V | — | drc |
| FFV1 | V | — | — |
| FFV2 | V | — | — |
| FLC | V | — | fli flc |
| FLI | V | — | fli flc |
| FLIC | V | — | fli flc |
| H.261 | V | — | h261 |
| H.263 | V | — | h263 |
| HEVC | V | High Efficiency Video Coding | hevc h265 265 |
| MPEG Video | V | — | mpgv mpv mp1v m1v mp2v m2v |
| MPEG-4 Visual | V | — | m4v mp4v |
| Theora | V | — | — |
| VC-1 | V | — | vc1 |
| YUV4MPEG2 | V | — | y4m |
| VP8 | V | — | — |
| YUV | V | — | — |
| AAC | A | Advanced Audio Codec | — |
| AC-3 | A | Audio Coding 3 | ac3 |
| AC-4 | A | Audio Coding 4 | ac4 |
| ADIF | A | Audio Data Interchange Format | — |
| ADTS | A | Audio Data Transport Stream | aac aacp adts |
| ALS | A | MPEG-4 Audio Lossless Coding | als |
| AMR | A | Adaptive Multi-Rate | amr |
| APAC | A | Apple Positional Audio Codec | — |
| Atrac | A | Adaptive Transform Acoustic Coding | — |
| Atrac3 | A | Adaptive Transform Acoustic Coding 3 | — |
| Atrac9 | A | Adaptive Transform Acoustic Coding 9 | — |
| APT-X100 | A | — | AUD AUE aud aue |
| AU | A | uLaw/AU Audio File | au |
| Auro-Cx | A | Auro-Cx 3D audio | — |
| CAF | A | Core Audio Format | caf |
| DAT | A | Digital Audio Tape | hda |
| DolbyE | A | — | dde |
| DSDIFF | A | Direct Stream Digital Interchange File Format | dff |
| DSD | A | Direct Stream Digital | — |
| DSF | A | Direct Stream Digital Stream File | dsf |
| DST | A | Direct Stream Transfer | — |
| DTS | A | Digital Theater Systems | dts dtshd dtsma dtshdma |
| DTS-HD | A | Digital Theater Systems | dts dtshd dtsma dtshdma |
| DTS-UHD | A | — | dtsx |
| E-AC-3 | A | Enhanced AC-3 | dd+ ec3 eac3 |
| Extended Module | A | — | xm |
| FLAC | A | Free Lossless Audio Codec | fla flac |
| G.719 | A | — | — |
| G.722 | A | — | — |
| G.722.1 | A | — | — |
| G.723 | A | — | — |
| G.729 | A | — | — |
| G.729.1 | A | — | — |
| IAMF | A | Immersive Audio Model and Formats | iamf |
| Impulse Tracker | A | — | it |
| LA | A | Lossless Audio Codec | la |
| MIDI | A | RIFF Musical Instrument Digital Interface | midi mid kar |
| Module | A | — | mod |
| Monkey's Audio | A | — | ape mac |
| MPEG Audio | A | — | m1a mpa mpa1 mp1 m2a mpa2 mp2 mp3 |
| OpenMG | A | — | oma omg aa3 at3 |
| Musepack SV7 | A | — | mpc |
| Musepack SV8 | A | — | mp+ |
| QCELP | A | — | — |
| QLCM | A | — | qcp |
| RIFF-MIDI | A | RIFF Musical Instrument Digital Interface | — |
| RKAU | A | RK Audio | rka |
| Scream Tracker 3 | A | — | s3m |
| Shorten | A | — | shn |
| SLS | A | MPEG-4 Scalable Lossless Coding | sls |
| S/PDIF | A | Sony/Philips Digital Interface | spdif |
| Speex | A | — | spx |
| USAC | A | Unified Speech and Audio Coding | — |
| Opus | A | — | — |
| TAK | A | — | tak |
| TrueHD | A | — | mlp thd vr |
| TwinVQ | A | Transform domain Weighted INterleave Vector Quantization | vqf |
| Vorbis | A | — | — |
| Wave | A | — | act at9 wav |
| Wave64 | A | — | w64 |
| WavPack | A | — | wv wvc |
| Amiga Icon | I | Amiga Workbench Icon | info |
| APNG | I | Animated Portable Network Graphic | apng png pns |
| Arri Raw | I | — | ari |
| ARW | I | Sony Alpha RAW | arw |
| Bitmap | I | — | bmp bms |
| BPG | I | Better Portable Graphics | bpg |
| DDS | I | DirectDraw Surface | dds |
| DNG | I | Digital Negative | dng |
| DPX | I | — | dpx cin |
| EXR | I | — | exr |
| DIB | I | RIFF Device Independent Bitmap | — |
| GIF | I | Graphics Interchange Format | gif gis |
| ICO | I | — | ico |
| JNG | I | JPEG Network Graphic | jng |
| JPEG | I | — | h3d jpeg jpg jpe jps mpo |
| JPEG 2000 | I | — | jp2 jph j2c j2k jhc |
| LZ77 | I | — | — |
| MNG | I | Multiple-Image Network Graphic | mng |
| NEF | I | Nikon Electronic Format | nef |
| NRW | I | Nikon RAW | nrw |
| PCX | I | Personal Computer eXchange | pcx |
| PNG | I | Portable Network Graphic | png pns |
| PSD | I | Photoshop File Format | psd |
| RIFF Palette | I | RIFF Palette | — |
| RLE | I | Run-length encoding | rle |
| TIFF | I | Tag Image File Format | 3fr arw cr2 dcr erf exif kdc mef mos nef nrw pef sr2 srf srw tiff tif |
| TGA | I | — | tga |
| 7-Zip | C | — | 7z |
| ACE | C | — | ace |
| ELF | C | — | so |
| ISO 9660 | C | — | iso |
| Mach-O | C | — | o dylib kext |
| Mach-O Universal | C | — | o dylib kext |
| MZ | C | — | exe dll efi |
| RAR | C | From Rarlabs | rar |
| TAR | C | — | tar |
| ZIP | C | — | zip docx odt xlsx ods |
| Adobe encore DVD | T | — | txt |
| AQTitle | T | — | aqt |
| ASS | T | — | ssa |
| Captions 32 | T | — | txt |
| Captions Inc | T | — | txt |
| CPC Captioning | T | — | txt |
| Cheeta | T | — | asc |
| N19 | T | — | stl |
| PAC | T | — | pac fpc |
| PDF | T | — | pdf |
| SAMI | T | — | smi sami |
| SCC | T | — | scc sc2 |
| SubRip | T | — | srt |
| TTML | T | Timed Text Markup Language | dfxp ttml imsc imscr |
| SSA | T | — | ssa |
| WebVTT | T | — | vtt |
| Blender | O | — | blenders |
| AutoCAD | O | — | — |
| PlayLater Video | V | — | — |
| WTV | M | Windows Recorded TV Show | wtv |
| VVC | V | Versatile Video Coding | vvc h266 266 |
| WebP | I | — | webp |

Обозначения типа выше приведены из базы MediaInfo без интерпретации: один символ сам по себе не является подтверждением поддержки записи.

## Общие сведения о файле — General

[Оригинальный каталог General.csv](https://github.com/MediaArea/MediaInfoLib/blob/b47ab462f9df8a89e971af1ef8753d802c71de8a/Source/Resource/Text/Stream/General.csv)

| Точное имя поля | Единица | Описание (оригинал) |
|---|---|---|
| `Count` | — | Count of objects available in this stream created by MediaInfo when analyzing file. This is mostly for internal use |
| `Status` | — | Status of bit field when parsing. Options are: 0=IsAccepted, 1=IsFilled, 2=IsUpdated, 3=IsFinished. This is mostly for internal use |
| `StreamCount` | — | Total number of streams available for this StreamKind. Counting starts at 1 |
| `StreamKind` | — | Name of stream type. Options are: Audio, General, Image, Menu, Other, Text, or Video |
| `StreamKind/String` | — | Name of stream type. Options are: Audio, General, Image, Menu, Other, Text, or Video |
| `StreamKindID` | — | Identification number for stream, assigned in order of parsing. Counting starts at 0 |
| `StreamKindPos` | — | Identification number for stream when multiple, assigned in order of parsing. Counting starts at 1 |
| `StreamOrder` | — | Stream order in the file for type of stream. Counting starts at 0 |
| `FirstPacketOrder` | — | Order of the first fully decodable packet parsed in the file for stream type. Counting starts at 0 |
| `Inform` | — | Last **Inform** call. This is mostly for internal use |
| `ID` | — | The identification number for this stream in this file |
| `ID/String` | — | The identification number for this stream in this file (String format) |
| `OriginalSourceMedium_ID` | — | Identification for this stream in the original medium of the material, taken from Tag metadata |
| `OriginalSourceMedium_ID/String` | — | Identification for this stream in the original medium of the material, taken from Tag metadata (String format) |
| `UniqueID` | — | The unique ID for this stream, should be copied with stream copy |
| `UniqueID/String` | — | The unique ID for this stream, should be copied with stream copy |
| `MenuID` | — | The menu ID for this stream in this file |
| `MenuID/String` | — | The menu ID for this stream in this file |
| `GeneralCount` | — | Total number of General streams in this file |
| `VideoCount` | — | Total number of Video streams in this file |
| `AudioCount` | — | Total number of Audio streams in this file |
| `TextCount` | — | Total number of Text streams in this file |
| `OtherCount` | — | Total number of Other streams in this file |
| `ImageCount` | — | Total number of Image streams in this file |
| `MenuCount` | — | Total number of Menu streams in this file |
| `Video_Format_List` | — | Video codecs found in this file, separated by forward slash ("/") |
| `Video_Format_WithHint_List` | — | Video codecs found in this file, separated by forward slash ("/") and including common alternative codec names |
| `Video_Codec_List` | — | Deprecated, do not use in new projects |
| `Video_Language_List` | — | List of video stream languages in this file separated by forward slash ("/") |
| `Audio_Format_List` | — | Audio codecs found in this file, separated by forward slash ("/") |
| `Audio_Format_WithHint_List` | — | Audio codecs found in this file, separated by forward slash ("/") and including common alternative codec names |
| `Audio_Codec_List` | — | Deprecated, do not use in new projects |
| `Audio_Language_List` | — | List of audio stream languages in this file separated by forward slash ("/") |
| `Audio_Channels_Total` | — | Total count of channels in all audio streams |
| `Text_Format_List` | — | Text codecs found in this file, separated by forward slash ("/") |
| `Text_Format_WithHint_List` | — | Text codecs found in this file, separated by forward slash ("/") and including common alternative codec names |
| `Text_Codec_List` | — | Deprecated, do not use in new projects |
| `Text_Language_List` | — | List of text stream languages in this file separated by forward slash ("/") |
| `Other_Format_List` | — | Other data formats found in this file, separated by forward slash ("/") |
| `Other_Format_WithHint_List` | — | Other data formats found in this file, separated by forward slash ("/") and including common alternative codec names |
| `Other_Codec_List` | — | Deprecated, do not use in new projects |
| `Other_Language_List` | — | List of other stream languages (typically Chapters) in this file separated by forward slash ("/") |
| `Image_Format_List` | — | Image codecs found in this file, separated by forward slash ("/") |
| `Image_Format_WithHint_List` | — | Image codecs found in this file, separated by forward slash ("/") and including common alternative codec names |
| `Image_Codec_List` | — | Deprecated, do not use in new projects |
| `Image_Language_List` | — | List of image stream languages in this file separated by forward slash ("/") |
| `Menu_Format_List` | — | Menu formats found in this file, separated by forward slash ("/") |
| `Menu_Format_WithHint_List` | — | Menu formats found in this file, separated by forward slash ("/") and including common alternative codec names |
| `Menu_Codec_List` | — | Deprecated, do not use in new projects |
| `Menu_Language_List` | — | List of menu stream languages in this file separated by forward slash ("/") |
| `CompleteName` | — | Full path for this file (Folder+Name+Extension) |
| `FolderName` | — | Folder name for this file |
| `FileNameExtension` | — | File name and extension |
| `FileName` | — | File name only |
| `FileExtension` | — | File extension only |
| `CompleteName_Last` | — | Complete name (Folder+Name+Extension) of the last file (in the case of a sequence of files) |
| `FolderName_Last` | — | Folder name only of the last file (in the case of a sequence of files) |
| `FileNameExtension_Last` | — | File name and extension of the last file (in the case of a sequence of files) |
| `FileName_Last` | — | File name only of the last file (in the case of a sequence of files) |
| `FileExtension_Last` | — | File extension only of the last file (in the case of a sequence of files) |
| `Format` | — | Format used |
| `Format/String` | — | Format used + additional features |
| `Format/Info` | — | More details about the identified Format |
| `Format/Url` | — | Link to a description of this format |
| `Format/Extensions` | — | Known extensions for the identified format |
| `Format_Commercial` | — | Commercial name used by vendor for these settings or Format field if there is no difference |
| `Format_Commercial_IfAny` | — | Commercial name used by vendor for these settings if there is one |
| `Format_Version` | — | Version for the identified format |
| `Format_Profile` | — | Profile of the Format |
| `Format_Level` | — | Level of the Format |
| `Format_Compression` | — | Compression method used |
| `Format_Settings` | — | Settings used and required by decoder |
| `Format_AdditionalFeatures` | — | Features required to fully support the file content |
| `CompliantWith` | — | Specifications the file is compliant with |
| `InternetMediaType` | — | Internet Media Type (aka MIME Type, Content-Type) |
| `CodecID` | — | Codec ID, if defined by the container |
| `CodecID/String` | — | Codec ID, if defined by the container (String format) |
| `CodecID/Info` | — | More information about this codec |
| `CodecID/Hint` | — | Common alternative names for this codec |
| `CodecID/Url` | — | A link to more details about this codec |
| `CodecID_Description` | — | Codec description, as defined by the container |
| `CodecID_Version` | — | Version of the CodecID |
| `CodecID_Compatible` | — | List of codecs that are compatible with the identified container |
| `Interleaved` | — | If Audio and video are muxed |
| `Codec` | — | Deprecated, do not use in new projects |
| `Codec/String` | — | Deprecated, do not use in new projects |
| `Codec/Info` | — | Deprecated, do not use in new projects |
| `Codec/Url` | — | Deprecated, do not use in new projects |
| `Codec/Extensions` | — | Deprecated, do not use in new projects |
| `Codec_Settings` | — | Deprecated, do not use in new projects |
| `Codec_Settings_Automatic` | — | Deprecated, do not use in new projects |
| `FileSize` | byte | File size, in bytes |
| `FileSize/String` | — | File size with measurement (measured in powers of 1024) |
| `FileSize/String1` | — | File size with measurement (measured in powers of 1024) rounded to the nearest integer |
| `FileSize/String2` | — | File size with measurement (measured in powers of 1024) rounded to the two most significant digits |
| `FileSize/String3` | — | File size with measurement (measured in powers of 1024) rounded to the three most significant digits |
| `FileSize/String4` | — | File size with measurement (measured in powers of 1024) rounded to the four most significant digits |
| `Duration` | ms | Play time of the content, in s (ms for text output) |
| `Duration/String` | — | Play time in format XXx YYy, with YYy value omitted if zero (e.g. 1 h 40 min) |
| `Duration/String1` | — | Play time in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Duration/String2` | — | Play time in format XXx YYy, with YYy omitted if value is zero |
| `Duration/String3` | — | Play time in format HH:MM:SS.mmm (e.g. 01:40:00.000) |
| `Duration/String4` | — | Play time in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Duration/String5` | — | Play time in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Duration_Start` | ms | Start time of stream, in UTC |
| `Duration_Start/String` | — | Не указано в каталоге |
| `Duration_Start/String1` | — | Не указано в каталоге |
| `Duration_Start/String2` | — | Не указано в каталоге |
| `Duration_Start/String3` | — | Не указано в каталоге |
| `Duration_Start/String4` | — | Не указано в каталоге |
| `Duration_Start/String5` | — | Не указано в каталоге |
| `Duration_End` | ms | End time of stream, in UTC |
| `Duration_End/String` | — | Не указано в каталоге |
| `Duration_End/String1` | — | Не указано в каталоге |
| `Duration_End/String2` | — | Не указано в каталоге |
| `Duration_End/String3` | — | Не указано в каталоге |
| `Duration_End/String4` | — | Не указано в каталоге |
| `Duration_End/String5` | — | Не указано в каталоге |
| `OverallBitRate_Mode` | — | Bit rate mode of all streams (CBR, VBR) |
| `OverallBitRate_Mode/String` | — | Bit rate mode of all streams, as word (Variable, Constant) |
| `OverallBitRate` | bps | Bit rate of all streams, in bps |
| `OverallBitRate/String` | — | Bit rate of all streams, with measurement (e.g. 14.2 kb/s) |
| `OverallBitRate_Minimum` | bps | Minimum total bit rate of all streams, in bps |
| `OverallBitRate_Minimum/String` | — | Minimum bit rate of all streams, with measurement |
| `OverallBitRate_Nominal` | bps | Nominal bit rate of all streams, in bps |
| `OverallBitRate_Nominal/String` | — | Nominal bit rate of all streams, with measurement |
| `OverallBitRate_Maximum` | bps | Maximum bit rate of all streams, in bps |
| `OverallBitRate_Maximum/String` | — | Maximum bit rate of all streams, with measurement |
| `FrameRate` | fps | Frames per second |
| `FrameRate/String` | — | Frames per second, with measurement |
| `FrameRate_Num` | — | Frames per second, numerator |
| `FrameRate_Den` | — | Frames per second, denominator |
| `FrameCount` | — | Frame count, if a stream has the same frame rate everywhere |
| `Delay` | ms | Delay fixed in the stream (relative), is s (ms for text output) |
| `Delay/String` | — | Delay with measurement and rounded to integer (e.g. 213 ms) |
| `Delay/String1` | — | Delay with measurement and rounded to integer (e.g. 213 ms) |
| `Delay/String2` | — | Delay with measurement and rounded to integer (e.g. 213 ms) |
| `Delay/String3` | — | Delay in format HH:MM:SS.mmm |
| `Delay/String4` | — | Delay in format HH:MM:SS:FF, with the last colon replaced by semicolon for drop frame if available |
| `Delay/String5` | — | Delay in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Delay_Settings` | — | Delay settings (in case of timecode, for example) |
| `Delay_DropFrame` | — | Delay drop frame |
| `Delay_Source` | — | Delay source (Container, Stream, or empty) |
| `Delay_Source/String` | — | Delay source (Container, Stream, or empty) |
| `StreamSize` | byte | Size of this stream, in bytes |
| `StreamSize/String` | — | Size of this stream, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `StreamSize/String1` | — | Size of this stream, with measurement (measured in powers of 1024) rounded to the nearest integer (e.g. 11 MiB) |
| `StreamSize/String2` | — | Size of this stream, measurement (measured in powers of 1024) rounded to the two most significant digits (e.g. 11 MiB) |
| `StreamSize/String3` | — | Size of this stream, measurement (measured in powers of 1024) rounded to the three most significant digits (e.g. 10.5 MiB) |
| `StreamSize/String4` | — | Size of this stream, measurement (measured in powers of 1024) rounded to the four most significant digits (e.g. 10.50 MiB) |
| `StreamSize/String5` | — | Size of this stream, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `StreamSize_Proportion` | — | Size of this stream divided by total file size |
| `StreamSize_Demuxed` | byte | Size of this stream after demuxing, in bytes |
| `StreamSize_Demuxed/String` | — | Size of this stream after demuxing, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `StreamSize_Demuxed/String1` | — | Size of this stream after demuxing, with measurement (measured in powers of 1024) rounded to the nearest integer (e.g. 11 MiB) |
| `StreamSize_Demuxed/String2` | — | Size of this stream after demuxing, measurement (measured in powers of 1024) rounded to the two most significant digits (e.g. 11 MiB) |
| `StreamSize_Demuxed/String3` | — | Size of this stream after demuxing, measurement (measured in powers of 1024) rounded to the three most significant digits (e.g. 10.5 MiB) |
| `StreamSize_Demuxed/String4` | — | Size of this stream after demuxing, measurement (measured in powers of 1024) rounded to the four most significant digits (e.g. 10.50 MiB) |
| `StreamSize_Demuxed/String5` | — | Size of this stream after demuxing, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `HeaderSize` | — | Header field size, in bytes |
| `DataSize` | — | Data field size, in bytes |
| `FooterSize` | — | Footer field size, in bytes |
| `IsStreamable` | — | Set if this file is streamable or not (Yes, No) |
| `Album_ReplayGain_Gain` | dB | The gain to apply to reach 89dB SPL on playback |
| `Album_ReplayGain_Gain/String` | — | The gain to apply to reach 89dB SPL on playback |
| `Album_ReplayGain_Peak` | — | The maximum absolute peak value of the item |
| `Encryption` | — | Encryption |
| `Encryption_Format` | — | Encryption format |
| `Encryption_Length` | — | Encryption length (128, 192 or 256 bits) |
| `Encryption_Method` | — | Encryption method |
| `Encryption_Mode` | — | Encryption mode |
| `Encryption_Padding` | — | Encryption padding |
| `Encryption_InitializationVector` | — | Encryption initialization vector |
| `UniversalAdID/String` | — | Universal Ad-ID, see https://ad-id.org for more information |
| `UniversalAdID_Registry` | — | Universal Ad-ID registry |
| `UniversalAdID_Value` | — | Universal Ad-ID value |
| `Title` | — | Title of file; Title |
| `Title_More` | — | More title information; Title |
| `Title/Url` | — | URL; Title |
| `Domain` | — | Universe that the file's contents belong to (e.g. Star Wars, Stargate, Buffy, Dragonball); Title |
| `Collection` | — | Name of the series (e.g. Star Wars movies, Stargate SG-1, Angel); Title |
| `Season` | — | Name of the season (e.g. first Star Wars Trilogy, Season 1); Title |
| `Season_Position` | — | Number of the Season; Title |
| `Season_Position_Total` | — | Total number of seasons; Title |
| `Movie` | — | Name of the movie (e.g. Star Wars: A New Hope); Title |
| `Movie_More` | — | More information about the Movie; Title |
| `Movie/Country` | — | Deprecated, use Country; Title |
| `Movie/Url` | — | Homepage for the movie; Title |
| `Album` | — | Name of the album (e.g. The Joshua Tree); Title |
| `Album_More` | — | More information about the Album; Title |
| `Album/Sort` | — | Alternate name of the album, optimized for sorting purposes (e.g. Joshua Tree, The); Title |
| `Album/Performer` | — | Album performer/artist of this file; Entity |
| `Album/Performer/Sort` | — | Alternate name for the performer, optimized for sorting purposes (e.g. Beatles, The); Entity |
| `Album/Performer/Url` | — | Homepage of the album performer/artist; Entity |
| `Comic` | — | Name of the comic book series; Title |
| `Comic_More` | — | More information about the comic book series; Title |
| `Comic/Position_Total` | — | Total number of comics; Title |
| `Part` | — | Name of the part (e.g. CD1, CD2); Title |
| `Part/Position` | — | Number of the part; Title |
| `Part/Position_Total` | — | Total number of parts; Title |
| `Reel` | — | Name of the reel; Title |
| `Reel/Position` | — | Number of the reel; Title |
| `Reel/Position_Total` | — | Total number of reel; Title |
| `Track` | — | Name of the track (e.g. track 1, track 2); Title |
| `Track_More` | — | More information about the Track; Title |
| `Track/Url` | — | Link to a site about this Track; Title |
| `Track/Sort` | — | Alternate name for the track, optimized for sorting purposes; Title |
| `Track/Position` | — | Number of this Track; Title |
| `Track/Position_Total` | — | Total number of tracks; Title |
| `PackageName` | — | MXF package name |
| `Grouping` | — | iTunes grouping; Title |
| `Chapter` | — | Name of the Chapter; Title |
| `SubTrack` | — | Name of the Subtrack; Title |
| `Original/Album` | — | Original name of the Album; Title |
| `Original/Movie` | — | Original name of the Movie; Title |
| `Original/Part` | — | Original name of the Part; Title |
| `Original/Track` | — | Original name of the Track; Title |
| `Compilation` | Yes | iTunes compilation; Title |
| `Compilation/String` | — | iTunes compilation; Title |
| `Performer` | — | Main performer(s)/artist(s); Entity |
| `Performer/Sort` | — | Alternate name for the performer, optimized for sorting purposes (e.g. Beatles, The); Entity |
| `Performer/Url` | — | Homepage of the performer/artist; Entity |
| `Original/Performer` | — | Original artist(s)/performer(s); Entity |
| `Accompaniment` | — | Band/orchestra/accompaniment/musician; Entity |
| `Composer` | — | Name of the original composer; Entity |
| `Composer/Nationality` | — | Nationality of the primary composer of the piece; Entity |
| `Composer/Sort` | — | Nationality of the primary composer of the piece (e.g. Mozart, Wolfgang Amadeus); Entity |
| `Arranger` | — | The person who arranged the piece (e.g. Ravel); Entity |
| `Lyricist` | — | The person who wrote the lyrics for the piece; Entity |
| `Original/Lyricist` | — | Original lyricist(s)/text writer(s); Entity |
| `Conductor` | — | The artist(s) who performed the work. In classical music this would be the conductor, orchestra, soloists, etc; Entity |
| `Director` | — | Name of the director; Entity |
| `CoDirector` | — | Name of the codirector; Entity |
| `AssistantDirector` | — | Name of the assistant director; Entity |
| `DirectorOfPhotography` | — | Name of the director of photography, also known as cinematographer; Entity |
| `SoundEngineer` | — | Name of the sound engineer or sound recordist; Entity |
| `ArtDirector` | — | Name of the person who oversees the artists and craftspeople who build the sets; Entity |
| `ProductionDesigner` | — | Name of the person responsible for designing the overall visual appearance of a movie; Entity |
| `Choreographer` | — | Name of the choreographer; Entity |
| `CostumeDesigner` | — | Name of the costume designer; Entity |
| `Actor` | — | Real name of an actor/actress playing a role in the movie; Entity |
| `Actor_Character` | — | Name of the character an actor or actress plays in this movie; Entity |
| `WrittenBy` | — | Author of the story or script; Entity |
| `ScreenplayBy` | — | Author of the screenplay or scenario (used for movies and TV shows); Entity |
| `EditedBy` | — | Editors name; Entity |
| `CommissionedBy` | — | Name of the person or organization that commissioned the subject of the file; Entity |
| `Producer` | — | Name of the producer of the media; Entity |
| `CoProducer` | — | Name of a co-producer of the media; Entity |
| `ExecutiveProducer` | — | Name of an executive producer of the media; Entity |
| `MusicBy` | — | Main musical artist for the media; Entity |
| `DistributedBy` | — | Company responsible for distribution of the content; Entity |
| `OriginalSourceForm/DistributedBy` | — | Name of the person or organization who supplied the original subject; Entity |
| `MasteredBy` | — | The engineer who mastered the content for a physical medium or for digital distribution; Entity |
| `EncodedBy` | — | Name of the person/organisation that encoded/ripped the audio file; Entity |
| `RemixedBy` | — | Name of the artist(s) that interpreted, remixed, or otherwise modified the content; Entity |
| `ProductionStudio` | — | Main production studio of the media; Entity |
| `ThanksTo` | — | A very general metadata tag for everyone else that wants to be listed; Entity |
| `Publisher` | — | Name of the organization publishing the media (i.e. the record label); Entity |
| `Publisher/URL` | — | Publisher's official webpage; Entity |
| `Label` | — | Brand or trademark associated with the marketing of music recordings and music videos; Entity |
| `Genre` | — | Main genre of the media (e.g. classical, ambient-house, synthpop, sci-fi, drama, etc.); Classification |
| `PodcastCategory` | — | Podcast category; Classification |
| `Mood` | — | Intended to reflect the mood of the item with a few keywords (e.g. Romantic, Sad, Uplifting, etc.); Classification |
| `ContentType` | — | The type or genre of the content (e.g. Documentary, Feature Film, Cartoon, Music Video, Music, Sound FX, etc.); Classification |
| `Subject` | — | Describes the topic of the file (e.g. "Aerial view of Seattle."); Classification |
| `Description` | — | A short description of the contents (e.g. "Two birds flying."); Classification |
| `Keywords` | — | Keywords for the content separated by a comma, used for searching; Classification |
| `Summary` | — | Plot outline or a summary of the story; Classification |
| `Synopsis` | — | Description of the story line of the item; Classification |
| `Period` | — | Describes the period that the piece is from or about (e.g. Renaissance); Classification |
| `LawRating` | — | Legal rating of a movie. Format depends on country of origin (e.g. PG, 16); Classification |
| `LawRating_Reason` | — | Reason for the law rating; Classification |
| `ICRA` | — | The ICRA rating (previously RSACi); Classification |
| `Released_Date` | — | Date/year that the content was released; Temporal |
| `Original/Released_Date` | — | Date/year that the content was originally released; Temporal |
| `Recorded_Date` | — | Time/date/year that the recording began; Temporal |
| `Encoded_Date` | — | Time/date/year that the encoding of this content was completed; Temporal |
| `Tagged_Date` | — | Time/date/year that the tags were added to this content; Temporal |
| `Written_Date` | — | Time/date/year that the composition of the music/script began; Temporal |
| `Mastered_Date` | — | Time/date/year that the content was digitally mastered; Temporal |
| `File_Created_Date` | — | Time that the file was created on the file system; Temporal |
| `File_Created_Date_Local` | — | Local time that the file was created on the file system (not to be used in an international database); Temporal |
| `File_Modified_Date` | — | Time that the file was last modified on the file system; Temporal |
| `File_Modified_Date_Local` | — | Local time that the file was last modified on the file system (not to be used in an international database); Temporal |
| `Recorded_Location` | — | Location where track was recorded, as Longitude+Latitude; Spatial |
| `Written_Location` | — | Location that the item was originally designed/written; Spatial |
| `Archival_Location` | — | Location where an item is archived (e.g. Louvre, Paris, France); Spatial |
| `Encoded_Application` | — | Name of the software package used to create the file (e.g. Microsoft WaveEdiTY); Technical |
| `Encoded_Application/String` | — | Name of the software package used to create the file, in the format "CompanyName ProductName (OperatingSystem) Version (Date)"; Technical |
| `Encoded_Application_CompanyName` | — | Name of the company of the encoding application; Technical |
| `Encoded_Application_Name` | — | Name of the encoding product; Technical |
| `Encoded_Application_Version` | — | Version of the encoding product; Technical |
| `Encoded_Application_Url` | — | URL associated with the encoding software; Technical |
| `Encoded_Library` | — | Software used to create the file; Technical |
| `Encoded_Library/String` | — | Software used to create the file, in the format "CompanyName ProductName (OperatingSystem) Version (Date)"; Technical |
| `Encoded_Library_CompanyName` | — | Name of the encoding software company; Technical |
| `Encoded_Library_Name` | — | Name of the encoding software; Technical |
| `Encoded_Library_Version` | — | Version of the encoding software; Technical |
| `Encoded_Library_Date` | — | Release date of the encoding software, in UTC; Technical |
| `Encoded_Library_Settings` | — | Parameters used by the encoding software; Technical |
| `Encoded_OperatingSystem` | — | Operating system of the encoding software; Technical |
| `Encoded_OperatingSystem/String` | — | Operating system used to create the file, in the format "CompanyName ProductName (OperatingSystem) Version (Date)"; Technical |
| `Encoded_OperatingSystem_CompanyName` | — | Name of the operating system company; Technical |
| `Encoded_OperatingSystem_Name` | — | Name of the operating system; Technical |
| `Encoded_OperatingSystem_Version` | — | Version of the operating system; Technical |
| `Encoded_Hardware` | — | Hardware of the encoding software; Technical |
| `Encoded_Hardware/String` | — | Hardware used to create the file, in the format "CompanyName ProductName (ModelName) Version"; Technical |
| `Encoded_Hardware_CompanyName` | — | Name of the hardware company; Technical |
| `Encoded_Hardware_Name` | — | Name of the hardware; Technical |
| `Encoded_Hardware_Model` | — | Model of the hardware; Technical |
| `Encoded_Hardware_Version` | — | Version of the hardware; Technical |
| `Cropped` | — | Describes whether an image has been cropped and, if so, how it was cropped; Technical |
| `Dimensions` | — | Specifies the size of the original subject of the file (e.g. 8.5 in h, 11 in w); Technical |
| `DotsPerInch` | — | Stores dots per inch setting of the digitization mechanism used to produce the file; Technical |
| `Lightness` | — | Describes the changes in lightness settings on the digitization mechanism made during the production of the file; Technical |
| `OriginalSourceMedium` | — | Original medium of the material (e.g. vinyl, Audio-CD, Super8 or BetaMax); Technical |
| `OriginalSourceForm` | — | Original form of the material (e.g. slide, paper, map); Technical |
| `OriginalSourceForm/NumColors` | — | Number of colors requested when digitizing (e.g. 256 for images or 32 bit RGB for video); Technical |
| `OriginalSourceForm/Name` | — | Name of the product the file was originally intended for; Technical |
| `OriginalSourceForm/Cropped` | — | Describes whether the original image has been cropped and, if so, how it was cropped (e.g. 16:9 to 4:3, top and bottom); Technical |
| `OriginalSourceForm/Sharpness` | — | Identifies changes in sharpness the digitization mechanism made during the production of the file; Technical |
| `Tagged_Application` | — | Software used to tag the file; Technical |
| `BPM` | — | Average number of beats per minute; Technical |
| `ISRC` | — | International Standard Recording Code, excluding the ISRC prefix and including hyphens; Identifier |
| `ISBN` | — | International Standard Book Number; Identifier |
| `ISAN` | — | International Standard Audiovisual Number; Identifier |
| `BarCode` | — | EAN-13 (13-digit European Article Numbering) or UPC-A (12-digit Universal Product Code) bar code identifier; Identifier |
| `LCCN` | — | Library of Congress Control Number; Identifier |
| `UMID` | — | Universal Media Identifier |
| `CatalogNumber` | — | A label-specific catalogue number used to identify the release (e.g. TIC 01); Identifier |
| `LabelCode` | — | Label code (e.g. 12345, meaning LC-12345); Identifier |
| `Owner` | — | Owner of the file; Legal |
| `Copyright` | — | Copyright attribution; Legal |
| `Copyright/Url` | — | Link to a site with copyright/legal information; Legal |
| `Producer_Copyright` | — | Copyright information as per the production copyright holder; Legal |
| `TermsOfUse` | — | License information (e.g. All Rights Reserved, Any Use Permitted); Legal |
| `ServiceName` | — | Name of assisted service; Legal |
| `ServiceChannel` | — | Channel of assisted service; Legal |
| `Service/Url` | — | URL of of assisted service; Legal |
| `ServiceProvider` | — | Provider of assisted service; Legal |
| `ServiceProvider/Url` | — | URL of provider of assisted service; Legal |
| `ServiceType` | — | Type of assisted service; Legal |
| `NetworkName` | — | Television network name; Legal |
| `OriginalNetworkName` | — | Television network name of original broadcast; Legal |
| `Country` | — | Country information of the content; Legal |
| `TimeZone` | — | Time zone information of the content; Legal |
| `Cover` | — | Is there a cover? Result will be "Yes" if present, empty if not; Info |
| `Cover_Description` | — | Short description of cover image file (e.g. Earth in space); Info |
| `Cover_Type` | — | Cover type (e.g. "Cover (front)"); Info |
| `Cover_Mime` | — | MIME type of cover file (e.g. image/png); Info |
| `Cover_Data` | — | Cover, in binary format, encoded as Base64; Info |
| `Lyrics` | — | Text of a song; Info |
| `Comment` | — | Any comment related to the content; Personal |
| `Rating` | — | A numeric value defining how much a person likes the song/movie, 1 to 5 (e.g. 2, 5.0); Personal |
| `Added_Date` | — | Date/year the item was added to the owners collection; Personal |
| `Played_First_Date` | — | Date the owner first played an item; Personal |
| `Played_Last_Date` | — | Date the owner last played an item; Personal |
| `Played_Count` | — | Number of times an item was played; Personal |
| `EPG_Positions_Begin` | — | Beginning position for Electronic Program Guide |
| `EPG_Positions_End` | — | Ending position for Electronic Program Guide |

## Видео — Video

[Оригинальный каталог Video.csv](https://github.com/MediaArea/MediaInfoLib/blob/b47ab462f9df8a89e971af1ef8753d802c71de8a/Source/Resource/Text/Stream/Video.csv)

| Точное имя поля | Единица | Описание (оригинал) |
|---|---|---|
| `Count` | — | Count of objects available in this stream created by MediaInfo when analyzing file. This is mostly for internal use |
| `Status` | — | Status of bit field when parsing. Options are: 0=IsAccepted, 1=IsFilled, 2=IsUpdated, 3=IsFinished. This is mostly for internal use |
| `StreamCount` | — | Total number of streams available for this StreamKind. Counting starts at 1 |
| `StreamKind` | — | Name of stream type. Options are: Audio, General, Image, Menu, Other, Text, or Video |
| `StreamKind/String` | — | Name of stream type. Options are: Audio, General, Image, Menu, Other, Text, or Video |
| `StreamKindID` | — | Identification number for stream, assigned in order of parsing. Counting starts at 0 |
| `StreamKindPos` | — | Identification number for stream when multiple, assigned in order of parsing. Counting starts at 1 |
| `StreamOrder` | — | Stream order in the file for type of stream. Counting starts at 0 |
| `FirstPacketOrder` | — | Order of the first fully decodable packet parsed in the file for stream type. Counting starts at 0 |
| `Inform` | — | Last **Inform** call. This is mostly for internal use |
| `ID` | — | The identification number for this stream in this file |
| `ID/String` | — | The identification number for this stream in this file (String format) |
| `OriginalSourceMedium_ID` | — | Identification for this stream in the original medium of the material |
| `OriginalSourceMedium_ID/String` | — | Identification for this stream in the original medium of the material (String format) |
| `UniqueID` | — | The unique ID for this stream, should be copied with stream copy |
| `UniqueID/String` | — | The unique ID for this stream, should be copied with stream copy |
| `MenuID` | — | The menu ID for this stream in this file |
| `MenuID/String` | — | The menu ID for this stream in this file |
| `Format` | — | Format used |
| `Format/String` | — | Format used and any additional features or settings |
| `Format/Info` | — | More details about the identified Format |
| `Format/Url` | — | Link to a description of this format |
| `Format_Commercial` | — | Commercial name used by vendor for these settings or Format field if there is no difference |
| `Format_Commercial_IfAny` | — | Commercial name used by vendor for these settings, if available |
| `Format_Version` | — | Version for the identified format |
| `Format_Profile` | — | Profile of the Format |
| `Format_Level` | — | Level of the Format |
| `Format_Tier` | — | Tier of the Format |
| `Format_Compression` | — | Compression method used |
| `Format_AdditionalFeatures` | — | Features from the format that are required to fully support the file content |
| `MultiView_BaseProfile` | — | Profile of the base stream for Multiview Video Coding |
| `MultiView_Count` | — | View count for Multiview Video Coding |
| `MultiView_Layout` | — | How views are muxed in the container for Multiview Video Coding |
| `HDR_Format` | — | High Dynamic Range Format used |
| `HDR_Format/String` | — | HDR Format used, along with version, profile, level, layers, settings, and compatibility information |
| `HDR_Format_Commercial` | — | Commercial name used by vendor for these HDR settings or HDR Format field if there is no difference |
| `HDR_Format_Version` | — | Version of HDR Format |
| `HDR_Format_Profile` | — | Profile of HDR Format |
| `HDR_Format_Level` | — | Level of HDR Format |
| `HDR_Format_Settings` | — | HDR Format settings |
| `HDR_Format_Compression` | — | Compression format of HDR Format |
| `HDR_Format_Compatibility` | — | HDR Format compatibility with commercial products (e.g. HDR10) |
| `Format_Settings` | — | Settings used and required by decoder |
| `Format_Settings_BVOP` | Yes | Whether BVOP settings are required for decoding MPEG (Yes, No) |
| `Format_Settings_BVOP/String` | — | Whether BVOP settings are required for decoding MPEG (Yes, No) |
| `Format_Settings_QPel` | Yes | Whether Quarter-pixel motion settings are required for decoding MPEG (Yes, No) |
| `Format_Settings_QPel/String` | — | Whether Quarter-pixel motion settings are required for decoding MPEG (Yes, No) |
| `Format_Settings_GMC` | warppoint | Whether Global Motion Compensation settings are required for decoding MPEG (Yes, No) |
| `Format_Settings_GMC/String` | — | Whether Global Motion Compensation settings are required for decoding MPEG (Yes, No) |
| `Format_Settings_Matrix` | — | Whether Matrix settings are required for decoding MPEG (Yes, No) |
| `Format_Settings_Matrix/String` | — | Whether Matrix settings are required for decoding MPEG (Yes, No) |
| `Format_Settings_Matrix_Data` | — | Matrix data, in base64-encoded binary format. Order: intra, non-intra, gray intra, gray non-intra |
| `Format_Settings_CABAC` | Yes | Whether CABAC support is required for decoding MPEG (Yes, No) |
| `Format_Settings_CABAC/String` | — | Whether CABAC support is  required for decoding MPEG (Yes, No) |
| `Format_Settings_RefFrames` | frame | Whether reference frames settings are required for decoding AVC (Yes, No) |
| `Format_Settings_RefFrames/String` | — | Whether reference frames settings are required for decoding AVC (Yes, No) |
| `Format_Settings_Pulldown` | — | Pulldown method (for film transferred to video) |
| `Format_Settings_Endianness` | — | Order of bytes required for decoding (Big, Little) |
| `Format_Settings_Packing` | — | Data packing method used in DPX frames (e.g. Packed, Filled A, Filled B) |
| `Format_Settings_FrameMode` | — | Frame mode for decoding (e.g. Frame doubling, Frame tripling) |
| `Format_Settings_GOP` | — | GOP method set for format (e.g. N=1, Variable) |
| `Format_Settings_PictureStructure` | — | Picture structure method set for format (e.g. Frame, Field) |
| `Format_Settings_Wrapping` | — | Wrapping mode set for format (e.g. Frame, Clip) |
| `Format_Settings_SliceCount` | slice per frame | Numer of slices per frame |
| `Format_Settings_SliceCount/String` | — | Numer of slices per frame |
| `InternetMediaType` | — | Internet Media Type a.k.a. MIME Type, Content-Type |
| `MuxingMode` | — | How this file is muxed in the container (e.g. Muxed in Video #1) |
| `CodecID` | — | Codec identifier as indicated by the container |
| `CodecID/String` | — | Codec identifier, as indicated by the container |
| `CodecID/Info` | — | More information about this codec identifier |
| `CodecID/Hint` | — | Common alternative names for this codec identifier |
| `CodecID/Url` | — | A link to more details about this codec identifier |
| `CodecID_Description` | — | Codec description, as defined by the container |
| `Codec` | — | Deprecated, do not use in new projects |
| `Codec/String` | — | Deprecated, do not use in new projects |
| `Codec/Family` | — | Deprecated, do not use in new projects |
| `Codec/Info` | — | Deprecated, do not use in new projects |
| `Codec/Url` | — | Deprecated, do not use in new projects |
| `Codec/CC` | — | Deprecated, do not use in new projects |
| `Codec_Profile` | — | Deprecated, do not use in new projects |
| `Codec_Description` | — | Deprecated, do not use in new projects |
| `Codec_Settings` | — | Deprecated, do not use in new projects |
| `Codec_Settings_PacketBitStream` | — | Deprecated, do not use in new projects |
| `Codec_Settings_BVOP` | — | Deprecated, do not use in new projects |
| `Codec_Settings_QPel` | — | Deprecated, do not use in new projects |
| `Codec_Settings_GMC` | warppoint | Deprecated, do not use in new projects |
| `Codec_Settings_GMC/String` | — | Deprecated, do not use in new projects |
| `Codec_Settings_Matrix` | — | Deprecated, do not use in new projects |
| `Codec_Settings_Matrix_Data` | — | Deprecated, do not use in new projects |
| `Codec_Settings_CABAC` | — | Deprecated, do not use in new projects |
| `Codec_Settings_RefFrames` | — | Deprecated, do not use in new projects |
| `Duration` | ms | Play time of the stream, in s (ms for text output) |
| `Duration/String` | — | Play time in format XXx YYy, with YYy value omitted if zero (e.g. 1 h 40 min) |
| `Duration/String1` | — | Play time in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Duration/String2` | — | Play time in format XXx YYy, with YYy omitted if value is zero |
| `Duration/String3` | — | Play time in format HH:MM:SS.mmm (e.g. 01:40:00.000) |
| `Duration/String4` | — | Play time in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Duration/String5` | — | Play time in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Duration_FirstFrame` | ms | Duration of the first frame (if different than other frames), in ms |
| `Duration_FirstFrame/String` | — | Duration of the first frame (if different than other frames), in format XXx YYy, with YYy value omitted if zero |
| `Duration_FirstFrame/String1` | — | Duration of the first frame (if different than other frames), in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Duration_FirstFrame/String2` | — | Duration of the first frame (if different than other frames), in format XXx YYy, with YYy omitted if value is zero |
| `Duration_FirstFrame/String3` | — | Duration of the first frame (if different than other frames), in format HH:MM:SS.mmm |
| `Duration_FirstFrame/String4` | — | Duration of the first frame (if different than other frames), in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Duration_FirstFrame/String5` | — | Duration of the first frame (if different than other frames), in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Duration_LastFrame` | ms | Duration of the last frame (if different than other frames), in ms |
| `Duration_LastFrame/String` | — | Duration of the last frame (if different than other frames), in format XXx YYy, with YYy value omitted if zero |
| `Duration_LastFrame/String1` | — | Duration of the last frame (if different than other frames), in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Duration_LastFrame/String2` | — | Duration of the last frame (if different than other frames), in format XXx YYy, with YYy omitted if value is zero |
| `Duration_LastFrame/String3` | — | Duration of the last frame (if different than other frames), in format HH:MM:SS.mmm |
| `Duration_LastFrame/String4` | — | Duration of the last frame (if different than other frames), in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Duration_LastFrame/String5` | — | Duration of the last frame (if different than other frames), in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Source_Duration` | ms | Duration of the file, of content stored in the file, in ms |
| `Source_Duration/String` | — | Duration of the file, of content stored in the file, in format XXx YYy, with YYy value omitted if zero |
| `Source_Duration/String1` | — | Duration of the file, of content stored in the file, in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Source_Duration/String2` | — | Duration of the file, of content stored in the file, in format XXx YYy, with YYy omitted if value is zero |
| `Source_Duration/String3` | — | Duration of the file, of content stored in the file, in format HH:MM:SS.mmm |
| `Source_Duration/String4` | — | Duration of the file, of content stored in the file, in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Source_Duration/String5` | — | Duration of the file, of content stored in the file, in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Source_Duration_FirstFrame` | ms | Duration of the first frame, of content stored in the file, in ms |
| `Source_Duration_FirstFrame/String` | — | Duration of the first frame, of content stored in the file, in format XXx YYy, with YYy value omitted if zero |
| `Source_Duration_FirstFrame/String1` | — | Duration of the first frame, of content stored in the file, in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Source_Duration_FirstFrame/String2` | — | Duration of the first frame, of content stored in the file, in format XXx YYy, with YYy omitted if value is zero |
| `Source_Duration_FirstFrame/String3` | — | Duration of the first frame, of content stored in the file, in format HH:MM:SS.mmm |
| `Source_Duration_FirstFrame/String4` | — | Duration of the first frame, of content stored in the file, in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Source_Duration_FirstFrame/String5` | — | Duration of the first frame, of content stored in the file, in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Source_Duration_LastFrame` | ms | Duration of the last frame, of content stored in the file, in ms |
| `Source_Duration_LastFrame/String` | — | Duration of the last frame, of content stored in the file, in format XXx YYy, with YYy value omitted if zero |
| `Source_Duration_LastFrame/String1` | — | Duration of the last frame, of content stored in the file, in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Source_Duration_LastFrame/String2` | — | Duration of the last frame, of content stored in the file, in format XXx YYy, with YYy omitted if value is zero |
| `Source_Duration_LastFrame/String3` | — | Duration of the last frame, of content stored in the file, in format HH:MM:SS.mmm |
| `Source_Duration_LastFrame/String4` | — | Duration of the last frame, of content stored in the file, in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Source_Duration_LastFrame/String5` | — | Duration of the last frame, of content stored in the file, in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `BitRate_Mode` | — | Bit rate mode of this stream (CBR, VBR) |
| `BitRate_Mode/String` | — | Bit rate mode of this stream, as word (Variable, Constant) |
| `BitRate` | bps | Bit rate of this stream, in bps |
| `BitRate/String` | — | Bit rate of this stream, with measurement (e.g. 128 kb/s) |
| `BitRate_Minimum` | bps | Minimum bit rate of this stream, in bps |
| `BitRate_Minimum/String` | — | Minimum bit rate of this stream, with measurement |
| `BitRate_Nominal` | bps | Nominal bit rate of this stream, in bps |
| `BitRate_Nominal/String` | — | Nominal bit rate of this stream, with measurement |
| `BitRate_Maximum` | bps | Maximum bit rate of this stream, in bps |
| `BitRate_Maximum/String` | — | Maximum bit rate of this stream, with measurement |
| `BitRate_Encoded` | bps | Encoded bit rate (with forced padding), if container padding is present, in bps |
| `BitRate_Encoded/String` | — | Encoded bit rate (with forced padding), if container padding is present, in bps |
| `Width` | pixel | Width of frame (trimmed to clean aperture size if present) in pixels, as integer (e.g. 1920) |
| `Width/String` | — | Width of frame (trimmed to clean aperture size if present) in pixels, presented in SI unit digit spacing style, with measurement (e.g. 1 920 pixels) |
| `Width_Offset` | pixel | Offset between original width and displayed width, in pixels |
| `Width_Offset/String` | — | Offset between original width and displayed width, in pixels |
| `Width_Original` | pixel | Width of frame (not including aperture size if present) in pixels, presented as integer (e.g. 1920) |
| `Width_Original/String` | — | Width of frame (not including aperture size if present) in pixels, present in SI unit digit spacing style, with measurement (e.g. 1 920 pixels) |
| `Width_CleanAperture` | pixel | Width of frame (trimmed to clean aperture size if present) in pixels, presented as integer (e.g. 1920) |
| `Width_CleanAperture/String` | — | Width of frame (trimmed to clean aperture size if present) in pixels, present in SI unit digit spacing style, with measurement (e.g. 1 920 pixels) |
| `Height` | pixel | Height of frame (including aperture size if present) in pixels, presented as integer (e.g. 1080) |
| `Height/String` | — | Height of frame (including aperture size if present) in pixels, present in SI unit digit spacing style, with measurement (e.g. 1 080 pixels) |
| `Height_Offset` | pixel | Offset between original height and displayed height, in pixels |
| `Height_Offset/String` | — | Offset between original height and displayed height, in pixels |
| `Height_Original` | pixel | Height of frame (not including aperture size if present) in pixels, presented as integer (e.g. 1080) |
| `Height_Original/String` | — | Height of frame (not including aperture size if present) in pixels, present in SI unit digit spacing style, with measurement (e.g. 1 080 pixels) |
| `Height_CleanAperture` | pixel | Height of frame (trimmed to clean aperture size if present) in pixels, presented as integer (e.g. 1080) |
| `Height_CleanAperture/String` | — | Height of frame (trimmed to clean aperture size if present) in pixels, present in SI unit digit spacing style, with measurement (e.g. 1 800 pixels) |
| `Stored_Width` | — | Width of frame, considering data stored in the codec |
| `Stored_Height` | — | Height of frame, considering data stored in the codec |
| `Sampled_Width` | — | Width of frame, from data derived from video stream |
| `Sampled_Height` | — | Height of frame, from data derived from video stream |
| `PixelAspectRatio` | — | Width of a pixel as compared to the height (e.g. 1.422) |
| `PixelAspectRatio/String` | — | Width of a pixel as compared to the height (e.g. 1.422) |
| `PixelAspectRatio_Original` | — | Width of a pixel as compared to the height (e.g. 1.422). This field is only shown if the container and codec values are different |
| `PixelAspectRatio_Original/String` | — | Width of a pixel as compared to the height (e.g. 1.422). This field is only shown if the container and codec values are different |
| `PixelAspectRatio_CleanAperture` | — | Width of a pixel as compared to the height, considering clean aperture dimensions (e.g. 1.422). This field is only shown if the values are different |
| `PixelAspectRatio_CleanAperture/String` | — | Width of a pixel as compared to the height, considering clean aperture dimensions (e.g. 1.422). This field is only shown if the values are different |
| `DisplayAspectRatio` | — | The proportional relationship between the width and height of a frame (e.g. 4:3) |
| `DisplayAspectRatio/String` | — | The proportional relationship between the width and height of a frame (e.g. 4:3) |
| `DisplayAspectRatio_Original` | — | The proportional relationship between the width and height of a frame (e.g. 4:3) |
| `DisplayAspectRatio_Original/String` | — | The proportional relationship between the width and height of a frame (e.g. 4:3) |
| `DisplayAspectRatio_CleanAperture` | — | The proportional relationship between the width and height of a frame, considering clean aperture dimensions (e.g. 4:3) |
| `DisplayAspectRatio_CleanAperture/String` | — | The proportional relationship between the width and height of a frame, considering clean aperture dimensions (e.g. 4:3) |
| `ActiveFormatDescription` | — | Active Format Description, as value code (e.g. 001) |
| `ActiveFormatDescription/String` | — | Active Format Description, as text (e.g. 4:3) |
| `ActiveFormatDescription_MuxingMode` | — | Muxing mode used for Active Format Description (AFD value). Options are A/53 (for Raw) or SMPTE ST 2016-3 (for Ancillary) |
| `Active_Width` | pixel | Width of frame, not considering black bars |
| `Active_Width/String` | — | Width of frame, not considering black bars, in pixels, presented in SI unit digit spacing style, with measurement (e.g. 1 920 pixels) |
| `Active_Height` | pixel | Height of frame, not considering black bars |
| `Active_Height/String` | — | Height of frame, not considering black bars, in pixels, presented in SI unit digit spacing style, with measurement (e.g. 1 080 pixels) |
| `Active_DisplayAspectRatio` | — | The proportional relationship between the active width and active height of a frame (e.g. 4:3) |
| `Active_DisplayAspectRatio/String` | — | The proportional relationship between the active width and active height of a frame (e.g. 4:3) |
| `Rotation` | — | Rotation of video, derived from track header data, in degrees |
| `Rotation/String` | — | Rotation of video, derived from track header data, in degrees |
| `FrameRate_Mode` | — | Frame rate mode, as acronym (e.g. CFR, VFR) |
| `FrameRate_Mode/String` | — | Frame rate mode, as word (e.g. Constant, Variable) |
| `FrameRate_Mode_Original` | — | Original frame rate mode, as acronym (e.g. CFR, VFR) |
| `FrameRate_Mode_Original/String` | — | Original frame rate mode, as word (Constant, Variable) |
| `FrameRate` | fps | Frames per second, as float (e.g. 29.970) |
| `FrameRate/String` | — | Frames per second, with measurement (e.g. 29.970 (29970/1000) FPS) |
| `FrameRate_Num` | — | Numerator for determined frames per second (e.g. 29970) |
| `FrameRate_Den` | — | Denominator for determined frames per second (e.g. 1000) |
| `FrameRate_Minimum` | fps | Minimum frames per second (e.g. 25.000) |
| `FrameRate_Minimum/String` | — | Minimum frames per second, with measurement (e.g. 25.000 FPS) |
| `FrameRate_Nominal` | fps | Frames per second rounded to closest standard (e.g. 24.98) |
| `FrameRate_Nominal/String` | — | Frames per second rounded to closest standard, with measurement (e.g. 29.97 fps) |
| `FrameRate_Maximum` | fps | Maximum frames per second |
| `FrameRate_Maximum/String` | — | Maximum frames per second, with measurement |
| `FrameRate_Original` | fps | Frames per second |
| `FrameRate_Original/String` | — | Frames per second, with measurement |
| `FrameRate_Original_Num` | — | Numerator for determined frames per second (e.g. 29970) |
| `FrameRate_Original_Den` | — | Denominator for determined frames per second (e.g. 1000) |
| `FrameRate_Real` | fps | Real (capture) frames per second |
| `FrameRate_Real/String` | — | Real (capture) frames per second (with measurement) |
| `FrameCount` | — | Numer of frames |
| `Source_FrameCount` | — | Number of frames according to media header (media/stts atom) data |
| `Standard` | — | Either the NTSC or PAL color encoding system, as stored in the content |
| `Resolution` | bit | Deprecated, do not use in new projects |
| `Resolution/String` | — | Deprecated, do not use in new projects |
| `Colorimetry` | — | Deprecated, do not use in new projects |
| `ColorSpace` | — | Color profile of the image (e.g. YUV) |
| `ChromaSubsampling` | — | Ratio of chroma to luma in encoded image (e.g. 4:2:2) |
| `ChromaSubsampling/String` | — | Ratio of chroma to luma in encoded image (e.g. 4:2:2) |
| `ChromaSubsampling_Position` | — | Position type of chroma subsampling |
| `BitDepth` | bit | Color information stored in the video frames, as integer (e.g. 10) |
| `BitDepth/String` | — | Color information stored in the video frames, as string (e.g. 10 bits) |
| `ScanType` | — | Way in which lines of video are displayed (e.g. Progressive) |
| `ScanType/String` | — | Way in which lines of video are displayed (e.g. Progressive) |
| `ScanType_Original` | — | Way in which lines of video are encoded (e.g. Progressive) |
| `ScanType_Original/String` | — | Way in which lines of video are encoded (e.g. Progressive) |
| `ScanType_StoreMethod` | — | Whether the video's ScanType is stored with fields separated or interleaved |
| `ScanType_StoreMethod_FieldsPerBlock` | — | Count of fields per container block |
| `ScanType_StoreMethod/String` | — | Whether the video's ScanType is stored with fields separated or interleaved |
| `ScanOrder` | — | Order in which lines are encoded, as acronym (e.g. TFF) |
| `ScanOrder/String` | — | Order in which lines are encoded, as acronym (e.g. TFF) |
| `ScanOrder_Stored` | — | Stored ScanOrder, displayed when the stored order is not same as the display order |
| `ScanOrder_Stored/String` | — | Stored ScanOrder, displayed when the stored order is not same as the display order |
| `ScanOrder_StoredDisplayedInverted` | — | Field is set to Yes when display and stored orders are inverted |
| `ScanOrder_Original` | — | Whether the video's ScanType is stored with fields separated or interleaved |
| `ScanOrder_Original/String` | — | Whether the video's ScanType is stored with fields separated or interleaved |
| `Interlacement` | — | Deprecated, do not use in new projects |
| `Interlacement/String` | — | Deprecated, do not use in new projects |
| `Compression_Mode` | — | Compression mode (Lossy, Lossless) |
| `Compression_Mode/String` | — | Compression mode (Lossy, Lossless) |
| `Compression_Ratio` | — | Stream size divided by uncompressed stream size |
| `Bits-(Pixel*Frame)` | — | Bits divided by Pixel multiplied by Frame |
| `Delay` | ms | Delay fixed in the stream (relative), in ms |
| `Delay/String` | — | Delay fixed in the stream (relative), with measurement |
| `Delay/String1` | — | Delay fixed in the stream (relative), with measurement |
| `Delay/String2` | — | Delay fixed in the stream (relative), with measurement |
| `Delay/String3` | — | Delay fixed in the stream (relative) in format HH:MM:SS.mmm, with measurement |
| `Delay/String4` | — | Delay in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Delay/String5` | — | Delay in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Delay_Settings` | — | Delay settings (e.g. in case of timecode) |
| `Delay_DropFrame` | — | Delay drop frame information |
| `Delay_Source` | — | Source location of the Delay (e.g. Container, Stream, empty) |
| `Delay_Source/String` | — | Source location of the Delay (e.g. Container, Stream, empty) |
| `Delay_Original` | ms | Delay, in ms |
| `Delay_Original/String` | — | Delay, with measurement |
| `Delay_Original/String1` | — | Delay, with measurement |
| `Delay_Original/String2` | — | Delay, with measurement |
| `Delay_Original/String3` | — | Delay, in format HH:MM:SS.mmm |
| `Delay_Original/String4` | — | Delay, in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Delay_Original/String5` | — | Delay, in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Delay_Original_Settings` | — | Delay settings (e.g. in case of timecode) |
| `Delay_Original_DropFrame` | — | Delay drop frame information |
| `Delay_Original_Source` | — | Delay source (e.g. Container, Stream, empty) |
| `TimeStamp_FirstFrame` | ms | Timestamp fixed in the stream (relative), in ms |
| `TimeStamp_FirstFrame/String` | — | Timestamp fixed in the stream (relative), with measurement |
| `TimeStamp_FirstFrame/String1` | — | Timestamp fixed in the stream (relative), with measurement |
| `TimeStamp_FirstFrame/String2` | — | Timestamp fixed in the stream (relative), with measurement |
| `TimeStamp_FirstFrame/String3` | — | Timestamp in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `TimeStamp_FirstFrame/String4` | — | Timestamp in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `TimeStamp_FirstFrame/String5` | — | Timestamp in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `TimeCode_FirstFrame` | — | Time code for first frame in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `TimeCode_LastFrame` | — | Time code for last frame (excluding the duration of the last frame) in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `TimeCode_DropFrame` | — | Time code drop frame |
| `TimeCode_Settings` | — | Additional time code settings |
| `TimeCode_Source` | — | Time code source (Container, Stream, SystemScheme1, SDTI, ANC, etc.) |
| `Gop_OpenClosed` |  | Time code information about Open/Closed GOP |
| `Gop_OpenClosed/String` | — | Time code information about Open/Closed GOP |
| `Gop_OpenClosed_FirstFrame` |  | Time code information about Open/Closed of first frame if GOP is Open for the other GOPs |
| `Gop_OpenClosed_FirstFrame/String` | — | Time code information about Open/Closed of first frame if GOP is Open for the other GOPs |
| `StreamSize` | byte | Size of this stream, in bytes |
| `StreamSize/String` | — | Size of this stream, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `StreamSize/String1` | — | Size of this stream, with measurement (measured in powers of 1024) rounded to the nearest integer (e.g. 11 MiB) |
| `StreamSize/String2` | — | Size of this stream, measurement (measured in powers of 1024) rounded to the two most significant digits (e.g. 11 MiB) |
| `StreamSize/String3` | — | Size of this stream, measurement (measured in powers of 1024) rounded to the three most significant digits (e.g. 10.5 MiB) |
| `StreamSize/String4` | — | Size of this stream, measurement (measured in powers of 1024) rounded to the four most significant digits (e.g. 10.50 MiB) |
| `StreamSize/String5` | — | Size of this stream, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `StreamSize_Proportion` | — | Size of this stream divided by total file size |
| `StreamSize_Demuxed` | byte | Size of this stream after demuxing, in bytes |
| `StreamSize_Demuxed/String` | — | Size of this stream after demuxing, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `StreamSize_Demuxed/String1` | — | Size of this stream after demuxing, with measurement (measured in powers of 1024) rounded to the nearest integer (e.g. 11 MiB) |
| `StreamSize_Demuxed/String2` | — | Size of this stream after demuxing, measurement (measured in powers of 1024) rounded to the two most significant digits (e.g. 11 MiB) |
| `StreamSize_Demuxed/String3` | — | Size of this stream after demuxing, measurement (measured in powers of 1024) rounded to the three most significant digits (e.g. 10.5 MiB) |
| `StreamSize_Demuxed/String4` | — | Size of this stream after demuxing, measurement (measured in powers of 1024) rounded to the four most significant digits (e.g. 10.50 MiB) |
| `StreamSize_Demuxed/String5` | — | Size of this stream after demuxing, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `Source_StreamSize` | byte | Size of content stored in the file, in bytes |
| `Source_StreamSize/String` | — | Size of content stored in the file, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `Source_StreamSize/String1` | — | Size of content stored in the file, with measurement (measured in powers of 1024) rounded to the nearest integer (e.g. 11 MiB) |
| `Source_StreamSize/String2` | — | Size of content stored in the file, measurement (measured in powers of 1024) rounded to the two most significant digits (e.g. 11 MiB) |
| `Source_StreamSize/String3` | — | Size of content stored in the file, measurement (measured in powers of 1024) rounded to the three most significant digits (e.g. 10.5 MiB) |
| `Source_StreamSize/String4` | — | Size of content stored in the file, measurement (measured in powers of 1024) rounded to the four most significant digits (e.g. 10.50 MiB) |
| `Source_StreamSize/String5` | — | Size of content stored in the file, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `Source_StreamSize_Proportion` | — | Size of this stream divided by total file size |
| `StreamSize_Encoded` | byte | Size of this stream when encoded, in bytes |
| `StreamSize_Encoded/String` | — | Size of this stream when encoded, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `StreamSize_Encoded/String1` | — | Size of this stream when encoded, with measurement (measured in powers of 1024) rounded to the nearest integer (e.g. 11 MiB) |
| `StreamSize_Encoded/String2` | — | Size of this stream when encoded, measurement (measured in powers of 1024) rounded to the two most significant digits (e.g. 11 MiB) |
| `StreamSize_Encoded/String3` | — | Size of this stream when encoded, measurement (measured in powers of 1024) rounded to the three most significant digits (e.g. 10.5 MiB) |
| `StreamSize_Encoded/String4` | — | Size of this stream when encoded, measurement (measured in powers of 1024) rounded to the four most significant digits (e.g. 10.50 MiB) |
| `StreamSize_Encoded/String5` | — | Size of this stream when encoded, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `StreamSize_Encoded_Proportion` | — | Encoded Stream size divided by file size |
| `Source_StreamSize_Encoded` | byte | Size of content stored in the file when encoded, in bytes |
| `Source_StreamSize_Encoded/String` | — | Size of content stored in the file when encoded, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `Source_StreamSize_Encoded/String1` | — | Size of content stored in the file when encoded, with measurement (measured in powers of 1024) rounded to the nearest integer (e.g. 11 MiB) |
| `Source_StreamSize_Encoded/String2` | — | Size of content stored in the file when encoded, measurement (measured in powers of 1024) rounded to the two most significant digits (e.g. 11 MiB) |
| `Source_StreamSize_Encoded/String3` | — | Size of content stored in the file when encoded, measurement (measured in powers of 1024) rounded to the three most significant digits (e.g. 10.5 MiB) |
| `Source_StreamSize_Encoded/String4` | — | Size of content stored in the file when encoded, measurement (measured in powers of 1024) rounded to the four most significant digits (e.g. 10.50 MiB) |
| `Source_StreamSize_Encoded/String5` | — | Size of content stored in the file when encoded, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `Source_StreamSize_Encoded_Proportion` | — | Source Encoded Stream size divided by file size |
| `Alignment` | — | How this stream is aligned in the container (e.g. Aligned, Split) |
| `Alignment/String` | — | How this stream is aligned in the container (e.g. Aligned, Split) |
| `Title` | — | Title of track |
| `Encoded_Application` | — | Name of the software package used to create the file (e.g. Microsoft WaveEdiTY); Technical |
| `Encoded_Application/String` | — | Name of the software package used to create the file, in the format "CompanyName ProductName (OperatingSystem) Version (Date)"; Technical |
| `Encoded_Application_CompanyName` | — | Name of the company of the encoding application; Technical |
| `Encoded_Application_Name` | — | Name of the encoding product; Technical |
| `Encoded_Application_Version` | — | Version of the encoding product; Technical |
| `Encoded_Application_Url` | — | URL associated with the encoding software; Technical |
| `Encoded_Library` | — | Software used to create the file; Technical |
| `Encoded_Library/String` | — | Software used to create the file, in the format "CompanyName ProductName (OperatingSystem) Version (Date)"; Technical |
| `Encoded_Library_CompanyName` | — | Name of the encoding software company; Technical |
| `Encoded_Library_Name` | — | Name of the encoding software; Technical |
| `Encoded_Library_Version` | — | Version of the encoding software; Technical |
| `Encoded_Library_Date` | — | Release date of the encoding software, in UTC; Technical |
| `Encoded_Library_Settings` | — | Parameters used by the encoding software; Technical |
| `Encoded_OperatingSystem` | — | Operating System of the encoding software; Technical |
| `Language` | — | Language, formatted as 2-letter ISO 639-1 if exists, else 3-letter ISO 639-2, and with optional ISO 3166-1 country separated by a dash if available (e.g. en, en-US, en-CN) |
| `Language/String` | — | Language, as full name (e.g. English) |
| `Language/String1` | — | Language, as full name (e.g. English) |
| `Language/String2` | — | Language, formatted as 2-letter ISO 639-1, if exists (e.g. en) |
| `Language/String3` | — | Language, formatted as 3-letter ISO 639-2, if exists (e.g. eng) |
| `Language/String4` | — | Language, formatted as 2-letter ISO 639-1, if exists, with optional ISO 3166-1 country separated by a dash if available (e.g. en-US) |
| `Language_More` | — | More information about Language (e.g. Director's Comment) |
| `ServiceKind` | — | Type of assisted service (e.g. visually impaired, commentary, voice over) |
| `ServiceKind/String` | — | Type of assisted service (e.g. visually impaired, commentary, voice over) |
| `Disabled` | Yes | Set if this stream should not be used (Yes, No) |
| `Disabled/String` | — | Set if this stream should not be used (Yes, No) |
| `Default` | Yes | Flag set if this stream should be used if no language found matches the user preference (Yes, No) |
| `Default/String` | — | Flag set if this stream should be used if no language found matches the user preference (Yes, No) |
| `Forced` | Yes | Flag set if this stream should be used regardless of user preferences, often used for sparse subtitle dialogue in an otherwise unsubtitled movie (Yes, No) |
| `Forced/String` | — | Flag set if this stream should be used regardless of user preferences, often used for sparse subtitle dialogue in an otherwise unsubtitled movie (Yes, No) |
| `AlternateGroup` | Yes | Number of a group in order to provide versions of the same track |
| `AlternateGroup/String` | — | Number of a group in order to provide versions of the same track |
| `Encoded_Date` | — | Time that the encoding of this item was completed, in UTC; Temporal |
| `Tagged_Date` | — | Time that the tags were added to this item, in UTC; Temporal |
| `Encryption` | — | Whether this stream is encrypted and, if available, how it is encrypted |
| `BufferSize` | — | The minimum size of the buffer needed to decode the sequence |
| `colour_description_present` | — | Presence of color description (Yes, No) |
| `colour_description_present_Source` | — | Presence of colour description (source) |
| `colour_description_present_Original` | — | Presence of colour description (if incoherencies) |
| `colour_description_present_Original_Source` | — | Presence of colour description (source if incoherencies) |
| `colour_range` | — | Color range for YUV color space |
| `colour_range_Source` | — | Colour range for YUV colour space (source) |
| `colour_range_Original` | — | Colour range for YUV colour space (if incoherencies) |
| `colour_range_Original_Source` | — | Colour range for YUV colour space (source if incoherencies) |
| `colour_primaries` | — | Chromaticity coordinates of the source primaries |
| `colour_primaries_Source` | — | Chromaticity coordinates of the source primaries (source) |
| `colour_primaries_Original` | — | Chromaticity coordinates of the source primaries (if incoherencies) |
| `colour_primaries_Original_Source` | — | Chromaticity coordinates of the source primaries (source if incoherencies) |
| `transfer_characteristics` | — | Opto-electronic transfer characteristic of the source picture |
| `transfer_characteristics_Source` | — | Opto-electronic transfer characteristic of the source picture (source) |
| `transfer_characteristics_Original` | — | Opto-electronic transfer characteristic of the source picture (if incoherencies) |
| `transfer_characteristics_Original_Source` | — | Opto-electronic transfer characteristic of the source picture (source if incoherencies) |
| `matrix_coefficients` | — | Matrix coefficients used in deriving luma and chroma signals from the green, blue, and red primaries |
| `matrix_coefficients_Source` | — | Matrix coefficients used in deriving luma and chroma signals from the green, blue, and red primaries (source) |
| `matrix_coefficients_Original` | — | Matrix coefficients used in deriving luma and chroma signals from the green, blue, and red primaries (if incoherencies) |
| `matrix_coefficients_Original_Source` | — | Matrix coefficients used in deriving luma and chroma signals from the green, blue, and red primaries (source if incoherencies) |
| `MasteringDisplay_ColorPrimaries` | — | Chromaticity coordinates of the source primaries of the mastering display |
| `MasteringDisplay_ColorPrimaries_Source` | — | Chromaticity coordinates of the source primaries of the mastering display (source) |
| `MasteringDisplay_ColorPrimaries_Original` | — | Chromaticity coordinates of the source primaries of the mastering display (if incoherencies) |
| `MasteringDisplay_ColorPrimaries_Original_Source` | — | Chromaticity coordinates of the source primaries of the mastering display (source if incoherencies) |
| `MasteringDisplay_Luminance` | — | Luminance of the mastering display |
| `MasteringDisplay_Luminance_Source` | — | Luminance of the mastering display (source) |
| `MasteringDisplay_Luminance_Min` | — | Min luminance of the mastering display |
| `MasteringDisplay_Luminance_Max` | — | Max luminance of the mastering display |
| `MasteringDisplay_Luminance_Original` | — | Luminance of the mastering display (if incoherencies) |
| `MasteringDisplay_Luminance_Original_Source` | — | Luminance of the mastering display (source if incoherencies) |
| `MasteringDisplay_Luminance_Original_Min` | — | Min luminance of the mastering display (source if incoherencies) |
| `MasteringDisplay_Luminance_Original_Max` | — | Max luminance of the mastering display (source if incoherencies) |
| `MaxCLL` | cd/m2 | Maximum content light level |
| `MaxCLL/String` | — | Maximum content light level, with measurement |
| `MaxCLL_Source` | — | Maximum content light level (source) |
| `MaxCLL_Original` | cd/m2 | Maximum content light level (if incoherencies), with measurement |
| `MaxCLL_Original/String` | — | Maximum content light level (if incoherencies) |
| `MaxCLL_Original_Source` | — | Maximum content light level (source if incoherencies) |
| `MaxFALL` | cd/m2 | Maximum frame average light level |
| `MaxFALL/String` | — | Maximum frame average light level |
| `MaxFALL_Source` | — | Maximum frame average light level (source), with measurement |
| `MaxFALL_Original` | cd/m2 | Maximum frame average light level (if incoherencies) |
| `MaxFALL_Original/String` | — | Maximum frame average light level (if incoherencies), with measurement |
| `MaxFALL_Original_Source` | — | Maximum frame average light level (source if incoherencies) |

## Аудио — Audio

[Оригинальный каталог Audio.csv](https://github.com/MediaArea/MediaInfoLib/blob/b47ab462f9df8a89e971af1ef8753d802c71de8a/Source/Resource/Text/Stream/Audio.csv)

| Точное имя поля | Единица | Описание (оригинал) |
|---|---|---|
| `Count` | — | Count of objects available in this stream created by MediaInfo when analyzing file. This is mostly for internal use |
| `Status` | — | Status of bit field when parsing. Options are: 0=IsAccepted, 1=IsFilled, 2=IsUpdated, 3=IsFinished. This is mostly for internal use |
| `StreamCount` | — | Total number of streams available for this StreamKind. Counting starts at 1 |
| `StreamKind` | — | Name of stream type. Options are: General, Video, Audio, Text, Image, Menu, or Other |
| `StreamKind/String` | — | Name of stream type. Options are: General, Video, Audio, Text, Image, Menu, or Other |
| `StreamKindID` | — | Identification number for stream, assigned in order of parsing. Counting starts at 0 |
| `StreamKindPos` | — | Identification number for stream when multiple, assigned in order of parsing. Counting starts at 1 |
| `StreamOrder` | — | Stream order in the file for type of stream. Counting starts at 0 |
| `FirstPacketOrder` | — | Order of the first fully decodable packet parsed in the file for stream type. Counting starts at 0 |
| `Inform` | — | Last **Inform** call. This is mostly for internal use |
| `ID` | — | The identification number for this stream in this file |
| `ID/String` | — | The identification number for this stream in this file (String format) |
| `OriginalSourceMedium_ID` | — | Identification for this stream in the original medium of the material |
| `OriginalSourceMedium_ID/String` | — | Identification for this stream in the original medium of the material (String format) |
| `UniqueID` | — | The unique ID for this stream, should be copied with stream copy |
| `UniqueID/String` | — | The unique ID for this stream, should be copied with stream copy |
| `MenuID` | — | The menu ID for this stream in this file |
| `MenuID/String` | — | The menu ID for this stream in this file |
| `Format` | — | Format used |
| `Format/String` | — | Format used and any additional features or settings |
| `Format/Info` | — | More details about the identified Format |
| `Format/Url` | — | Link to a description of this format |
| `Format_Commercial` | — | Commercial name used by vendor for these settings or Format field if there is no difference |
| `Format_Commercial_IfAny` | — | Commercial name used by vendor for these settings, if available |
| `Format_Version` | — | Version for the identified format |
| `Format_Profile` | — | Profile of the Format |
| `Format_Level` | — | Level of the Format |
| `Format_Compression` | — | Compression method used |
| `Format_Settings` | — | Settings used and required by decoder |
| `Format_Settings_SBR` | Yes | Whether Spectral band replication settings used in encoding. Options are Yes (NBC)/No (Explicit). Note: NBC stands for "Not Backwards Compatable" |
| `Format_Settings_SBR/String` | — | Whether Spectral band replication settings used in encoding. Options are Yes (NBC)/No (Explicit). Note: NBC stands for "Not Backwards Compatable" |
| `Format_Settings_PS` | Yes | Whether Parametric Stereo settings used in encoding. Options are Yes (NBC)/No (Explicit). Note: NBC stands for "Not Backwards Compatable" |
| `Format_Settings_PS/String` | — | Whether Parametric Stereo settings used in encoding. Options are Yes (NBC)/No (Explicit). Note: NBC stands for "Not Backwards Compatable" |
| `Format_Settings_Mode` | — | Profile for format settings used in encoding (e.g. Joint Stereo) |
| `Format_Settings_ModeExtension` | — | Extended format settings profile for Joint Stereo, derived from header data (e.g. Intensity Stereo + MS Stereo) |
| `Format_Settings_Emphasis` | — | Emphasis format settings for MPEG audio, derived from header data (e.g. 50/15ms) |
| `Format_Settings_Floor` | — | Settings for Vorbis spectral "floor" (a low-resolution representation of the audio spectrum for the given channel in the current frame) vector (e.g. Floor0) |
| `Format_Settings_Firm` | — | Agency or company responsible for format settings used in encoding (e.g. Microsoft) |
| `Format_Settings_Endianness` | — | Order of bytes required for decoding. Options are Big/Little |
| `Format_Settings_Sign` | — | How numbers are stored in stream's encoding. Options are Signed/Unsigned |
| `Format_Settings_Law` | — | U-law or A-law |
| `Format_Settings_ITU` | — | ITU Telecommunication Standardization Sector compression standard used in encoding (e.g. G.726) |
| `Format_Settings_Wrapping` | — | Wrapping mode set for format (e.g. Frame, Clip) |
| `Format_AdditionalFeatures` | — | Features from the format that are required to fully support the file content |
| `Matrix_Format` | — | Matrix format used in encoding (e.g. DTS Neural Audio) |
| `InternetMediaType` | — | Internet Media Type (aka MIME Type, Content-Type) |
| `MuxingMode` | — | How this file is muxed in the container (e.g. Muxed in Video #1) |
| `MuxingMode_MoreInfo` | — | More information about MuxingMode |
| `CodecID` | — | Codec identifier as indicated by the container |
| `CodecID/String` | — | Codec identifier, as indicated by the container |
| `CodecID/Info` | — | More information about this codec identifier |
| `CodecID/Hint` | — | Common alternative names for this codec identifier |
| `CodecID/Url` | — | A link to more details about this codec identifier |
| `CodecID_Description` | — | Codec description indicated by the container |
| `Codec` | — | Deprecated, do not use in new projects |
| `Codec/String` | — | Deprecated, do not use in new projects |
| `Codec/Family` | — | Deprecated, do not use in new projects |
| `Codec/Info` | — | Deprecated, do not use in new projects |
| `Codec/Url` | — | Deprecated, do not use in new projects |
| `Codec/CC` | — | Deprecated, do not use in new projects |
| `Codec_Description` | — | Deprecated, do not use in new projects |
| `Codec_Profile` | — | Deprecated, do not use in new projects |
| `Codec_Settings` | — | Deprecated, do not use in new projects |
| `Codec_Settings_Automatic` | — | Deprecated, do not use in new projects |
| `Codec_Settings_Floor` | — | Deprecated, do not use in new projects |
| `Codec_Settings_Firm` | — | Deprecated, do not use in new projects |
| `Codec_Settings_Endianness` | — | Deprecated, do not use in new projects |
| `Codec_Settings_Sign` | — | Deprecated, do not use in new projects |
| `Codec_Settings_Law` | — | Deprecated, do not use in new projects |
| `Codec_Settings_ITU` | — | Deprecated, do not use in new projects |
| `Duration` | ms | Play time of the stream, in s (ms for text output) |
| `Duration/String` | — | Play time in format XXx YYy, with YYy value omitted if zero (e.g. 1 h 40 min) |
| `Duration/String1` | — | Play time in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Duration/String2` | — | Play time in format XXx YYy, with YYy omitted if value is zero |
| `Duration/String3` | — | Play time in format HH:MM:SS.mmm (e.g. 01:40:00.000) |
| `Duration/String4` | — | Play time in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Duration/String5` | — | Play time in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Duration_FirstFrame` | ms | Duration of the first frame (if different than other frames), in ms |
| `Duration_FirstFrame/String` | — | Duration of the first frame (if different than other frames), in format XXx YYy, with YYy value omitted if zero |
| `Duration_FirstFrame/String1` | — | Duration of the first frame (if different than other frames), in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Duration_FirstFrame/String2` | — | Duration of the first frame (if different than other frames), in format XXx YYy, with YYy omitted if value is zero |
| `Duration_FirstFrame/String3` | — | Duration of the first frame (if different than other frames), in format HH:MM:SS.mmm |
| `Duration_FirstFrame/String4` | — | Duration of the first frame (if different than other frames), in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Duration_FirstFrame/String5` | — | Duration of the first frame (if different than other frames), in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Duration_LastFrame` | ms | Duration of the last frame (if different than other frames), in ms |
| `Duration_LastFrame/String` | — | Duration of the last frame (if different than other frames), in format XXx YYy, with YYy value omitted if zero |
| `Duration_LastFrame/String1` | — | Duration of the last frame (if different than other frames), in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Duration_LastFrame/String2` | — | Duration of the last frame (if different than other frames), in format XXx YYy, with YYy omitted if value is zero |
| `Duration_LastFrame/String3` | — | Duration of the last frame (if different than other frames), in format HH:MM:SS.mmm |
| `Duration_LastFrame/String4` | — | Duration of the last frame (if different than other frames), in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Duration_LastFrame/String5` | — | Duration of the last frame (if different than other frames), in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Source_Duration` | ms | Duration of content stored in the file, in ms |
| `Source_Duration/String` | — | Duration of content stored in the file, in format XXx YYy, with YYy value omitted if zero |
| `Source_Duration/String1` | — | Duration of content stored in the file, in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Source_Duration/String2` | — | Duration of content stored in the file, in format XXx YYy, with YYy omitted if value is zero |
| `Source_Duration/String3` | — | Duration of content stored in the file, in format HH:MM:SS.mmm |
| `Source_Duration/String4` | — | Duration of content stored in the file, in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Source_Duration/String5` | — | Duration of content stored in the file, in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Source_Duration_FirstFrame` | ms | Duration of the first frame of content stored in the file (if different than other frames), in ms |
| `Source_Duration_FirstFrame/String` | — | Duration of the first frame of content stored in the file (if different than other frames), in format XXx YYy, with YYy value omitted if zero |
| `Source_Duration_FirstFrame/String1` | — | Duration of the first frame of content stored in the file (if different than other frames), in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Source_Duration_FirstFrame/String2` | — | Duration of the first frame of content stored in the file (if different than other frames), in format XXx YYy, with YYy omitted if value is zero |
| `Source_Duration_FirstFrame/String3` | — | Duration of the first frame of content stored in the file (if different than other frames), in format HH:MM:SS.mmm |
| `Source_Duration_FirstFrame/String4` | — | Duration of the first frame of content stored in the file (if different than other frames), in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Source_Duration_FirstFrame/String5` | — | Duration of the first frame of content stored in the file (if different than other frames), in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Source_Duration_LastFrame` | ms | Duration of the last frame of content stored in the file (if different than other frames), in ms |
| `Source_Duration_LastFrame/String` | — | Duration of the last frame of content stored in the file (if different than other frames), in format XXx YYy, with YYy value omitted if zero |
| `Source_Duration_LastFrame/String1` | — | Duration of the last frame of content stored in the file (if different than other frames), in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Source_Duration_LastFrame/String2` | — | Duration of the last frame of content stored in the file (if different than other frames), in format XXx YYy, with YYy omitted if value is zero |
| `Source_Duration_LastFrame/String3` | — | Duration of the last frame of content stored in the file (if different than other frames), in format HH:MM:SS.mmm |
| `Source_Duration_LastFrame/String4` | — | Duration of the last frame of content stored in the file (if different than other frames), in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Source_Duration_LastFrame/String5` | — | Duration of the last frame of content stored in the file (if different than other frames), in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `BitRate_Mode` | — | Bit rate mode of this stream (CBR, VBR) |
| `BitRate_Mode/String` | — | Bit rate mode of this stream, as word (Variable, Constant) |
| `BitRate` | bps | Bit rate of this stream, in bps |
| `BitRate/String` | — | Bit rate of this stream, with measurement (e.g. 128 kb/s) |
| `BitRate_Minimum` | bps | Minimum bit rate of this stream, in bps |
| `BitRate_Minimum/String` | — | Minimum bit rate of this stream, with measurement |
| `BitRate_Nominal` | bps | Nominal bit rate of this stream, in bps |
| `BitRate_Nominal/String` | — | Nominal bit rate of this stream, with measurement |
| `BitRate_Maximum` | bps | Maximum bit rate of this stream, in bps |
| `BitRate_Maximum/String` | — | Maximum bit rate of this stream, with measurement |
| `BitRate_Encoded` | bps | Encoded bit rate (with forced padding), if container padding is present, in bps |
| `BitRate_Encoded/String` | — | Encoded bit rate (with forced padding), if container padding is present, in bps |
| `Channel(s)` | channel | Number of channels (e.g. 2) |
| `Channel(s)/String` | — | Number of channels (with measurement) |
| `Channel(s)_Original` | channel | Number of channels (e.g. 6) |
| `Channel(s)_Original/String` | — | Number of channels, with measurement (e.g. 6 channels) |
| `Matrix_Channel(s)` | channel | Number of channels after matrix decoding |
| `Matrix_Channel(s)/String` | — | Number of channels after matrix decoding, with measurement |
| `ChannelPositions` | — | Position of channels (e.g. Front: L C R, Side: L R, Back: L R, LFE) |
| `ChannelPositions_Original` | — | Position of channels (e.g. Front: L C R, Side: L R, Back: L R, LFE) |
| `ChannelPositions/String2` | — | Position of channels in x/y.z format (e.g. 3/2/0.1) |
| `ChannelPositions_Original/String2` | — | Position of channels in x/y.z format (e.g. 3/2/0.1) |
| `Matrix_ChannelPositions` | — | Position of channels after matrix decoding |
| `Matrix_ChannelPositions/String2` | — | Position of channels after matrix decoding in x/y.z format |
| `ChannelLayout` | — | Layout of channels (e.g. L R C LFE Ls Rs Lb Rb) |
| `ChannelLayout_Original` | — | Layout of channels (e.g. L R C LFE Ls Rs Lb Rb) |
| `ChannelLayoutID` | — | ID of layout of channels (e.g. MXF descriptor channel assignment). Warning, sometimes this is not enough for uniquely identifying a layout (e.g. MXF descriptor channel assignment is SMPTE 377-4). For AC-3, the form is x,y with x=acmod and y=lfeon |
| `SamplesPerFrame` | — | Samples per frame (e.g. 1536) |
| `SamplingRate` | Hz | Sampling rate, in Hertz (e.g. 48000) |
| `SamplingRate/String` | — | Sampling rate, in Hertz, with measurement (e.g. 48.0 KHz) |
| `SamplingCount` | — | Sample count (based on sampling rate) |
| `Source_SamplingCount` | — | Source Sample count (based on sampling rate), with information derived from header metadata |
| `FrameRate` | fps | Frames per second, as float (e.g. 29.970) |
| `FrameRate/String` | — | Frames per second, with measurement (e.g. 29.970 (29970/1000) FPS) |
| `FrameRate_Num` | — | Numerator for determined frames per second (e.g. 29970) |
| `FrameRate_Den` | — | Denominator for determined frames per second (e.g. 1000) |
| `FrameCount` | — | Frame count |
| `Source_FrameCount` | — | Source frame count |
| `Resolution` | bit | Deprecated, do not use in new projects |
| `Resolution/String` | — | Deprecated, do not use in new projects |
| `BitDepth` | bit | Number of bits in each sample (resolution) of stream (e.g. 16). This field will show the significant bits if the stored bit depth is different |
| `BitDepth/String` | — | Number of bits in each sample (resolution) of stream (e.g. 16). This field will show the significant bits if the stored bit depth is different |
| `BitDepth_Detected` | bit | Number of bits in each sample (resolution), as detected during scan of the input by the muxer, in bits (e.g. 24) |
| `BitDepth_Detected/String` | — | Number of bits in each sample (resolution), as detected during scan of the input by the muxer, in bits (e.g. 24) |
| `BitDepth_Stored` | bit | Stored number of bits in each sample (resolution), in bits (e.g. 24) |
| `BitDepth_Stored/String` | — | Stored number of bits in each sample (resolution), in bits (e.g. 24) |
| `Compression_Mode` | — | Compression mode (Lossy, Lossless) |
| `Compression_Mode/String` | — | Compression mode (Lossy, Lossless) |
| `Compression_Ratio` | — | Stream size divided by uncompressed stream size |
| `Delay` | ms | Delay fixed in the stream (relative), in ms |
| `Delay/String` | — | Delay fixed in the stream (relative), with measurement |
| `Delay/String1` | — | Delay fixed in the stream (relative), with measurement |
| `Delay/String2` | — | Delay fixed in the stream (relative), with measurement |
| `Delay/String3` | — | Delay fixed in the stream (relative) in format HH:MM:SS.mmm, with measurement |
| `Delay/String4` | — | Delay in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Delay/String5` | — | Delay in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Delay_Settings` | — | Delay settings, (e.g. in case of timecode) |
| `Delay_DropFrame` | — | Delay drop frame information |
| `Delay_Source` | — | Source location of the Delay (e.g. Container, Stream, empty) |
| `Delay_Source/String` | — | Source location of the Delay (e.g. Container, Stream, empty) |
| `Delay_Original` | ms | Delay, in ms |
| `Delay_Original/String` | — | Delay, with measurement |
| `Delay_Original/String1` | — | Delay, with measurement |
| `Delay_Original/String2` | — | Delay, with measurement |
| `Delay_Original/String3` | — | Delay, in format HH:MM:SS.mmm |
| `Delay_Original/String4` | — | Delay, in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Delay_Original/String5` | — | Delay, in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Delay_Original_Settings` | — | Delay settings (e.g. in case of timecode) |
| `Delay_Original_DropFrame` | — | Delay drop frame information |
| `Delay_Original_Source` | — | Delay source (e.g. Container, Stream, empty) |
| `Video_Delay` | ms | Delay fixed in the stream relative to video, in ms (e.g. -80) |
| `Video_Delay/String` | — | Delay fixed in the stream relative to video, in ms, with measurement (e.g. -80 ms) |
| `Video_Delay/String1` | — | Delay fixed in the stream relative to video, in ms, with measurement (e.g. -80 ms) |
| `Video_Delay/String2` | — | Delay fixed in the stream relative to video, in ms, with measurement (e.g. -80 ms) |
| `Video_Delay/String3` | — | Delay fixed in the stream relative to video, in format HH:MM:SS.mmm (e.g. -00:00:00.080) |
| `Video_Delay/String4` | — | Delay in format HH:MM:SS:FF, with the last colon replaced by semicolon for drop frame if available |
| `Video_Delay/String5` | — | Delay fixed in the stream relative to video, in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Video0_Delay` | ms | Deprecated, do not use in new projects |
| `Video0_Delay/String` | — | Deprecated, do not use in new projects |
| `Video0_Delay/String1` | — | Deprecated, do not use in new projects |
| `Video0_Delay/String2` | — | Deprecated, do not use in new projects |
| `Video0_Delay/String3` | — | Deprecated, do not use in new projects |
| `Video0_Delay/String4` | — | Deprecated, do not use in new projects |
| `Video0_Delay/String5` | — | Deprecated, do not use in new projects |
| `TimeCode_FirstFrame` | — | Time code for first frame in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `TimeCode_LastFrame` | — | Time code for last frame (excluding the duration of the last frame) in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `TimeCode_DropFrame` | — | Time code drop frame |
| `TimeCode_Settings` | — | Additional time code settings |
| `TimeCode_Source` | — | Time code source (Container, Stream, SystemScheme1, SDTI, ANC, etc.) |
| `ReplayGain_Gain` | dB | The gain to apply to reach 89dB SPL on playback |
| `ReplayGain_Gain/String` | — | The gain to apply to reach 89dB SPL on playback |
| `ReplayGain_Peak` | — | The maximum absolute peak value of the item |
| `StreamSize` | byte | Size of this stream, in bytes |
| `StreamSize/String` | — | Size of this stream, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `StreamSize/String1` | — | Size of this stream, with measurement (measured in powers of 1024) rounded to the nearest integer (e.g. 11 MiB) |
| `StreamSize/String2` | — | Size of this stream, measurement (measured in powers of 1024) rounded to the two most significant digits (e.g. 11 MiB) |
| `StreamSize/String3` | — | Size of this stream, measurement (measured in powers of 1024) rounded to the three most significant digits (e.g. 10.5 MiB) |
| `StreamSize/String4` | — | Size of this stream, measurement (measured in powers of 1024) rounded to the four most significant digits (e.g. 10.50 MiB) |
| `StreamSize/String5` | — | Size of this stream, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `StreamSize_Proportion` | — | Size of this stream divided by total file size |
| `StreamSize_Demuxed` | byte | Size of this stream after demuxing, in bytes |
| `StreamSize_Demuxed/String` | — | Size of this stream after demuxing, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `StreamSize_Demuxed/String1` | — | Size of this stream after demuxing, with measurement (measured in powers of 1024) rounded to the nearest integer (e.g. 11 MiB) |
| `StreamSize_Demuxed/String2` | — | Size of this stream after demuxing, measurement (measured in powers of 1024) rounded to the two most significant digits (e.g. 11 MiB) |
| `StreamSize_Demuxed/String3` | — | Size of this stream after demuxing, measurement (measured in powers of 1024) rounded to the three most significant digits (e.g. 10.5 MiB) |
| `StreamSize_Demuxed/String4` | — | Size of this stream after demuxing, measurement (measured in powers of 1024) rounded to the four most significant digits (e.g. 10.50 MiB) |
| `StreamSize_Demuxed/String5` | — | Size of this stream after demuxing, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `Source_StreamSize` | byte | Size of content stored in the file, in bytes |
| `Source_StreamSize/String` | — | Size of content stored in the file, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `Source_StreamSize/String1` | — | Size of content stored in the file, with measurement (measured in powers of 1024) rounded to the nearest integer (e.g. 11 MiB) |
| `Source_StreamSize/String2` | — | Size of content stored in the file, measurement (measured in powers of 1024) rounded to the two most significant digits (e.g. 11 MiB) |
| `Source_StreamSize/String3` | — | Size of content stored in the file, measurement (measured in powers of 1024) rounded to the three most significant digits (e.g. 10.5 MiB) |
| `Source_StreamSize/String4` | — | Size of content stored in the file, measurement (measured in powers of 1024) rounded to the four most significant digits (e.g. 10.50 MiB) |
| `Source_StreamSize/String5` | — | Size of content stored in the file, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `Source_StreamSize_Proportion` | — | Size of this stream divided by total file size |
| `StreamSize_Encoded` | byte | Size of this stream when encoded, in bytes |
| `StreamSize_Encoded/String` | — | Size of this stream when encoded, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `StreamSize_Encoded/String1` | — | Size of this stream when encoded, with measurement (measured in powers of 1024) rounded to the nearest integer (e.g. 11 MiB) |
| `StreamSize_Encoded/String2` | — | Size of this stream when encoded, measurement (measured in powers of 1024) rounded to the two most significant digits (e.g. 11 MiB) |
| `StreamSize_Encoded/String3` | — | Size of this stream when encoded, measurement (measured in powers of 1024) rounded to the three most significant digits (e.g. 10.5 MiB) |
| `StreamSize_Encoded/String4` | — | Size of this stream when encoded, measurement (measured in powers of 1024) rounded to the four most significant digits (e.g. 10.50 MiB) |
| `StreamSize_Encoded/String5` | — | Size of this stream when encoded, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `StreamSize_Encoded_Proportion` | — | Encoded Stream size divided by file size |
| `Source_StreamSize_Encoded` | byte | Size of content stored in the file when encoded, in bytes |
| `Source_StreamSize_Encoded/String` | — | Size of content stored in the file when encoded, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `Source_StreamSize_Encoded/String1` | — | Size of content stored in the file when encoded, with measurement (measured in powers of 1024) rounded to the nearest integer (e.g. 11 MiB) |
| `Source_StreamSize_Encoded/String2` | — | Size of content stored in the file when encoded, measurement (measured in powers of 1024) rounded to the two most significant digits (e.g. 11 MiB) |
| `Source_StreamSize_Encoded/String3` | — | Size of content stored in the file when encoded, measurement (measured in powers of 1024) rounded to the three most significant digits (e.g. 10.5 MiB) |
| `Source_StreamSize_Encoded/String4` | — | Size of content stored in the file when encoded, measurement (measured in powers of 1024) rounded to the four most significant digits (e.g. 10.50 MiB) |
| `Source_StreamSize_Encoded/String5` | — | Size of content stored in the file when encoded, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `Source_StreamSize_Encoded_Proportion` | — | Source Encoded Stream size divided by file size |
| `Alignment` | — | How this stream is aligned in the container (e.g. Aligned, Split) |
| `Alignment/String` | — | How this stream is aligned in the container (e.g. Aligned, Split) |
| `Interleave_VideoFrames` | — | For interleaved video, between how many video frames this stream is inserted (e.g. 0.51 video frame) |
| `Interleave_Duration` | ms | For interleaved video, between how much time, in ms, this stream is inserted (e.g. 21 ms) |
| `Interleave_Duration/String` | — | For interleaved video, between how many video frames this stream is inserted, as duration with measurement and amount of frame (s) in parenthesis (e.g. 21 ms (0.51 video frame)) |
| `Interleave_Preload` | ms | How much time is buffered before the first video frame, in ms (e.g. 500) |
| `Interleave_Preload/String` | — | How much time is buffered before the first video frame, in ms with measurement (e.g. 500 ms) |
| `Title` | — | Title of track |
| `Encoded_Application` | — | Name of the software package used to create the file (e.g. Microsoft WaveEdiTY); Technical |
| `Encoded_Application/String` | — | Name of the software package used to create the file, in the format "CompanyName ProductName (OperatingSystem) Version (Date)"; Technical |
| `Encoded_Application_CompanyName` | — | Name of the company of the encoding application; Technical |
| `Encoded_Application_Name` | — | Name of the encoding product; Technical |
| `Encoded_Application_Version` | — | Version of the encoding product; Technical |
| `Encoded_Application_Url` | — | URL associated with the encoding software; Technical |
| `Encoded_Library` | — | Software used to create the file; Technical |
| `Encoded_Library/String` | — | Software used to create the file, in the format "CompanyName ProductName (OperatingSystem) Version (Date)"; Technical |
| `Encoded_Library_CompanyName` | — | Name of the encoding software company; Technical |
| `Encoded_Library_Name` | — | Name of the encoding software; Technical |
| `Encoded_Library_Version` | — | Version of the encoding software; Technical |
| `Encoded_Library_Date` | — | Release date of the encoding software, in UTC; Technical |
| `Encoded_Library_Settings` | — | Parameters used by the encoding software; Technical |
| `Encoded_OperatingSystem` | — | Operating System of the encoding software; Technical |
| `Language` | — | Language, formatted as 2-letter ISO 639-1 if exists, else 3-letter ISO 639-2, and with optional ISO 3166-1 country separated by a dash if available (e.g. en, en-US, en-CN) |
| `Language/String` | — | Language, as full name (e.g. English) |
| `Language/String1` | — | Language, as full name (e.g. English) |
| `Language/String2` | — | Language, formatted as 2-letter ISO 639-1, if exists (e.g. en) |
| `Language/String3` | — | Language, formatted as 3-letter ISO 639-2, if exists (e.g. eng) |
| `Language/String4` | — | Language, formatted as 2-letter ISO 639-1, if exists, with optional ISO 3166-1 country separated by a dash if available (e.g. en-US) |
| `Language_More` | — | More information about Language (e.g. Director's Comment) |
| `ServiceKind` | — | Type of assisted service (e.g. visually impaired, commentary, voice over) |
| `ServiceKind/String` | — | Type of assisted service (e.g. visually impaired, commentary, voice over) |
| `Disabled` | Yes | Set if this stream should not be used (Yes, No) |
| `Disabled/String` | — | Set if this stream should not be used (Yes, No) |
| `Default` | Yes | Flag set if this stream should be used if no language found matches the user preference (Yes, No) |
| `Default/String` | — | Flag set if this stream should be used if no language found matches the user preference (Yes, No) |
| `Forced` | Yes | Flag set if this stream should be used regardless of user preferences, often used for sparse subtitle dialogue in an otherwise unsubtitled movie (Yes, No) |
| `Forced/String` | — | Flag set if this stream should be used regardless of user preferences, often used for sparse subtitle dialogue in an otherwise unsubtitled movie (Yes, No) |
| `AlternateGroup` | Yes | Number of a group in order to provide versions of the same track |
| `AlternateGroup/String` | — | Number of a group in order to provide versions of the same track |
| `Encoded_Date` | — | Time that the encoding of this item was completed, in UTC; Temporal |
| `Tagged_Date` | — | Time that the tags were added to this item, in UTC; Temporal |
| `Encryption` | — | Whether this stream is encrypted and, if available, how it is encrypted |

## Текст и субтитры — Text

[Оригинальный каталог Text.csv](https://github.com/MediaArea/MediaInfoLib/blob/b47ab462f9df8a89e971af1ef8753d802c71de8a/Source/Resource/Text/Stream/Text.csv)

| Точное имя поля | Единица | Описание (оригинал) |
|---|---|---|
| `Count` | — | Count of objects available in this stream created by MediaInfo when analyzing file. This is mostly for internal use |
| `Status` | — | Status of bit field when parsing. Options are: 0=IsAccepted, 1=IsFilled, 2=IsUpdated, 3=IsFinished. This is mostly for internal use |
| `StreamCount` | — | Total number of streams available for this StreamKind. Counting starts at 1 |
| `StreamKind` | — | Name of stream type. Options are: Audio, General, Image, Menu, Other, Text, or Video |
| `StreamKind/String` | — | Name of stream type. Options are: Audio, General, Image, Menu, Other, Text, or Video |
| `StreamKindID` | — | Identification number for stream, assigned in order of parsing. Counting starts at 0 |
| `StreamKindPos` | — | Identification number for stream when multiple, assigned in order of parsing. Counting starts at 1 |
| `StreamOrder` | — | Stream order in the file for type of stream. Counting starts at 0 |
| `FirstPacketOrder` | — | Order of the first fully decodable packet parsed in the file for stream type. Counting starts at 0 |
| `Inform` | — | Last **Inform** call. This is mostly for internal use |
| `ID` | — | The identification number for this stream in this file |
| `ID/String` | — | The identification number for this stream in this file (String format) |
| `OriginalSourceMedium_ID` | — | Identification for this stream in the original medium of the material |
| `OriginalSourceMedium_ID/String` | — | Identification for this stream in the original medium of the material (String format) |
| `UniqueID` | — | The unique ID for this stream, should be copied with stream copy |
| `UniqueID/String` | — | The unique ID for this stream, should be copied with stream copy |
| `MenuID` | — | The menu ID for this stream in this file |
| `MenuID/String` | — | The menu ID for this stream in this file |
| `Format` | — | Format used |
| `Format/String` | — | Format used and any additional features or settings |
| `Format/Info` | — | More details about the identified Format |
| `Format/Url` | — | Link to a description of this format |
| `Format_Commercial` | — | Commercial name used by vendor for these settings or Format field if there is no difference |
| `Format_Commercial_IfAny` | — | Commercial name used by vendor for these settings, if available |
| `Format_Version` | — | Version for the identified format |
| `Format_Profile` | — | Profile of the Format |
| `Format_Compression` | — | Compression method used |
| `Format_Settings` | — | Settings used and required by decoder |
| `Format_Settings_Wrapping` | — | Wrapping mode set for format (e.g. Frame, Clip) |
| `Format_AdditionalFeatures` | — | Features from the format that are required to fully support the file content |
| `InternetMediaType` | — | Internet Media Type (aka MIME Type, Content-Type) |
| `MuxingMode` | — | How this file is muxed in the container (e.g. Muxed in Video #1) |
| `MuxingMode_MoreInfo` | — | More information about MuxingMode |
| `CodecID` | — | Codec identifier as indicated by the container |
| `CodecID/String` | — | Codec identifier, as indicated by the container |
| `CodecID/Info` | — | More information about this codec identifier |
| `CodecID/Hint` | — | Common alternative names for this codec identifier |
| `CodecID/Url` | — | A link to more details about this codec identifier |
| `CodecID_Description` | — | Codec description, as defined by the container |
| `Codec` | — | Deprecated |
| `Codec/String` | — | Deprecated |
| `Codec/Info` | — | Deprecated |
| `Codec/Url` | — | Deprecated |
| `Codec/CC` | — | Deprecated |
| `Duration` | ms | Play time of the stream, in ms |
| `Duration/String` | — | Play time of the stream in format XXx YYy, with YYy value omitted if zero (e.g. 1 h 40 min) |
| `Duration/String1` | — | Play time of the stream in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Duration/String2` | — | Play time of the stream in format XXx YYy, with YYy omitted if value is zero |
| `Duration/String3` | — | Play time of the stream in format HH:MM:SS.mmm (e.g. 01:40:00.000) |
| `Duration/String4` | — | Play time of the stream in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Duration/String5` | — | Play time  of the streamin format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Duration_Start2End` | ms | Play time from first display to last display, in ms |
| `Duration_Start2End/String` | — | Play time from first display to last display in format XXx YYy, with YYy value omitted if zero (e.g. 1 h 40 min) |
| `Duration_Start2End/String1` | — | Play time from first display to last display in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Duration_Start2End/String2` | — | Play time from first display to last display in format XXx YYy, with YYy omitted if value is zero |
| `Duration_Start2End/String3` | — | Play time from first display to last display in format HH:MM:SS.mmm |
| `Duration_Start2End/String4` | — | Play time from first display to last display in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Duration_Start2End/String5` | — | Play time from first display to last display in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Duration_Start_Command` | ms | Timestamp of first command, in ms |
| `Duration_Start_Command/String` | — | Timestamp of first command in format XXx YYy, with YYy value omitted if zero (e.g. 1 h 40 min) |
| `Duration_Start_Command/String1` | — | Timestamp of first command in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Duration_Start_Command/String2` | — | Timestamp of first command in format XXx YYy, with YYy omitted if value is zero |
| `Duration_Start_Command/String3` | — | Timestamp of first command in format HH:MM:SS.mmm |
| `Duration_Start_Command/String4` | — | Timestamp of first command in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Duration_Start_Command/String5` | — | Timestamp of first command in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Duration_Start` | ms | Timestamp of first display, in ms |
| `Duration_Start/String` | — | Timestamp of first display in format XXx YYy, with YYy value omitted if zero (e.g. 1 h 40 min) |
| `Duration_Start/String1` | — | Timestamp of first display in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Duration_Start/String2` | — | Timestamp of first display in format XXx YYy, with YYy omitted if value is zero |
| `Duration_Start/String3` | — | Timestamp of first display in format HH:MM:SS.mmm |
| `Duration_Start/String4` | — | Timestamp of first display in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Duration_Start/String5` | — | Timestamp of first display in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Duration_End` | ms | Timestamp of after the last display, in s (ms for text output) |
| `Duration_End/String` | — | Timestamp of after the last display in format XXx YYy, with YYy value omitted if zero (e.g. 1 h 40 min) |
| `Duration_End/String1` | — | Timestamp of after the last display in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Duration_End/String2` | — | Timestamp of after the last display in format XXx YYy, with YYy omitted if value is zero |
| `Duration_End/String3` | — | Timestamp of after the last display in format HH:MM:SS.mmm |
| `Duration_End/String4` | — | Timestamp of after the last display in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Duration_End/String5` | — | Timestamp of after the last display in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Duration_End_Command` | ms | Timestamp of the last command, in s (ms for text output) |
| `Duration_End_Command/String` | — | Timestamp of the last command in format XXx YYy, with YYy value omitted if zero (e.g. 1 h 40 min) |
| `Duration_End_Command/String1` | — | Timestamp of the last command in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Duration_End_Command/String2` | — | Timestamp of the last command in format XXx YYy, with YYy omitted if value is zero |
| `Duration_End_Command/String3` | — | PTimestamp of the last command in format HH:MM:SS.mmm |
| `Duration_End_Command/String4` | — | Timestamp of the last command in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Duration_End_Command/String5` | — | Timestamp of the last command in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Duration_FirstFrame` | ms | Duration of the first frame (if different than other frames), in ms |
| `Duration_FirstFrame/String` | — | Duration of the first frame (if different than other frames), in format XXx YYy, with YYy value omitted if zero |
| `Duration_FirstFrame/String1` | — | Duration of the first frame (if different than other frames), in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Duration_FirstFrame/String2` | — | Duration of the first frame (if different than other frames), in format XXx YYy, with YYy omitted if value is zero |
| `Duration_FirstFrame/String3` | — | Duration of the first frame (if different than other frames), in format HH:MM:SS.mmm |
| `Duration_FirstFrame/String4` | — | Duration of the first frame (if different than other frames), in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Duration_FirstFrame/String5` | — | Duration of the first frame (if different than other frames), in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Duration_LastFrame` | ms | Duration of the last frame (if different than other frames), in ms |
| `Duration_LastFrame/String` | — | Duration of the last frame (if different than other frames), in format XXx YYy, with YYy value omitted if zero |
| `Duration_LastFrame/String1` | — | Duration of the last frame (if different than other frames), in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Duration_LastFrame/String2` | — | Duration of the last frame (if different than other frames), in format XXx YYy, with YYy omitted if value is zero |
| `Duration_LastFrame/String3` | — | Duration of the last frame (if different than other frames), in format HH:MM:SS.mmm |
| `Duration_LastFrame/String4` | — | Duration of the last frame (if different than other frames), in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Duration_LastFrame/String5` | — | Duration of the last frame (if different than other frames), in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Duration_Base` | — | Temporal coordinate system used for timestamps |
| `Source_Duration` | ms | Duration of content stored in the file (if different than duration), in ms |
| `Source_Duration/String` | — | Duration of content stored in the file (if different than duration), in format XXx YYy, with YYy value omitted if zero |
| `Source_Duration/String1` | — | Duration of content stored in the file (if different than duration), in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Source_Duration/String2` | — | Duration of content stored in the file (if different than duration), in format XXx YYy, with YYy omitted if value is zero |
| `Source_Duration/String3` | — | Duration of content stored in the file (if different than duration), in format HH:MM:SS.mmm |
| `Source_Duration/String4` | — | Duration of content stored in the file (if different than duration), in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Source_Duration/String5` | — | Duration of content stored in the file (if different than duration), in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Source_Duration_FirstFrame` | ms | Duration of the first frame of content stored in the file (if different than other frames),in ms |
| `Source_Duration_FirstFrame/String` | — | Duration of the first frame of content stored in the file (if different than other frames),in format XXx YYy, with YYy value omitted if zero |
| `Source_Duration_FirstFrame/String1` | — | Duration of the first frame of content stored in the file (if different than other frames),in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Source_Duration_FirstFrame/String2` | — | Duration of the first frame of content stored in the file (if different than other frames),in format XXx YYy, with YYy omitted if value is zero |
| `Source_Duration_FirstFrame/String3` | — | Duration of the first frame of content stored in the file (if different than other frames),in format HH:MM:SS.mmm |
| `Source_Duration_FirstFrame/String4` | — | Duration of the first frame of content stored in the file (if different than other frames),in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Source_Duration_FirstFrame/String5` | — | Duration of the first frame of content stored in the file (if different than other frames),in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Source_Duration_LastFrame` | ms | Duration of the last frame of content stored in the file (if different than other frames),in ms |
| `Source_Duration_LastFrame/String` | — | Duration of the last frame of content stored in the file (if different than other frames),in format XXx YYy, with YYy value omitted if zero |
| `Source_Duration_LastFrame/String1` | — | Duration of the last frame of content stored in the file (if different than other frames), in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Source_Duration_LastFrame/String2` | — | Duration of the last frame of content stored in the file (if different than other frames), in format XXx YYy, with YYy omitted if value is zero |
| `Source_Duration_LastFrame/String3` | — | Duration of the last frame of content stored in the file (if different than other frames), in format HH:MM:SS.mmm |
| `Source_Duration_LastFrame/String4` | — | Duration of the last frame of content stored in the file (if different than other frames), in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Source_Duration_LastFrame/String5` | — | Duration of the last frame of content stored in the file (if different than other frames), in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `BitRate_Mode` | — | Bit rate mode of this stream (CBR, VBR) |
| `BitRate_Mode/String` | — | Bit rate mode of this stream, as word (Variable, Constant) |
| `BitRate` | bps | Bit rate of this stream, in bps |
| `BitRate/String` | — | Bit rate of this stream, with measurement (e.g. 128 kb/s) |
| `BitRate_Minimum` | bps | Minimum bit rate of this stream, in bps |
| `BitRate_Minimum/String` | — | Minimum bit rate of this stream, with measurement |
| `BitRate_Nominal` | bps | Nominal bit rate of this stream, in bps |
| `BitRate_Nominal/String` | — | Nominal bit rate of this stream, with measurement |
| `BitRate_Maximum` | bps | Maximum bit rate of this stream, in bps |
| `BitRate_Maximum/String` | — | Maximum bit rate of this stream, with measurement |
| `BitRate_Encoded` | bps | Encoded bit rate (with forced padding), if container padding is present, in bps |
| `BitRate_Encoded/String` | — | Encoded bit rate (with forced padding), if container padding is present, in bps |
| `Width` | character | Width of frame (trimmed to "clean aperture" size if present) in characters |
| `Width/String` | — | Width of frame (trimmed to "clean aperture" size if present) in characters, presented in SI unit digit spacing style, with measurement |
| `Height` | character | Height of frame (including aperture size if present) in characters |
| `Height/String` | — | Height of frame (including aperture size if present) in characters, present in SI unit digit spacing style, with measurement |
| `DisplayAspectRatio` | — | The proportional relationship between the width and height of a frame (e.g. 4:3) |
| `DisplayAspectRatio/String` | — | The proportional relationship between the width and height of a frame (e.g. 4:3) |
| `DisplayAspectRatio_Original` | — | The proportional relationship between the width and height of a frame (e.g. 4:3) |
| `DisplayAspectRatio_Original/String` | — | The proportional relationship between the width and height of a frame (e.g. 4:3) |
| `FrameRate_Mode` | — | Frame rate mode, as acronym (e.g. CFR, VFR) |
| `FrameRate_Mode/String` | — | Frame rate mode, as word (e.g. Constant, Variable) |
| `FrameRate_Mode_Original` | — | Frame rate mode, as acronym (e.g. CFR, VFR) |
| `FrameRate_Mode_Original/String` | — | Frame rate mode, as word (e.g. Constant, Variable) |
| `FrameRate` | fps | Frames per second, as float (e.g. 29.970) |
| `FrameRate/String` | — | Frames per second, with measurement (e.g. 29.970 (29970/1000) FPS) |
| `FrameRate_Num` | — | Numerator for determined frames per second (e.g. 29970) |
| `FrameRate_Den` | — | Denominator for determined frames per second (e.g. 1000) |
| `FrameRate_Minimum` | fps | Minimum frames per second (e.g. 25.000) |
| `FrameRate_Minimum/String` | — | Minimum frames per second, with measurement (e.g. 25.000 FPS) |
| `FrameRate_Nominal` | fps | Frames per second rounded to closest standard (e.g. 29.97) |
| `FrameRate_Nominal/String` | — | Frames per second rounded to closest standard, with measurement (e.g. 29.97 fps) |
| `FrameRate_Maximum` | fps | Maximum frames per second |
| `FrameRate_Maximum/String` | — | Maximum frames per second, with measurement |
| `FrameRate_Original` | fps | Frames per second |
| `FrameRate_Original/String` | — | Frames per second, with measurement |
| `FrameRate_Original_Num` | — | Frames per second, numerator |
| `FrameRate_Original_Den` | — | Frames per second, denominator |
| `FrameCount` | — | Numer of frames |
| `ElementCount` | — | Number of displayed elements |
| `Source_FrameCount` | — | Number of frames according to media header (media/stts atom) data |
| `ColorSpace` | — | Color profile of the image (e.g. YUV) |
| `ChromaSubsampling` | — | Ratio of chroma to luma in encoded image (e.g. 4:2:2) |
| `Resolution` | bit | Deprecated, do not use in new projects |
| `Resolution/String` | — | Deprecated, do not use in new projects |
| `BitDepth` | bit | Color information stored in the video frames, as integer (e.g. 10) |
| `BitDepth/String` | — | Color information stored in the video frames, as string (e.g. 10 bits) |
| `Compression_Mode` | — | Compression mode (Lossy, Lossless) |
| `Compression_Mode/String` | — | Compression mode (Lossy, Lossless) |
| `Compression_Ratio` | — | Stream size divided by uncompressed stream size |
| `Delay` | ms | Delay fixed in the stream (relative), in ms |
| `Delay/String` | — | Delay fixed in the stream (relative), with measurement |
| `Delay/String1` | — | Delay fixed in the stream (relative), with measurement |
| `Delay/String2` | — | Delay fixed in the stream (relative), with measurement |
| `Delay/String3` | — | Delay fixed in the stream (relative) in format HH:MM:SS.mmm, with measurement |
| `Delay/String4` | — | Delay in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Delay/String5` | — | Delay in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Delay_Settings` | — | Delay settings (e.g. in case of timecode) |
| `Delay_DropFrame` | — | Delay drop frame information |
| `Delay_Source` | — | Source location of the Delay (e.g. Container, Stream, empty) |
| `Delay_Source/String` | — | Source location of the Delay (e.g. Container, Stream, empty) |
| `Delay_Original` | ms | Delay, in ms |
| `Delay_Original/String` | — | Delay, with measurement |
| `Delay_Original/String1` | — | Delay, with measurement |
| `Delay_Original/String2` | — | Delay, with measurement |
| `Delay_Original/String3` | — | Delay, in format HH:MM:SS.mmm |
| `Delay_Original/String4` | — | Delay, in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Delay_Original/String5` | — | Delay, in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Delay_Original_Settings` | — | Delay settings (e.g. in case of timecode) |
| `Delay_Original_DropFrame` | — | Delay drop frame information |
| `Delay_Original_Source` | — | Delay source (e.g. Container, Stream, empty) |
| `Video_Delay` | ms | Delay fixed in the stream relative to video, in ms (e.g. -80) |
| `Video_Delay/String` | — | Delay fixed in the stream relative to video, in ms, with measurement (e.g. -80 ms) |
| `Video_Delay/String1` | — | Delay fixed in the stream relative to video, in ms, with measurement (e.g. -80 ms) |
| `Video_Delay/String2` | — | Delay fixed in the stream relative to video, in ms, with measurement (e.g. -80 ms) |
| `Video_Delay/String3` | — | Delay fixed in the stream relative to video, in format HH:MM:SS.mmm (e.g. -00:00:00.080) |
| `Video_Delay/String4` | — | Delay in format HH:MM:SS:FF, with the last colon replaced by semicolon for drop frame if available |
| `Video_Delay/String5` | — | Delay fixed in the stream relative to video, in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Video0_Delay` | ms | Deprecated, do not use in new projects |
| `Video0_Delay/String` | — | Deprecated, do not use in new projects |
| `Video0_Delay/String1` | — | Deprecated, do not use in new projects |
| `Video0_Delay/String2` | — | Deprecated, do not use in new projects |
| `Video0_Delay/String3` | — | Deprecated, do not use in new projects |
| `Video0_Delay/String4` | — | Deprecated, do not use in new projects |
| `Video0_Delay/String5` | — | Deprecated, do not use in new projects |
| `TimeCode_FirstFrame` | — | Time code for first frame in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `TimeCode_LastFrame` | — | Time code for last frame (excluding the duration of the last frame) in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `TimeCode_DropFrame` | — | Time code drop frame |
| `TimeCode_Settings` | — | Additional time code settings |
| `TimeCode_Source` | — | Time code source (Container, Stream, SystemScheme1, SDTI, ANC, etc.) |
| `TimeCode_MaxFrameNumber` | — | Maximum frame number in time codes |
| `TimeCode_MaxFrameNumber_Theory` | — | Theoritical maximum frame number in time codes |
| `StreamSize` | byte | Size of this stream, in bytes |
| `StreamSize/String` | — | Size of this stream, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `StreamSize/String1` | — | Size of this stream, with measurement (measured in powers of 1024) rounded to the nearest integer (e.g. 11 MiB) |
| `StreamSize/String2` | — | Size of this stream, measurement (measured in powers of 1024) rounded to the two most significant digits (e.g. 11 MiB) |
| `StreamSize/String3` | — | Size of this stream, measurement (measured in powers of 1024) rounded to the three most significant digits (e.g. 10.5 MiB) |
| `StreamSize/String4` | — | Size of this stream, measurement (measured in powers of 1024) rounded to the four most significant digits (e.g. 10.50 MiB) |
| `StreamSize/String5` | — | Size of this stream, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `StreamSize_Proportion` | — | Size of this stream divided by total file size |
| `StreamSize_Demuxed` | byte | Size of this stream after demuxing, in bytes |
| `StreamSize_Demuxed/String` | — | Size of this stream after demuxing, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `StreamSize_Demuxed/String1` | — | Size of this stream after demuxing, with measurement (measured in powers of 1024) rounded to the nearest integer (e.g. 11 MiB) |
| `StreamSize_Demuxed/String2` | — | Size of this stream after demuxing, measurement (measured in powers of 1024) rounded to the two most significant digits (e.g. 11 MiB) |
| `StreamSize_Demuxed/String3` | — | Size of this stream after demuxing, measurement (measured in powers of 1024) rounded to the three most significant digits (e.g. 10.5 MiB) |
| `StreamSize_Demuxed/String4` | — | Size of this stream after demuxing, measurement (measured in powers of 1024) rounded to the four most significant digits (e.g. 10.50 MiB) |
| `StreamSize_Demuxed/String5` | — | Size of this stream after demuxing, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `Source_StreamSize` | byte | Size of content stored in the file, in bytes |
| `Source_StreamSize/String` | — | Size of content stored in the file, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `Source_StreamSize/String1` | — | Size of content stored in the file, with measurement (measured in powers of 1024) rounded to the nearest integer (e.g. 11 MiB) |
| `Source_StreamSize/String2` | — | Size of content stored in the file, measurement (measured in powers of 1024) rounded to the two most significant digits (e.g. 11 MiB) |
| `Source_StreamSize/String3` | — | Size of content stored in the file, measurement (measured in powers of 1024) rounded to the three most significant digits (e.g. 10.5 MiB) |
| `Source_StreamSize/String4` | — | Size of content stored in the file, measurement (measured in powers of 1024) rounded to the four most significant digits (e.g. 10.50 MiB) |
| `Source_StreamSize/String5` | — | Size of content stored in the file, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `Source_StreamSize_Proportion` | — | Size of this stream divided by total file size |
| `StreamSize_Encoded` | byte | Size of this stream when encoded, in bytes |
| `StreamSize_Encoded/String` | — | Size of this stream when encoded, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `StreamSize_Encoded/String1` | — | Size of this stream when encoded, with measurement (measured in powers of 1024) rounded to the nearest integer (e.g. 11 MiB) |
| `StreamSize_Encoded/String2` | — | Size of this stream when encoded, measurement (measured in powers of 1024) rounded to the two most significant digits (e.g. 11 MiB) |
| `StreamSize_Encoded/String3` | — | Size of this stream when encoded, measurement (measured in powers of 1024) rounded to the three most significant digits (e.g. 10.5 MiB) |
| `StreamSize_Encoded/String4` | — | Size of this stream when encoded, measurement (measured in powers of 1024) rounded to the four most significant digits (e.g. 10.50 MiB) |
| `StreamSize_Encoded/String5` | — | Size of this stream when encoded, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `StreamSize_Encoded_Proportion` | — | Encoded Stream size divided by file size |
| `Source_StreamSize_Encoded` | byte | Size of content stored in the file when encoded, in bytes |
| `Source_StreamSize_Encoded/String` | — | Size of content stored in the file when encoded, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `Source_StreamSize_Encoded/String1` | — | Size of content stored in the file when encoded, with measurement (measured in powers of 1024) rounded to the nearest integer (e.g. 11 MiB) |
| `Source_StreamSize_Encoded/String2` | — | Size of content stored in the file when encoded, measurement (measured in powers of 1024) rounded to the two most significant digits (e.g. 11 MiB) |
| `Source_StreamSize_Encoded/String3` | — | Size of content stored in the file when encoded, measurement (measured in powers of 1024) rounded to the three most significant digits (e.g. 10.5 MiB) |
| `Source_StreamSize_Encoded/String4` | — | Size of content stored in the file when encoded, measurement (measured in powers of 1024) rounded to the four most significant digits (e.g. 10.50 MiB) |
| `Source_StreamSize_Encoded/String5` | — | Size of content stored in the file when encoded, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `Source_StreamSize_Encoded_Proportion` | — | Source Encoded Stream size divided by file size |
| `Title` | — | Title of file |
| `Encoded_Application` | — | Name of the software package used to create the file (e.g. Microsoft WaveEdiTY); Technical |
| `Encoded_Application/String` | — | Name of the software package used to create the file, in the format "CompanyName ProductName (OperatingSystem) Version (Date)"; Technical |
| `Encoded_Application_CompanyName` | — | Name of the company of the encoding application; Technical |
| `Encoded_Application_Name` | — | Name of the encoding product; Technical |
| `Encoded_Application_Version` | — | Version of the encoding product; Technical |
| `Encoded_Application_Url` | — | URL associated with the encoding software; Technical |
| `Encoded_Library` | — | Software used to create the file; Technical |
| `Encoded_Library/String` | — | Software used to create the file, in the format "CompanyName ProductName (OperatingSystem) Version (Date)"; Technical |
| `Encoded_Library_CompanyName` | — | Name of the encoding software company; Technical |
| `Encoded_Library_Name` | — | Name of the encoding software; Technical |
| `Encoded_Library_Version` | — | Version of the encoding software; Technical |
| `Encoded_Library_Date` | — | Release date of the encoding software, in UTC; Technical |
| `Encoded_Library_Settings` | — | Parameters used by the encoding software; Technical |
| `Encoded_OperatingSystem` | — | Operating System of the encoding software; Technical |
| `Language` | — | Language, formatted as 2-letter ISO 639-1 if exists, else 3-letter ISO 639-2, and with optional ISO 3166-1 country separated by a dash if available (e.g. en, en-US, en-CN) |
| `Language/String` | — | Language, as full name (e.g. English) |
| `Language/String1` | — | Language, as full name (e.g. English) |
| `Language/String2` | — | Language, formatted as 2-letter ISO 639-1, if exists (e.g. en) |
| `Language/String3` | — | Language, formatted as 3-letter ISO 639-2, if exists (e.g. eng) |
| `Language/String4` | — | Language, formatted as 2-letter ISO 639-1, if exists, with optional ISO 3166-1 country separated by a dash if available (e.g. en-US) |
| `Language_More` | — | More information about Language (e.g. Director's Comment) |
| `ServiceKind` | — | Type of assisted service (e.g. visually impaired, commentary, voice over) |
| `ServiceKind/String` | — | Type of assisted service (e.g. visually impaired, commentary, voice over) |
| `Disabled` | Yes | Set if this stream should not be used (Yes, No) |
| `Disabled/String` | — | Set if this stream should not be used (Yes, No) |
| `Default` | Yes | Flag set if this stream should be used if no language found matches the user preference (Yes, No) |
| `Default/String` | — | Flag set if this stream should be used if no language found matches the user preference (Yes, No) |
| `Forced` | Yes | Flag set if this stream should be used regardless of user preferences, often used for sparse subtitle dialogue in an otherwise unsubtitled movie (Yes, No) |
| `Forced/String` | — | Flag set if this stream should be used regardless of user preferences, often used for sparse subtitle dialogue in an otherwise unsubtitled movie (Yes, No) |
| `AlternateGroup` | Yes | Number of a group in order to provide versions of the same track |
| `AlternateGroup/String` | — | Number of a group in order to provide versions of the same track |
| `Summary` | — | Plot outline or a summary of the story |
| `Encoded_Date` | — | Time/date/year that the encoding of this content was completed; Temporal |
| `Tagged_Date` | — | Time/date/year that the tags were added to this content; Temporal |
| `Encryption` | — | Whether this stream is encrypted and, if available, how it is encrypted |
| `Events_Total` | — | Не указано в каталоге |
| `Events_MinDuration` | ms | Minimum duration per event, in ms |
| `Events_MinDuration/String` | — | Minimum duration per event in format XXx YYy, with YYy value omitted if zero |
| `Events_MinDuration/String1` | — | Minimum duration per event in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Events_MinDuration/String2` | — | Minimum duration per event in format XXx YYy, with YYy omitted if value is zero |
| `Events_MinDuration/String3` | — | Minimum duration per event in format HH:MM:SS.mmm |
| `Events_MinDuration/String4` | — | Minimum duration per event in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Events_MinDuration/String5` | — | Minimum duration per event in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Events_PopOn` | — | Не указано в каталоге |
| `Events_RollUp` | — | Не указано в каталоге |
| `Events_PaintOn` | — | Не указано в каталоге |
| `Lines_Count` | — | Не указано в каталоге |
| `Lines_MaxCountPerEvent` | — | Не указано в каталоге |
| `Lines_MaxCharacterCount` | — | Не указано в каталоге |
| `FirstDisplay_Delay_Frames` | — | Не указано в каталоге |
| `FirstDisplay_Type` | — | Не указано в каталоге |

## Изображения — Image

[Оригинальный каталог Image.csv](https://github.com/MediaArea/MediaInfoLib/blob/b47ab462f9df8a89e971af1ef8753d802c71de8a/Source/Resource/Text/Stream/Image.csv)

| Точное имя поля | Единица | Описание (оригинал) |
|---|---|---|
| `Count` | — | Count of objects available in this stream created by MediaInfo when analyzing file. This is mostly for internal use |
| `Status` | — | Status of bit field when parsing. Options are: 0=IsAccepted, 1=IsFilled, 2=IsUpdated, 3=IsFinished. This is mostly for internal use |
| `StreamCount` | — | Total number of streams available for this StreamKind. Counting starts at 1 |
| `StreamKind` | — | Name of stream type. Options are: Audio, General, Image, Menu, Other, Text, or Video |
| `StreamKind/String` | — | Name of stream type. Options are: Audio, General, Image, Menu, Other, Text, or Video |
| `StreamKindID` | — | Identification number for stream, assigned in order of parsing. Counting starts at 0 |
| `StreamKindPos` | — | Identification number for stream when multiple, assigned in order of parsing. Counting starts at 1 |
| `StreamOrder` | — | Stream order in the file for type of stream. Counting starts at 0 |
| `FirstPacketOrder` | — | Order of the first fully decodable packet parsed in the file for stream type. Counting starts at 0 |
| `Inform` | — | Last **Inform** call. This is mostly for internal use |
| `ID` | — | The identification number for this stream in this file |
| `ID/String` | — | The identification number for this stream in this file (String format) |
| `OriginalSourceMedium_ID` | — | Identification for this stream in the original medium of the material |
| `OriginalSourceMedium_ID/String` | — | Identification for this stream in the original medium of the material (String format) |
| `UniqueID` | — | The unique ID for this stream, should be copied with stream copy |
| `UniqueID/String` | — | The unique ID for this stream, should be copied with stream copy |
| `MenuID` | — | The menu ID for this stream in this file |
| `MenuID/String` | — | The menu ID for this stream in this file |
| `Type` | — | Type |
| `Type/String` | — | Type |
| `Title` | — | Title of track |
| `Format` | — | Format used |
| `Format/String` | — | Format used and any additional features or settings |
| `Format/Info` | — | More details about the identified Format |
| `Format/Url` | — | Link to a description of this format |
| `Format_Commercial` | — | Commercial name used by vendor for these settings or Format field if there is no difference |
| `Format_Commercial_IfAny` | — | Commercial name used by vendor for these settings, if available |
| `Format_Version` | — | Version for the identified format |
| `Format_Profile` | — | Profile of the Format |
| `Format_Compression` | — | Compression method used |
| `Format_AdditionalFeatures` | — | Format features needed for fully supporting the content |
| `HDR_Format` | — | High Dynamic Range Format used |
| `HDR_Format/String` | — | HDR Format used, along with version, profile, level, layers, settings, and compatibility information |
| `HDR_Format_Commercial` | — | Commercial name used by vendor for these HDR settings or HDR Format field if there is no difference |
| `HDR_Format_Version` | — | Version of HDR Format |
| `HDR_Format_Profile` | — | Profile of HDR Format |
| `HDR_Format_Level` | — | Level of HDR Format |
| `HDR_Format_Settings` | — | HDR Format settings |
| `HDR_Format_Compatibility` | — | HDR Format compatibility with commercial products (e.g. HDR10) |
| `Format_Settings` | — | Settings used and required by decoder |
| `Format_Settings_Endianness` | — | Order of bytes required for decoding. Options are Big/Little |
| `Format_Settings_Packing` | — | Data packing method used in DPX frames (e.g. Packed, Filled A, Filled B) |
| `Format_Settings_Wrapping` | — | Wrapping mode set for format (e.g. Frame, Clip) |
| `Format_Settings_SliceCount` | slice per frame | Number of slices per image (e.g. cells of a HEIF grid) |
| `Format_Settings_SliceCount/String` | — | Number of slices per image (e.g. cells of a HEIF grid) |
| `MuxingMode` | — | How this file is muxed in the container (e.g. Muxed in Video #1) |
| `InternetMediaType` | — | Internet Media Type (aka MIME Type, Content-Type) |
| `CodecID` | — | Codec identifier as indicated by the container |
| `CodecID/String` | — | Codec identifier, as indicated by the container |
| `CodecID/Info` | — | More information about this codec identifier |
| `CodecID/Hint` | — | Common alternative names for this codec identifier |
| `CodecID/Url` | — | A link to more details about this codec identifier |
| `CodecID_Description` | — | Codec description, as defined by the container |
| `Codec` | — | Deprecated, do not use in new projects |
| `Codec/String` | — | Deprecated, do not use in new projects |
| `Codec/Family` | — | Deprecated, do not use in new projects |
| `Codec/Info` | — | Deprecated, do not use in new projects |
| `Codec/Url` | — | Deprecated, do not use in new projects |
| `Width` | pixel | Width of frame (trimmed to "clean aperture" size if present) in pixels, as integer (e.g. 1920) |
| `Width/String` | — | Width of frame (trimmed to "clean aperture" size if present) in pixels, presented in SI unit digit spacing style, with measurement (e.g. 1 920 pixels) |
| `Width_Offset` | pixel | Offset between original width and displayed width, in pixels |
| `Width_Offset/String` | — | Offset between original width and displayed width, in pixels |
| `Width_Original` | pixel | Width of frame (not including aperture size if present) in pixels, presented as integer (e.g. 1920) |
| `Width_Original/String` | — | Width of frame (not including aperture size if present) in pixels, present in SI unit digit spacing style, with measurement (e.g. 1 920 pixels) |
| `Height` | pixel | Height of frame (including aperture size if present) in pixels, presented as integer (e.g. 1080) |
| `Height/String` | — | Height of frame (including aperture size if present) in pixels, present in SI unit digit spacing style, with measurement (e.g. 1 080 pixels) |
| `Height_Offset` | pixel | Offset between original height and displayed height, in pixels |
| `Height_Offset/String` | — | Offset between original height and displayed height, in pixels |
| `Height_Original` | pixel | Height of frame (not including aperture size if present) in pixels, presented as integer (e.g. 1080) |
| `Height_Original/String` | — | Height of frame (not including aperture size if present) in pixels, present in SI unit digit spacing style, with measurement (e.g. 1 080 pixels) |
| `PixelAspectRatio` | — | Pixel Aspect ratio |
| `PixelAspectRatio/String` | — | Pixel Aspect ratio |
| `PixelAspectRatio_Original` | — | Original (in the raw stream) Pixel Aspect ratio |
| `PixelAspectRatio_Original/String` | — | Original (in the raw stream) Pixel Aspect ratio |
| `DisplayAspectRatio` | — | The proportional relationship between the width and height of a frame (e.g. 4:3) |
| `DisplayAspectRatio/String` | — | The proportional relationship between the width and height of a frame (e.g. 4:3) |
| `DisplayAspectRatio_Original` | — | The proportional relationship between the width and height of a frame (e.g. 4:3) |
| `DisplayAspectRatio_Original/String` | — | The proportional relationship between the width and height of a frame (e.g. 4:3) |
| `Active_Width` | pixel | Width of frame, not considering black bars |
| `Active_Width/String` | — | Width of frame, not considering black bars, in pixels, presented in SI unit digit spacing style, with measurement (e.g. 1 920 pixels) |
| `Active_Height` | pixel | Height of frame, not considering black bars |
| `Active_Height/String` | — | Height of frame, not considering black bars, in pixels, presented in SI unit digit spacing style, with measurement (e.g. 1 080 pixels) |
| `Active_DisplayAspectRatio` | — | The proportional relationship between the active width and active height of a frame (e.g. 4:3) |
| `Active_DisplayAspectRatio/String` | — | The proportional relationship between the active width and active height of a frame (e.g. 4:3) |
| `Rotation` | — | Rotation of the image, derived from the Exif orientation, in degrees |
| `Rotation/String` | — | Rotation of the image, derived from the Exif orientation, in degrees |
| `ColorSpace` | — | Color profile of the image (e.g. YUV) |
| `ChromaSubsampling` | — | Ratio of chroma to luma in encoded image (e.g. 4:2:2) |
| `Resolution` | bit | Deprecated, do not use in new projects |
| `Resolution/String` | — | Deprecated, do not use in new projects |
| `BitDepth` | bit | Color information stored in the frame, as integer (e.g. 10) |
| `BitDepth/String` | — | Color information stored in the frame, as string (e.g. 10 bits) |
| `Compression_Mode` | — | Compression mode (Lossy, Lossless) |
| `Compression_Mode/String` | — | Compression mode (Lossy, Lossless) |
| `Compression_Ratio` | — | Stream size divided by uncompressed stream size |
| `StreamSize` | byte | Size of this stream, in bytes |
| `StreamSize/String` | — | Size of this stream, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `StreamSize/String1` | — | Size of this stream, with measurement (measured in powers of 1024) rounded to the nearest integer (e.g. 11 MiB) |
| `StreamSize/String2` | — | Size of this stream, measurement (measured in powers of 1024) rounded to the two most significant digits (e.g. 11 MiB) |
| `StreamSize/String3` | — | Size of this stream, measurement (measured in powers of 1024) rounded to the three most significant digits (e.g. 10.5 MiB) |
| `StreamSize/String4` | — | Size of this stream, measurement (measured in powers of 1024) rounded to the four most significant digits (e.g. 10.50 MiB) |
| `StreamSize/String5` | — | Size of this stream, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `StreamSize_Proportion` | — | Size of this stream divided by total file size |
| `StreamSize_Demuxed` | byte | Size of this stream after demuxing, in bytes |
| `StreamSize_Demuxed/String` | — | Size of this stream after demuxing, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `StreamSize_Demuxed/String1` | — | Size of this stream after demuxing, with measurement (measured in powers of 1024) rounded to the nearest integer (e.g. 11 MiB) |
| `StreamSize_Demuxed/String2` | — | Size of this stream after demuxing, measurement (measured in powers of 1024) rounded to the two most significant digits (e.g. 11 MiB) |
| `StreamSize_Demuxed/String3` | — | Size of this stream after demuxing, measurement (measured in powers of 1024) rounded to the three most significant digits (e.g. 10.5 MiB) |
| `StreamSize_Demuxed/String4` | — | Size of this stream after demuxing, measurement (measured in powers of 1024) rounded to the four most significant digits (e.g. 10.50 MiB) |
| `StreamSize_Demuxed/String5` | — | Size of this stream after demuxing, with measurement (measured in powers of 1024) and percentage value (e.g. 10.5 MiB (98%)) |
| `Encoded_Library` | — | Software used to create the file; Technical |
| `Encoded_Library/String` | — | Software used to create the file, in the format "CompanyName ProductName (OperatingSystem) Version (Date)"; Technical |
| `Encoded_Library_Name` | — | Name of the encoding software; Technical |
| `Encoded_Library_Version` | — | Version of the encoding software; Technical |
| `Encoded_Library_Date` | — | Release date of the encoding software, in UTC; Technical |
| `Encoded_Library_Settings` | — | Parameters used by the encoding software; Technical |
| `Language` | — | Language, formatted as 2-letter ISO 639-1 if exists, else 3-letter ISO 639-2, and with optional ISO 3166-1 country separated by a dash if available (e.g. en, en-US, en-CN) |
| `Language/String` | — | Language, as full name (e.g. English) |
| `Language/String1` | — | Language, as full name (e.g. English) |
| `Language/String2` | — | Language, formatted as 2-letter ISO 639-1, if exists (e.g. en) |
| `Language/String3` | — | Language, formatted as 3-letter ISO 639-2, if exists (e.g. eng) |
| `Language/String4` | — | Language, formatted as 2-letter ISO 639-1, if exists, with optional ISO 3166-1 country separated by a dash if available (e.g. en-US) |
| `Language_More` | — | More information about Language (e.g. Director's Comment) |
| `ServiceKind` | — | Type of assisted service (e.g. visually impaired, commentary, voice over) |
| `ServiceKind/String` | — | Type of assisted service (e.g. visually impaired, commentary, voice over) |
| `Disabled` | Yes | Set if this stream should not be used (Yes, No) |
| `Disabled/String` | — | Set if this stream should not be used (Yes, No) |
| `Default` | Yes | Flag set if this stream should be used if no language found matches the user preference (Yes, No) |
| `Default/String` | — | Flag set if this stream should be used if no language found matches the user preference (Yes, No) |
| `Forced` | Yes | Flag set if this stream should be used regardless of user preferences, often used for sparse subtitle dialogue in an otherwise unsubtitled movie (Yes, No) |
| `Forced/String` | — | Flag set if this stream should be used regardless of user preferences, often used for sparse subtitle dialogue in an otherwise unsubtitled movie (Yes, No) |
| `AlternateGroup` | Yes | Number of a group in order to provide versions of the same track |
| `AlternateGroup/String` | — | Number of a group in order to provide versions of the same track |
| `Summary` | — | Plot outline or a summary of the story |
| `Encoded_Date` | — | Time that the encoding of this item was completed, in UTC; Temporal |
| `Tagged_Date` | — | Time that the tags were added to this item, in UTC; Temporal |
| `Encryption` | — | Whether this stream is encrypted and, if available, how it is encrypted |
| `colour_description_present` | — | Presence of color description (Yes, No) |
| `colour_description_present_Source` | — | Presence of colour description (source) |
| `colour_description_present_Original` | — | Presence of colour description (if incoherencies) |
| `colour_description_present_Original_Source` | — | Presence of colour description (source if incoherencies) |
| `colour_range` | — | Color range for YUV color space |
| `colour_range_Source` | — | Colour range for YUV colour space (source) |
| `colour_range_Original` | — | Colour range for YUV colour space (if incoherencies) |
| `colour_range_Original_Source` | — | Colour range for YUV colour space (source if incoherencies) |
| `colour_primaries` | — | Chromaticity coordinates of the source primaries |
| `colour_primaries_Source` | — | Chromaticity coordinates of the source primaries (source) |
| `colour_primaries_Original` | — | Chromaticity coordinates of the source primaries (if incoherencies) |
| `colour_primaries_Original_Source` | — | Chromaticity coordinates of the source primaries (source if incoherencies) |
| `transfer_characteristics` | — | Opto-electronic transfer characteristic of the source picture |
| `transfer_characteristics_Source` | — | Opto-electronic transfer characteristic of the source picture (source) |
| `transfer_characteristics_Original` | — | Opto-electronic transfer characteristic of the source picture (if incoherencies) |
| `transfer_characteristics_Original_Source` | — | Opto-electronic transfer characteristic of the source picture (source if incoherencies) |
| `matrix_coefficients` | — | Matrix coefficients used in deriving luma and chroma signals from the green, blue, and red primaries |
| `matrix_coefficients_Source` | — | Matrix coefficients used in deriving luma and chroma signals from the green, blue, and red primaries (source) |
| `matrix_coefficients_Original` | — | Matrix coefficients used in deriving luma and chroma signals from the green, blue, and red primaries (if incoherencies) |
| `matrix_coefficients_Original_Source` | — | Matrix coefficients used in deriving luma and chroma signals from the green, blue, and red primaries (source if incoherencies) |
| `MasteringDisplay_ColorPrimaries` | — | Chromaticity coordinates of the source primaries of the mastering display |
| `MasteringDisplay_ColorPrimaries_Source` | — | Chromaticity coordinates of the source primaries of the mastering display (source) |
| `MasteringDisplay_ColorPrimaries_Original` | — | Chromaticity coordinates of the source primaries of the mastering display (if incoherencies) |
| `MasteringDisplay_ColorPrimaries_Original_Source` | — | Chromaticity coordinates of the source primaries of the mastering display (source if incoherencies) |
| `MasteringDisplay_Luminance` | — | Luminance of the mastering display |
| `MasteringDisplay_Luminance_Source` | — | Luminance of the mastering display (source) |
| `MasteringDisplay_Luminance_Min` | — | Min luminance of the mastering display |
| `MasteringDisplay_Luminance_Max` | — | Max luminance of the mastering display |
| `MasteringDisplay_Luminance_Original` | — | Luminance of the mastering display (if incoherencies) |
| `MasteringDisplay_Luminance_Original_Source` | — | Luminance of the mastering display (source if incoherencies) |
| `MasteringDisplay_Luminance_Original_Min` | — | Min luminance of the mastering display (source if incoherencies) |
| `MasteringDisplay_Luminance_Original_Max` | — | Max luminance of the mastering display (source if incoherencies) |
| `MaxCLL` | cd/m2 | Maximum content light level |
| `MaxCLL/String` | — | Maximum content light level, with measurement |
| `MaxCLL_Source` | — | Maximum content light level (source) |
| `MaxCLL_Original` | cd/m2 | Maximum content light level (if incoherencies), with measurement |
| `MaxCLL_Original/String` | — | Maximum content light level (if incoherencies) |
| `MaxCLL_Original_Source` | — | Maximum content light level (source if incoherencies) |
| `MaxFALL` | cd/m2 | Maximum frame average light level |
| `MaxFALL/String` | — | Maximum frame average light level |
| `MaxFALL_Source` | — | Maximum frame average light level (source), with measurement |
| `MaxFALL_Original` | cd/m2 | Maximum frame average light level (if incoherencies) |
| `MaxFALL_Original/String` | — | Maximum frame average light level (if incoherencies), with measurement |
| `MaxFALL_Original_Source` | — | Maximum frame average light level (source if incoherencies) |

## Другие потоки — Other

[Оригинальный каталог Other.csv](https://github.com/MediaArea/MediaInfoLib/blob/b47ab462f9df8a89e971af1ef8753d802c71de8a/Source/Resource/Text/Stream/Other.csv)

| Точное имя поля | Единица | Описание (оригинал) |
|---|---|---|
| `Count` | — | Count of objects available in this stream |
| `Status` | — | bit field (0=IsAccepted, 1=IsFilled, 2=IsUpdated, 3=IsFinished) |
| `StreamCount` | — | Count of streams of that kind available |
| `StreamKind` | — | Stream type name |
| `StreamKind/String` | — | Stream type name |
| `StreamKindID` | — | Number of the stream (base=0) |
| `StreamKindPos` | — | When multiple streams, number of the stream (base=1) |
| `StreamOrder` | — | Stream order in the file, whatever is the kind of stream (base=0) |
| `FirstPacketOrder` | — | Order of the first fully decodable packet met in the file, whatever is the kind of stream (base=0) |
| `Inform` | — | Last **Inform** call |
| `ID` | — | The ID for this stream in this file |
| `ID/String` | — | The ID for this stream in this file |
| `OriginalSourceMedium_ID` | — | The ID for this stream in the original medium of the material |
| `OriginalSourceMedium_ID/String` | — | The ID for this stream in the original medium of the material |
| `UniqueID` | — | The unique ID for this stream, should be copied with stream copy |
| `UniqueID/String` | — | The unique ID for this stream, should be copied with stream copy |
| `MenuID` | — | The menu ID for this stream in this file |
| `MenuID/String` | — | The menu ID for this stream in this file |
| `Type` | — | Type |
| `Format` | — | Format used |
| `Format/String` | — | Format used + additional features |
| `Format/Info` | — | Info about Format |
| `Format/Url` | — | Link |
| `Format_Commercial` | — | Commercial name used by vendor for theses setings or Format field if there is no difference |
| `Format_Commercial_IfAny` | — | Commercial name used by vendor for theses setings if there is one |
| `Format_Version` | — | Version of this format |
| `Format_Profile` | — | Profile of the Format |
| `Format_Compression` | — | Compression method used |
| `Format_Settings` | — | Settings needed for decoder used |
| `Format_Settings_Wrapping` | — | Wrapping mode set for format (e.g. Frame, Clip) |
| `Format_AdditionalFeatures` | — | Format features needed for fully supporting the content |
| `MuxingMode` | — | How this file is muxed in the container |
| `MuxingMode_MoreInfo` | — | More information about MuxingMode |
| `CodecID` | — | Codec ID (found in some containers) |
| `CodecID/String` | — | Codec ID (found in some containers) |
| `CodecID/Info` | — | Info about this codec |
| `CodecID/Hint` | — | A hint/popular name for this codec |
| `CodecID/Url` | — | A link to more details about this codec ID |
| `CodecID_Description` | — | Manual description given by the container |
| `Duration` | ms | Play time of the stream in ms |
| `Duration/String` | — | Play time in format : XXx YYy only, YYy omited if zero |
| `Duration/String1` | — | Play time in format : HHh MMmn SSs MMMms, XX omited if zero |
| `Duration/String2` | — | Play time in format : XXx YYy only, YYy omited if zero |
| `Duration/String3` | — | Play time in format : HH:MM:SS.MMM |
| `Duration/String4` | — | Play time in format : HH:MM:SS:FF, last colon replaced by semicolon for drop frame if available |
| `Duration/String5` | — | Play time in format : HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Duration_Start` | — | Не указано в каталоге |
| `Duration_End` | — | Не указано в каталоге |
| `Source_Duration` | ms | Source Play time of the stream, in ms |
| `Source_Duration/String` | — | Source Play time in format : XXx YYy only, YYy omited if zero |
| `Source_Duration/String1` | — | Source Play time in format : HHh MMmn SSs MMMms, XX omited if zero |
| `Source_Duration/String2` | — | Source Play time in format : XXx YYy only, YYy omited if zero |
| `Source_Duration/String3` | — | Source Play time in format : HH:MM:SS.MMM |
| `Source_Duration/String4` | — | Play time in format : HH:MM:SS:FF, last colon replaced by semicolon for drop frame if available |
| `Source_Duration/String5` | — | Play time in format : HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Source_Duration_FirstFrame` | ms | Source Duration of the first frame if it is longer than others, in ms |
| `Source_Duration_FirstFrame/String` | — | Source Duration of the first frame if it is longer than others, in format : XXx YYy only, YYy omited if zero |
| `Source_Duration_FirstFrame/String1` | — | Source Duration of the first frame if it is longer than others, in format : HHh MMmn SSs MMMms, XX omited if zero |
| `Source_Duration_FirstFrame/String2` | — | Source Duration of the first frame if it is longer than others, in format : XXx YYy only, YYy omited if zero |
| `Source_Duration_FirstFrame/String3` | — | Source Duration of the first frame if it is longer than others, in format : HH:MM:SS.MMM |
| `Source_Duration_FirstFrame/String4` | — | Play time in format : HH:MM:SS:FF, last colon replaced by semicolon for drop frame if available |
| `Source_Duration_FirstFrame/String5` | — | Play time in format : HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Source_Duration_LastFrame` | ms | Source Duration of the last frame if it is longer than others, in ms |
| `Source_Duration_LastFrame/String` | — | Source Duration of the last frame if it is longer than others, in format : XXx YYy only, YYy omited if zero |
| `Source_Duration_LastFrame/String1` | — | Source Duration of the last frame if it is longer than others, in format : HHh MMmn SSs MMMms, XX omited if zero |
| `Source_Duration_LastFrame/String2` | — | Source Duration of the last frame if it is longer than others, in format : XXx YYy only, YYy omited if zero |
| `Source_Duration_LastFrame/String3` | — | Source Duration of the last frame if it is longer than others, in format : HH:MM:SS.MMM |
| `Source_Duration_LastFrame/String4` | — | Play time in format : HH:MM:SS:FF, last colon replaced by semicolon for drop frame if available |
| `Source_Duration_LastFrame/String5` | — | Play time in format : HH:MM:SS.mmm (HH:MM:SS:FF) |
| `BitRate_Mode` | — | Bit rate mode (VBR, CBR) |
| `BitRate_Mode/String` | — | Bit rate mode (Variable, Cconstant) |
| `BitRate` | bps | Bit rate in bps |
| `BitRate/String` | — | Bit rate (with measurement) |
| `BitRate_Minimum` | bps | Minimum Bit rate in bps |
| `BitRate_Minimum/String` | — | Minimum Bit rate (with measurement) |
| `BitRate_Nominal` | bps | Nominal Bit rate in bps |
| `BitRate_Nominal/String` | — | Nominal Bit rate (with measurement) |
| `BitRate_Maximum` | bps | Maximum Bit rate in bps |
| `BitRate_Maximum/String` | — | Maximum Bit rate (with measurement) |
| `BitRate_Encoded` | bps | Encoded (with forced padding) bit rate in bps, if some container padding is present |
| `BitRate_Encoded/String` | — | Encoded (with forced padding) bit rate (with measurement), if some container padding is present |
| `FrameRate` | fps | Frames per second |
| `FrameRate/String` | — | Frames per second (with measurement) |
| `FrameRate_Num` | — | Frames per second, numerator |
| `FrameRate_Den` | — | Frames per second, denominator |
| `FrameCount` | — | Number of frames |
| `Source_FrameCount` | — | Source Number of frames |
| `Delay` | ms | Delay fixed in the stream (relative) IN MS |
| `Delay/String` | — | Delay with measurement |
| `Delay/String1` | — | Delay with measurement |
| `Delay/String2` | — | Delay with measurement |
| `Delay/String3` | — | Delay in format : HH:MM:SS.MMM |
| `Delay/String4` | — | Delay in format : HH:MM:SS:FF, last colon replaced by semicolon for drop frame if available |
| `Delay/String5` | — | Delay in format : HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Delay_Settings` | — | Delay settings (in case of timecode for example) |
| `Delay_DropFrame` | — | Delay drop frame |
| `Delay_Source` | — | Delay source (Container or Stream or empty) |
| `Delay_Source/String` | — | Delay source (Container or Stream or empty) |
| `Delay_Original` | ms | Delay fixed in the raw stream (relative) IN MS |
| `Delay_Original/String` | — | Delay with measurement |
| `Delay_Original/String1` | — | Delay with measurement |
| `Delay_Original/String2` | — | Delay with measurement |
| `Delay_Original/String3` | — | Delay in format: HH:MM:SS.MMM |
| `Delay_Original/String4` | — | Delay in format: HH:MM:SS:FF, last colon replaced by semicolon for drop frame if available |
| `Delay_Original/String5` | — | Delay in format : HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Delay_Original_Settings` | — | Delay settings (in case of timecode for example) |
| `Delay_Original_DropFrame` | — | Delay drop frame info |
| `Delay_Original_Source` | — | Delay source (Stream or empty) |
| `Video_Delay` | ms | Delay fixed in the stream (absolute / video) |
| `Video_Delay/String` | — | Не указано в каталоге |
| `Video_Delay/String1` | — | Не указано в каталоге |
| `Video_Delay/String2` | — | Не указано в каталоге |
| `Video_Delay/String3` | — | Не указано в каталоге |
| `Video_Delay/String4` | — | Не указано в каталоге |
| `Video_Delay/String5` | — | Не указано в каталоге |
| `Video0_Delay` | ms | Deprecated, do not use in new projects |
| `Video0_Delay/String` | — | Deprecated, do not use in new projects |
| `Video0_Delay/String1` | — | Deprecated, do not use in new projects |
| `Video0_Delay/String2` | — | Deprecated, do not use in new projects |
| `Video0_Delay/String3` | — | Deprecated, do not use in new projects |
| `Video0_Delay/String4` | — | Deprecated, do not use in new projects |
| `Video0_Delay/String5` | — | Deprecated, do not use in new projects |
| `TimeStamp_FirstFrame` | ms | TimeStamp fixed in the stream (relative) IN MS |
| `TimeStamp_FirstFrame/String` | — | TimeStamp with measurement |
| `TimeStamp_FirstFrame/String1` | — | TimeStamp with measurement |
| `TimeStamp_FirstFrame/String2` | — | TimeStamp with measurement |
| `TimeStamp_FirstFrame/String3` | — | TimeStamp in format : HH:MM:SS.MMM |
| `TimeStamp_FirstFrame/String4` | — | TimeStamp in format : HH:MM:SS:FF, last colon replaced by semicolon for drop frame if available |
| `TimeStamp_FirstFrame/String5` | — | TimeStamp in format : HH:MM:SS.mmm (HH:MM:SS:FF) |
| `TimeCode_FirstFrame` | — | Time code in HH:MM:SS:FF, last colon replaced by semicolon for drop frame if available format |
| `TimeCode_LastFrame` | — | Time code of the last frame (excluding the duration of the last frame) in HH:MM:SS:FF, last colon replaced by semicolon for drop frame if available format |
| `TimeCode_DropFrame` | — | Delay drop frame |
| `TimeCode_Settings` | — | Time code settings |
| `TimeCode_Stripped` | Yes | Time code is Stripped (only 1st time code, no discontinuity) |
| `TimeCode_Stripped/String` | — | Time code is Stripped (only 1st time code, no discontinuity) |
| `TimeCode_Source` | — | Time code source (Container, Stream, SystemScheme1, SDTI, ANC...) |
| `StreamSize` | byte | Streamsize in bytes |
| `StreamSize/String` | — | Streamsize in with percentage value |
| `StreamSize/String1` | — | Не указано в каталоге |
| `StreamSize/String2` | — | Не указано в каталоге |
| `StreamSize/String3` | — | Не указано в каталоге |
| `StreamSize/String4` | — | Не указано в каталоге |
| `StreamSize/String5` | — | Streamsize in with percentage value |
| `StreamSize_Proportion` | — | Stream size divided by file size |
| `StreamSize_Demuxed` | byte | StreamSize in bytes of hte stream after demux |
| `StreamSize_Demuxed/String` | — | StreamSize_Demuxed in with percentage value |
| `StreamSize_Demuxed/String1` | — | Не указано в каталоге |
| `StreamSize_Demuxed/String2` | — | Не указано в каталоге |
| `StreamSize_Demuxed/String3` | — | Не указано в каталоге |
| `StreamSize_Demuxed/String4` | — | Не указано в каталоге |
| `StreamSize_Demuxed/String5` | — | StreamSize_Demuxed in with percentage value (note: theoritical value, not for real use) |
| `Source_StreamSize` | byte | Source Streamsize in bytes |
| `Source_StreamSize/String` | — | Source Streamsize in with percentage value |
| `Source_StreamSize/String1` | — | Не указано в каталоге |
| `Source_StreamSize/String2` | — | Не указано в каталоге |
| `Source_StreamSize/String3` | — | Не указано в каталоге |
| `Source_StreamSize/String4` | — | Не указано в каталоге |
| `Source_StreamSize/String5` | — | Source Streamsize in with percentage value |
| `Source_StreamSize_Proportion` | — | Source Stream size divided by file size |
| `StreamSize_Encoded` | byte | Encoded Streamsize in bytes |
| `StreamSize_Encoded/String` | — | Encoded Streamsize in with percentage value |
| `StreamSize_Encoded/String1` | — | Не указано в каталоге |
| `StreamSize_Encoded/String2` | — | Не указано в каталоге |
| `StreamSize_Encoded/String3` | — | Не указано в каталоге |
| `StreamSize_Encoded/String4` | — | Не указано в каталоге |
| `StreamSize_Encoded/String5` | — | Encoded Streamsize in with percentage value |
| `StreamSize_Encoded_Proportion` | — | Encoded Stream size divided by file size |
| `Source_StreamSize_Encoded` | byte | Source Encoded Streamsize in bytes |
| `Source_StreamSize_Encoded/String` | — | Source Encoded Streamsize in with percentage value |
| `Source_StreamSize_Encoded/String1` | — | Не указано в каталоге |
| `Source_StreamSize_Encoded/String2` | — | Не указано в каталоге |
| `Source_StreamSize_Encoded/String3` | — | Не указано в каталоге |
| `Source_StreamSize_Encoded/String4` | — | Не указано в каталоге |
| `Source_StreamSize_Encoded/String5` | — | Source Encoded Streamsize in with percentage value |
| `Source_StreamSize_Encoded_Proportion` | — | Source Encoded Stream size divided by file size |
| `Title` | — | Name of this menu |
| `Language` | — | Language (2-letter ISO 639-1 if exists, else 3-letter ISO 639-2, and with optional ISO 3166-1 country separated by a dash if available, e.g. en, en-us, zh-cn) |
| `Language/String` | — | Language (full) |
| `Language/String1` | — | Language (full) |
| `Language/String2` | — | Language (2-letter ISO 639-1 if exists, else empty) |
| `Language/String3` | — | Language (3-letter ISO 639-2 if exists, else empty) |
| `Language/String4` | — | Language (2-letter ISO 639-1 if exists with optional ISO 3166-1 country separated by a dash if available, e.g. en, en-us, zh-cn, else empty) |
| `Language_More` | — | More info about Language (e.g. Director's Comment) |
| `ServiceKind` | — | Service kind, e.g. visually impaired, commentary, voice over |
| `ServiceKind/String` | — | Service kind (full) |
| `Disabled` | Yes | Set if that track should not be used |
| `Disabled/String` | — | Set if that track should not be used |
| `Default` | Yes | Set if that track should be used if no language found matches the user preference. |
| `Default/String` | — | Set if that track should be used if no language found matches the user preference. |
| `Forced` | Yes | Set if that track should be used if no language found matches the user preference. |
| `Forced/String` | — | Set if that track should be used if no language found matches the user preference. |
| `AlternateGroup` | Yes | Number of a group in order to provide versions of the same track |
| `AlternateGroup/String` | — | Number of a group in order to provide versions of the same track |

## Меню и главы — Menu

[Оригинальный каталог Menu.csv](https://github.com/MediaArea/MediaInfoLib/blob/b47ab462f9df8a89e971af1ef8753d802c71de8a/Source/Resource/Text/Stream/Menu.csv)

| Точное имя поля | Единица | Описание (оригинал) |
|---|---|---|
| `Count` | — | Count of objects available in this stream created by MediaInfo when analyzing file. This is mostly for internal use |
| `Status` | — | Status of bit field when parsing. Options are: 0=IsAccepted, 1=IsFilled, 2=IsUpdated, 3=IsFinished. This is mostly for internal use |
| `StreamCount` | — | Total number of streams available for this StreamKind. Counting starts at 1 |
| `StreamKind` | — | Name of stream type. Options are: Audio, General, Image, Menu, Other, Text, or Video |
| `StreamKind/String` | — | Name of stream type. Options are: Audio, General, Image, Menu, Other, Text, or Video |
| `StreamKindID` | — | Identification number for stream, assigned in order of parsing. Counting starts at 0 |
| `StreamKindPos` | — | Identification number for stream when multiple, assigned in order of parsing. Counting starts at 1 |
| `StreamOrder` | — | Stream order in the file for type of stream. Counting starts at 0 |
| `FirstPacketOrder` | — | Order of the first fully decodable packet parsed in the file for stream type. Counting starts at 0 |
| `Inform` | — | Last **Inform** call. This is mostly for internal use |
| `ID` | — | The identification number for this stream in this file |
| `ID/String` | — | The identification number for this stream in this file (String format) |
| `OriginalSourceMedium_ID` | — | Identification for this stream in the original medium of the material |
| `OriginalSourceMedium_ID/String` | — | Identification for this stream in the original medium of the material (String format) |
| `UniqueID` | — | The unique ID for this stream, should be copied with stream copy |
| `UniqueID/String` | — | The unique ID for this stream, should be copied with stream copy |
| `MenuID` | — | The menu ID for this stream in this file |
| `MenuID/String` | — | The menu ID for this stream in this file |
| `Format` | — | Format used |
| `Format/String` | — | Format used and any additional features or settings |
| `Format/Info` | — | More details about the identified Format |
| `Format/Url` | — | Link to a description of this format |
| `Format_Commercial` | — | Commercial name used by vendor for these settings or Format field if there is no difference |
| `Format_Commercial_IfAny` | — | Commercial name used by vendor for these settings, if available |
| `Format_Version` | — | Version for the identified format |
| `Format_Profile` | — | Profile of the Format |
| `Format_Compression` | — | Compression method used |
| `Format_Settings` | — | Settings used and required by decoder |
| `Format_AdditionalFeatures` | — | Features from the format that are required to fully support the file content |
| `CodecID` | — | Codec identifier as indicated by the container |
| `CodecID/String` | — | Codec identifier, as indicated by the container |
| `CodecID/Info` | — | More information about this codec identifier |
| `CodecID/Hint` | — | Common alternative names for this codec identifier |
| `CodecID/Url` | — | A link to more details about this codec identifier |
| `CodecID_Description` | — | Codec description, as defined by the container |
| `Codec` | — | Deprecated |
| `Codec/String` | — | Deprecated |
| `Codec/Info` | — | Deprecated |
| `Codec/Url` | — | Deprecated |
| `Duration` | ms | Play time of the stream, in s (ms for text output) |
| `Duration/String` | — | Play time in format XXx YYy, with YYy value omitted if zero (e.g. 1 h 40 min) |
| `Duration/String1` | — | Play time in format HHh MMmn SSs MMMms, with any fields omitted if zero |
| `Duration/String2` | — | Play time in format XXx YYy, with YYy omitted if value is zero |
| `Duration/String3` | — | Play time in format HH:MM:SS.mmm (e.g. 01:40:00.000) |
| `Duration/String4` | — | Play time in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Duration/String5` | — | Play time in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Duration_Start` | — | Start time of stream, in UTC |
| `Duration_End` | — | End time of stream, in UTC |
| `Delay` | ms | Delay fixed in the stream (relative), in ms |
| `Delay/String` | — | Delay fixed in the stream (relative), with measurement |
| `Delay/String1` | — | Delay fixed in the stream (relative), with measurement |
| `Delay/String2` | — | Delay fixed in the stream (relative), with measurement |
| `Delay/String3` | — | Delay fixed in the stream (relative) in format HH:MM:SS.mmm, with measurement |
| `Delay/String4` | — | Delay in format HH:MM:SS:FF, with last colon replaced by semicolon for drop frame if available |
| `Delay/String5` | — | Delay in format HH:MM:SS.mmm (HH:MM:SS:FF) |
| `Delay_Settings` | — | Delay settings (in case of timecode for example) |
| `Delay_DropFrame` | — | Delay drop frame |
| `Delay_Source` | — | Part of the file where the delay was set (e.g. Container, Stream, or empty) |
| `FrameRate_Mode` | — | Frame rate mode, as acronym (e.g. CFR, VFR) |
| `FrameRate_Mode/String` | — | Frame rate mode, as word (e.g. Constant, Variable) |
| `FrameRate` | fps | Frames per second, as float (e.g. 29.970) |
| `FrameRate/String` | — | Frames per second, with measurement (e.g. 29.970 (29970/1000) FPS) |
| `FrameRate_Num` | — | Numerator for determined frames per second (e.g. 29970) |
| `FrameRate_Den` | — | Denominator for determined frames per second (e.g. 1000) |
| `FrameCount` | — | Numer of frames |
| `List_StreamKind` | — | List of programs available |
| `List_StreamPos` | — | List of programs available |
| `List` | — | List of programs available |
| `List/String` | — | List of programs available |
| `Title` | — | Name of this menu |
| `Language` | — | Language, formatted as 2-letter ISO 639-1 if exists, else 3-letter ISO 639-2, and with optional ISO 3166-1 country separated by a dash if available (e.g. en, en-US, en-CN) |
| `Language/String` | — | Language, as full name (e.g. English) |
| `Language/String1` | — | Language, as full name (e.g. English) |
| `Language/String2` | — | Language, formatted as 2-letter ISO 639-1, if exists (e.g. en) |
| `Language/String3` | — | Language, formatted as 3-letter ISO 639-2, if exists (e.g. eng) |
| `Language/String4` | — | Language, formatted as 2-letter ISO 639-1, if exists, with optional ISO 3166-1 country separated by a dash if available (e.g. en-US) |
| `Language_More` | — | More information about Language (e.g. Director's Comment) |
| `ServiceKind` | — | Type of assisted service (e.g. visually impaired, commentary, voice over) |
| `ServiceKind/String` | — | Type of assisted service (e.g. visually impaired, commentary, voice over) |
| `ServiceName` | — | Name of assisted service; Legal |
| `ServiceChannel` | — | Channel of assisted service; Legal |
| `Service/Url` | — | URL of assisted service; Legal |
| `ServiceProvider` | — | Provider of assisted service; Legal |
| `ServiceProvider/Url` | — | URL of provider of assisted service; Legal |
| `ServiceType` | — | Type of assisted service; Legal |
| `NetworkName` | — | Television network name; Legal |
| `Original/NetworkName` | — | Television network name of original broadcast; Legal |
| `Countries` | — | Country information of the content; Legal |
| `TimeZones` | — | TimeZone information of the content; Legal |
| `LawRating` | — | Legal rating of a movie. Format depends on country of origin (e.g. PG, 16); Classification |
| `LawRating_Reason` | — | Reason of the law rating; Classification |
| `Disabled` | Yes | Set if this stream should not be used (Yes, No) |
| `Disabled/String` | — | Set if this stream should not be used (Yes, No) |
| `Default` | Yes | Flag set if this stream should be used if no language found matches the user preference (Yes, No) |
| `Default/String` | — | Flag set if this stream should be used if no language found matches the user preference (Yes, No) |
| `Forced` | Yes | Flag set if this stream should be used regardless of user preferences, often used for sparse subtitle dialogue in an otherwise unsubtitled movie (Yes, No) |
| `Forced/String` | — | Flag set if this stream should be used regardless of user preferences, often used for sparse subtitle dialogue in an otherwise unsubtitled movie (Yes, No) |
| `AlternateGroup` | Yes | Number of a group in order to provide versions of the same track |
| `AlternateGroup/String` | — | Number of a group in order to provide versions of the same track |
| `Chapters_Pos_Begin` | — | Used by third-party developers to know about the beginning of the chapters list, to be used by Get (Stream_Menu, x, Pos), where Pos is an Integer between Chapters_Pos_Begin and Chapters_Pos_End |
| `Chapters_Pos_End` | — | Used by third-party developers to know about the end of the chapters list (this position excluded) |

## Общие поля потоков — Generic

[Оригинальный каталог Generic.csv](https://github.com/MediaArea/MediaInfoLib/blob/b47ab462f9df8a89e971af1ef8753d802c71de8a/Source/Resource/Text/Stream/Generic.csv)

| Точное имя поля | Единица | Описание (оригинал) |
|---|---|---|
| `Format` | — | Не указано в каталоге |
| `Format/String` | — | Не указано в каталоге |
| `Format/Info` | — | Не указано в каталоге |
| `Format/Url` | — | Не указано в каталоге |
| `Format_Commercial` | — | Не указано в каталоге |
| `Format_Commercial_IfAny` | — | Не указано в каталоге |
| `Format_Version` | — | Не указано в каталоге |
| `Format_Profile` | — | Не указано в каталоге |
| `Format_Level` | — | Не указано в каталоге |
| `Format_Tier` | — | Не указано в каталоге |
| `Format_Compression` | — | Не указано в каталоге |
| `Format_Settings` | — | Не указано в каталоге |
| `Format_AdditionalFeatures` | — | Не указано в каталоге |
| `InternetMediaType` | — | Не указано в каталоге |
| `CodecID` | — | Не указано в каталоге |
| `CodecID/Info` | — | Не указано в каталоге |
| `CodecID/Hint` | — | Не указано в каталоге |
| `CodecID/Url` | — | Не указано в каталоге |
| `CodecID_Description` | — | Не указано в каталоге |
| `Codec` | — | Не указано в каталоге |
| `Codec/String` | — | Не указано в каталоге |
| `Codec/Info` | — | Не указано в каталоге |
| `Codec/Url` | — | Не указано в каталоге |
| `Codec/CC` | — | Не указано в каталоге |
| `Width` | — | Не указано в каталоге |
| `Height` | — | Не указано в каталоге |
| `Duration` | — | Не указано в каталоге |
| `Duration/String` | — | Не указано в каталоге |
| `Duration/String1` | — | Не указано в каталоге |
| `Duration/String2` | — | Не указано в каталоге |
| `Duration/String3` | — | Не указано в каталоге |
| `Duration/String4` | — | Не указано в каталоге |
| `Duration/String5` | — | Не указано в каталоге |
| `Source_Duration` | — | Не указано в каталоге |
| `Source_Duration/String` | — | Не указано в каталоге |
| `Source_Duration/String1` | — | Не указано в каталоге |
| `Source_Duration/String2` | — | Не указано в каталоге |
| `Source_Duration/String3` | — | Не указано в каталоге |
| `Source_Duration/String4` | — | Не указано в каталоге |
| `Source_Duration/String5` | — | Не указано в каталоге |
| `BitRate_Mode` | — | Не указано в каталоге |
| `BitRate_Mode/String` | — | Не указано в каталоге |
| `BitRate` | — | Не указано в каталоге |
| `BitRate/String` | — | Не указано в каталоге |
| `BitRate_Minimum` | — | Не указано в каталоге |
| `BitRate_Minimum/String` | — | Не указано в каталоге |
| `BitRate_Nominal` | — | Не указано в каталоге |
| `BitRate_Nominal/String` | — | Не указано в каталоге |
| `BitRate_Maximum` | — | Не указано в каталоге |
| `BitRate_Maximum/String` | — | Не указано в каталоге |
| `BitRate_Encoded` | — | Не указано в каталоге |
| `BitRate_Encoded/String` | — | Не указано в каталоге |
| `FrameRate` | — | Не указано в каталоге |
| `FrameRate/String` | — | Не указано в каталоге |
| `FrameRate_Num` | — | Не указано в каталоге |
| `FrameRate_Den` | — | Не указано в каталоге |
| `FrameCount` | — | Не указано в каталоге |
| `Source_FrameCount` | — | Не указано в каталоге |
| `ColorSpace` | — | Не указано в каталоге |
| `ChromaSubsampling` | — | Не указано в каталоге |
| `Resolution` | — | Не указано в каталоге |
| `Resolution/String` | — | Не указано в каталоге |
| `BitDepth` | — | Не указано в каталоге |
| `BitDepth/String` | — | Не указано в каталоге |
| `Compression_Mode` | — | Не указано в каталоге |
| `Compression_Mode/String` | — | Не указано в каталоге |
| `Compression_Ratio` | — | Не указано в каталоге |
| `Delay` | — | Не указано в каталоге |
| `Delay/String` | — | Не указано в каталоге |
| `Delay/String1` | — | Не указано в каталоге |
| `Delay/String2` | — | Не указано в каталоге |
| `Delay/String3` | — | Не указано в каталоге |
| `Delay/String4` | — | Не указано в каталоге |
| `Delay/String5` | — | Не указано в каталоге |
| `Delay_Settings` | — | Не указано в каталоге |
| `Delay_DropFrame` | — | Не указано в каталоге |
| `Delay_Source` | — | Не указано в каталоге |
| `Delay_Source/String` | — | Не указано в каталоге |
| `Delay_Original` | — | Не указано в каталоге |
| `Delay_Original/String` | — | Не указано в каталоге |
| `Delay_Original/String1` | — | Не указано в каталоге |
| `Delay_Original/String2` | — | Не указано в каталоге |
| `Delay_Original/String3` | — | Не указано в каталоге |
| `Delay_Original/String4` | — | Не указано в каталоге |
| `Delay_Original/String5` | — | Не указано в каталоге |
| `Delay_Original_Settings` | — | Не указано в каталоге |
| `Delay_Original_DropFrame` | — | Не указано в каталоге |
| `Delay_Original_Source` | — | Не указано в каталоге |
| `Video_Delay` | — | Не указано в каталоге |
| `Video_Delay/String` | — | Не указано в каталоге |
| `Video_Delay/String1` | — | Не указано в каталоге |
| `Video_Delay/String2` | — | Не указано в каталоге |
| `Video_Delay/String3` | — | Не указано в каталоге |
| `Video_Delay/String4` | — | Не указано в каталоге |
| `Video_Delay/String5` | — | Не указано в каталоге |
| `TimeCode_FirstFrame` | — | Не указано в каталоге |
| `TimeCode_LastFrame` | — | Не указано в каталоге |
| `TimeCode_DropFrame` | — | Не указано в каталоге |
| `TimeCode_Settings` | — | Не указано в каталоге |
| `TimeCode_Source` | — | Не указано в каталоге |
| `StreamSize` | — | Не указано в каталоге |
| `StreamSize/String` | — | Не указано в каталоге |
| `StreamSize/String1` | — | Не указано в каталоге |
| `StreamSize/String2` | — | Не указано в каталоге |
| `StreamSize/String3` | — | Не указано в каталоге |
| `StreamSize/String4` | — | Не указано в каталоге |
| `StreamSize/String5` | — | Не указано в каталоге |
| `StreamSize_Proportion` | — | Не указано в каталоге |
| `Source_StreamSize` | — | Не указано в каталоге |
| `Source_StreamSize/String` | — | Не указано в каталоге |
| `Source_StreamSize/String1` | — | Не указано в каталоге |
| `Source_StreamSize/String2` | — | Не указано в каталоге |
| `Source_StreamSize/String3` | — | Не указано в каталоге |
| `Source_StreamSize/String4` | — | Не указано в каталоге |
| `Source_StreamSize/String5` | — | Не указано в каталоге |
| `Source_StreamSize_Proportion` | — | Не указано в каталоге |
| `StreamSize_Encoded` | — | Не указано в каталоге |
| `StreamSize_Encoded/String` | — | Не указано в каталоге |
| `StreamSize_Encoded/String1` | — | Не указано в каталоге |
| `StreamSize_Encoded/String2` | — | Не указано в каталоге |
| `StreamSize_Encoded/String3` | — | Не указано в каталоге |
| `StreamSize_Encoded/String4` | — | Не указано в каталоге |
| `StreamSize_Encoded/String5` | — | Не указано в каталоге |
| `StreamSize_Encoded_Proportion` | — | Не указано в каталоге |
| `Source_StreamSize_Encoded` | — | Не указано в каталоге |
| `Source_StreamSize_Encoded/String` | — | Не указано в каталоге |
| `Source_StreamSize_Encoded/String1` | — | Не указано в каталоге |
| `Source_StreamSize_Encoded/String2` | — | Не указано в каталоге |
| `Source_StreamSize_Encoded/String3` | — | Не указано в каталоге |
| `Source_StreamSize_Encoded/String4` | — | Не указано в каталоге |
| `Source_StreamSize_Encoded/String5` | — | Не указано в каталоге |
| `Source_StreamSize_Encoded_Proportion` | — | Не указано в каталоге |
| `Language` | — | Не указано в каталоге |
| `ServiceName` | — | Не указано в каталоге |
| `ServiceProvider` | — | Не указано в каталоге |

## Основные спецификации для сопоставления

| Формат | Спецификация |
|---|---|
| Matroska | [RFC 9559](https://www.rfc-editor.org/rfc/rfc9559.html) |
| WebM | [WebM Container Guidelines](https://www.webmproject.org/docs/container/) |
| MP4 и ISO BMFF | [ISO/IEC 14496-12:2026](https://www.iso.org/obp/ui?_escaped_fragment_=iso%3Astd%3Aiso-iec%3A14496%3A-12%3Aed-8%3Av1%3Aen) |
| Ogg Opus | [RFC 7845](https://www.rfc-editor.org/rfc/rfc7845.html) |
| FLAC | [RFC 9639](https://www.rfc-editor.org/rfc/rfc9639.html) |
| Vorbis comments | [Xiph.org](https://www.xiph.org/vorbis/doc/v-comment.html) |
| ID3v2 | [ID3 frames](https://id3.org/Frames) |
