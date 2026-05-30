#pragma once
#include "Inline.h"
#include <vector>
#include <memory>

namespace docmodel {

class Block : public Node {
public:
    enum class Type {
        Paragraph,
        Heading,
        List,
        ListItem,
        CodeBlock
    };

    Block(Type t, Provenance p);
    virtual ~Block() = default;

    Type getType() const;
    virtual const std::vector<std::shared_ptr<Inline>>& getInlines() const;
    virtual const std::vector<std::shared_ptr<Block>>& getBlocks() const;

private:
    Type type_;
};

class TextBlock : public Block {
public:
    TextBlock(Type t, std::vector<std::shared_ptr<Inline>> inlines, Provenance p);
    const std::vector<std::shared_ptr<Inline>>& getInlines() const override;
private:
    std::vector<std::shared_ptr<Inline>> inlines_;
};

class ContainerBlock : public Block {
public:
    ContainerBlock(Type t, std::vector<std::shared_ptr<Block>> blocks, Provenance p);
    const std::vector<std::shared_ptr<Block>>& getBlocks() const override;
private:
    std::vector<std::shared_ptr<Block>> blocks_;
};

} // namespace docmodel
