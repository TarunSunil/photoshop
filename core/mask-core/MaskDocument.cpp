#include "mask-core/MaskDocument.hpp"

namespace lumen {

void MaskDocument::addMask(const Mask& mask)
{
    m_masks.push_back(mask);
}

QVector<Mask> MaskDocument::masks() const
{
    return m_masks;
}

} // namespace lumen
