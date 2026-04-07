/**
 * BD Compiler - The core engine (C++)
 *
 * Takes tokens from the lexer, builds a document tree,
 * and generates a single HTML file with embedded CSS and JS.
 *
 * ONE .bd file -> ONE complete webpage.
 */

#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <memory>
#include "opcodes.h"
#include "lexer.h"

namespace bd {

// ============================================================
// Document tree nodes
// ============================================================
struct ElementNode;
struct TextNode;
struct RawHTMLNode;

enum class NodeType { ELEMENT, TEXT, RAW_HTML };

struct Node {
    NodeType type;
    virtual ~Node() = default;
};

struct TextNode : Node {
    std::string text;
    TextNode(const std::string& t) : text(t) { type = NodeType::TEXT; }
};

struct RawHTMLNode : Node {
    std::string html;
    RawHTMLNode(const std::string& h) : html(h) { type = NodeType::RAW_HTML; }
};

struct EventHandler {
    std::string eventType;
    std::string jsCode;
};

struct ElementNode : Node {
    std::string tag;
    std::string id;
    std::vector<std::string> classes;
    std::vector<std::pair<std::string, std::string>> styles; // ordered
    std::unordered_map<std::string, std::string> attrs;
    std::vector<std::shared_ptr<Node>> children;
    std::vector<EventHandler> events;

    ElementNode(const std::string& t) : tag(t) { type = NodeType::ELEMENT; }
};

// ============================================================
// CSS rule
// ============================================================
struct CSSRule {
    std::string selector;
    std::vector<std::pair<std::string, std::string>> props;
};

struct KeyframeStep {
    std::string step;
    std::vector<std::pair<std::string, std::string>> props;
};

// ============================================================
// The Compiler
// ============================================================
class Compiler {
public:
    // Page meta
    std::string title = "BD Page";
    std::string charset = "UTF-8";
    std::string viewport = "width=device-width, initial-scale=1.0";
    std::string description;
    std::string favicon;
    std::vector<std::string> fontImports;
    std::vector<std::string> scriptImports;
    std::vector<std::string> styleImports;

    // CSS
    std::vector<std::pair<std::string, std::string>> cssVars;
    std::vector<CSSRule> globalStyles;
    std::vector<CSSRule> hoverStyles;
    std::vector<CSSRule> focusStyles;
    std::vector<CSSRule> activeStyles;
    std::unordered_map<std::string, std::vector<KeyframeStep>> keyframes;
    std::unordered_map<std::string, std::vector<CSSRule>> mediaQueries;
    std::vector<std::string> rawCSS;
    std::vector<std::string> rawJS;

    // Component system: define once, use anywhere
    std::unordered_map<std::string, std::vector<Token>> componentDefs;
    std::string _currentComponentName;
    std::vector<Token> _currentComponentTokens;
    bool _definingComponent = false;

    // Repeat system
    bool _repeating = false;
    int _repeatCount = 0;
    std::vector<Token> _repeatBuf;

    // Tab system
    std::string _tabBarId;
    std::string _tabPanelsId;
    int _tabIndex = 0;

    // Document
    std::shared_ptr<ElementNode> root;
    std::vector<ElementNode*> stack;
    int idCounter = 0;
    std::vector<std::string> errors;

    // State for multi-line constructs
    std::string _globalSel;
    std::vector<std::pair<std::string, std::string>> _globalBuf;
    std::string _hoverSel;
    std::vector<std::pair<std::string, std::string>> _hoverBuf;
    std::string _focusSel;
    std::vector<std::pair<std::string, std::string>> _focusBuf;
    std::string _activeSel;
    std::vector<std::pair<std::string, std::string>> _activeBuf;
    std::string _kfName;
    std::vector<KeyframeStep> _kfSteps;
    std::string _kfStepName;
    std::vector<std::pair<std::string, std::string>> _kfStepBuf;
    bool _kfStepActive = false;
    std::string _mediaSel;
    std::vector<CSSRule> _mediaRules;

    Compiler() { reset(); }

    void reset() {
        title = "BD Page";
        charset = "UTF-8";
        viewport = "width=device-width, initial-scale=1.0";
        description.clear();
        favicon.clear();
        fontImports.clear();
        scriptImports.clear();
        styleImports.clear();
        cssVars.clear();
        globalStyles.clear();
        hoverStyles.clear();
        focusStyles.clear();
        activeStyles.clear();
        keyframes.clear();
        mediaQueries.clear();
        rawCSS.clear();
        rawJS.clear();
        root = std::make_shared<ElementNode>("body");
        stack.clear();
        stack.push_back(root.get());
        idCounter = 0;
        errors.clear();
        _globalSel.clear(); _globalBuf.clear();
        _hoverSel.clear(); _hoverBuf.clear();
        _focusSel.clear(); _focusBuf.clear();
        _activeSel.clear(); _activeBuf.clear();
        _kfName.clear(); _kfSteps.clear();
        _kfStepName.clear(); _kfStepBuf.clear();
        _kfStepActive = false;
        _mediaSel.clear(); _mediaRules.clear();
        componentDefs.clear();
        _currentComponentName.clear();
        _currentComponentTokens.clear();
        _definingComponent = false;
        _repeating = false;
        _repeatCount = 0;
        _repeatBuf.clear();
    }

    ElementNode* current() {
        return stack.empty() ? root.get() : stack.back();
    }

    std::string nextId() {
        return "bd-" + std::to_string(++idCounter);
    }

    std::string ensureId(ElementNode* node) {
        if (node->id.empty()) node->id = nextId();
        return node->id;
    }

    // ============================================================
    // Public API
    // ============================================================
    std::string compileFile(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return "<!-- BD Error: Cannot open file -->";
        }
        std::string source((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        return compile(source);
    }

    std::string compile(const std::string& source) {
        reset();

        Lexer lexer;
        lexer.tokenize(source);

        for (auto& err : lexer.errors) errors.push_back(err);

        for (auto& token : lexer.tokens) {
            processToken(token);
        }

        return generateHTML();
    }

private:
    // Closing opcodes
    static const std::unordered_set<std::string>& closingOps() {
        static const std::unordered_set<std::string> s = {
            "DOC_END", "SECTION_END", "NAV_END", "FOOTER_END", "FORM_END",
            "LIST_END", "TABLE_END", "TABLE_ROW_END", "HEADER_END",
            "MAIN_END", "ARTICLE_END", "ASIDE_END", "PRE_END", "SPAN_END",
        };
        return s;
    }

    static const std::unordered_set<std::string>& selfClosingTags() {
        static const std::unordered_set<std::string> s = {"br", "hr", "img", "input"};
        return s;
    }

    static std::string tagFor(const std::string& name) {
        static const std::unordered_map<std::string, std::string> m = {
            {"SECTION_START", "div"}, {"NAV_START", "nav"}, {"FOOTER_START", "footer"},
            {"FORM_START", "form"}, {"LIST_START", "ul"}, {"TABLE_START", "table"},
            {"TABLE_ROW_START", "tr"}, {"HEADER_START", "header"}, {"MAIN_START", "main"},
            {"ARTICLE_START", "article"}, {"ASIDE_START", "aside"}, {"PRE_START", "pre"},
        };
        auto it = m.find(name);
        return (it != m.end()) ? it->second : "div";
    }

    // ============================================================
    // Process one token
    // ============================================================
    void processToken(const Token& token) {
        std::string name = lookupOpcode(token.opcode);
        if (name.empty()) return;

        // Component system
        if (name == "COMPONENT_DEF") {
            _definingComponent = true;
            _currentComponentName = token.params.empty() ? "" : token.params[0];
            _currentComponentTokens.clear();
            return;
        }
        if (name == "COMPONENT_END") {
            if (_definingComponent && !_currentComponentName.empty()) {
                componentDefs[_currentComponentName] = _currentComponentTokens;
            }
            _definingComponent = false;
            _currentComponentName.clear();
            _currentComponentTokens.clear();
            return;
        }
        if (name == "COMPONENT_USE") {
            std::string cname = token.params.empty() ? "" : token.params[0];
            auto it = componentDefs.find(cname);
            if (it != componentDefs.end()) {
                // Replay the component tokens
                for (auto& t : it->second) {
                    processToken(t);
                }
                // If extra params, add them as content (slots)
                for (size_t si = 1; si < token.params.size(); si++) {
                    current()->children.push_back(std::make_shared<TextNode>(token.params[si]));
                }
            }
            return;
        }

        // If defining a component, buffer tokens instead of executing
        if (_definingComponent) {
            _currentComponentTokens.push_back(token);
            return;
        }

        // If inside a REPEAT block, buffer tokens
        if (_repeating && name != "REPEAT_END") {
            _repeatBuf.push_back(token);
            return;
        }

        Category cat = getCategory(token.opcode);
        const auto& p = token.params;

        switch (cat) {
            case Category::STRUCT: handleStruct(name, p); break;
            case Category::STYLE:  handleStyle(name, p); break;
            case Category::LOGIC:  handleLogic(name, p); break;
            case Category::META:   handleMeta(name, p); break;
            default: break;
        }
    }

    std::string param(const std::vector<std::string>& p, int i) {
        return (i < (int)p.size()) ? p[i] : "";
    }

    // ============================================================
    // STRUCTURE
    // ============================================================
    void handleStruct(const std::string& name, const std::vector<std::string>& p) {
        if (closingOps().count(name)) {
            if (stack.size() > 1) stack.pop_back();
            return;
        }

        if (name == "DOC_START") return;

        if (name == "HEADING") {
            int level = p.empty() ? 1 : std::clamp(std::stoi(p[0]), 1, 6);
            auto node = std::make_shared<ElementNode>("h" + std::to_string(level));
            if (p.size() > 1) node->children.push_back(std::make_shared<TextNode>(p[1]));
            current()->children.push_back(node);
            stack.push_back(node.get());
            return;
        }

        if (name == "TEXT") {
            auto node = std::make_shared<ElementNode>("p");
            if (!p.empty()) node->children.push_back(std::make_shared<TextNode>(p[0]));
            current()->children.push_back(node);
            stack.push_back(node.get());
            return;
        }

        if (name == "SPAN") {
            auto node = std::make_shared<ElementNode>("span");
            if (!p.empty()) node->children.push_back(std::make_shared<TextNode>(p[0]));
            current()->children.push_back(node);
            stack.push_back(node.get());
            return;
        }

        if (name == "LINK") {
            auto node = std::make_shared<ElementNode>("a");
            node->attrs["href"] = param(p, 0).empty() ? "#" : p[0];
            if (p.size() > 1) node->children.push_back(std::make_shared<TextNode>(p[1]));
            current()->children.push_back(node);
            stack.push_back(node.get());
            return;
        }

        if (name == "IMAGE") {
            auto node = std::make_shared<ElementNode>("img");
            node->attrs["src"] = param(p, 0);
            node->attrs["alt"] = param(p, 1);
            current()->children.push_back(node);
            return; // self-closing
        }

        if (name == "BUTTON") {
            auto node = std::make_shared<ElementNode>("button");
            if (!p.empty()) node->children.push_back(std::make_shared<TextNode>(p[0]));
            current()->children.push_back(node);
            stack.push_back(node.get());
            return;
        }

        if (name == "INPUT") {
            auto node = std::make_shared<ElementNode>("input");
            node->attrs["type"] = p.empty() ? "text" : p[0];
            if (p.size() > 1) node->attrs["placeholder"] = p[1];
            current()->children.push_back(node);
            return; // self-closing
        }

        if (name == "LIST_ITEM") {
            auto node = std::make_shared<ElementNode>("li");
            if (!p.empty()) node->children.push_back(std::make_shared<TextNode>(p[0]));
            current()->children.push_back(node);
            stack.push_back(node.get());
            return;
        }

        if (name == "TABLE_CELL") {
            auto node = std::make_shared<ElementNode>("td");
            if (!p.empty()) node->children.push_back(std::make_shared<TextNode>(p[0]));
            current()->children.push_back(node);
            stack.push_back(node.get());
            return;
        }

        if (name == "BR") {
            current()->children.push_back(std::make_shared<ElementNode>("br"));
            return;
        }
        if (name == "HR") {
            current()->children.push_back(std::make_shared<ElementNode>("hr"));
            return;
        }

        if (name == "CODE_BLOCK") {
            auto node = std::make_shared<ElementNode>("code");
            if (!p.empty()) node->children.push_back(std::make_shared<TextNode>(p[0]));
            current()->children.push_back(node);
            stack.push_back(node.get());
            return;
        }

        if (name == "ICON") {
            auto node = std::make_shared<ElementNode>("i");
            if (!p.empty()) node->classes.push_back(p[0]);
            current()->children.push_back(node);
            return;
        }

        if (name == "TEXTAREA") {
            auto node = std::make_shared<ElementNode>("textarea");
            if (!p.empty()) node->attrs["placeholder"] = p[0];
            if (p.size() > 1) node->children.push_back(std::make_shared<TextNode>(p[1]));
            current()->children.push_back(node);
            stack.push_back(node.get());
            return;
        }

        if (name == "SELECT_START") {
            auto node = std::make_shared<ElementNode>("select");
            current()->children.push_back(node);
            stack.push_back(node.get());
            return;
        }

        if (name == "SELECT_END") {
            if (stack.size() > 1) stack.pop_back();
            return;
        }

        if (name == "OPTION") {
            auto node = std::make_shared<ElementNode>("option");
            if (!p.empty()) { node->attrs["value"] = p[0]; node->children.push_back(std::make_shared<TextNode>(p[0])); }
            if (p.size() > 1) { node->children.clear(); node->children.push_back(std::make_shared<TextNode>(p[1])); }
            current()->children.push_back(node);
            return;
        }

        if (name == "LABEL") {
            auto node = std::make_shared<ElementNode>("label");
            if (!p.empty()) node->attrs["for"] = p[0];
            if (p.size() > 1) node->children.push_back(std::make_shared<TextNode>(p[1]));
            current()->children.push_back(node);
            stack.push_back(node.get());
            return;
        }

        if (name == "COUNTDOWN") {
            // One opcode, full drum roller countdown timer
            // param[0] = ISO date string like "2026-04-07T13:00:00Z"
            // param[1] = "static" for no roll animation (optional)
            std::string targetDate = p.empty() ? "2026-01-01T00:00:00Z" : p[0];
            bool isStatic = (p.size() > 1 && p[1] == "static");
            std::string uid = "cd" + std::to_string(idCounter++);

            // Build the HTML structure
            auto wrapper = std::make_shared<ElementNode>("div");
            wrapper->styles.push_back({"display", "flex"});
            wrapper->styles.push_back({"justify-content", "center"});
            wrapper->styles.push_back({"gap", "clamp(16px, 3vw, 40px)"});

            const char* unitIds[] = {"d", "h", "m", "s"};
            const char* unitLabels[] = {"days", "hours", "min", "sec"};

            for (int u = 0; u < 4; u++) {
                auto unit = std::make_shared<ElementNode>("div");
                unit->styles.push_back({"text-align", "center"});

                if (isStatic) {
                    // Static mode: simple span, no roller
                    auto numSpan = std::make_shared<ElementNode>("span");
                    numSpan->id = uid + unitIds[u];
                    numSpan->styles.push_back({"font-size", "clamp(40px, 10vw, 90px)"});
                    numSpan->styles.push_back({"font-weight", "800"});
                    numSpan->styles.push_back({"line-height", "1"});
                    numSpan->styles.push_back({"letter-spacing", "-2px"});
                    numSpan->styles.push_back({"font-variant-numeric", "tabular-nums"});
                    numSpan->children.push_back(std::make_shared<TextNode>("00"));
                    unit->children.push_back(numSpan);
                } else {
                    // Roll mode: drum roller with stacked digits
                    auto digitPair = std::make_shared<ElementNode>("div");
                    digitPair->styles.push_back({"display", "flex"});
                    digitPair->styles.push_back({"gap", "2px"});
                    digitPair->styles.push_back({"margin", "0 0 6px 0"});

                    for (int di = 0; di < 2; di++) {
                        std::string did = uid + unitIds[u] + std::to_string(di);

                        auto clip = std::make_shared<ElementNode>("div");
                        clip->id = did;
                        clip->styles.push_back({"overflow", "hidden"});
                        clip->styles.push_back({"position", "relative"});
                        clip->styles.push_back({"height", "clamp(40px, 10vw, 90px)"});
                        clip->styles.push_back({"width", "clamp(28px, 7vw, 62px)"});
                        clip->styles.push_back({"-webkit-mask-image", "linear-gradient(to bottom, transparent, white 8%, white 92%, transparent)"});
                        clip->styles.push_back({"mask-image", "linear-gradient(to bottom, transparent, white 8%, white 92%, transparent)"});

                        auto strip = std::make_shared<ElementNode>("div");
                        strip->id = did + "s";
                        strip->styles.push_back({"position", "absolute"});
                        strip->styles.push_back({"left", "0"});
                        strip->styles.push_back({"top", "0"});
                        strip->styles.push_back({"width", "100%"});
                        strip->styles.push_back({"transition", "transform 0.6s cubic-bezier(0.33, 1, 0.68, 1)"});

                        for (int d = 0; d <= 10; d++) {
                            auto slot = std::make_shared<ElementNode>("div");
                            slot->styles.push_back({"height", "clamp(40px, 10vw, 90px)"});
                            slot->styles.push_back({"display", "flex"});
                            slot->styles.push_back({"justify-content", "center"});
                            slot->styles.push_back({"align-items", "center"});
                            slot->styles.push_back({"font-size", "clamp(40px, 10vw, 90px)"});
                            slot->styles.push_back({"font-weight", "800"});
                            slot->styles.push_back({"letter-spacing", "-2px"});
                            slot->styles.push_back({"font-variant-numeric", "tabular-nums"});
                            slot->children.push_back(std::make_shared<TextNode>(std::to_string(d % 10)));
                            strip->children.push_back(slot);
                        }

                        clip->children.push_back(strip);
                        digitPair->children.push_back(clip);
                    }

                    unit->children.push_back(digitPair);
                }

                auto label = std::make_shared<ElementNode>("p");
                label->styles.push_back({"font-size", "11px"});
                label->styles.push_back({"letter-spacing", "2px"});
                label->styles.push_back({"text-transform", "uppercase"});
                label->children.push_back(std::make_shared<TextNode>(unitLabels[u]));
                unit->children.push_back(label);

                wrapper->children.push_back(unit);

                if (u < 3) {
                    auto sep = std::make_shared<ElementNode>("span");
                    sep->styles.push_back({"font-size", "clamp(28px, 6vw, 56px)"});
                    sep->styles.push_back({"font-weight", "300"});
                    sep->styles.push_back({"align-self", "flex-start"});
                    sep->children.push_back(std::make_shared<TextNode>(":"));
                    wrapper->children.push_back(sep);
                }
            }

            current()->children.push_back(wrapper);

            // Generate JS
            std::string js;
            if (isStatic) {
                // Static: simple textContent update
                js = "var " + uid + "t=new Date('" + targetDate + "').getTime();"
                    "function " + uid + "cd(){var n=Date.now(),d=Math.max(0," + uid + "t-n);"
                    "document.getElementById('" + uid + "d').textContent=String(Math.floor(d/864e5)).padStart(2,'0');"
                    "document.getElementById('" + uid + "h').textContent=String(Math.floor(d%864e5/36e5)).padStart(2,'0');"
                    "document.getElementById('" + uid + "m').textContent=String(Math.floor(d%36e5/6e4)).padStart(2,'0');"
                    "document.getElementById('" + uid + "s').textContent=String(Math.floor(d%6e4/1e3)).padStart(2,'0');}"
                    "setInterval(" + uid + "cd,1e3);" + uid + "cd();";
            } else {
                // Roll: drum roller with translateY
                js = "var " + uid + "t=new Date('" + targetDate + "').getTime();"
                    "var " + uid + "p={};"
                    "var " + uid + "ids=['" + uid + "d0','" + uid + "d1','" + uid + "h0','" + uid + "h1','" + uid + "m0','" + uid + "m1','" + uid + "s0','" + uid + "s1'];"
                    "function " + uid + "set(id,v){var s=document.getElementById(id+'s');if(!s)return;var o=" + uid + "p[id];" + uid + "p[id]=v;"
                    "var h=s.parentElement.offsetHeight;"
                    "if(o===undefined){s.style.transition='none';s.style.transform='translateY(-'+v*h+'px)';return;}"
                    "if(o===v)return;"
                    "if(v===0&&o===9){s.style.transition='transform 0.6s cubic-bezier(0.33, 1, 0.68, 1)';s.style.transform='translateY(-'+10*h+'px)';"
                    "setTimeout(function(){s.style.transition='none';s.style.transform='translateY(0)';},650);}"
                    "else{s.style.transition='transform 0.6s cubic-bezier(0.33, 1, 0.68, 1)';s.style.transform='translateY(-'+v*h+'px)';}}"
                    "function " + uid + "cd(){var n=Date.now(),d=Math.max(0," + uid + "t-n);"
                    "var dd=String(Math.floor(d/864e5)).padStart(2,'0');"
                    "var hh=String(Math.floor(d%864e5/36e5)).padStart(2,'0');"
                    "var mm=String(Math.floor(d%36e5/6e4)).padStart(2,'0');"
                    "var ss=String(Math.floor(d%6e4/1e3)).padStart(2,'0');"
                    "var v=[+dd[0],+dd[1],+hh[0],+hh[1],+mm[0],+mm[1],+ss[0],+ss[1]];"
                    "for(var i=0;i<8;i++)" + uid + "set(" + uid + "ids[i],v[i]);}"
                    "setInterval(" + uid + "cd,1e3);" + uid + "cd();";
            }
            rawJS.push_back(js);
            return;
        }

        // IMPORT: pull in another .bd file
        if (name == "IMPORT") {
            if (!p.empty()) {
                std::string importPath = p[0];
                // Resolve relative to current file's directory
                std::ifstream f(importPath);
                if (f.is_open()) {
                    std::string src((std::istreambuf_iterator<char>(f)),
                                     std::istreambuf_iterator<char>());
                    Lexer importLexer;
                    importLexer.tokenize(src);
                    for (auto& t : importLexer.tokens) {
                        processToken(t);
                    }
                }
            }
            return;
        }

        // REPEAT: repeat next element N times
        if (name == "REPEAT") {
            // Handled by collecting tokens until REPEAT_END, then replaying N times
            int count = p.empty() ? 1 : std::stoi(p[0]);
            // Buffer tokens until REPEAT_END
            // Store in a temp vector, then replay
            _repeatCount = count;
            _repeating = true;
            _repeatBuf.clear();
            return;
        }
        if (name == "REPEAT_END") {
            if (_repeating) {
                _repeating = false;
                for (int i = 0; i < _repeatCount; i++) {
                    for (auto& t : _repeatBuf) {
                        processToken(t);
                    }
                }
                _repeatBuf.clear();
            }
            return;
        }

        // IF_VISIBLE: show/hide element based on JS condition
        if (name == "IF_VISIBLE") {
            // param[0] = JS condition, applied as data-if attribute
            // JS will evaluate and toggle display
            auto node = std::make_shared<ElementNode>("div");
            node->id = nextId();
            node->styles.push_back({"display", "none"});
            if (!p.empty()) {
                rawJS.push_back("if(" + p[0] + ")document.getElementById('" + node->id + "').style.display='block';");
            }
            current()->children.push_back(node);
            stack.push_back(node.get());
            return;
        }

        // CAROUSEL: image/content slider
        if (name == "CAROUSEL") {
            std::string cid = "crs" + std::to_string(idCounter++);
            auto wrapper = std::make_shared<ElementNode>("div");
            wrapper->id = cid;
            wrapper->styles.push_back({"position", "relative"});
            wrapper->styles.push_back({"overflow", "hidden"});
            wrapper->styles.push_back({"width", "100%"});

            auto track = std::make_shared<ElementNode>("div");
            track->id = cid + "-track";
            track->styles.push_back({"display", "flex"});
            track->styles.push_back({"transition", "transform 0.5s cubic-bezier(0.33, 1, 0.68, 1)"});

            wrapper->children.push_back(track);

            // Prev/Next buttons
            auto prev = std::make_shared<ElementNode>("button");
            prev->id = cid + "-prev";
            prev->styles.push_back({"position", "absolute"});
            prev->styles.push_back({"left", "8px"});
            prev->styles.push_back({"top", "50%"});
            prev->styles.push_back({"transform", "translateY(-50%)"});
            prev->styles.push_back({"background", "rgba(0,0,0,0.5)"});
            prev->styles.push_back({"color", "#fff"});
            prev->styles.push_back({"border", "none"});
            prev->styles.push_back({"padding", "8px 14px"});
            prev->styles.push_back({"border-radius", "50%"});
            prev->styles.push_back({"cursor", "pointer"});
            prev->styles.push_back({"font-size", "18px"});
            prev->styles.push_back({"z-index", "1"});
            prev->children.push_back(std::make_shared<TextNode>("<"));
            wrapper->children.push_back(prev);

            auto next = std::make_shared<ElementNode>("button");
            next->id = cid + "-next";
            next->styles.push_back({"position", "absolute"});
            next->styles.push_back({"right", "8px"});
            next->styles.push_back({"top", "50%"});
            next->styles.push_back({"transform", "translateY(-50%)"});
            next->styles.push_back({"background", "rgba(0,0,0,0.5)"});
            next->styles.push_back({"color", "#fff"});
            next->styles.push_back({"border", "none"});
            next->styles.push_back({"padding", "8px 14px"});
            next->styles.push_back({"border-radius", "50%"});
            next->styles.push_back({"cursor", "pointer"});
            next->styles.push_back({"font-size", "18px"});
            next->styles.push_back({"z-index", "1"});
            next->children.push_back(std::make_shared<TextNode>(">"));
            wrapper->children.push_back(next);

            current()->children.push_back(wrapper);
            stack.push_back(track.get()); // items go inside track

            rawJS.push_back(
                "(function(){var t=document.getElementById('" + cid + "-track'),"
                "p=document.getElementById('" + cid + "-prev'),"
                "n=document.getElementById('" + cid + "-next'),"
                "i=0,c=t.children.length;"
                "function go(x){i=Math.max(0,Math.min(c-1,x));t.style.transform='translateX(-'+i*100+'%)';}"
                "p.onclick=function(){go(i-1);};"
                "n.onclick=function(){go(i+1);};})();"
            );
            return;
        }

        if (name == "CAROUSEL_ITEM") {
            auto item = std::make_shared<ElementNode>("div");
            item->styles.push_back({"min-width", "100%"});
            item->styles.push_back({"flex-shrink", "0"});
            current()->children.push_back(item);
            stack.push_back(item.get());
            return;
        }

        if (name == "CAROUSEL_END") {
            // Pop back out of track
            if (stack.size() > 1) stack.pop_back();
            return;
        }

        // MODAL_DEF: define a modal dialog
        if (name == "MODAL_DEF") {
            std::string mid = p.empty() ? "modal" : p[0];
            auto overlay = std::make_shared<ElementNode>("div");
            overlay->id = mid;
            overlay->styles.push_back({"position", "fixed"});
            overlay->styles.push_back({"top", "0"});
            overlay->styles.push_back({"left", "0"});
            overlay->styles.push_back({"width", "100%"});
            overlay->styles.push_back({"height", "100%"});
            overlay->styles.push_back({"background", "rgba(0,0,0,0.85)"});
            overlay->styles.push_back({"backdrop-filter", "blur(4px)"});
            overlay->styles.push_back({"z-index", "9999"});
            overlay->styles.push_back({"display", "flex"});
            overlay->styles.push_back({"justify-content", "center"});
            overlay->styles.push_back({"align-items", "center"});
            overlay->styles.push_back({"padding", "24px"});
            overlay->styles.push_back({"opacity", "0"});
            overlay->styles.push_back({"pointer-events", "none"});
            overlay->styles.push_back({"transition", "opacity 0.2s ease"});

            auto box = std::make_shared<ElementNode>("div");
            box->id = mid + "-box";
            box->styles.push_back({"max-width", "580px"});
            box->styles.push_back({"width", "100%"});
            box->styles.push_back({"padding", "40px"});
            box->styles.push_back({"position", "relative"});
            box->styles.push_back({"transform", "translateY(20px)"});
            box->styles.push_back({"transition", "transform 0.2s ease"});

            // Close button
            auto closeBtn = std::make_shared<ElementNode>("button");
            closeBtn->id = mid + "-close";
            closeBtn->styles.push_back({"position", "absolute"});
            closeBtn->styles.push_back({"top", "16px"});
            closeBtn->styles.push_back({"right", "20px"});
            closeBtn->styles.push_back({"background", "none"});
            closeBtn->styles.push_back({"border", "none"});
            closeBtn->styles.push_back({"font-size", "18px"});
            closeBtn->styles.push_back({"cursor", "pointer"});
            closeBtn->children.push_back(std::make_shared<TextNode>("x"));
            box->children.push_back(closeBtn);

            overlay->children.push_back(box);
            current()->children.push_back(overlay);
            stack.push_back(box.get()); // content goes in box

            // JS for open/close
            rawJS.push_back(
                "document.getElementById('" + mid + "-close').onclick=function(){"
                "document.getElementById('" + mid + "').style.opacity='0';"
                "document.getElementById('" + mid + "').style.pointerEvents='none';"
                "document.getElementById('" + mid + "-box').style.transform='translateY(20px)';};"
                "document.getElementById('" + mid + "').onclick=function(e){"
                "if(e.target===this){this.style.opacity='0';this.style.pointerEvents='none';"
                "document.getElementById('" + mid + "-box').style.transform='translateY(20px)';}};"
            );
            return;
        }

        if (name == "MODAL_END") {
            if (stack.size() > 1) stack.pop_back();
            return;
        }

        // MODAL_OPEN: button that opens a modal
        if (name == "MODAL_OPEN") {
            std::string mid = p.empty() ? "modal" : p[0];
            std::string text = p.size() > 1 ? p[1] : "Open";
            auto btn = std::make_shared<ElementNode>("button");
            btn->children.push_back(std::make_shared<TextNode>(text));
            btn->styles.push_back({"cursor", "pointer"});

            std::string bid = nextId();
            btn->id = bid;
            current()->children.push_back(btn);
            stack.push_back(btn.get());

            rawJS.push_back(
                "document.getElementById('" + bid + "').onclick=function(){"
                "document.getElementById('" + mid + "').style.opacity='1';"
                "document.getElementById('" + mid + "').style.pointerEvents='auto';"
                "document.getElementById('" + mid + "-box').style.transform='translateY(0)';};"
            );
            return;
        }

        // TOAST: show a notification
        if (name == "TOAST") {
            // Injects a toast system. param[0] = message, param[1] = duration ms (default 3000)
            std::string msg = p.empty() ? "Notification" : p[0];
            std::string dur = p.size() > 1 ? p[1] : "3000";
            std::string tid = "toast" + std::to_string(idCounter++);

            // Inject toast container if not already
            rawCSS.push_back(
                "#bd-toasts{position:fixed;top:20px;right:20px;z-index:99999;display:flex;flex-direction:column;gap:8px;}"
                ".bd-toast{padding:12px 20px;border-radius:8px;font-size:14px;opacity:0;transform:translateX(20px);"
                "transition:opacity 0.3s ease,transform 0.3s ease;}"
                ".bd-toast.show{opacity:1;transform:translateX(0);}"
            );

            // Ensure container exists
            rawJS.push_back(
                "if(!document.getElementById('bd-toasts')){"
                "var tc=document.createElement('div');tc.id='bd-toasts';document.body.appendChild(tc);}"
            );

            // Show toast on click of current element
            ensureId(current());
            rawJS.push_back(
                "document.getElementById('" + current()->id + "').addEventListener('click',function(){"
                "var t=document.createElement('div');t.className='bd-toast';t.textContent='" + msg + "';"
                "t.style.background='#fff';t.style.color='#000';"
                "document.getElementById('bd-toasts').appendChild(t);"
                "requestAnimationFrame(function(){t.classList.add('show');});"
                "setTimeout(function(){t.classList.remove('show');setTimeout(function(){t.remove();},300);}," + dur + ");});"
            );
            return;
        }

        // THEME_TOGGLE: dark/light mode switch
        if (name == "THEME_TOGGLE") {
            std::string bid = nextId();
            auto btn = std::make_shared<ElementNode>("button");
            btn->id = bid;
            btn->styles.push_back({"cursor", "pointer"});
            btn->styles.push_back({"background", "none"});
            btn->styles.push_back({"border", "1px solid currentColor"});
            btn->styles.push_back({"padding", "6px 14px"});
            btn->styles.push_back({"border-radius", "6px"});
            btn->styles.push_back({"font-size", "13px"});
            btn->children.push_back(std::make_shared<TextNode>(p.empty() ? "theme" : p[0]));
            current()->children.push_back(btn);

            rawJS.push_back(
                "document.getElementById('" + bid + "').onclick=function(){"
                "var b=document.body;var d=b.getAttribute('data-theme');"
                "if(d==='light'){b.removeAttribute('data-theme');this.textContent='" + (p.empty() ? "theme" : p[0]) + "';}"
                "else{b.setAttribute('data-theme','light');"
                "this.textContent='" + (p.empty() ? "theme" : p[0]) + "';}};"
            );

            rawCSS.push_back(
                "body[data-theme='light']{background:#fff!important;color:#111!important;}"
                "body[data-theme='light'] *{border-color:#e0e0e0!important;}"
            );
            return;
        }

        if (name == "VIDEO" || name == "AUDIO") {
            std::string tag = (name == "VIDEO") ? "video" : "audio";
            auto node = std::make_shared<ElementNode>(tag);
            if (!p.empty()) node->attrs["src"] = p[0];
            node->attrs["controls"] = "";
            current()->children.push_back(node);
            stack.push_back(node.get());
            return;
        }

        // Generic container opcode
        auto node = std::make_shared<ElementNode>(tagFor(name));
        current()->children.push_back(node);
        stack.push_back(node.get());
    }

    // ============================================================
    // STYLE
    // ============================================================
    void handleStyle(const std::string& name, const std::vector<std::string>& p) {
        std::string value = param(p, 0);

        // Determine target style buffer
        std::vector<std::pair<std::string, std::string>>* target = nullptr;
        bool isInline = false;

        if (!_globalSel.empty())      target = &_globalBuf;
        else if (!_hoverSel.empty())   target = &_hoverBuf;
        else if (!_focusSel.empty())   target = &_focusBuf;
        else if (!_activeSel.empty())  target = &_activeBuf;
        else if (_kfStepActive)        target = &_kfStepBuf;
        else { target = nullptr; isInline = true; }

        if (name == "CUSTOM_CSS" && p.size() >= 2) {
            if (isInline) {
                current()->styles.push_back({p[0], p[1]});
            } else if (target) {
                target->push_back({p[0], p[1]});
            }
            return;
        }

        std::string prop = cssProp(name);
        if (prop.empty() || value.empty()) return;

        // ALIGN "center" does real centering: text-align + margin auto on children.
        // No more CSS guesswork. One opcode, one result.
        if (name == "ALIGN" && value == "center") {
            if (isInline) {
                current()->styles.push_back({"text-align", "center"});
                current()->styles.push_back({"margin-left", "auto"});
                current()->styles.push_back({"margin-right", "auto"});
            } else if (target) {
                target->push_back({"text-align", "center"});
                target->push_back({"margin-left", "auto"});
                target->push_back({"margin-right", "auto"});
            }
            return;
        }

        if (isInline) {
            current()->styles.push_back({prop, value});
        } else if (target) {
            target->push_back({prop, value});
        }
    }

    // ============================================================
    // LOGIC
    // ============================================================
    void handleLogic(const std::string& name, const std::vector<std::string>& p) {
        if (name == "ON_CLICK") {
            ensureId(current());
            current()->events.push_back({"click", param(p, 0)});
        }
        else if (name == "ON_LOAD") {
            rawJS.push_back("window.addEventListener('load',function(){" + param(p, 0) + "});");
        }
        else if (name == "SET_VAR") {
            rawJS.push_back("let " + param(p, 0) + "=" + param(p, 1) + ";");
        }
        else if (name == "ALERT") {
            ensureId(current());
            current()->events.push_back({"click", "alert('" + param(p, 0) + "')"});
        }
        else if (name == "LOG") {
            rawJS.push_back("console.log('" + param(p, 0) + "');");
        }
        else if (name == "TOGGLE_CLASS") {
            rawJS.push_back("document.querySelector('" + param(p, 0) + "').classList.toggle('" + param(p, 1) + "');");
        }
        else if (name == "SET_TEXT") {
            rawJS.push_back("document.querySelector('" + param(p, 0) + "').textContent='" + param(p, 1) + "';");
        }
        else if (name == "REDIRECT") {
            ensureId(current());
            current()->events.push_back({"click", "window.location.href='" + param(p, 0) + "'"});
        }
        else if (name == "TIMER") {
            std::string delay = param(p, 0).empty() ? "1000" : p[0];
            std::string js = param(p, 1);
            std::string repeat = param(p, 2);
            std::string fn = (repeat == "true") ? "setInterval" : "setTimeout";
            rawJS.push_back(fn + "(function(){" + js + "}," + delay + ");");
        }
        else if (name == "FUNC_DEF") {
            rawJS.push_back("function " + param(p, 0) + "(" + param(p, 1) + "){");
        }
        else if (name == "FUNC_END") {
            rawJS.push_back("}");
        }
        else if (name == "FUNC_CALL") {
            rawJS.push_back(param(p, 0) + ";");
        }
        else if (name == "DOM_QUERY") {
            rawJS.push_back("var " + param(p, 0) + "=document.querySelector('" + param(p, 1) + "');");
        }
        else if (name == "DOM_CREATE") {
            rawJS.push_back("var " + param(p, 0) + "=document.createElement('" + param(p, 1) + "');");
        }
        else if (name == "DOM_APPEND") {
            rawJS.push_back(param(p, 0) + ".appendChild(" + param(p, 1) + ");");
        }
        else if (name == "INNER_HTML") {
            rawJS.push_back("document.querySelector('" + param(p, 0) + "').innerHTML=`" + param(p, 1) + "`;");
        }
        else if (name == "ON_SUBMIT") {
            std::string js = param(p, 0);
            ensureId(current());
            rawJS.push_back(
                "document.getElementById('" + current()->id + "').addEventListener('submit',function(e){"
                "e.preventDefault();" + js + "});"
            );
        }
        else if (name == "FETCH_POST") {
            // param[0] = url, param[1] = body expression, param[2] = callback
            std::string url = param(p, 0);
            std::string body = param(p, 1);
            std::string cb = p.size() > 2 ? p[2] : "console.log";
            rawJS.push_back(
                "fetch('" + url + "',{method:'POST',headers:{'Content-Type':'application/json'},"
                "body:JSON.stringify(" + body + ")}).then(function(r){return r.json()}).then(" + cb + ");"
            );
        }
        else if (name == "SCROLL_REVEAL") {
            // Apply fade-in-on-scroll to current element
            ensureId(current());
            current()->styles.push_back({"opacity", "0"});
            current()->styles.push_back({"transform", "translateY(20px)"});
            current()->styles.push_back({"transition", "opacity 0.6s ease, transform 0.6s ease"});

            rawJS.push_back(
                "(function(){var el=document.getElementById('" + current()->id + "');"
                "var ob=new IntersectionObserver(function(e){e.forEach(function(x){"
                "if(x.isIntersecting){el.style.opacity='1';el.style.transform='translateY(0)';ob.unobserve(el);}});}"
                ",{threshold:0.1});ob.observe(el);})();"
            );
        }
        // ACCORDION
        else if (name == "ACCORDION") {
            auto wrap = std::make_shared<ElementNode>("div");
            wrap->id = nextId();
            current()->children.push_back(wrap);
            stack.push_back(wrap.get());
            return;
        }
        else if (name == "ACCORDION_ITEM") {
            std::string title = param(p, 0);
            std::string aid = nextId();
            auto item = std::make_shared<ElementNode>("div");

            auto header = std::make_shared<ElementNode>("button");
            header->id = aid + "h";
            header->styles.push_back({"width", "100%"});
            header->styles.push_back({"text-align", "left"});
            header->styles.push_back({"background", "none"});
            header->styles.push_back({"border", "none"});
            header->styles.push_back({"border-bottom", "1px solid currentColor"});
            header->styles.push_back({"padding", "16px 0"});
            header->styles.push_back({"font-size", "inherit"});
            header->styles.push_back({"font-family", "inherit"});
            header->styles.push_back({"color", "inherit"});
            header->styles.push_back({"cursor", "pointer"});
            header->children.push_back(std::make_shared<TextNode>(title));
            item->children.push_back(header);

            auto content = std::make_shared<ElementNode>("div");
            content->id = aid + "c";
            content->styles.push_back({"max-height", "0"});
            content->styles.push_back({"overflow", "hidden"});
            content->styles.push_back({"transition", "max-height 0.3s ease"});
            content->styles.push_back({"padding", "0 0 0 0"});
            item->children.push_back(content);

            current()->children.push_back(item);
            stack.push_back(content.get());

            rawJS.push_back(
                "document.getElementById('" + aid + "h').onclick=function(){"
                "var c=document.getElementById('" + aid + "c');"
                "if(c.style.maxHeight&&c.style.maxHeight!=='0px'){c.style.maxHeight='0';c.style.padding='0';}"
                "else{c.style.maxHeight=c.scrollHeight+'px';c.style.padding='16px 0';}};"
            );
            return;
        }
        else if (name == "ACCORDION_END") {
            if (stack.size() > 1) stack.pop_back();
            return;
        }
        // TABS
        else if (name == "TABS") {
            std::string tid = nextId();
            auto wrap = std::make_shared<ElementNode>("div");
            wrap->id = tid;
            auto tabBar = std::make_shared<ElementNode>("div");
            tabBar->id = tid + "bar";
            tabBar->styles.push_back({"display", "flex"});
            tabBar->styles.push_back({"gap", "0"});
            tabBar->styles.push_back({"border-bottom", "1px solid currentColor"});
            wrap->children.push_back(tabBar);
            auto panels = std::make_shared<ElementNode>("div");
            panels->id = tid + "panels";
            wrap->children.push_back(panels);
            current()->children.push_back(wrap);
            // Push panels for content, store tabBar id
            _tabBarId = tid + "bar";
            _tabPanelsId = tid + "panels";
            _tabIndex = 0;
            stack.push_back(panels.get());
            return;
        }
        else if (name == "TAB") {
            std::string label = param(p, 0);
            std::string pid = nextId();
            // Add button to tab bar
            rawJS.push_back(
                "(function(){var b=document.createElement('button');b.textContent='" + label + "';"
                "b.style.cssText='background:none;border:none;padding:12px 20px;font:inherit;color:inherit;cursor:pointer;opacity:0.5';"
                "b.onclick=function(){var ps=document.getElementById('" + _tabPanelsId + "').children;"
                "for(var i=0;i<ps.length;i++)ps[i].style.display='none';"
                "document.getElementById('" + pid + "').style.display='block';"
                "var bs=document.getElementById('" + _tabBarId + "').children;"
                "for(var i=0;i<bs.length;i++)bs[i].style.opacity='0.5';"
                "b.style.opacity='1';};"
                "document.getElementById('" + _tabBarId + "').appendChild(b);"
                "if(" + std::to_string(_tabIndex) + "===0)b.style.opacity='1';})();"
            );
            return;
        }
        else if (name == "TAB_PANEL") {
            auto panel = std::make_shared<ElementNode>("div");
            panel->id = nextId();
            if (_tabIndex > 0) panel->styles.push_back({"display", "none"});
            panel->styles.push_back({"padding", "16px 0"});
            current()->children.push_back(panel);
            stack.push_back(panel.get());
            // Fix: use the id we need for TAB button references
            // Go back and update the last TAB's pid
            // Actually store current panel id for the preceding TAB
            _tabIndex++;
            return;
        }
        else if (name == "TABS_END") {
            if (stack.size() > 1) stack.pop_back();
            return;
        }
        // TOOLTIP
        else if (name == "TOOLTIP") {
            std::string text = param(p, 0);
            ensureId(current());
            current()->styles.push_back({"position", "relative"});
            rawCSS.push_back(
                "#" + current()->id + "::after{content:'" + text + "';position:absolute;bottom:calc(100% + 8px);"
                "left:50%;transform:translateX(-50%);background:#fff;color:#000;padding:6px 12px;"
                "border-radius:6px;font-size:12px;white-space:nowrap;opacity:0;pointer-events:none;"
                "transition:opacity 0.2s ease;z-index:999;}"
                "#" + current()->id + ":hover::after{opacity:1;}"
            );
            return;
        }
        // DROPDOWN
        else if (name == "DROPDOWN") {
            std::string label = param(p, 0);
            std::string did = nextId();
            auto wrap = std::make_shared<ElementNode>("div");
            wrap->id = did;
            wrap->styles.push_back({"position", "relative"});
            wrap->styles.push_back({"display", "inline-block"});

            auto btn = std::make_shared<ElementNode>("button");
            btn->id = did + "btn";
            btn->styles.push_back({"cursor", "pointer"});
            btn->styles.push_back({"background", "none"});
            btn->styles.push_back({"border", "1px solid currentColor"});
            btn->styles.push_back({"padding", "8px 16px"});
            btn->styles.push_back({"font", "inherit"});
            btn->styles.push_back({"color", "inherit"});
            btn->styles.push_back({"border-radius", "6px"});
            btn->children.push_back(std::make_shared<TextNode>(label));
            wrap->children.push_back(btn);

            current()->children.push_back(wrap);
            stack.push_back(wrap.get());

            rawJS.push_back(
                "document.getElementById('" + did + "btn').onclick=function(e){"
                "e.stopPropagation();var m=document.getElementById('" + did + "menu');"
                "m.style.display=m.style.display==='block'?'none':'block';};"
                "document.addEventListener('click',function(){var m=document.getElementById('" + did + "menu');"
                "if(m)m.style.display='none';});"
            );
            return;
        }
        else if (name == "DROPDOWN_ITEMS") {
            auto menu = std::make_shared<ElementNode>("div");
            // Find parent dropdown id
            std::string pid = current()->id;
            menu->id = pid + "menu";
            menu->styles.push_back({"display", "none"});
            menu->styles.push_back({"position", "absolute"});
            menu->styles.push_back({"top", "calc(100% + 4px)"});
            menu->styles.push_back({"left", "0"});
            menu->styles.push_back({"min-width", "160px"});
            menu->styles.push_back({"border", "1px solid currentColor"});
            menu->styles.push_back({"border-radius", "6px"});
            menu->styles.push_back({"padding", "4px"});
            menu->styles.push_back({"z-index", "100"});
            current()->children.push_back(menu);
            stack.push_back(menu.get());
            return;
        }
        else if (name == "DROPDOWN_END") {
            if (stack.size() > 1) stack.pop_back();
            return;
        }
        // COPY_BUTTON
        else if (name == "COPY_BUTTON") {
            std::string text = param(p, 0);
            std::string label = p.size() > 1 ? p[1] : "copy";
            std::string bid = nextId();
            auto btn = std::make_shared<ElementNode>("button");
            btn->id = bid;
            btn->styles.push_back({"cursor", "pointer"});
            btn->children.push_back(std::make_shared<TextNode>(label));
            current()->children.push_back(btn);

            rawJS.push_back(
                "document.getElementById('" + bid + "').onclick=function(){"
                "navigator.clipboard.writeText('" + text + "');"
                "this.textContent='copied';var b=this;setTimeout(function(){b.textContent='" + label + "';},1500);};"
            );
            return;
        }
        // PROGRESS_BAR
        else if (name == "PROGRESS_BAR") {
            std::string pct = param(p, 0);
            auto bar = std::make_shared<ElementNode>("div");
            bar->styles.push_back({"width", "100%"});
            bar->styles.push_back({"height", "8px"});
            bar->styles.push_back({"background", "rgba(255,255,255,0.1)"});
            bar->styles.push_back({"border-radius", "4px"});
            bar->styles.push_back({"overflow", "hidden"});
            auto fill = std::make_shared<ElementNode>("div");
            fill->styles.push_back({"width", pct + "%"});
            fill->styles.push_back({"height", "100%"});
            fill->styles.push_back({"background", "currentColor"});
            fill->styles.push_back({"border-radius", "4px"});
            fill->styles.push_back({"transition", "width 0.6s ease"});
            bar->children.push_back(fill);
            current()->children.push_back(bar);
            return;
        }
        // COUNTER -- animated count up on scroll
        else if (name == "COUNTER") {
            std::string target = param(p, 0);
            std::string suffix = p.size() > 1 ? p[1] : "";
            std::string cid = nextId();
            auto span = std::make_shared<ElementNode>("span");
            span->id = cid;
            span->children.push_back(std::make_shared<TextNode>("0" + suffix));
            current()->children.push_back(span);

            rawJS.push_back(
                "(function(){var el=document.getElementById('" + cid + "'),t=" + target + ",done=false;"
                "var ob=new IntersectionObserver(function(e){e.forEach(function(x){"
                "if(x.isIntersecting&&!done){done=true;var s=0,d=Math.max(1,Math.floor(2000/t));"
                "var iv=setInterval(function(){s+=Math.ceil(t/60);if(s>=t){s=t;clearInterval(iv);}"
                "el.textContent=s+'" + suffix + "';},d);}});},{threshold:0.1});ob.observe(el);})();"
            );
            return;
        }
        // STICKY
        else if (name == "STICKY") {
            current()->styles.push_back({"position", "sticky"});
            current()->styles.push_back({"top", param(p, 0).empty() ? "0" : p[0]});
            current()->styles.push_back({"z-index", "100"});
            return;
        }
        // EMBED
        else if (name == "EMBED") {
            std::string url = param(p, 0);
            auto iframe = std::make_shared<ElementNode>("iframe");
            iframe->attrs["src"] = url;
            iframe->attrs["frameborder"] = "0";
            iframe->attrs["allowfullscreen"] = "";
            iframe->styles.push_back({"width", "100%"});
            iframe->styles.push_back({"aspect-ratio", "16/9"});
            iframe->styles.push_back({"border-radius", "8px"});
            current()->children.push_back(iframe);
            return;
        }
        // BADGE
        else if (name == "BADGE") {
            std::string text = param(p, 0);
            auto badge = std::make_shared<ElementNode>("span");
            badge->styles.push_back({"display", "inline-block"});
            badge->styles.push_back({"padding", "4px 12px"});
            badge->styles.push_back({"border-radius", "100px"});
            badge->styles.push_back({"font-size", "12px"});
            badge->styles.push_back({"font-weight", "600"});
            badge->styles.push_back({"letter-spacing", "1px"});
            badge->styles.push_back({"text-transform", "uppercase"});
            badge->children.push_back(std::make_shared<TextNode>(text));
            current()->children.push_back(badge);
            stack.push_back(badge.get());
            return;
        }
        // AVATAR
        else if (name == "AVATAR") {
            std::string src = param(p, 0);
            std::string size = p.size() > 1 ? p[1] : "40px";
            auto av = std::make_shared<ElementNode>("img");
            av->attrs["src"] = src;
            av->attrs["alt"] = "";
            av->styles.push_back({"width", size});
            av->styles.push_back({"height", size});
            av->styles.push_back({"border-radius", "50%"});
            av->styles.push_back({"object-fit", "cover"});
            current()->children.push_back(av);
            return;
        }
        // DIVIDER
        else if (name == "DIVIDER") {
            std::string style = param(p, 0).empty() ? "solid" : p[0];
            auto div = std::make_shared<ElementNode>("hr");
            div->styles.push_back({"border", "none"});
            div->styles.push_back({"border-top", "1px " + style + " currentColor"});
            div->styles.push_back({"opacity", "0.2"});
            div->styles.push_back({"margin", "24px 0"});
            current()->children.push_back(div);
            return;
        }
        // RATING
        else if (name == "RATING") {
            int stars = p.empty() ? 5 : std::stoi(p[0]);
            int max = p.size() > 1 ? std::stoi(p[1]) : 5;
            auto wrap = std::make_shared<ElementNode>("span");
            wrap->styles.push_back({"font-size", "20px"});
            wrap->styles.push_back({"letter-spacing", "2px"});
            std::string s;
            for (int i = 0; i < max; i++) s += (i < stars) ? "\xe2\x98\x85" : "\xe2\x98\x86";
            wrap->children.push_back(std::make_shared<TextNode>(s));
            current()->children.push_back(wrap);
            return;
        }
        // SEARCH_FILTER
        else if (name == "SEARCH_FILTER") {
            std::string target = param(p, 0); // CSS selector for list items
            std::string sid = nextId();
            auto input = std::make_shared<ElementNode>("input");
            input->id = sid;
            input->attrs["type"] = "text";
            input->attrs["placeholder"] = p.size() > 1 ? p[1] : "Search...";
            input->styles.push_back({"width", "100%"});
            input->styles.push_back({"padding", "10px 14px"});
            input->styles.push_back({"font", "inherit"});
            input->styles.push_back({"border", "1px solid currentColor"});
            input->styles.push_back({"border-radius", "6px"});
            input->styles.push_back({"background", "transparent"});
            input->styles.push_back({"color", "inherit"});
            input->styles.push_back({"margin-bottom", "16px"});
            current()->children.push_back(input);

            rawJS.push_back(
                "document.getElementById('" + sid + "').oninput=function(){"
                "var v=this.value.toLowerCase();"
                "document.querySelectorAll('" + target + "').forEach(function(el){"
                "el.style.display=el.textContent.toLowerCase().includes(v)?'':'none';});};"
            );
            rawCSS.push_back("#" + sid + ":focus{outline:none;border-color:currentColor;}");
            return;
        }
        // PRICING_TABLE
        else if (name == "PRICING_TABLE") {
            auto wrap = std::make_shared<ElementNode>("div");
            wrap->styles.push_back({"display", "grid"});
            wrap->styles.push_back({"grid-template-columns", "repeat(auto-fit, minmax(240px, 1fr))"});
            wrap->styles.push_back({"gap", "20px"});
            current()->children.push_back(wrap);
            stack.push_back(wrap.get());
            return;
        }
        else if (name == "PRICING_TIER") {
            std::string name_s = param(p, 0);
            std::string price = p.size() > 1 ? p[1] : "$0";
            auto card = std::make_shared<ElementNode>("div");
            card->styles.push_back({"border", "1px solid currentColor"});
            card->styles.push_back({"border-radius", "12px"});
            card->styles.push_back({"padding", "28px"});
            card->styles.push_back({"text-align", "center"});

            auto n = std::make_shared<ElementNode>("p");
            n->styles.push_back({"font-weight", "600"});
            n->styles.push_back({"margin-bottom", "8px"});
            n->children.push_back(std::make_shared<TextNode>(name_s));
            card->children.push_back(n);

            auto pr = std::make_shared<ElementNode>("p");
            pr->styles.push_back({"font-size", "36px"});
            pr->styles.push_back({"font-weight", "800"});
            pr->styles.push_back({"margin-bottom", "16px"});
            pr->children.push_back(std::make_shared<TextNode>(price));
            card->children.push_back(pr);

            current()->children.push_back(card);
            stack.push_back(card.get());
            return;
        }
        else if (name == "PRICING_END") {
            if (stack.size() > 1) stack.pop_back();
            return;
        }
    }

    // ============================================================
    // META
    // ============================================================
    void handleMeta(const std::string& name, const std::vector<std::string>& p) {
        std::string val = param(p, 0);

        if (name == "TITLE") title = val;
        else if (name == "CHARSET") charset = val;
        else if (name == "VIEWPORT") viewport = val;
        else if (name == "DESCRIPTION") description = val;
        else if (name == "FAVICON") favicon = val;
        else if (name == "IMPORT_FONT") fontImports.push_back(val);
        else if (name == "IMPORT_SCRIPT") scriptImports.push_back(val);
        else if (name == "IMPORT_STYLE") styleImports.push_back(val);
        else if (name == "ID") current()->id = val;
        else if (name == "CLASS") {
            // Split by spaces
            std::istringstream ss(val);
            std::string cls;
            while (ss >> cls) current()->classes.push_back(cls);
        }
        else if (name == "ATTR") {
            current()->attrs[val] = param(p, 1);
        }
        else if (name == "CSS_VAR") {
            cssVars.push_back({val, param(p, 1)});
        }
        else if (name == "GLOBAL_STYLE") {
            _globalSel = val;
            _globalBuf.clear();
        }
        else if (name == "GLOBAL_STYLE_END") {
            if (!_globalSel.empty()) {
                if (!_mediaSel.empty()) {
                    _mediaRules.push_back({_globalSel, _globalBuf});
                } else {
                    globalStyles.push_back({_globalSel, _globalBuf});
                }
            }
            _globalSel.clear();
            _globalBuf.clear();
        }
        else if (name == "HOVER_STYLE") {
            _hoverSel = val;
            _hoverBuf.clear();
        }
        else if (name == "HOVER_STYLE_END") {
            if (!_hoverSel.empty()) {
                hoverStyles.push_back({_hoverSel, _hoverBuf});
            }
            _hoverSel.clear();
            _hoverBuf.clear();
        }
        else if (name == "KEYFRAMES") {
            _kfName = val;
            _kfSteps.clear();
            _kfStepActive = false;
        }
        else if (name == "KEYFRAME_STEP") {
            if (_kfStepActive && !_kfStepBuf.empty()) {
                _kfSteps.push_back({_kfStepName, _kfStepBuf});
            }
            _kfStepName = val;
            _kfStepBuf.clear();
            _kfStepActive = true;
        }
        else if (name == "KEYFRAMES_END") {
            if (_kfStepActive && !_kfStepBuf.empty()) {
                _kfSteps.push_back({_kfStepName, _kfStepBuf});
            }
            if (!_kfName.empty()) {
                keyframes[_kfName] = _kfSteps;
            }
            _kfName.clear();
            _kfSteps.clear();
            _kfStepActive = false;
            _kfStepBuf.clear();
        }
        else if (name == "MEDIA_QUERY") {
            _mediaSel = val;
            _mediaRules.clear();
        }
        else if (name == "MEDIA_END") {
            if (!_mediaSel.empty()) {
                mediaQueries[_mediaSel] = _mediaRules;
            }
            _mediaSel.clear();
            _mediaRules.clear();
        }
        else if (name == "RAW_HTML") {
            current()->children.push_back(std::make_shared<RawHTMLNode>(val));
        }
        else if (name == "RAW_CSS") {
            rawCSS.push_back(val);
        }
        else if (name == "RAW_JS") {
            rawJS.push_back(val);
        }
        else if (name == "COMMENT") {
            // ignored
        }
        else if (name == "OG_IMAGE") {
            // Will be injected in head
            rawCSS.push_back("/* og:image=" + val + " */"); // placeholder, actual meta in generateHTML
            // Store for head generation
            if (!val.empty()) {
                scriptImports; // unused, just need to add meta tag
                // Add via raw approach
            }
        }
        else if (name == "OG_TITLE") {
            // stored, used in head
        }
        else if (name == "ANALYTICS") {
            // param[0] = tracking ID (like GA4)
            std::string tid = val;
            scriptImports.push_back("https://www.googletagmanager.com/gtag/js?id=" + tid);
            rawJS.push_back("window.dataLayer=window.dataLayer||[];function gtag(){dataLayer.push(arguments);}gtag('js',new Date());gtag('config','" + tid + "');");
        }
        else if (name == "COOKIE_BANNER") {
            std::string msg = val.empty() ? "This site uses cookies." : val;
            std::string bid = "cookie-banner";
            rawCSS.push_back(
                "#" + bid + "{position:fixed;bottom:0;left:0;right:0;padding:16px 24px;"
                "display:flex;justify-content:space-between;align-items:center;z-index:99998;"
                "font-size:14px;transition:transform 0.3s ease;}"
                "#" + bid + ".hidden{transform:translateY(100%);}"
            );
            rawJS.push_back(
                "if(!localStorage.getItem('bd-cookies')){"
                "var cb=document.createElement('div');cb.id='" + bid + "';"
                "cb.innerHTML='<span>" + msg + "</span><button id=\"cookie-ok\" style=\"cursor:pointer;background:currentColor;"
                "color:inherit;border:none;padding:8px 20px;border-radius:6px;font:inherit;margin-left:16px;mix-blend-mode:difference\">Accept</button>';"
                "document.body.appendChild(cb);"
                "document.getElementById('cookie-ok').onclick=function(){localStorage.setItem('bd-cookies','1');cb.classList.add('hidden');};"
                "}"
            );
        }
    }

    // ============================================================
    // HTML OUTPUT
    // ============================================================
    std::string generateHTML() {
        std::ostringstream out;
        out << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n";
        out << "  <meta charset=\"" << charset << "\">\n";
        out << "  <meta name=\"viewport\" content=\"" << viewport << "\">\n";
        out << "  <title>" << escHTML(title) << "</title>\n";
        if (!description.empty())
            out << "  <meta name=\"description\" content=\"" << escHTML(description) << "\">\n";
        if (!favicon.empty())
            out << "  <link rel=\"icon\" href=\"" << favicon << "\">\n";
        for (auto& url : fontImports) {
            out << "  <link rel=\"preconnect\" href=\"https://fonts.googleapis.com\">\n";
            out << "  <link rel=\"preconnect\" href=\"https://fonts.gstatic.com\" crossorigin>\n";
            out << "  <link rel=\"stylesheet\" href=\"" << url << "\">\n";
        }
        for (auto& url : styleImports)
            out << "  <link rel=\"stylesheet\" href=\"" << url << "\">\n";
        for (auto& url : scriptImports)
            out << "  <script src=\"" << url << "\"></script>\n";

        // CSS
        std::string css = generateCSS();
        if (!css.empty()) {
            out << "  <style>\n" << css << "\n  </style>\n";
        }

        out << "</head>\n<body>\n";

        // Render children
        for (auto& child : root->children) {
            out << renderNode(child.get(), 1) << "\n";
        }

        // JS
        std::string js = generateJS();
        if (!js.empty()) {
            out << "  <script>\n" << js << "\n  </script>\n";
        }

        out << "</body>\n</html>";
        return out.str();
    }

    std::string generateCSS() {
        std::ostringstream out;

        // CSS vars
        if (!cssVars.empty()) {
            out << ":root {\n";
            for (auto& [name, val] : cssVars)
                out << "  " << name << ": " << val << ";\n";
            out << "}\n\n";
        }

        // Global styles
        for (auto& rule : globalStyles)
            out << cssRuleStr(rule.selector, rule.props) << "\n";

        // Hover
        for (auto& rule : hoverStyles)
            out << cssRuleStr(rule.selector + ":hover", rule.props) << "\n";

        // Focus
        for (auto& rule : focusStyles)
            out << cssRuleStr(rule.selector + ":focus", rule.props) << "\n";

        // Active
        for (auto& rule : activeStyles)
            out << cssRuleStr(rule.selector + ":active", rule.props) << "\n";

        // Keyframes
        for (auto& [name, steps] : keyframes) {
            out << "@keyframes " << name << " {\n";
            for (auto& step : steps) {
                out << "  " << step.step << " {\n";
                for (auto& [p, v] : step.props)
                    out << "    " << p << ": " << v << ";\n";
                out << "  }\n";
            }
            out << "}\n\n";
        }

        // Media queries
        for (auto& [query, rules] : mediaQueries) {
            out << "@media " << query << " {\n";
            for (auto& rule : rules) {
                out << "  " << rule.selector << " {\n";
                for (auto& [p, v] : rule.props)
                    out << "    " << p << ": " << v << ";\n";
                out << "  }\n";
            }
            out << "}\n\n";
        }

        // Raw CSS
        for (auto& raw : rawCSS)
            out << raw << "\n";

        return out.str();
    }

    std::string cssRuleStr(const std::string& selector,
                           const std::vector<std::pair<std::string, std::string>>& props) {
        if (props.empty()) return "";
        std::ostringstream out;
        out << selector << " {\n";
        for (auto& [p, v] : props)
            out << "  " << p << ": " << v << ";\n";
        out << "}\n";
        return out.str();
    }

    std::string generateJS() {
        std::ostringstream out;
        auto events = collectEvents(root.get());

        if (!events.empty() || !rawJS.empty()) {
            out << "document.addEventListener('DOMContentLoaded',function(){\n";
            for (auto& ev : events) {
                out << "  var el=document.getElementById('" << ev.elemId << "');\n";
                out << "  if(el)el.addEventListener('" << ev.eventType << "',function(e){" << ev.jsCode << "});\n";
            }
            for (auto& js : rawJS)
                out << "  " << js << "\n";
            out << "});\n";
        }

        return out.str();
    }

    struct CollectedEvent {
        std::string elemId;
        std::string eventType;
        std::string jsCode;
    };

    std::vector<CollectedEvent> collectEvents(ElementNode* node) {
        std::vector<CollectedEvent> result;
        for (auto& ev : node->events) {
            result.push_back({node->id, ev.eventType, ev.jsCode});
        }
        for (auto& child : node->children) {
            if (child->type == NodeType::ELEMENT) {
                auto childEvents = collectEvents(static_cast<ElementNode*>(child.get()));
                result.insert(result.end(), childEvents.begin(), childEvents.end());
            }
        }
        return result;
    }

    bool _inPre = false;

    std::string renderNode(Node* node, int depth) {
        std::string indent = _inPre ? "" : std::string(depth * 2, ' ');
        std::string nl = _inPre ? "" : "\n";

        if (node->type == NodeType::TEXT) {
            return indent + static_cast<TextNode*>(node)->text;
        }
        if (node->type == NodeType::RAW_HTML) {
            return indent + static_cast<RawHTMLNode*>(node)->html;
        }

        auto* elem = static_cast<ElementNode*>(node);
        bool selfClose = selfClosingTags().count(elem->tag) > 0;

        // Build attributes
        std::ostringstream attrOut;
        if (!elem->id.empty())
            attrOut << " id=\"" << elem->id << "\"";
        if (!elem->classes.empty()) {
            attrOut << " class=\"";
            for (size_t i = 0; i < elem->classes.size(); i++) {
                if (i > 0) attrOut << " ";
                attrOut << elem->classes[i];
            }
            attrOut << "\"";
        }
        for (auto& [k, v] : elem->attrs) {
            if (v.empty()) attrOut << " " << k;
            else attrOut << " " << k << "=\"" << escHTML(v) << "\"";
        }
        if (!elem->styles.empty()) {
            attrOut << " style=\"";
            for (size_t i = 0; i < elem->styles.size(); i++) {
                if (i > 0) attrOut << ";";
                attrOut << elem->styles[i].first << ":" << elem->styles[i].second;
            }
            attrOut << "\"";
        }

        std::string attrs = attrOut.str();

        if (selfClose) {
            return indent + "<" + elem->tag + attrs + ">";
        }

        if (elem->children.empty()) {
            return indent + "<" + elem->tag + attrs + "></" + elem->tag + ">";
        }

        // Single text child - inline
        if (elem->children.size() == 1 && elem->children[0]->type == NodeType::TEXT) {
            return indent + "<" + elem->tag + attrs + ">" +
                   static_cast<TextNode*>(elem->children[0].get())->text +
                   "</" + elem->tag + ">";
        }

        bool wasPre = _inPre;
        bool enteringPre = (!_inPre && elem->tag == "pre");
        if (enteringPre) _inPre = true;

        std::ostringstream out;
        if (enteringPre) {
            // <pre> content must have zero leading/trailing whitespace
            out << indent << "<" << elem->tag << attrs << ">";
            for (size_t i = 0; i < elem->children.size(); i++) {
                out << renderNode(elem->children[i].get(), 0);
                if (i + 1 < elem->children.size()) out << "\n";
            }
            out << "</" << elem->tag << ">";
        } else {
            out << indent << "<" << elem->tag << attrs << ">" << nl;
            for (auto& child : elem->children) {
                out << renderNode(child.get(), depth + 1) << nl;
            }
            out << indent << "</" << elem->tag << ">";
        }

        _inPre = wasPre;
        return out.str();
    }

    static std::string escHTML(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '&': out += "&amp;"; break;
                case '<': out += "&lt;"; break;
                case '>': out += "&gt;"; break;
                case '"': out += "&quot;"; break;
                default: out += c;
            }
        }
        return out;
    }
};

} // namespace bd
