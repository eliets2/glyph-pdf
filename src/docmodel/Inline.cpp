#include "Inline.h"

namespace docmodel {

static const std::vector<std::shared_ptr<Inline>> empty_children;

Inline::Inline(Type t, Provenance p) : Node(std::move(p)), type_(t) {}
Inline::Type Inline::getType() const { return type_; }
std::string Inline::getText() const { return ""; }
const std::vector<std::shared_ptr<Inline>>& Inline::getChildren() const { return empty_children; }

TextInline::TextInline(std::string text, Provenance p) : Inline(Type::Text, std::move(p)), text_(std::move(text)) {}
std::string TextInline::getText() const { return text_; }

ContainerInline::ContainerInline(Type t, std::vector<std::shared_ptr<Inline>> children, Provenance p) 
    : Inline(t, std::move(p)), children_(std::move(children)) {}
const std::vector<std::shared_ptr<Inline>>& ContainerInline::getChildren() const { return children_; }

} // namespace docmodel
