#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace tinyxml2
{

enum XMLError
{
    XML_SUCCESS = 0,
    XML_ERROR_FILE_NOT_FOUND,
    XML_ERROR_FILE_READ_ERROR,
    XML_ERROR_PARSING
};

class XMLElement;

class XMLDocument
{
public:
    XMLDocument();

    XMLError LoadFile(const char* filename);
    const char* ErrorStr() const;

    XMLElement* FirstChildElement(const char* name = nullptr);

private:
    struct Node
    {
        std::string name;
        std::vector<std::pair<std::string, std::string>> attributes;
        std::string text;
        Node* parent = nullptr;
        std::vector<Node*> children;
    };

    std::vector<Node*> ownedNodes_;
    std::vector<std::unique_ptr<XMLElement>> elementHandles_;
    Node* root_ = nullptr;
    std::string error_;

    Node* NewNode();
    void Reset();
    XMLError Parse(const std::string& xml);
    XMLElement* MakeHandle(Node* node);

    friend class XMLElement;
};

class XMLElement
{
public:
    XMLElement(XMLDocument* doc, XMLDocument::Node* node);

    const char* Name() const;

    const char* Attribute(const char* name) const;
    int IntAttribute(const char* name, int defaultValue = 0) const;

    XMLElement* FirstChildElement(const char* name = nullptr) const;
    XMLElement* NextSiblingElement(const char* name = nullptr) const;

    const char* GetText() const;

private:
    friend class XMLDocument;

    XMLDocument* doc_ = nullptr;
    XMLDocument::Node* node_ = nullptr;
};

} // namespace tinyxml2
