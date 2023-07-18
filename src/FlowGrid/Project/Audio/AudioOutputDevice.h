#pragma once

#include "AudioDevice.h"

struct AudioOutputDevice : AudioDevice {
    AudioOutputDevice(ComponentArgs &&, AudioCallback);
    ~AudioOutputDevice();

    ma_device *Get() const override;
    inline IO GetIoType() const override { return IO_Out; }

protected:
    void Init() override;
    void Uninit() override;
};
