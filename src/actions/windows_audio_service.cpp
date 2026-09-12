#include "actions/windows_audio_service.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>

#include <algorithm>
#include <string>

namespace strokes::actions {
namespace {

ActionResult com_failure(std::string code, std::string message, HRESULT result) {
  return ActionResult::failed(ActionError::platform_failure, std::move(code),
                              std::move(message) + " (HRESULT " +
                                  std::to_string(static_cast<unsigned long>(result)) + ").");
}

}  // namespace

float WindowsAudioService::adjusted_level(float current, VolumeOperation operation,
                                          std::optional<double> amount) noexcept {
  const float step = static_cast<float>(amount.value_or(2.0) / 100.0);
  const float next = operation == VolumeOperation::increase ? current + step : current - step;
  return std::clamp(next, 0.0F, 1.0F);
}

ActionResult WindowsAudioService::perform(VolumeOperation operation,
                                          std::optional<double> amount) {
  const HRESULT initialized = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  const bool uninitialize = initialized == S_OK || initialized == S_FALSE;
  if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
    return com_failure("audio_com_failed", "Windows audio initialization failed", initialized);

  IMMDeviceEnumerator* enumerator = nullptr;
  HRESULT result = ::CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                      IID_PPV_ARGS(&enumerator));
  if (FAILED(result)) {
    if (uninitialize) ::CoUninitialize();
    return com_failure("audio_enumerator_failed", "The audio device enumerator is unavailable",
                       result);
  }
  IMMDevice* device = nullptr;
  result = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device);
  enumerator->Release();
  if (FAILED(result)) {
    if (uninitialize) ::CoUninitialize();
    return com_failure("audio_device_unavailable", "No default output audio device is available",
                       result);
  }
  IAudioEndpointVolume* volume = nullptr;
  result = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                            reinterpret_cast<void**>(&volume));
  device->Release();
  if (FAILED(result)) {
    if (uninitialize) ::CoUninitialize();
    return com_failure("audio_volume_unavailable", "Output volume control is unavailable", result);
  }

  if (operation == VolumeOperation::mute_toggle) {
    BOOL muted = FALSE;
    result = volume->GetMute(&muted);
    if (SUCCEEDED(result)) result = volume->SetMute(!muted, nullptr);
  } else {
    float current = 0.0F;
    result = volume->GetMasterVolumeLevelScalar(&current);
    if (SUCCEEDED(result)) {
      result = volume->SetMasterVolumeLevelScalar(adjusted_level(current, operation, amount),
                                                  nullptr);
    }
  }
  volume->Release();
  if (uninitialize) ::CoUninitialize();
  if (FAILED(result))
    return com_failure("audio_operation_failed", "Windows could not change output volume", result);
  return ActionResult::succeeded();
}

}  // namespace strokes::actions
