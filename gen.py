import os

src_dir = 'src/docmodel'
os.makedirs(src_dir, exist_ok=True)

inline_h = '''#pragma once
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
'''

inline_cpp = '''#include "Inline.h"

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
'''

block_h = '''#pragma once
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
'''

block_cpp = '''#include "Block.h"

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
'''

semantic_doc_h = '''#pragma once
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
'''

semantic_doc_cpp = '''#include "SemanticDocument.h"

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
'''

document_fuzzer_h = '''#pragma once
#include "SemanticDocument.h"
#include <memory>

namespace docmodel {

class DocumentFuzzer {
public:
    static std::shared_ptr<SemanticDocument> generateRandomDocument(int seed, int max_depth = 3);
};

} // namespace docmodel
'''

document_fuzzer_cpp = '''#include "DocumentFuzzer.h"
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
'''

cmakelists_txt = '''cmake_minimum_required(VERSION 3.20)
project(docmodel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(docmodel STATIC
    ProvenanceTag.h
    Inline.h
    Inline.cpp
    Block.h
    Block.cpp
    SemanticDocument.h
    SemanticDocument.cpp
    DocumentFuzzer.h
    DocumentFuzzer.cpp
)

target_include_directories(docmodel PUBLIC )
'''

with open('src/docmodel/Inline.h', 'w') as f: f.write(inline_h)
with open('src/docmodel/Inline.cpp', 'w') as f: f.write(inline_cpp)
with open('src/docmodel/Block.h', 'w') as f: f.write(block_h)
with open('src/docmodel/Block.cpp', 'w') as f: f.write(block_cpp)
with open('src/docmodel/SemanticDocument.h', 'w') as f: f.write(semantic_doc_h)
with open('src/docmodel/SemanticDocument.cpp', 'w') as f: f.write(semantic_doc_cpp)
with open('src/docmodel/DocumentFuzzer.h', 'w') as f: f.write(document_fuzzer_h)
with open('src/docmodel/DocumentFuzzer.cpp', 'w') as f: f.write(document_fuzzer_cpp)
with open('src/docmodel/CMakeLists.txt', 'w') as f: f.write(cmakelists_txt)
