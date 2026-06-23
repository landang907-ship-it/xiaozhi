// esp32s3_audio_codec.h
// AudioCodec subclass cho ESP32-S3-WROOM-1.
// Bridges I2S_NUM_0 (spk, 16-bit) + I2S_NUM_1 (mic, 32-bit slot for 24-bit INMP441).
// Phase 2.2.

#ifndef _ESP32S3_AUDIO_CODEC_H
#define _ESP32S3_AUDIO_CODEC_H

#include "audio_codec.h"

class Esp32S3AudioCodec : public AudioCodec {
public:
    // speaker_handle: I2S_NUM_0 TX (16-bit slot, MAX98357A)
    // mic_handle:    I2S_NUM_1 RX (32-bit slot, INMP441 24-bit data left-justified)
    Esp32S3AudioCodec(i2s_chan_handle_t speaker_handle,
                      i2s_chan_handle_t mic_handle,
                      int input_sample_rate,
                      int output_sample_rate);
    virtual ~Esp32S3AudioCodec();

    virtual void Start() override;
    virtual void EnableInput(bool enable) override;
    virtual void EnableOutput(bool enable) override;

private:
    virtual int Read(int16_t* dest, int samples) override;
    virtual int Write(const int16_t* data, int samples) override;
};

#endif // _ESP32S3_AUDIO_CODEC_H
