#pragma once

#include "shared-types/Adjustment.hpp"
#include "shared-types/Layer.hpp"
#include "shared-types/Mask.hpp"

#include <QImage>
#include <QObject>
#include <QString>
#include <QVector>

namespace lumen {

class DocumentModel final : public QObject {
    Q_OBJECT

public:
    explicit DocumentModel(QObject* parent = nullptr);

    bool openSourceImage(const QString& path);
    void clear();

    [[nodiscard]] bool hasDocument() const;
    [[nodiscard]] QString sourcePath() const;
    [[nodiscard]] QSize sourceSize() const;
    [[nodiscard]] const QImage& sourceImage() const;
    [[nodiscard]] QVector<Adjustment> adjustments() const;
    [[nodiscard]] QVector<Layer> layers() const;
    [[nodiscard]] QVector<Mask> masks() const;

    void setScalarAdjustment(AdjustmentType type, double value);
    [[nodiscard]] double scalarAdjustment(AdjustmentType type) const;

signals:
    void changed();

private:
    Adjustment* findAdjustment(AdjustmentType type);
    const Adjustment* findAdjustment(AdjustmentType type) const;

    QString m_projectId;
    QString m_sourcePath;
    QImage m_sourceImage;
    QVector<Layer> m_layers;
    QVector<Mask> m_masks;
    QVector<Adjustment> m_adjustments;
};

} // namespace lumen
