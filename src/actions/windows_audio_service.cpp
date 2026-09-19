#include "actions/windows_audio_service.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>

namespace strokes::actions {
namespace {

ActionResult com_failure(std::string code, std::string message, HRESULT result) {
  return ActionResult::failed(ActionError::platform_failure, std::move(code),
                              std::move(message) + " (HRESULT " +
                                  std::to_string(static_cast<unsigned long>(result)) + ").");
}

ActionResult with_endpoint_volume(
    const std::function<HRESULT(IAudioEndpointVolume*)>& operation) {
  const HRESULT initialized = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  const bool uninitialize = initialized == S_OK || initialized == S_FALSE;
  if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
    return com_failure("audio_com_failed", "Windows audio initialization failed", initialized);

  IMMDeviceEnumerator* enumerator = nullptr;
  HRESULT result = ::CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                      IID_PPV_ARGS(&enumerator));
  if (SUCCEEDED(result)) {
    IMMDevice* device = nullptr;
    result = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device);
    enumerator->Release();
    if (SUCCEEDED(result)) {
      IAudioEndpointVolume* volume = nullptr;
      result = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                                reinterpret_cast<void**>(&volume));
      device->Release();
      if (SUCCEEDED(result)) {
        result = operation(volume);
        volume->Release();
      }
    }
  }
  if (uninitialize) ::CoUninitialize();
  if (FAILED(result))
    return com_failure("audio_operation_failed", "Windows audio operation failed", result);
  return ActionResult::succeeded();
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
  return with_endpoint_volume([&](IAudioEndpointVolume* endpoint) {
    if (operation == VolumeOperation::mute_toggle) {
      BOOL muted = FALSE;
      HRESULT result = endpoint->GetMute(&muted);
      return SUCCEEDED(result) ? endpoint->SetMute(!muted, nullptr) : result;
    }
    float current = 0.0F;
    HRESULT result = endpoint->GetMasterVolumeLevelScalar(&current);
    return SUCCEEDED(result)
               ? endpoint->SetMasterVolumeLevelScalar(adjusted_level(current, operation, amount),
                                                       nullptr)
               : result;
  });
}

std::optional<double> WindowsAudioService::volume() const {
  float value = 0.0F;
  const auto result = with_endpoint_volume(
      [&](IAudioEndpointVolume* endpoint) { return endpoint->GetMasterVolumeLevelScalar(&value); });
  return result.success ? std::optional<double>{value * 100.0} : std::nullopt;
}

ActionResult WindowsAudioService::set_volume(double value) {
  if (!std::isfinite(value) || value < 0.0 || value > 100.0)
    return ActionResult::failed(ActionError::invalid_definition, "invalid_volume",
                                "Volume must be between 0 and 100.");
  return with_endpoint_volume([&](IAudioEndpointVolume* endpoint) {
    return endpoint->SetMasterVolumeLevelScalar(static_cast<float>(value / 100.0), nullptr);
  });
}

std::optional<bool> WindowsAudioService::is_muted() const {
  BOOL muted = FALSE;
  const auto result = with_endpoint_volume(
      [&](IAudioEndpointVolume* endpoint) { return endpoint->GetMute(&muted); });
  return result.success ? std::optional<bool>{muted != FALSE} : std::nullopt;
}

}  // namespace strokes::actions
