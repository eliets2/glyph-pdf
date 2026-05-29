#include "Block.h"

namespace docmodel {

static const std::vector<std::shared_ptr<Inline>> empty_inlines;
static const std::vector<std::shared_ptr<Block>> empty_blocks;

Block::Block(Type t, Provenance p) : Node(std::move(p)), type_(t) {}
Block::Type Block::getType() const { return type_; }
const std::vector<std::shared_ptr<Inline>>& Block::getInlines() const { return empty_inlines; }
const std::vector<std::shared_ptr<Block>>& Block::getBlocks() const { return empty_blocks; }

TextBlock::TextBlock(Type t, std::vector<std::shared_ptr<Inline>> inlines, Provenance p)
    : Block(t, std::move(p)), inlines_(std::move(inlines)) {}
const std::vector<std::shared_ptr<Inline>>& TextBlock::getInlines() const { return inlines_; }

ContainerBlock::ContainerBlock(Type t, std::vector<std::shared_ptr<Block>> blocks, Provenance p)
    : Block(t, std::move(p)), blocks_(std::move(blocks)) {}
const std::vector<std::shared_ptr<Block>>& ContainerBlock::getBlocks() const { return blocks_; }

} // namespace docmodel
