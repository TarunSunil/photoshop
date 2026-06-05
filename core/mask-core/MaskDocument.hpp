#pragma once

#include "shared-types/Mask.hpp"

#include <QVector>

namespace lumen {

class MaskDocument {
public:
    void addMask(const Mask& mask);
    [[nodiscard]] QVector<Mask> masks() const;

private:
    QVector<Mask> m_masks;
};

} // namespace lumen
