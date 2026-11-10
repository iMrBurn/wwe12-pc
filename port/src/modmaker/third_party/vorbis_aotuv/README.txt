aoTuV beta6.03-2020 (https://github.com/AO-Yumi/vorbis_aotuv, tag beta6.03-2020, commit 1eda0f0; libvorbis by
Xiph.org with Aoyumi's tuning, BSD 3-Clause, see COPYING): Vorbis encoder of the Mod Maker, used to write ported
entrance themes like the disc's Wwise Vorbis themes (quality 0.2 at 32 kHz stereo gives the disc themes' exact setup).
Only the files the encoder needs are kept (lib/*.c of libvorbis, lib/books, lib/modes, include/vorbis/codec.h,
vorbisenc.h), unchanged.
