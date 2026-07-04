#include "AlarmDisplayEngine.h"

#include "../../../../config/UIColors.h"

namespace ALARMS {

namespace {

IconType iconForSeverity(AlarmSeverity severity) {
    switch (severity) {
        case AlarmSeverity::Critical:
            return IconType::ALARM_CRITICAL;
        case AlarmSeverity::Warning:
            return IconType::ALARM_WARNING;
        case AlarmSeverity::Info:
        default:
            return IconType::ALARM_INFO;
    }
}

uint32_t colorForSeverity(AlarmSeverity severity) {
    switch (severity) {
        case AlarmSeverity::Critical:
            return UI::COLOR::ALARM_CRITICAL;
        case AlarmSeverity::Warning:
            return UI::COLOR::ALARM_WARNING;
        case AlarmSeverity::Info:
        default:
            return UI::COLOR::ALARM_INFO;
    }
}

AlarmDisplayResult buildIconResult(const AlarmDisplaySnapshot& snapshot) {
    AlarmDisplayResult result;
    result.clearLayer = false;
    result.content.active = true;
    result.content.type = CommandType::SHOW_ICON;
    result.content.icon = iconForSeverity(snapshot.severity);
    return result;
}

AlarmDisplayResult buildSolidResult(const AlarmDisplaySnapshot& snapshot) {
    AlarmDisplayResult result;
    result.clearLayer = false;
    result.content.active = true;
    result.content.type = CommandType::SHOW_SOLID;
    result.content.color = colorForSeverity(snapshot.severity);
    return result;
}

AlarmDisplayResult buildScrollTextResult(const AlarmDisplaySnapshot& snapshot) {
    if (!snapshot.alarmName || snapshot.alarmName[0] == '\0') {
        return buildIconResult(snapshot);
    }

    AlarmDisplayResult result;
    result.clearLayer = false;
    result.content.active = true;
    result.content.type = CommandType::SHOW_TEXT;
    result.content.color = colorForSeverity(snapshot.severity);
    strlcpy(result.content.text, snapshot.alarmName, sizeof(result.content.text));
    return result;
}

}  // namespace

AlarmDisplayResult AlarmDisplayEngine::build(
    RTC::MatrixAlarmMode mode,
    const AlarmDisplaySnapshot& snapshot) const {
    if (!snapshot.active) {
        return AlarmDisplayResult{};
    }

    switch (mode) {
        case RTC::MatrixAlarmMode::ICON:
            return buildIconResult(snapshot);
        case RTC::MatrixAlarmMode::SCROLL_TEXT:
            return buildScrollTextResult(snapshot);
        case RTC::MatrixAlarmMode::SOLID_COLOR:
        default:
            return buildSolidResult(snapshot);
    }
}

}  // namespace ALARMS
