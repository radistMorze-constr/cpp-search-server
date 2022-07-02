#include "document.h"

Document::Document() = default;

Document::Document(int id, double relevance, int rating)
    : id(id)
    , relevance(relevance)
    , rating(rating) 
{
}

std::ostream& operator<<(std::ostream& os, const Document& document) {
    os << "{ document_id = " << document.id << ", relevance = " << document.relevance;
    os << ", rating = " << document.rating << " }";
    return os;
}