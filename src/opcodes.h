/**
 * BD (Binary Decoder) - Opcode Definitions
 *
 * Every instruction is an 8-bit binary string.
 * Prefix determines category:
 *   00xxxxxx = Structure (replaces HTML)
 *   01xxxxxx = Style (replaces CSS)
 *   10xxxxxx = Logic (replaces JavaScript)
 *   11xxxxxx = Meta & Config
 */

#pragma once
#include <string>
#include <unordered_map>

namespace bd {

enum class Category { STRUCT, STYLE, LOGIC, META, UNKNOWN };

inline Category getCategory(const std::string& opcode) {
    if (opcode.size() < 2) return Category::UNKNOWN;
    char a = opcode[0], b = opcode[1];
    if (a == '0' && b == '0') return Category::STRUCT;
    if (a == '0' && b == '1') return Category::STYLE;
    if (a == '1' && b == '0') return Category::LOGIC;
    if (a == '1' && b == '1') return Category::META;
    return Category::UNKNOWN;
}

// Master opcode table: binary -> name
inline const std::unordered_map<std::string, std::string>& allOpcodes() {
    static const std::unordered_map<std::string, std::string> table = {
        // ===== STRUCTURE (00) =====
        {"00000001", "DOC_START"},
        {"00000010", "DOC_END"},
        {"00000011", "SECTION_START"},
        {"00000100", "SECTION_END"},
        {"00000101", "TEXT"},
        {"00000110", "HEADING"},
        {"00000111", "LINK"},
        {"00001000", "IMAGE"},
        {"00001001", "LIST_START"},
        {"00001010", "LIST_ITEM"},
        {"00001011", "LIST_END"},
        {"00001100", "INPUT"},
        {"00001101", "BUTTON"},
        {"00001110", "FORM_START"},
        {"00001111", "FORM_END"},
        {"00010000", "NAV_START"},
        {"00010001", "NAV_END"},
        {"00010010", "FOOTER_START"},
        {"00010011", "FOOTER_END"},
        {"00010100", "SPAN"},
        {"00010101", "SPAN_END"},
        {"00010110", "BR"},
        {"00010111", "HR"},
        {"00011000", "VIDEO"},
        {"00011001", "AUDIO"},
        {"00011010", "TABLE_START"},
        {"00011011", "TABLE_ROW_START"},
        {"00011100", "TABLE_CELL"},
        {"00011101", "TABLE_ROW_END"},
        {"00011110", "TABLE_END"},
        {"00011111", "HEADER_START"},
        {"00100000", "HEADER_END"},
        {"00100001", "MAIN_START"},
        {"00100010", "MAIN_END"},
        {"00100011", "ARTICLE_START"},
        {"00100100", "ARTICLE_END"},
        {"00100101", "ASIDE_START"},
        {"00100110", "ASIDE_END"},
        {"00100111", "ICON"},
        {"00101000", "CODE_BLOCK"},
        {"00101001", "PRE_START"},
        {"00101010", "PRE_END"},
        {"00101011", "TEXTAREA"},
        {"00101100", "SELECT_START"},
        {"00101101", "SELECT_END"},
        {"00101110", "OPTION"},
        {"00101111", "LABEL"},
        {"00110000", "COMPONENT_DEF"},
        {"00110001", "COMPONENT_END"},
        {"00110010", "COMPONENT_USE"},
        {"00110011", "COUNTDOWN"},
        {"00110100", "IMPORT"},
        {"00110101", "REPEAT"},
        {"00110110", "REPEAT_END"},
        {"00110111", "IF_VISIBLE"},
        {"00111000", "CAROUSEL"},
        {"00111001", "CAROUSEL_ITEM"},
        {"00111010", "CAROUSEL_END"},
        {"00111011", "MODAL_DEF"},
        {"00111100", "MODAL_END"},
        {"00111101", "MODAL_OPEN"},
        {"00111110", "TOAST"},
        {"00111111", "THEME_TOGGLE"},

        // ===== STYLE (01) =====
        {"01000001", "BG"},
        {"01000010", "COLOR"},
        {"01000011", "FONT_SIZE"},
        {"01000100", "FONT_FAMILY"},
        {"01000101", "MARGIN"},
        {"01000110", "PADDING"},
        {"01000111", "BORDER"},
        {"01001000", "WIDTH"},
        {"01001001", "HEIGHT"},
        {"01001010", "DISPLAY"},
        {"01001011", "FLEX"},
        {"01001100", "GRID"},
        {"01001101", "POSITION"},
        {"01001110", "ALIGN"},
        {"01001111", "SHADOW"},
        {"01010000", "RADIUS"},
        {"01010001", "GRADIENT"},
        {"01010010", "ANIMATION"},
        {"01010011", "TRANSITION"},
        {"01010100", "OPACITY"},
        {"01010101", "OVERFLOW"},
        {"01010110", "TRANSFORM"},
        {"01010111", "CURSOR"},
        {"01011000", "GAP"},
        {"01011001", "MAX_WIDTH"},
        {"01011010", "MIN_HEIGHT"},
        {"01011011", "LINE_HEIGHT"},
        {"01011100", "LETTER_SPACING"},
        {"01011101", "TEXT_DECORATION"},
        {"01011110", "BORDER_BOTTOM"},
        {"01011111", "JUSTIFY"},
        {"01100000", "FLEX_DIR"},
        {"01100001", "FLEX_WRAP"},
        {"01100010", "Z_INDEX"},
        {"01100011", "BOX_SIZING"},
        {"01100100", "TEXT_TRANSFORM"},
        {"01100101", "FONT_WEIGHT"},
        {"01100110", "CUSTOM_CSS"},
        {"01100111", "BACKDROP_FILTER"},
        {"01101000", "PLACE_ITEMS"},
        {"01101001", "MIN_WIDTH"},
        {"01101010", "MAX_HEIGHT"},
        {"01101011", "WORD_BREAK"},
        {"01101100", "WHITE_SPACE"},
        {"01101101", "OUTLINE"},
        {"01101110", "LIST_STYLE"},
        {"01110000", "ALIGN_ITEMS"},
        {"01110001", "VISIBILITY"},
        {"01110010", "FLEX_GROW"},
        {"01110011", "FLEX_SHRINK"},
        {"01110100", "FLEX_BASIS"},
        {"01110101", "ORDER"},
        {"01110110", "ALIGN_SELF"},
        {"01110111", "GRID_TEMPLATE"},
        {"01111000", "GRID_AREA"},
        {"01111001", "GRID_COL"},
        {"01111010", "GRID_ROW"},
        {"01111011", "OBJECT_FIT"},
        {"01111100", "TEXT_SHADOW"},

        // ===== LOGIC (10) =====
        {"10000001", "ON_CLICK"},
        {"10000011", "ON_LOAD"},
        {"10000100", "SET_VAR"},
        {"10001011", "ALERT"},
        {"10001100", "LOG"},
        {"10001101", "TOGGLE_CLASS"},
        {"10010000", "SET_TEXT"},
        {"10010011", "TIMER"},
        {"10011000", "FUNC_DEF"},
        {"10011001", "FUNC_END"},
        {"10011010", "FUNC_CALL"},
        {"10011101", "DOM_QUERY"},
        {"10011110", "DOM_CREATE"},
        {"10011111", "DOM_APPEND"},
        {"10100100", "REDIRECT"},
        {"10101010", "INNER_HTML"},
        {"10101011", "ON_SUBMIT"},
        {"10101100", "FETCH_POST"},
        {"10101101", "SCROLL_REVEAL"},
        {"10101110", "ACCORDION"},
        {"10101111", "ACCORDION_ITEM"},
        {"10110000", "ACCORDION_END"},
        {"10110001", "TABS"},
        {"10110010", "TAB"},
        {"10110011", "TAB_PANEL"},
        {"10110100", "TABS_END"},
        {"10110101", "TOOLTIP"},
        {"10110110", "DROPDOWN"},
        {"10110111", "DROPDOWN_ITEMS"},
        {"10111000", "DROPDOWN_END"},
        {"10111001", "COPY_BUTTON"},
        {"10111010", "PROGRESS_BAR"},
        {"10111011", "COUNTER"},
        {"10111100", "STICKY"},
        {"10111101", "EMBED"},
        {"10111110", "BADGE"},
        {"10111111", "AVATAR"},
        {"10000010", "DIVIDER"},
        {"10000110", "RATING"},
        {"10000111", "SORT_TABLE"},
        {"10001000", "SEARCH_FILTER"},
        {"10001001", "PRICING_TABLE"},
        {"10001010", "PRICING_TIER"},
        {"10001110", "PRICING_END"},

        // ===== META (11) =====
        {"11000001", "TITLE"},
        {"11000010", "CHARSET"},
        {"11000011", "VIEWPORT"},
        {"11000100", "DESCRIPTION"},
        {"11000101", "FAVICON"},
        {"11000110", "IMPORT_FONT"},
        {"11000111", "IMPORT_SCRIPT"},
        {"11001000", "IMPORT_STYLE"},
        {"11001001", "ID"},
        {"11001010", "CLASS"},
        {"11001011", "ATTR"},
        {"11001100", "COMMENT"},
        {"11001101", "MEDIA_QUERY"},
        {"11001110", "MEDIA_END"},
        {"11001111", "KEYFRAMES"},
        {"11010000", "KEYFRAME_STEP"},
        {"11010001", "KEYFRAMES_END"},
        {"11010010", "CSS_VAR"},
        {"11010011", "GLOBAL_STYLE"},
        {"11010100", "GLOBAL_STYLE_END"},
        {"11010101", "HOVER_STYLE"},
        {"11010110", "HOVER_STYLE_END"},
        {"11011010", "RAW_HTML"},
        {"11011011", "RAW_CSS"},
        {"11011100", "RAW_JS"},
        {"11011101", "OG_IMAGE"},
        {"11011110", "OG_TITLE"},
        {"11011111", "ANALYTICS"},
        {"11100001", "COOKIE_BANNER"},
    };
    return table;
}

inline std::string lookupOpcode(const std::string& opcode) {
    auto& table = allOpcodes();
    auto it = table.find(opcode);
    return (it != table.end()) ? it->second : "";
}

// Style opcode name -> CSS property
inline const std::unordered_map<std::string, std::string>& styleToCSS() {
    static const std::unordered_map<std::string, std::string> table = {
        {"BG", "background"},
        {"COLOR", "color"},
        {"FONT_SIZE", "font-size"},
        {"FONT_FAMILY", "font-family"},
        {"MARGIN", "margin"},
        {"PADDING", "padding"},
        {"BORDER", "border"},
        {"WIDTH", "width"},
        {"HEIGHT", "height"},
        {"DISPLAY", "display"},
        {"FLEX", "flex"},
        {"GRID", "grid"},
        {"POSITION", "position"},
        {"ALIGN", "text-align"},
        {"SHADOW", "box-shadow"},
        {"RADIUS", "border-radius"},
        {"GRADIENT", "background"},
        {"ANIMATION", "animation"},
        {"TRANSITION", "transition"},
        {"OPACITY", "opacity"},
        {"OVERFLOW", "overflow"},
        {"TRANSFORM", "transform"},
        {"CURSOR", "cursor"},
        {"GAP", "gap"},
        {"MAX_WIDTH", "max-width"},
        {"MIN_HEIGHT", "min-height"},
        {"LINE_HEIGHT", "line-height"},
        {"LETTER_SPACING", "letter-spacing"},
        {"TEXT_DECORATION", "text-decoration"},
        {"BORDER_BOTTOM", "border-bottom"},
        {"JUSTIFY", "justify-content"},
        {"FLEX_DIR", "flex-direction"},
        {"FLEX_WRAP", "flex-wrap"},
        {"Z_INDEX", "z-index"},
        {"BOX_SIZING", "box-sizing"},
        {"TEXT_TRANSFORM", "text-transform"},
        {"FONT_WEIGHT", "font-weight"},
        {"BACKDROP_FILTER", "backdrop-filter"},
        {"PLACE_ITEMS", "place-items"},
        {"ALIGN_ITEMS", "align-items"},
        {"VISIBILITY", "visibility"},
        {"FLEX_GROW", "flex-grow"},
        {"FLEX_SHRINK", "flex-shrink"},
        {"FLEX_BASIS", "flex-basis"},
        {"ORDER", "order"},
        {"ALIGN_SELF", "align-self"},
        {"GRID_TEMPLATE", "grid-template-columns"},
        {"GRID_AREA", "grid-area"},
        {"GRID_COL", "grid-column"},
        {"GRID_ROW", "grid-row"},
        {"OBJECT_FIT", "object-fit"},
        {"TEXT_SHADOW", "text-shadow"},
        {"MIN_WIDTH", "min-width"},
        {"MAX_HEIGHT", "max-height"},
        {"WORD_BREAK", "word-break"},
        {"WHITE_SPACE", "white-space"},
        {"OUTLINE", "outline"},
        {"LIST_STYLE", "list-style"},
    };
    return table;
}

inline std::string cssProp(const std::string& name) {
    auto& table = styleToCSS();
    auto it = table.find(name);
    return (it != table.end()) ? it->second : "";
}

} // namespace bd
