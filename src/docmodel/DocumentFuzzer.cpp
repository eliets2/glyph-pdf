// SPDX-License-Identifier: Apache-2.0
#include "DocumentFuzzer.h"
#include <random>
#include <string>

namespace docmodel {

static Provenance generateRandomProvenance(std::mt19937& gen) {
    Provenance p;
    std::uniform_int_distribution<> tag_dist(0, 2);
    p.tag = static_cast<ProvenanceTag>(tag_dist(gen));
    p.source_file = "generated_" + std::to_string(gen() % 1000) + ".pdf";
    p.page_index = gen() % 100;
    
    std::uniform_real_distribution<> coord_dist(0.0, 1000.0);
    p.bbox.x0 = coord_dist(gen);
    p.bbox.y0 = coord_dist(gen);
    p.bbox.x1 = p.bbox.x0 + coord_dist(gen) / 10.0;
    p.bbox.y1 = p.bbox.y0 + coord_dist(gen) / 10.0;
    return p;
}

static std::shared_ptr<Inline> generateRandomInline(std::mt19937& gen) {
    auto p = generateRandomProvenance(gen);
    std::uniform_int_distribution<> type_dist(0, 3);
    auto type = static_cast<Inline::Type>(type_dist(gen));
    
    if (type == Inline::Type::Text || type == Inline::Type::Code) {
        return std::make_shared<TextInline>("Random text " + std::to_string(gen() % 1000), std::move(p));
    } else {
        std::vector<std::shared_ptr<Inline>> children;
        children.push_back(std::make_shared<TextInline>("Inner text", generateRandomProvenance(gen)));
        return std::make_shared<ContainerInline>(type, std::move(children), std::move(p));
    }
}

static std::shared_ptr<Block> generateRandomBlock(std::mt19937& gen) {
    auto p = generateRandomProvenance(gen);
    std::uniform_int_distribution<> type_dist(0, 4);
    auto type = static_cast<Block::Type>(type_dist(gen));
    
    if (type == Block::Type::Paragraph || type == Block::Type::Heading || type == Block::Type::CodeBlock) {
        std::vector<std::shared_ptr<Inline>> inlines;
        int num_inlines = (gen() % 5) + 1;
        for (int i = 0; i < num_inlines; ++i) {
            inlines.push_back(generateRandomInline(gen));
        }
        return std::make_shared<TextBlock>(type, std::move(inlines), std::move(p));
    } else {
        std::vector<std::shared_ptr<Block>> blocks;
        int num_blocks = (gen() % 3) + 1;
        for (int i = 0; i < num_blocks; ++i) {
            // Keep it simple, just TextBlocks inside to avoid deep recursion
            std::vector<std::shared_ptr<Inline>> inlines;
            inlines.push_back(generateRandomInline(gen));
            blocks.push_back(std::make_shared<TextBlock>(Block::Type::Paragraph, std::move(inlines), generateRandomProvenance(gen)));
        }
        return std::make_shared<ContainerBlock>(type, std::move(blocks), std::move(p));
    }
}

static std::shared_ptr<Section> generateRandomSection(std::mt19937& gen, int depth, int max_depth) {
    auto p = generateRandomProvenance(gen);
    std::string title = "Section " + std::to_string(gen() % 100);
    
    std::vector<std::shared_ptr<Block>> blocks;
    int num_blocks = (gen() % 5) + 1;
    for (int i = 0; i < num_blocks; ++i) {
        blocks.push_back(generateRandomBlock(gen));
    }
    
    std::vector<std::shared_ptr<Section>> subsections;
    if (depth < max_depth) {
        int num_subsections = gen() % 3;
        for (int i = 0; i < num_subsections; ++i) {
            subsections.push_back(generateRandomSection(gen, depth + 1, max_depth));
        }
    }
    
    return std::make_shared<Section>(std::move(title), std::move(blocks), std::move(subsections), std::move(p));
}

std::shared_ptr<SemanticDocument> DocumentFuzzer::generateRandomDocument(int seed, int max_depth) {
    std::mt19937 gen(seed);
    auto p = generateRandomProvenance(gen);
    
    std::vector<std::shared_ptr<Section>> sections;
    int num_sections = (gen() % 4) + 1;
    for (int i = 0; i < num_sections; ++i) {
        sections.push_back(generateRandomSection(gen, 1, max_depth));
    }
    
    return std::make_shared<SemanticDocument>(std::move(sections), std::move(p));
}

} // namespace docmodel
