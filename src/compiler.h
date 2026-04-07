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
                for (auto& t : it->second) {
                    processToken(t);
                }
            }
            return;
        }

        // If defining a component, buffer tokens instead of executing
        if (_definingComponent) {
            _currentComponentTokens.push_back(token);
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
