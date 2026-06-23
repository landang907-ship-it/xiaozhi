// audio_codec.h
// Abstract base class for board audio codecs.
// Phase 2.2 (base class) - never extracted before, blocking P2.2 build.
//
// Concrete subclass (e.g. Esp32S3AudioCodec) implements Read()/Write() that
// do the actual I2S DMA transfer. Base class owns the state + enable/volume
// logic and provides safe Start/EnableInput/EnableOutput defaults.

#ifndef _AUDIO_CODEC_H
#define _AUDIO_CODEC_H

#include <driver/i2s_std.h>
#include <stdint.h>

class AudioCodec {
public:
    AudioCodec(i2s_chan_handle_t speaker_handle,
               i2s_chan_handle_t mic_handle,
               int input_sample_rate,
               int output_sample_rate);
    virtual ~AudioCodec();

    // Activate codec (channels already enabled by board HAL; this is a hook
    // for chips that need extra codec chip config beyond DMA setup).
    virtual void Start();

    // Enable/disable mic input (gate I2S read + flag).
    virtual void EnableInput(bool enable);

    // Enable/disable speaker output (gate I2S write + flag).
    virtual void EnableOutput(bool enable);

    // Set speaker output volume (0..100). Clamped silently.
    virtual void SetOutputVolume(int volume);

    // Public read/write API for external callers (e.g. a loopback or
    // transport task). They forward to the protected virtuals, so derived
    // classes still implement the actual DMA transfer once.
    inline int  ReadSamples(int16_t* dest, int samples) {
        return Read(dest, samples);
    }
    inline int  WriteSamples(const int16_t* data, int samples) {
        return Write(data, samples);
    }

    inline int input_sample_rate() const { return input_sample_rate_; }
    inline int output_sample_rate() const { return output_sample_rate_; }
    inline int input_channels() const { return input_channels_; }
    inline int output_channels() const { return output_channels_; }
    inline int output_volume() const { return output_volume_; }
    inline bool duplex() const { return duplex_; }
    inline bool input_enabled() const { return input_enabled_; }
    inline bool output_enabled() const { return output_enabled_; }
    inline float input_gain() const { return input_gain_; }
    inline i2s_chan_handle_t speaker_handle() const { return tx_handle_; }
    inline i2s_chan_handle_t mic_handle() const { return rx_handle_; }

protected:
    // Blocking read: read up to `samples` 16-bit mono samples into `dest`.
    // Returns number of samples actually read, 0 on error/disabled.
    virtual int Read(int16_t* dest, int samples) = 0;

    // Blocking write: write `samples` 16-bit mono samples from `data`.
    // Returns number of samples actually written.
    virtual int Write(const int16_t* data, int samples) = 0;

    i2s_chan_handle_t tx_handle_ = nullptr;
    i2s_chan_handle_t rx_handle_ = nullptr;

    int input_sample_rate_ = 0;
    int output_sample_rate_ = 0;
    int input_channels_ = 1;
    int output_channels_ = 1;
    int output_volume_ = 70;        // 0..100
    float input_gain_ = 1.0f;       // 1.0 = passthrough
    bool duplex_ = false;
    bool input_enabled_ = false;
    bool output_enabled_ = false;
};

#endif // _AUDIO_CODEC_H
