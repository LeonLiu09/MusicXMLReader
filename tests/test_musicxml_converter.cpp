#include <cassert>
#include <iostream>
#include <string>

#define PUGIXML_HEADER_ONLY
#include "../musicxml_converter.hpp"

namespace
{

musicxml_arduino::ConversionResult convert_xml(const char* xml)
{
    pugi::xml_document document;
    pugi::xml_parse_result load_result = document.load_string(xml);
    assert(load_result);

    musicxml_arduino::ConversionResult result;
    std::string error;
    const bool converted =
        musicxml_arduino::convert_document(document, result, error);
    if (!converted)
        std::cerr << error << '\n';
    assert(converted);
    return result;
}

void test_output_bits()
{
    using musicxml_arduino::output_bit_for;
    assert(output_bit_for(1, 0) == 0);
    assert(output_bit_for(6, 0) == 5);
    assert(output_bit_for(1, 1) == 6);
    assert(output_bit_for(6, 6) == 41);
    assert(output_bit_for(1, -1) == -1);
    assert(output_bit_for(1, 7) == -1);
    assert(output_bit_for(7, 1) == -1);
}

void test_current_canon_file()
{
    pugi::xml_document document;
    assert(document.load_file("musicdoc/Canon in D major.musicxml"));

    musicxml_arduino::ConversionResult result;
    std::string error;
    assert(musicxml_arduino::convert_document(document, result, error));
    assert(result.note_count == 325);
    assert(result.events.size() == 273);
    assert(result.mapped_note_count == 325);
    assert(result.nonzero_event_count == 273);
    assert(result.zero_event_count == 0);
    assert(result.used_default_tempo);
    assert(result.events.at(0).string_masks_by_fret[3] == 0x10);
    assert(result.events.at(0).duration_ms == 3000);
    assert(result.events.at(2).string_masks_by_fret[1] == 0x02);
    assert(result.events.at(4).string_masks_by_fret[1] == 0x20);
    assert(result.events.at(4).string_masks_by_fret[2] == 0x04);
}

void test_tempo_changes()
{
    const char* xml = R"xml(
<score-partwise>
  <part id="P1">
    <measure number="1">
      <attributes><divisions>1</divisions></attributes>
      <direction><sound tempo="120"/></direction>
      <note><pitch><step>F</step><octave>4</octave></pitch><duration>1</duration>
        <notations><technical><string>1</string><fret>1</fret></technical></notations>
      </note>
      <direction><sound tempo="60"/></direction>
      <note><pitch><step>C</step><octave>4</octave></pitch><duration>1</duration>
        <notations><technical><string>2</string><fret>1</fret></technical></notations>
      </note>
    </measure>
  </part>
</score-partwise>)xml";

    const auto result = convert_xml(xml);
    assert(result.events.size() == 2);
    assert(result.events[0].duration_ms == 500);
    assert(result.events[1].duration_ms == 1000);
    assert(result.events[0].bpm == 120.0);
    assert(result.events[1].bpm == 60.0);
    assert(!result.used_default_tempo);
}

void test_metronome_conversion_and_rest()
{
    const char* xml = R"xml(
<score-partwise>
  <part id="P1">
    <measure number="1">
      <attributes><divisions>2</divisions></attributes>
      <direction>
        <direction-type><metronome><beat-unit>eighth</beat-unit>
          <per-minute>120</per-minute></metronome></direction-type>
      </direction>
      <note><rest/><duration>2</duration></note>
    </measure>
  </part>
</score-partwise>)xml";

    const auto result = convert_xml(xml);
    assert(result.events.size() == 1);
    assert(result.events[0].bpm == 60.0);
    assert(result.events[0].duration_ms == 1000);
    for (std::uint8_t mask : result.events[0].string_masks_by_fret)
        assert(mask == 0);
}

void test_conflicting_frets_are_rejected()
{
    const char* xml = R"xml(
<score-partwise>
  <part id="P1">
    <measure number="1">
      <attributes><divisions>1</divisions></attributes>
      <note><pitch><step>F</step><octave>4</octave></pitch><duration>1</duration>
        <notations><technical><string>1</string><fret>0</fret></technical></notations>
      </note>
      <note><chord/><pitch><step>F</step><octave>4</octave></pitch><duration>1</duration>
        <notations><technical><string>1</string><fret>1</fret></technical></notations>
      </note>
    </measure>
  </part>
</score-partwise>)xml";

    pugi::xml_document document;
    assert(document.load_string(xml));
    musicxml_arduino::ConversionResult result;
    std::string error;
    assert(!musicxml_arduino::convert_document(document, result, error));
    assert(error.find("同时按不同品位") != std::string::npos);
}

void test_open_string_sixth_fret_and_seventh_fret()
{
    const char* xml = R"xml(
<score-partwise>
  <part id="P1">
    <measure number="1">
      <attributes><divisions>1</divisions></attributes>
      <note><pitch><step>E</step><octave>4</octave></pitch><duration>1</duration>
        <notations><technical><string>1</string><fret>0</fret></technical></notations>
      </note>
      <note><chord/><pitch><step>F</step><octave>3</octave></pitch><duration>1</duration>
        <notations><technical><string>6</string><fret>6</fret></technical></notations>
      </note>
      <note><pitch><step>F</step><octave>4</octave></pitch><duration>1</duration>
        <notations><technical><string>2</string><fret>7</fret></technical></notations>
      </note>
    </measure>
  </part>
</score-partwise>)xml";

    const auto result = convert_xml(xml);
    assert(result.events.size() == 2);
    assert(result.mapped_note_count == 2);
    assert(result.events[0].string_masks_by_fret[0] == 0x01);
    assert(result.events[0].string_masks_by_fret[6] == 0x20);
    for (std::uint8_t mask : result.events[1].string_masks_by_fret)
        assert(mask == 0);
    assert(result.nonzero_event_count == 1);
    assert(result.zero_event_count == 1);
}

} // namespace

int main()
{
    test_output_bits();
    test_current_canon_file();
    test_tempo_changes();
    test_metronome_conversion_and_rest();
    test_conflicting_frets_are_rejected();
    test_open_string_sixth_fret_and_seventh_fret();
    std::cout << "MusicXML 转 Arduino 测试全部通过。\n";
    return 0;
}
