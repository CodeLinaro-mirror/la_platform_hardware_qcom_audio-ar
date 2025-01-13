/*
* Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

package android.hardware.audio.focus;
import android.media.audio.common.AudioUsage;
import android.media.audio.common.AudioDevice;

@VintfStability
interface IStreamUpdateCallback {
  void onMetadataUpdated(boolean doDuck, float gain);
}
