#pragma once

#include "AudioDevice.h"

struct AudioInputDevice : AudioDevice {
    AudioInputDevice(ComponentArgs &&, AudioCallback, UserData user_data = nullptr);
    ~AudioInputDevice();

    ma_device *Get() const override;
    inline IO GetIoType() const override { return IO_In; }

protected:
    void Init() override;
    void Uninit() override;
};
