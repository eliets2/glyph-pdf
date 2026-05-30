#include "SemanticDocument.h"

namespace docmodel {

Section::Section(std::string title, std::vector<std::shared_ptr<Block>> blocks, std::vector<std::shared_ptr<Section>> subsections, Provenance p)
    : Node(std::move(p)), title_(std::move(title)), blocks_(std::move(blocks)), subsections_(std::move(subsections)) {}

const std::string& Section::getTitle() const { return title_; }
const std::vector<std::shared_ptr<Block>>& Section::getBlocks() const { return blocks_; }
const std::vector<std::shared_ptr<Section>>& Section::getSubsections() const { return subsections_; }

SemanticDocument::SemanticDocument(std::vector<std::shared_ptr<Section>> sections, Provenance p)
    : Node(std::move(p)), sections_(std::move(sections)) {}

const std::vector<std::shared_ptr<Section>>& SemanticDocument::getSections() const { return sections_; }

} // namespace docmodel
