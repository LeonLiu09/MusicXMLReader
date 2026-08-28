#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

// 使用 pugixml 的 header-only 模式。
// 这样 VS Code 只编译 main.cpp 时，也会包含 pugixml 的完整实现。
#define PUGIXML_HEADER_ONLY
#include "lib/pugixml/pugixml.hpp"

// 把 MusicXML 使用的英文时值转换成容易阅读的中文。
std::string note_type_in_chinese(const std::string& type)
{
    if (type == "whole")   return "全音符";
    if (type == "half")    return "二分音符";
    if (type == "quarter") return "四分音符";
    if (type == "eighth")  return "八分音符";
    if (type == "16th")    return "十六分音符";
    return type.empty() ? "未知" : type;
}

int main()
{
#ifdef _WIN32
    // 让 Windows 终端能够正确显示 UTF-8 中文。
    SetConsoleOutputCP(CP_UTF8);
#endif

    const char* file_name = "musicdoc/Canon in D major.musicxml";

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(file_name);

    if (!result)
    {
        std::cerr << "读取 MusicXML 失败：" << result.description() << '\n';
        return 1;
    }

    pugi::xml_node score = doc.child("score-partwise");
    if (!score)
    {
        std::cerr << "文件中没有找到 <score-partwise> 根节点。\n";
        return 1;
    }

    // 1. 读取标题。
    std::string title = score.child("work").child("work-title").text().as_string();
    if (title.empty())
        title = score.child("movement-title").text().as_string();

    std::cout << "标题：" << (title.empty() ? "未标注" : title) << '\n';

    // 2. 读取速度。MusicXML 常用下面两种方式保存速度：
    //    <sound tempo="80"/> 或 <metronome><per-minute>80</per-minute>。
    pugi::xpath_node sound_tempo = score.select_node(".//sound[@tempo]");
    pugi::xpath_node metronome_tempo = score.select_node(".//metronome/per-minute");

    if (sound_tempo)
    {
        std::cout << "全曲速度："
                  << sound_tempo.node().attribute("tempo").value()
                  << " BPM\n";
    }
    else if (metronome_tempo)
    {
        std::cout << "全曲速度："
                  << metronome_tempo.node().text().as_string()
                  << " BPM\n";
    }
    else
    {
        std::cout << "全曲速度：MusicXML 文件中未标注\n";
    }

    // 3. 找到第一声部，按顺序读取它的所有小节。
    pugi::xml_node part = score.child("part");
    if (!part)
    {
        std::cerr << "没有找到声部。\n";
        return 1;
    }

    int divisions = 0;
    int measure_count = 0;

    for (pugi::xml_node measure : part.children("measure"))
    {
        ++measure_count;

        // MusicXML 中的 divisions 会一直有效，直到后面的小节重新声明它。
        pugi::xml_node divisions_node =
            measure.child("attributes").child("divisions");
        if (divisions_node)
            divisions = divisions_node.text().as_int();

        std::string measure_number = measure.attribute("number").as_string();
        if (measure_number.empty())
            measure_number = std::to_string(measure_count);

        std::cout << "\n第 " << measure_number << " 小节：\n";
        std::cout << "divisions = " << divisions
                  << "（一个四分音符包含的 duration 单位数）\n";

        int note_number = 0;
        for (pugi::xml_node note : measure.children("note"))
        {
            ++note_number;

            pugi::xml_node pitch = note.child("pitch");
            pugi::xml_node technical = note.child("notations").child("technical");

            std::string type = note.child("type").text().as_string();
            int duration = note.child("duration").text().as_int();
            int string_number = technical.child("string").text().as_int(-1);
            int fret_number = technical.child("fret").text().as_int(-1);

            std::cout << "音符 " << note_number;
            if (note.child("chord"))
                std::cout << "（与前一个音同时弹奏）";
            std::cout << "：\n";

            if (pitch)
            {
                std::cout << "  音高："
                          << pitch.child("step").text().as_string()
                          << pitch.child("octave").text().as_string()
                          << '\n';
            }

            if (string_number >= 0 && fret_number >= 0)
            {
                std::cout << "  弦：第 " << string_number << " 弦\n";
                std::cout << "  品：第 " << fret_number << " 品\n";
            }
            else
            {
                std::cout << "  弦、品：未标注\n";
            }

            std::cout << "  时值：" << note_type_in_chinese(type)
                      << "（duration = " << duration;
            if (divisions > 0)
                std::cout << "，相当于 "
                          << static_cast<double>(duration) / divisions
                          << " 个四分音符";
            std::cout << "）\n";
        }

        if (note_number == 0)
            std::cout << "这一小节没有音符。\n";
    }

    if (measure_count == 0)
    {
        std::cerr << "没有找到任何小节。\n";
        return 1;
    }

    return 0;
}
