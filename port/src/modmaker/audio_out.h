// Mod Maker: a song played from any point (the entrance preview's theme), with XAudio2. After Kaiko's audio_out
// (his SvR 2011 Mod Maker, port/modmaker/audio_out.cpp, MIT, with credit), here fed 16-bit samples already decoded
// (theme_convert::Decode) instead of a file read with Media Foundation. One song at a time, from the UI thread.
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <xaudio2.h>

#include <cstdint>
#include <memory>
#include <vector>

#pragma comment(lib, "xaudio2.lib")

namespace audio_out {

class Player {
 public:
  ~Player() { Unload(); }

  // The song (interleaved 16-bit samples); a new one stops the one before.
  void Set(std::vector<int16_t> samples, int channels, int rate) {
    Unload();
    if (channels <= 0 || rate <= 0 || samples.empty())
      return;
    song_ = std::make_shared<std::vector<int16_t>>(std::move(samples));
    fmt_ = {};
    fmt_.wFormatTag = WAVE_FORMAT_PCM;
    fmt_.nChannels = WORD(channels);
    fmt_.nSamplesPerSec = DWORD(rate);
    fmt_.wBitsPerSample = 16;
    fmt_.nBlockAlign = WORD(2 * channels);
    fmt_.nAvgBytesPerSec = fmt_.nSamplesPerSec * fmt_.nBlockAlign;
  }
  bool Ready() const { return song_ != nullptr; }
  double Length() const { return song_ ? double(song_->size()) / fmt_.nChannels / fmt_.nSamplesPerSec : 0; }

  // Plays from `at` seconds (also while playing: a jump); past the end: silence.
  void Play(double at) {
    if (!song_ || !Voice())
      return;
    voice_->Stop();
    voice_->FlushSourceBuffers();
    playing_ = false;
    const UINT32 frames = UINT32(song_->size() / fmt_.nChannels);
    const double begin = at * fmt_.nSamplesPerSec;
    if (begin < 0 || begin >= frames)
      return;
    XAUDIO2_BUFFER b = {};
    b.Flags = XAUDIO2_END_OF_STREAM;
    b.AudioBytes = UINT32(song_->size() * 2);
    b.pAudioData = reinterpret_cast<const BYTE *>(song_->data());
    b.PlayBegin = UINT32(begin);
    if (SUCCEEDED(voice_->SubmitSourceBuffer(&b)) && SUCCEEDED(voice_->Start()))
      playing_ = true;
  }
  void Pause() {
    if (voice_)
      voice_->Stop();
    playing_ = false;
  }
  bool Playing() const { return playing_; }
  void SetVolume(float v) {
    if (voice_)
      voice_->SetVolume(v);
  }

  void Unload() {
    if (voice_)
      voice_->Stop(), voice_->FlushSourceBuffers(), voice_->DestroyVoice(), voice_ = nullptr;
    playing_ = false;
    song_.reset();
  }

 private:
  bool Voice() {
    if (!xa_) {
      if (FAILED(XAudio2Create(&xa_, 0, XAUDIO2_DEFAULT_PROCESSOR)))
        return xa_ = nullptr, false;
      if (FAILED(xa_->CreateMasteringVoice(&master_)))
        return master_ = nullptr, false;
    }
    if (voice_) {
      XAUDIO2_VOICE_DETAILS d;
      voice_->GetVoiceDetails(&d);
      if (d.InputChannels == fmt_.nChannels && d.InputSampleRate == fmt_.nSamplesPerSec)
        return true;
      voice_->DestroyVoice(), voice_ = nullptr;
    }
    return SUCCEEDED(xa_->CreateSourceVoice(&voice_, &fmt_));
  }

  IXAudio2 *xa_ = nullptr;  // (kept for the Mod Maker's life: XAudio2 is set up once)
  IXAudio2MasteringVoice *master_ = nullptr;
  IXAudio2SourceVoice *voice_ = nullptr;
  std::shared_ptr<std::vector<int16_t>> song_;
  WAVEFORMATEX fmt_ = {};
  bool playing_ = false;
};

}  // namespace audio_out
