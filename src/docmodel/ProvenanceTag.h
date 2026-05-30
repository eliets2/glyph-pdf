#pragma once

#include <string>
#include <utility>

namespace docmodel {

enum class ProvenanceTag {
    BornPDF,
    BornDjot,
    BornOCR
};

struct BBox {
    double x0 = 0.0;
    double y0 = 0.0;
    double x1 = 0.0;
    double y1 = 0.0;
};

struct Provenance {
    ProvenanceTag tag = ProvenanceTag::BornPDF;
    std::string source_file;
    int page_index = -1;
    BBox bbox;
};

class Node {
public:
    explicit Node(Provenance p) : provenance(std::move(p)) {}
    virtual ~Node() = default;

    const Provenance& getProvenance() const { return provenance; }
    void setProvenance(const Provenance& p) { provenance = p; }

protected:
    Provenance provenance;
};

} // namespace docmodel
