#include "actions/windows_media_service.h"

namespace strokes::actions {

std::vector<INPUT> WindowsMediaService::make_inputs(MediaOperation operation) {
  WORD key{};
  switch (operation) {
    case MediaOperation::play_pause: key = VK_MEDIA_PLAY_PAUSE; break;
    case MediaOperation::next_track: key = VK_MEDIA_NEXT_TRACK; break;
    case MediaOperation::previous_track: key = VK_MEDIA_PREV_TRACK; break;
    case MediaOperation::stop: key = VK_MEDIA_STOP; break;
  }
  if (key == 0) return {};
  INPUT down{}, up{};
  down.type = up.type = INPUT_KEYBOARD;
  down.ki.wVk = up.ki.wVk = key;
  up.ki.dwFlags = KEYEVENTF_KEYUP;
  return {down, up};
}

ActionResult WindowsMediaService::perform(MediaOperation operation) {
  auto inputs = make_inputs(operation);
  if (inputs.empty())
    return ActionResult::failed(ActionError::unsupported_operation, "unknown_media_operation",
                                "The media operation is unsupported.");
  const UINT sent = sender_(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
  if (sent == inputs.size()) return ActionResult::succeeded();
  if (sent == 1) (void)sender_(1, &inputs[1], sizeof(INPUT));
  return ActionResult::failed(ActionError::platform_failure, "media_input_failed",
                              "Windows did not accept the media command.");
}

}  // namespace strokes::actions
