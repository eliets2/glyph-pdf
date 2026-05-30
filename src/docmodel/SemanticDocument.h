#pragma once
#include "Block.h"
#include <string>
#include <vector>
#include <memory>

namespace docmodel {

class Section : public Node {
public:
    Section(std::string title, std::vector<std::shared_ptr<Block>> blocks, std::vector<std::shared_ptr<Section>> subsections, Provenance p);
    
    const std::string& getTitle() const;
    const std::vector<std::shared_ptr<Block>>& getBlocks() const;
    const std::vector<std::shared_ptr<Section>>& getSubsections() const;

private:
    std::string title_;
    std::vector<std::shared_ptr<Block>> blocks_;
    std::vector<std::shared_ptr<Section>> subsections_;
};

class SemanticDocument : public Node {
public:
    SemanticDocument(std::vector<std::shared_ptr<Section>> sections, Provenance p);

    const std::vector<std::shared_ptr<Section>>& getSections() const;

private:
    std::vector<std::shared_ptr<Section>> sections_;
};

} // namespace docmodel
