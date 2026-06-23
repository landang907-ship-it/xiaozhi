// audio_codec.cc
// Definitions for the AudioCodec base class declared in audio_codec.h.
//
// Phase 2.4e-2c fix: previously only the header existed; the linker complained
// about undefined references to AudioCodec::AudioCodec(...) and
// AudioCodec::~AudioCodec() because no .cc file provided their bodies.
//
// The constructor just stores the handles/rates; the destructor is the default
// virtual dtor (subclasses own their DMA buffers).

#include "audio_codec.h"

AudioCodec::AudioCodec(i2s_chan_handle_t speaker_handle,
                       i2s_chan_handle_t mic_handle,
                       int input_sample_rate,
                       int output_sample_rate)
    : tx_handle_(speaker_handle),
      rx_handle_(mic_handle),
      input_sample_rate_(input_sample_rate),
      output_sample_rate_(output_sample_rate) {
    // Defaults for the rest of the fields match the in-class initializers in
    // the header; nothing else to do here.
}

AudioCodec::~AudioCodec() = default;

// Default no-op implementations for non-pure virtual hooks. Concrete
// subclasses (e.g. Esp32S3AudioCodec) override these if they need to do
// anything beyond toggling the input_enabled_ / output_enabled_ flags.
void AudioCodec::Start() {}

void AudioCodec::EnableInput(bool enable) {
    input_enabled_ = enable;
}

void AudioCodec::EnableOutput(bool enable) {
    output_enabled_ = enable;
}

void AudioCodec::SetOutputVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    output_volume_ = volume;
}
