#pragma once
#include "ProvenanceTag.h"
#include <string>
#include <vector>
#include <memory>

namespace docmodel {

class Inline : public Node {
public:
    enum class Type {
        Text,
        Emph,
        Strong,
        Code
    };

    Inline(Type t, Provenance p);
    virtual ~Inline() = default;

    Type getType() const;
    virtual std::string getText() const;
    virtual const std::vector<std::shared_ptr<Inline>>& getChildren() const;

private:
    Type type_;
};

class TextInline : public Inline {
public:
    TextInline(std::string text, Provenance p);
    std::string getText() const override;
private:
    std::string text_;
};

class ContainerInline : public Inline {
public:
    ContainerInline(Type t, std::vector<std::shared_ptr<Inline>> children, Provenance p);
    const std::vector<std::shared_ptr<Inline>>& getChildren() const override;
private:
    std::vector<std::shared_ptr<Inline>> children_;
};

} // namespace docmodel
