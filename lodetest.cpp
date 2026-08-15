#include <iostream>
#include "pugixml.hpp"

int main()
{
    pugi::xml_document doc;

    pugi::xml_parse_result result =
        doc.load_file("musicdoc/Canon in D major.musicxml");

    if (!result)
    {
        std::cerr << "XML load failed!\n";
        std::cerr << result.description() << '\n';
        return 1;
    }

    std::cout << "XML loaded successfully!\n";
    return 0;
}