#include <iomanip>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

// 使用 pugixml 的 header-only 模式。
// 这样 VS Code 只编译 main.cpp 时，也会包含 pugixml 的完整实现。
#define PUGIXML_HEADER_ONLY
#include "musicxml_converter.hpp"

int main()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    const char* input_file = "musicdoc/Canon in D major.musicxml";
    const char* output_file =
        "arduino/MusicXMLShiftRegister/song_data.h";

    pugi::xml_document document;
    pugi::xml_parse_result load_result = document.load_file(input_file);
    if (!load_result)
    {
        std::cerr << "读取 MusicXML 失败："
                  << load_result.description() << '\n';
        return 1;
    }

    musicxml_arduino::ConversionResult result;
    std::string error;
    if (!musicxml_arduino::convert_document(document, result, error))
    {
        std::cerr << "转换失败：" << error << '\n';
        return 1;
    }

    if (!musicxml_arduino::write_arduino_header(
            output_file, result, error))
    {
        std::cerr << "生成 Arduino 数据失败：" << error << '\n';
        return 1;
    }

    std::cout << "标题："
              << (result.title.empty() ? "未标注" : result.title) << '\n';
    std::cout << "MusicXMLReader 版本："
              << musicxml_arduino::kVersion << '\n';
    std::cout << "Arduino：Uno R3\n";
    std::cout << "控制引脚：D11=数据，D13=移位时钟，D10=锁存\n";
    std::cout << "输出规则：U1 的 QA 对应空弦阴极，QB-QG 对应第 1-6 品阴极；"
                 "U2 的 QA-QF 对应第 1-6 弦阳极。\n";
    std::cout << "显示方式：Arduino 动态扫描 7 个品位，"
                 "不是让 42 个灯静态直连。\n\n";

    for (std::size_t index = 0; index < result.events.size(); ++index)
    {
        const musicxml_arduino::SongEvent& event = result.events[index];
        std::cout << "事件 " << (index + 1)
                  << " | 小节 " << event.measure_number
                  << " | BPM " << std::fixed << std::setprecision(2)
                  << event.bpm
                  << " | " << event.duration_ms << " ms"
                  << " | 空弦及第1-6品弦掩码="
                  << musicxml_arduino::fret_masks_in_hex(
                         event.string_masks_by_fret)
                  << " | "
                  << musicxml_arduino::describe_fret_masks(
                         event.string_masks_by_fret)
                  << '\n';
    }

    std::cout << "\n转换完成：\n";
    std::cout << "  原始音符数：" << result.note_count << '\n';
    std::cout << "  Arduino 事件数：" << result.events.size() << '\n';
    std::cout << "  已映射到空弦及第 1-6 品的音符数："
              << result.mapped_note_count << '\n';
    std::cout << "  至少点亮一路的事件数："
              << result.nonzero_event_count << '\n';
    std::cout << "  全灭但保留节拍的事件数："
              << result.zero_event_count << '\n';
    std::cout << "  Arduino 数据文件：" << output_file << '\n';

    if (result.used_default_tempo)
        std::cout << "  速度提示：乐谱未在开头给出有效 BPM，"
                     "未指定部分使用默认 80 BPM。\n";

    for (const std::string& warning : result.warnings)
        std::cout << "  警告：" << warning << '\n';

    return 0;
}
