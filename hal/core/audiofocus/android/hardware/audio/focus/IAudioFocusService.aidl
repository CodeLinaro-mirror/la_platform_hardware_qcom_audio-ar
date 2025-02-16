/*
* Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

package android.hardware.audio.focus;
import android.hardware.audio.focus.IStreamUpdateCallback;
import android.media.audio.common.AudioUsage;
import android.hardware.audio.focus.IFocusSession;
import android.media.audio.common.AudioDevice;


@VintfStability
interface IAudioFocusService {
    IFocusSession requestFocus(IStreamUpdateCallback callback, AudioUsage usage, in AudioDevice device, float gain);
    void abandonFocus(in IFocusSession focusSession);
}
