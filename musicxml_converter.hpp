#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <regex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "lib/pugixml/pugixml.hpp"

namespace musicxml_arduino
{

inline constexpr const char* kVersion = "v2.2";
constexpr double kDefaultBpm = 80.0;
constexpr int kStringCount = 6;
constexpr int kFretCount = 7;

struct NoteTarget
{
    int string_number = -1;
    int fret_number = -1;
    bool is_rest = false;
};

struct SongEvent
{
    std::string measure_number;
    double start_quarters = 0.0;
    double end_quarters = 0.0;
    double bpm = kDefaultBpm;
    // 下标 0 表示空弦，下标 1-6 表示第 1-6 品；低 6 位对应第 1-6 弦。
    std::array<std::uint8_t, kFretCount> string_masks_by_fret{};
    std::uint32_t duration_ms = 0;
    std::vector<NoteTarget> notes;
};

struct TempoChange
{
    double position_quarters = 0.0;
    double bpm = kDefaultBpm;
    std::size_t order = 0;
};

struct ConversionResult
{
    std::string title;
    std::vector<SongEvent> events;
    std::vector<TempoChange> tempo_changes;
    std::vector<std::string> warnings;
    std::size_t note_count = 0;
    std::size_t mapped_note_count = 0;
    std::size_t nonzero_event_count = 0;
    std::size_t zero_event_count = 0;
    bool used_default_tempo = false;
};

inline bool nearly_equal(double left, double right)
{
    return std::fabs(left - right) < 1e-9;
}

inline bool first_positive_number(const std::string& text, double& value)
{
    static const std::regex number_pattern(
        R"(([0-9]+(?:\.[0-9]+)?))");
    std::smatch match;
    if (!std::regex_search(text, match, number_pattern))
        return false;

    try
    {
        value = std::stod(match.str(1));
    }
    catch (...)
    {
        return false;
    }
    return std::isfinite(value) && value > 0.0;
}

inline double beat_unit_in_quarters(const std::string& beat_unit)
{
    if (beat_unit == "maxima")  return 32.0;
    if (beat_unit == "long")    return 16.0;
    if (beat_unit == "breve")   return 8.0;
    if (beat_unit == "whole")   return 4.0;
    if (beat_unit == "half")    return 2.0;
    if (beat_unit == "quarter") return 1.0;
    if (beat_unit == "eighth")  return 0.5;
    if (beat_unit == "16th")    return 0.25;
    if (beat_unit == "32nd")    return 0.125;
    if (beat_unit == "64th")    return 0.0625;
    if (beat_unit == "128th")   return 0.03125;
    if (beat_unit == "256th")   return 0.015625;
    if (beat_unit == "512th")   return 0.0078125;
    if (beat_unit == "1024th")  return 0.00390625;
    return 0.0;
}

inline bool tempo_from_metronome(
    const pugi::xml_node& direction, double& quarter_bpm)
{
    pugi::xml_node metronome =
        direction.child("direction-type").child("metronome");
    if (!metronome)
        return false;

    double per_minute = 0.0;
    if (!first_positive_number(
            metronome.child("per-minute").text().as_string(), per_minute))
        return false;

    double beat_length = beat_unit_in_quarters(
        metronome.child("beat-unit").text().as_string());
    if (beat_length <= 0.0)
        return false;

    int dot_count = 0;
    for (pugi::xml_node dot : metronome.children("beat-unit-dot"))
    {
        (void)dot;
        ++dot_count;
    }

    double dot_addition = beat_length / 2.0;
    for (int index = 0; index < dot_count; ++index)
    {
        beat_length += dot_addition;
        dot_addition /= 2.0;
    }

    quarter_bpm = per_minute * beat_length;
    return std::isfinite(quarter_bpm) && quarter_bpm > 0.0;
}

inline double playback_offset_in_quarters(
    const pugi::xml_node& direction,
    const pugi::xml_node& sound,
    int divisions)
{
    if (divisions <= 0)
        return 0.0;

    pugi::xml_node sound_offset = sound.child("offset");
    if (sound_offset)
        return sound_offset.text().as_double() / divisions;

    pugi::xml_node direction_offset = direction.child("offset");
    if (direction_offset &&
        std::string(direction_offset.attribute("sound").value()) == "yes")
    {
        return direction_offset.text().as_double() / divisions;
    }
    return 0.0;
}

inline void add_tempo_change(
    std::vector<TempoChange>& changes,
    double position,
    double bpm,
    std::size_t& order)
{
    if (std::isfinite(bpm) && bpm > 0.0)
        changes.push_back({position, bpm, order++});
}

inline int output_bit_for(int string_number, int fret_number)
{
    if (string_number < 1 || string_number > kStringCount)
        return -1;
    if (fret_number < 0 || fret_number >= kFretCount)
        return -1;
    return fret_number * kStringCount + string_number - 1;
}

inline double tempo_at(
    const std::vector<TempoChange>& changes,
    double position,
    bool& used_default)
{
    double bpm = kDefaultBpm;
    bool found = false;
    for (const TempoChange& change : changes)
    {
        if (change.position_quarters <= position + 1e-9)
        {
            bpm = change.bpm;
            found = true;
        }
        else
        {
            break;
        }
    }
    if (!found)
        used_default = true;
    return bpm;
}

inline std::uint32_t duration_in_milliseconds(
    const SongEvent& event,
    const std::vector<TempoChange>& changes,
    bool& used_default)
{
    double position = event.start_quarters;
    const double end = event.end_quarters;
    double milliseconds = 0.0;

    while (position < end - 1e-9)
    {
        const double bpm = tempo_at(changes, position, used_default);
        double segment_end = end;
        for (const TempoChange& change : changes)
        {
            if (change.position_quarters > position + 1e-9)
            {
                segment_end = std::min(segment_end,
                                       change.position_quarters);
                break;
            }
        }
        milliseconds +=
            (segment_end - position) * 60000.0 / bpm;
        position = segment_end;
    }

    if (!std::isfinite(milliseconds) || milliseconds <= 0.0)
        return 0;
    if (milliseconds >
        static_cast<double>(std::numeric_limits<std::uint32_t>::max()))
        return 0;

    return static_cast<std::uint32_t>(std::llround(milliseconds));
}

inline bool apply_output_masks(
    SongEvent& event,
    ConversionResult& result,
    std::string& error)
{
    std::array<int, kStringCount + 1> fret_for_string{};
    fret_for_string.fill(-1);

    for (const NoteTarget& note : event.notes)
    {
        if (note.is_rest ||
            note.fret_number < 0 ||
            note.fret_number >= kFretCount)
            continue;

        if (note.string_number < 1 || note.string_number > kStringCount)
        {
            error = "第 " + event.measure_number +
                    " 小节存在空弦或第 1-6 品音符，但弦号不在 1-6 范围内。";
            return false;
        }

        int& existing_fret = fret_for_string[note.string_number];
        if (existing_fret >= 0 && existing_fret != note.fret_number)
        {
            error = "第 " + event.measure_number + " 小节的同一个和弦要求第 " +
                    std::to_string(note.string_number) +
                    " 弦同时按不同品位。";
            return false;
        }
        existing_fret = note.fret_number;

        event.string_masks_by_fret[note.fret_number] |=
            static_cast<std::uint8_t>(1u << (note.string_number - 1));
        ++result.mapped_note_count;
    }
    return true;
}

inline bool convert_document(
    const pugi::xml_document& document,
    ConversionResult& result,
    std::string& error)
{
    result = ConversionResult{};
    error.clear();

    pugi::xml_node score = document.child("score-partwise");
    if (!score)
    {
        error = "文件中没有找到 <score-partwise> 根节点。";
        return false;
    }

    result.title = score.child("work").child("work-title").text().as_string();
    if (result.title.empty())
        result.title = score.child("movement-title").text().as_string();

    pugi::xml_node part = score.child("part");
    if (!part)
    {
        error = "没有找到第一声部 <part>。";
        return false;
    }

    int divisions = 0;
    double absolute_quarters = 0.0;
    std::size_t tempo_order = 0;
    int measure_index = 0;

    for (pugi::xml_node measure : part.children("measure"))
    {
        ++measure_index;
        std::string measure_number = measure.attribute("number").as_string();
        if (measure_number.empty())
            measure_number = std::to_string(measure_index);

        double cursor_quarters = 0.0;
        double furthest_quarters = 0.0;
        std::size_t last_event_index = result.events.size();

        for (pugi::xml_node child : measure.children())
        {
            const std::string child_name = child.name();

            if (child_name == "attributes")
            {
                pugi::xml_node divisions_node = child.child("divisions");
                if (divisions_node)
                {
                    const int new_divisions = divisions_node.text().as_int();
                    if (new_divisions <= 0)
                    {
                        error = "第 " + measure_number +
                                " 小节的 divisions 必须大于 0。";
                        return false;
                    }
                    divisions = new_divisions;
                }
                continue;
            }

            if (child_name == "direction")
            {
                if (divisions <= 0)
                {
                    error = "读取速度前没有有效的 divisions。";
                    return false;
                }

                pugi::xml_node sound = child.child("sound");
                double bpm = 0.0;
                bool has_bpm = false;
                if (sound && sound.attribute("tempo"))
                {
                    bpm = sound.attribute("tempo").as_double();
                    has_bpm = std::isfinite(bpm) && bpm > 0.0;
                    if (!has_bpm)
                        result.warnings.push_back(
                            "第 " + measure_number +
                            " 小节的 <sound tempo> 无效，已忽略。" );
                }
                else
                {
                    has_bpm = tempo_from_metronome(child, bpm);
                }

                if (has_bpm)
                {
                    const double offset = playback_offset_in_quarters(
                        child, sound, divisions);
                    add_tempo_change(
                        result.tempo_changes,
                        absolute_quarters + cursor_quarters + offset,
                        bpm,
                        tempo_order);
                }
                continue;
            }

            if (child_name == "sound" && child.attribute("tempo"))
            {
                if (divisions <= 0)
                {
                    error = "读取速度前没有有效的 divisions。";
                    return false;
                }
                const double bpm = child.attribute("tempo").as_double();
                if (std::isfinite(bpm) && bpm > 0.0)
                {
                    const double offset = child.child("offset")
                        ? child.child("offset").text().as_double() / divisions
                        : 0.0;
                    add_tempo_change(
                        result.tempo_changes,
                        absolute_quarters + cursor_quarters + offset,
                        bpm,
                        tempo_order);
                }
                else
                {
                    result.warnings.push_back(
                        "第 " + measure_number +
                        " 小节的 <sound tempo> 无效，已忽略。" );
                }
                continue;
            }

            if (child_name == "backup")
            {
                error = "第 " + measure_number +
                        " 小节包含 <backup>，说明乐谱可能有多个声线；"
                        "本阶段只支持单声线时间轴。";
                return false;
            }

            if (child_name == "forward")
            {
                if (divisions <= 0)
                {
                    error = "读取 <forward> 前没有有效的 divisions。";
                    return false;
                }
                const int duration = child.child("duration").text().as_int();
                if (duration <= 0)
                {
                    error = "第 " + measure_number +
                            " 小节的 <forward> duration 必须大于 0。";
                    return false;
                }
                SongEvent silence;
                silence.measure_number = measure_number;
                silence.start_quarters = absolute_quarters + cursor_quarters;
                cursor_quarters +=
                    static_cast<double>(duration) / divisions;
                silence.end_quarters = absolute_quarters + cursor_quarters;
                result.events.push_back(std::move(silence));
                furthest_quarters = std::max(
                    furthest_quarters, cursor_quarters);
                last_event_index = result.events.size();
                continue;
            }

            if (child_name != "note")
                continue;

            if (divisions <= 0)
            {
                error = "第 " + measure_number +
                        " 小节的音符之前没有有效的 divisions。";
                return false;
            }

            const int duration = child.child("duration").text().as_int();
            if (duration <= 0)
            {
                error = "第 " + measure_number +
                        " 小节存在 duration 小于等于 0 的音符。";
                return false;
            }

            NoteTarget target;
            target.is_rest = static_cast<bool>(child.child("rest"));
            pugi::xml_node technical =
                child.child("notations").child("technical");
            target.string_number =
                technical.child("string").text().as_int(-1);
            target.fret_number =
                technical.child("fret").text().as_int(-1);

            ++result.note_count;
            const double note_length =
                static_cast<double>(duration) / divisions;

            if (child.child("chord"))
            {
                if (last_event_index >= result.events.size())
                {
                    error = "第 " + measure_number +
                            " 小节以 <chord/> 音符开始，无法找到主音符。";
                    return false;
                }
                SongEvent& event = result.events[last_event_index];
                if (!nearly_equal(event.end_quarters - event.start_quarters,
                                  note_length))
                {
                    result.warnings.push_back(
                        "第 " + measure_number +
                        " 小节的和弦音符 duration 不一致，使用主音符时值。" );
                }
                event.notes.push_back(target);
            }
            else
            {
                SongEvent event;
                event.measure_number = measure_number;
                event.start_quarters = absolute_quarters + cursor_quarters;
                event.end_quarters = event.start_quarters + note_length;
                event.notes.push_back(target);
                result.events.push_back(std::move(event));
                last_event_index = result.events.size() - 1;
                cursor_quarters += note_length;
                furthest_quarters = std::max(
                    furthest_quarters, cursor_quarters);
            }
        }

        absolute_quarters += furthest_quarters;
    }

    if (measure_index == 0 || result.events.empty())
    {
        error = "第一声部中没有可转换的事件。";
        return false;
    }

    std::stable_sort(
        result.tempo_changes.begin(),
        result.tempo_changes.end(),
        [](const TempoChange& left, const TempoChange& right)
        {
            if (!nearly_equal(left.position_quarters,
                              right.position_quarters))
                return left.position_quarters < right.position_quarters;
            return left.order < right.order;
        });

    for (SongEvent& event : result.events)
    {
        if (!apply_output_masks(event, result, error))
            return false;

        event.bpm = tempo_at(
            result.tempo_changes,
            event.start_quarters,
            result.used_default_tempo);
        event.duration_ms = duration_in_milliseconds(
            event, result.tempo_changes, result.used_default_tempo);
        if (event.duration_ms == 0)
        {
            error = "第 " + event.measure_number +
                    " 小节的事件持续时间无法转换成毫秒。";
            return false;
        }

        const bool has_lit_led = std::any_of(
            event.string_masks_by_fret.begin(),
            event.string_masks_by_fret.end(),
            [](std::uint8_t mask) { return mask != 0; });
        if (has_lit_led)
            ++result.nonzero_event_count;
        else
            ++result.zero_event_count;
    }

    return true;
}

inline std::string mask_in_hex(std::uint8_t mask)
{
    std::ostringstream stream;
    stream << "0x" << std::uppercase << std::hex
           << std::setw(2) << std::setfill('0')
           << static_cast<unsigned int>(mask);
    return stream.str();
}

inline std::string fret_masks_in_hex(
    const std::array<std::uint8_t, kFretCount>& masks)
{
    std::ostringstream stream;
    stream << '[';
    for (int fret_index = 0; fret_index < kFretCount; ++fret_index)
    {
        if (fret_index > 0)
            stream << ", ";
        stream << mask_in_hex(masks[fret_index]);
    }
    stream << ']';
    return stream.str();
}

inline std::string describe_fret_masks(
    const std::array<std::uint8_t, kFretCount>& masks)
{
    const bool has_lit_led = std::any_of(
        masks.begin(), masks.end(),
        [](std::uint8_t mask) { return mask != 0; });
    if (!has_lit_led)
        return "全部熄灭";

    std::ostringstream stream;
    bool first = true;
    for (int fret = 0; fret < kFretCount; ++fret)
    {
        for (int string_number = 1;
             string_number <= kStringCount;
             ++string_number)
        {
            const std::uint8_t string_bit = static_cast<std::uint8_t>(
                1u << (string_number - 1));
            if ((masks[fret] & string_bit) == 0)
                continue;
            if (!first)
                stream << "，";
            stream << "第 " << string_number << " 弦";
            if (fret == 0)
                stream << "空弦";
            else
                stream << "第 " << fret << " 品";
            first = false;
        }
    }
    return stream.str();
}

inline bool write_arduino_header(
    const std::string& path,
    const ConversionResult& result,
    std::string& error)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        error = "无法写入 " + path +
                "。请确认 Arduino 草图目录存在且文件未被占用。";
        return false;
    }

    output << "// 本文件由 MusicXMLReader " << kVersion
           << " 自动生成，请不要手工修改。\n"
           << "#pragma once\n\n"
           << "#include <Arduino.h>\n"
           << "#include <avr/pgmspace.h>\n\n"
           << "const uint8_t MATRIX_FRET_COUNT = 7;\n"
           << "const uint8_t MATRIX_STRING_COUNT = 6;\n\n"
           << "struct SongEvent\n"
           << "{\n"
           << "    uint8_t stringMasksByFret[MATRIX_FRET_COUNT];\n"
           << "    uint32_t durationMs;\n"
           << "};\n\n"
           << "const SongEvent SONG_EVENTS[] PROGMEM =\n"
           << "{\n";

    for (const SongEvent& event : result.events)
    {
        output << "    {{";
        for (int fret_index = 0; fret_index < kFretCount; ++fret_index)
        {
            if (fret_index > 0)
                output << ", ";
            output << mask_in_hex(
                event.string_masks_by_fret[fret_index]);
        }
        output << "}, " << event.duration_ms << "UL},"
               << " // 小节 " << event.measure_number
               << ", BPM " << std::fixed << std::setprecision(2)
               << event.bpm << "\n";
    }

    output << "};\n\n"
           << "const uint16_t SONG_EVENT_COUNT = "
           << result.events.size() << ";\n";

    if (!output)
    {
        error = "写入 " + path + " 时发生错误。";
        return false;
    }
    return true;
}

} // namespace musicxml_arduino
