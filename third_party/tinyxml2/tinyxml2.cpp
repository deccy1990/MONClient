#include "tinyxml2.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace tinyxml2
{

namespace
{

static void SkipWhitespace(const std::string& s, size_t& i)
{
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])))
        ++i;
}

static bool StartsWith(const std::string& s, size_t i, const char* token)
{
    for (size_t j = 0; token[j] != '\0'; ++j)
    {
        if (i + j >= s.size() || s[i + j] != token[j])
            return false;
    }
    return true;
}

static std::string ReadUntil(const std::string& s, size_t& i, char end)
{
    size_t start = i;
    while (i < s.size() && s[i] != end)
        ++i;
    return s.substr(start, i - start);
}

static std::string Trim(const std::string& s)
{
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
        ++start;

    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])))
        --end;

    return s.substr(start, end - start);
}

} // namespace

XMLDocument::XMLDocument() = default;

XMLDocument::Node* XMLDocument::NewNode()
{
    ownedNodes_.push_back(new Node());
    return ownedNodes_.back();
}

XMLElement* XMLDocument::MakeHandle(Node* node)
{
    elementHandles_.push_back(std::make_unique<XMLElement>(this, node));
    return elementHandles_.back().get();
}

void XMLDocument::Reset()
{
    elementHandles_.clear();
    for (Node* n : ownedNodes_)
        delete n;
    ownedNodes_.clear();
    root_ = nullptr;
    error_.clear();
}

const char* XMLDocument::ErrorStr() const
{
    return error_.empty() ? "" : error_.c_str();
}

static bool ReadFileToString(const char* filename, std::string& out)
{
    std::ifstream file(filename, std::ios::in | std::ios::binary);
    if (!file.is_open())
        return false;

    std::ostringstream ss;
    ss << file.rdbuf();
    out = ss.str();
    return true;
}

XMLError XMLDocument::LoadFile(const char* filename)
{
    Reset();

    std::string xml;
    if (!ReadFileToString(filename, xml))
    {
        error_ = "file not found";
        return XML_ERROR_FILE_NOT_FOUND;
    }

    return Parse(xml);
}

XMLError XMLDocument::Parse(const std::string& xml)
{
    std::vector<Node*> stack;

    size_t i = 0;
    while (i < xml.size())
    {
        if (xml[i] != '<')
        {
            size_t start = i;
            while (i < xml.size() && xml[i] != '<')
                ++i;

            if (!stack.empty())
            {
                stack.back()->text += xml.substr(start, i - start);
            }
            continue;
        }

        if (StartsWith(xml, i, "<?"))
        {
            i += 2;
            while (i + 1 < xml.size() && !StartsWith(xml, i, "?>"))
                ++i;
            i = (i + 1 < xml.size()) ? i + 2 : xml.size();
            continue;
        }

        if (StartsWith(xml, i, "<!--"))
        {
            i += 4;
            while (i + 2 < xml.size() && !StartsWith(xml, i, "-->"))
                ++i;
            i = (i + 2 < xml.size()) ? i + 3 : xml.size();
            continue;
        }

        ++i; // skip '<'

        bool closingTag = false;
        if (i < xml.size() && xml[i] == '/')
        {
            closingTag = true;
            ++i;
        }

        SkipWhitespace(xml, i);

        size_t nameStart = i;
        while (i < xml.size() && (std::isalnum(static_cast<unsigned char>(xml[i])) || xml[i] == '_' || xml[i] == '-'))
            ++i;
        std::string tagName = xml.substr(nameStart, i - nameStart);

        if (tagName.empty())
        {
            error_ = "parse error: empty tag name";
            return XML_ERROR_PARSING;
        }

        if (closingTag)
        {
            while (i < xml.size() && xml[i] != '>')
                ++i;
            if (i < xml.size())
                ++i;

            if (stack.empty() || stack.back()->name != tagName)
            {
                error_ = "parse error: mismatched closing tag";
                return XML_ERROR_PARSING;
            }

            stack.back()->text = Trim(stack.back()->text);
            stack.pop_back();
            continue;
        }

        Node* node = NewNode();
        node->name = tagName;

        SkipWhitespace(xml, i);

        while (i < xml.size() && xml[i] != '>' && xml[i] != '/')
        {
            size_t attrNameStart = i;
            while (i < xml.size() && (std::isalnum(static_cast<unsigned char>(xml[i])) || xml[i] == '_' || xml[i] == '-'))
                ++i;
            std::string attrName = xml.substr(attrNameStart, i - attrNameStart);

            SkipWhitespace(xml, i);
            if (i >= xml.size() || xml[i] != '=')
            {
                error_ = "parse error: expected '='";
                return XML_ERROR_PARSING;
            }
            ++i; // '='
            SkipWhitespace(xml, i);

            if (i >= xml.size() || (xml[i] != '"' && xml[i] != '\''))
            {
                error_ = "parse error: expected quote";
                return XML_ERROR_PARSING;
            }
            char quote = xml[i++];
            std::string attrValue = ReadUntil(xml, i, quote);
            if (i < xml.size())
                ++i; // closing quote

            node->attributes.emplace_back(std::move(attrName), std::move(attrValue));

            SkipWhitespace(xml, i);
        }

        bool selfClosing = false;
        if (i < xml.size() && xml[i] == '/')
        {
            selfClosing = true;
            ++i;
        }

        if (i < xml.size() && xml[i] == '>')
            ++i;

        if (!stack.empty())
        {
            node->parent = stack.back();
            stack.back()->children.push_back(node);
        }
        else if (!root_)
        {
            root_ = node;
        }
        else
        {
            error_ = "parse error: multiple root elements";
            return XML_ERROR_PARSING;
        }

        if (!selfClosing)
            stack.push_back(node);
    }

    if (!stack.empty())
    {
        error_ = "parse error: unclosed tag";
        return XML_ERROR_PARSING;
    }

    if (!root_)
    {
        error_ = "parse error: no root element";
        return XML_ERROR_PARSING;
    }

    return XML_SUCCESS;
}

XMLElement* XMLDocument::FirstChildElement(const char* name)
{
    if (!root_)
        return nullptr;

    if (!name || root_->name == name)
        return MakeHandle(root_);

    for (Node* child : root_->children)
    {
        if (child->name == name)
            return MakeHandle(child);
    }

    return nullptr;
}

XMLElement::XMLElement(XMLDocument* doc, XMLDocument::Node* node)
    : doc_(doc), node_(node)
{
}

const char* XMLElement::Name() const
{
    return node_ ? node_->name.c_str() : "";
}

const char* XMLElement::Attribute(const char* name) const
{
    if (!node_)
        return nullptr;

    for (const auto& kv : node_->attributes)
    {
        if (kv.first == name)
            return kv.second.c_str();
    }
    return nullptr;
}

int XMLElement::IntAttribute(const char* name, int defaultValue) const
{
    const char* attr = Attribute(name);
    if (!attr)
        return defaultValue;
    return std::atoi(attr);
}

XMLElement* XMLElement::FirstChildElement(const char* name) const
{
    if (!node_ || !doc_)
        return nullptr;

    for (XMLDocument::Node* child : node_->children)
    {
        if (!name || child->name == name)
            return doc_->MakeHandle(child);
    }
    return nullptr;
}

XMLElement* XMLElement::NextSiblingElement(const char* name) const
{
    if (!node_ || !doc_ || !node_->parent)
        return nullptr;

    const auto& siblings = node_->parent->children;
    bool foundSelf = false;
    for (XMLDocument::Node* sib : siblings)
    {
        if (!foundSelf)
        {
            foundSelf = (sib == node_);
            continue;
        }

        if (!name || sib->name == name)
            return doc_->MakeHandle(sib);
    }

    return nullptr;
}

const char* XMLElement::GetText() const
{
    return node_ ? node_->text.c_str() : nullptr;
}

} // namespace tinyxml2

