ocaml-ffmpeg
============

ocaml-ffmpeg is an OCaml interface for the [FFmpeg](http://ffmpeg.org/) Multimedia framework.

The modules currently available are :

`Avutil` : base module containing the share types and utilities

`Avcodec` : the module containing decoders and encoders for audio, video and subtitle codecs.

`Av` : the module containing demuxers and muxers for reading and writing multimedia container formats.

`Avdevice` : the module containing input and output devices for grabbing from and rendering to many common multimedia input/output software frameworks.

`Avfilter` : the module containing audio and video filters.

`Swresample` : the module performing audio resampling, rematrixing and sample format conversion operations.
	
`Swscale` : the module performing image scaling and color space/pixel format conversion operations.

`Avdevice` : the module containing input and output devices for grabbing from and rendering to many common multimedia input/output software frameworks.


Please read the COPYING file before using this software.


Prerequisites:
==============

- ocaml >= 4.05.0
- FFmpeg >= 3.0
- dune >= 2.0
- findlib >= 0.8.1

Compilation:
============

	$ dune build

This should build both the native and the byte-code version of the
extension library.


Installation:
=============

	$ dune install

This should install the library file (using ocamlfind) in the
appropriate place.


Documentation:
=============

The [API documentation is available here](https://www.liquidsoap.info/ocaml-ffmpeg/docs/api/index.html).


Examples:
=============

The [audio_decoding](https://github.com/savonet/ocaml-ffmpeg/blob/master/examples/audio_decoding/audio_decoding.ml) example shows how to read frames from an audio file and convert them into bytes.

The [audio_device](https://github.com/savonet/ocaml-ffmpeg/blob/master/examples/audio_device/audio_device.ml) example shows how to read 500 audio frames from an input audio device or an URL and write them into an output audio device or a file.

The [decode_audio](https://github.com/savonet/ocaml-ffmpeg/blob/master/examples/decode_audio/decode_audio.ml) example shows how to parse packets from a mapped file, decode them and write the resulting frames into a file.

The [demuxing_decoding](https://github.com/savonet/ocaml-ffmpeg/blob/master/examples/demuxing_decoding/demuxing_decoding.ml) example shows how to demuxing and decoding audio, video and subtitle frames from a file, converts them into bytes and write them in raw files.

The [encode_audio](https://github.com/savonet/ocaml-ffmpeg/blob/master/examples/encode_audio/encode_audio.ml) example shows how to convert a float array into stereo frames and encode them into packets.

The [encode_video](https://github.com/savonet/ocaml-ffmpeg/blob/master/examples/encode_video/encode_video.ml) example shows how to create video frames and write them encoded into a file.

The [encoding](https://github.com/savonet/ocaml-ffmpeg/blob/master/examples/encoding/encoding.ml) example shows how to create a multimedia file with audio and video streams.

The [player](https://github.com/savonet/ocaml-ffmpeg/blob/master/examples/player/player.ml) example shows how to read a multimedia file and write audio and video frames to output devices.

The [remuxing](https://github.com/savonet/ocaml-ffmpeg/blob/master/examples/remuxing/remuxing.ml) example shows how to remuxing multimedia file packets without decoding them.

The [transcode_aac](https://github.com/savonet/ocaml-ffmpeg/blob/master/examples/transcode_aac/transcode_aac.ml) example shows how to transcode an audio file into an AAC audio file.

The [transcoding](https://github.com/savonet/ocaml-ffmpeg/blob/master/examples/transcoding/transcoding.ml) example shows how to transcode audio streams into the AAC codec, video streams into the H264 codec and write them to an output file.


Author:
=======

This author of this software may be contacted by electronic mail
at the following address: savonet-users@lists.sourceforge.net.
