#include "waveform/renderers/waveformrendermark.h"

#include <QDomNode>
#include <QPainter>
#include <QPainterPath>

#include "control/controlproxy.h"
#include "engine/controls/cuecontrol.h"
#include "moc_waveformrendermark.cpp"
#include "track/track.h"
#include "util/color/color.h"
#include "util/painterscope.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "waveform/waveform.h"
#include "widget/wimagestore.h"
#include "widget/wskincolor.h"
#include "widget/wwidget.h"

namespace {
    const int kMaxCueLabelLength = 23;
    } // namespace

WaveformRenderMark::WaveformRenderMark(
        WaveformWidgetRenderer* waveformWidgetRenderer) :
    WaveformRendererAbstract(waveformWidgetRenderer) {
}

void WaveformRenderMark::setup(const QDomNode& node, const SkinContext& context) {
    WaveformSignalColors signalColors = *m_waveformRenderer->getWaveformSignalColors();
    m_marks.setup(m_waveformRenderer->getGroup(), node, context, signalColors);
}

void WaveformRenderMark::draw(QPainter* painter, QPaintEvent* /*event*/) {
    PainterScope PainterScope(painter);
    // Maps mark objects to their positions in the widget.
    QMap<WaveformMarkPointer, int> marksOnScreen;
    /*
    //DEBUG
    for (int i = 0; i < m_markPoints.size(); i++) {
        if (m_waveformWidget->getTrackSamples())
            painter->drawText(40*i,12+12*(i%3),QString::number(m_markPoints[i]->get() / (double)m_waveformWidget->getTrackSamples()));
    }
    */

    painter->setWorldMatrixEnabled(false);

    for (const auto& pMark : m_marks) {
        if (!pMark->isValid()) {
            continue;
        }

        if (pMark->hasVisible() && !pMark->isVisible()) {
            continue;
        }

        // Generate image on first paint can't be done in setup since we need to
        // wait for the render widget to be resized yet.
        if (pMark->m_image.isNull()) {
            generateMarkImage(pMark);
        }

        double samplePosition = pMark->getSamplePosition();
        if (samplePosition != -1.0) {
            double currentMarkPoint =
                    m_waveformRenderer->transformSamplePositionInRendererWorld(samplePosition);
            if (m_waveformRenderer->getOrientation() == Qt::Horizontal) {
                // Pixmaps are expected to have the mark stroke at the center,
                // and preferrably have an odd width in order to have the stroke
                // exactly at the sample position.
                const int markHalfWidth =
                        static_cast<int>(pMark->m_image.width() / 2.0 /
                                m_waveformRenderer->getDevicePixelRatio());

                // Check if the current point needs to be displayed.
                if (currentMarkPoint > -markHalfWidth && currentMarkPoint < m_waveformRenderer->getWidth() + markHalfWidth) {
                    const int drawOffset = static_cast<int>(currentMarkPoint) - markHalfWidth;
                    painter->drawImage(drawOffset, 0, pMark->m_image);
                    marksOnScreen[pMark] = drawOffset;
                }
            } else {
                const int markHalfHeight = static_cast<int>(pMark->m_image.height() / 2.0);
                if (currentMarkPoint > -markHalfHeight &&
                        currentMarkPoint < m_waveformRenderer->getHeight() +
                                        markHalfHeight) {
                    const int drawOffset = static_cast<int>(currentMarkPoint) - markHalfHeight;
                    painter->drawImage(0, drawOffset, pMark->m_image);
                    marksOnScreen[pMark] = drawOffset;
                }
            }
        }
    }
    m_waveformRenderer->setMarkPositions(marksOnScreen);
}

void WaveformRenderMark::onResize() {
    // Delete all marks' images. New images will be created on next paint.
    for (const auto& pMark : m_marks) {
        pMark->m_image = QImage();
    }
}

void WaveformRenderMark::onSetTrack() {
    slotCuesUpdated();

    TrackPointer trackInfo = m_waveformRenderer->getTrackInfo();
    if (!trackInfo) {
        return;
    }
    connect(trackInfo.get(),
            &Track::cuesUpdated,
            this,
            &WaveformRenderMark::slotCuesUpdated);
}

void WaveformRenderMark::slotCuesUpdated() {
    TrackPointer trackInfo = m_waveformRenderer->getTrackInfo();
    if (!trackInfo) {
        return;
    }

    QList<CuePointer> loadedCues = trackInfo->getCuePoints();
    for (const CuePointer& pCue : loadedCues) {
        int hotCue = pCue->getHotCue();
        if (hotCue == Cue::kNoHotCue) {
            continue;
        }

        // Here we assume no two cues can have the same hotcue assigned,
        // because WaveformMarkSet stores one mark for each hotcue.
        WaveformMarkPointer pMark = m_marks.getHotCueMark(hotCue);
        if (pMark.isNull()) {
            continue;
        }

        QString newLabel = pCue->getLabel();
        QColor newColor = mixxx::RgbColor::toQColor(pCue->getColor());
        if (pMark->m_text.isNull() || newLabel != pMark->m_text ||
                !pMark->fillColor().isValid() ||
                newColor != pMark->fillColor()) {
            pMark->m_text = newLabel;
            int dimBrightThreshold = m_waveformRenderer->getDimBrightThreshold();
            pMark->setBaseColor(newColor, dimBrightThreshold);
            generateMarkImage(pMark);
        }
    }
}

void WaveformRenderMark::generateMarkImage(WaveformMarkPointer pMark) {
    // Load the pixmap from file.
    // If that succeeds loading the text and stroke is skipped.
    float devicePixelRatio = m_waveformRenderer->getDevicePixelRatio();
    if (!pMark->m_pixmapPath.isEmpty()) {
        QString path = pMark->m_pixmapPath;
        // Use devicePixelRatio to properly scale the image
        QImage image = *WImageStore::getImage(path, devicePixelRatio);
        //QImage image = QImage(path);
        // If loading the image didn't fail, then we're done. Otherwise fall
        // through and render a label.
        if (!image.isNull()) {
            pMark->m_image =
                    image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            //WImageStore::correctImageColors(&pMark->m_image);
            // Set the pixel/device ratio AFTER loading the image in order to get
            // a truely scaled source image.
            // See https://doc.qt.io/qt-5/qimage.html#setDevicePixelRatio
            // Also, without this some Qt-internal issue results in an offset
            // image when calculating the center line of pixmaps in draw().
            pMark->m_image.setDevicePixelRatio(devicePixelRatio);
            return;
        }
    }

    QPainter painter;

    // Determine mark text.
    QString label = pMark->m_text;
    if (pMark->getHotCue() >= 0) {
        if (!label.isEmpty()) {
            label.prepend(": ");
        }
        label.prepend(QString::number(pMark->getHotCue() + 1));
        if (label.size() > kMaxCueLabelLength) {
            label = label.left(kMaxCueLabelLength - 3) + "...";
        }
    }

    // This alone would pick the OS default font, or that set by Qt5 Settings (qt5ct)
    // respectively. This would mostly not be notable since contemporary OS and distros
    // use a proven sans-serif anyway. Though, some user fonts may be lacking glyphs
    // we use for the intro/outro markers for example.
    QFont font;
    // So, let's just use Open Sans which is used by all official skins to achieve
    // a consistent skin design.
    font.setFamily("Open Sans");
    // Use a pixel size like everywhere else in Mixxx, which can be scaled well
    // in general.
    // Point sizes would work if only explicit Qt scaling QT_SCALE_FACTORS is used,
    // though as soon as other OS-based font and app scaling mechanics join the
    // party the resulting font size is hard to predict (affects all supported OS).
    font.setPixelSize(13);
    font.setWeight(75); // bold
    font.setItalic(false);

    QFontMetrics metrics(font);

    //fixed margin ...
    QRect wordRect = metrics.tightBoundingRect(label);
    const int marginX = 1;
    const int marginY = 1;
    wordRect.moveTop(marginX + 1);
    wordRect.moveLeft(marginY + 1);
    wordRect.setHeight(wordRect.height() + (wordRect.height() % 2));
    wordRect.setWidth(wordRect.width() + (wordRect.width()) % 2);
    //even wordrect to have an even Image >> draw the line in the middle !

    int labelRectWidth = wordRect.width() + 2 * marginX + 4;
    int labelRectHeight = wordRect.height() + 2 * marginY + 4;

    QRectF labelRect(0, 0, (float)labelRectWidth, (float)labelRectHeight);

    int width;
    int height;

    if (m_waveformRenderer->getOrientation() == Qt::Horizontal) {
        width = 2 * labelRectWidth + 1;
        height = m_waveformRenderer->getHeight();
    } else {
        width = m_waveformRenderer->getWidth();
        height = 2 * labelRectHeight + 1;
    }

    pMark->m_image = QImage(
            static_cast<int>(width * devicePixelRatio),
            static_cast<int>(height * devicePixelRatio),
            QImage::Format_ARGB32_Premultiplied);
    pMark->m_image.setDevicePixelRatio(devicePixelRatio);

    Qt::Alignment markAlignH = pMark->m_align & Qt::AlignHorizontal_Mask;
    Qt::Alignment markAlignV = pMark->m_align & Qt::AlignVertical_Mask;

    if (markAlignH == Qt::AlignHCenter) {
        labelRect.moveLeft((width - labelRectWidth) / 2);
    } else if (markAlignH == Qt::AlignRight) {
        labelRect.moveRight(width - 1);
    }

    if (markAlignV == Qt::AlignVCenter) {
        labelRect.moveTop((height - labelRectHeight) / 2);
    } else if (markAlignV == Qt::AlignBottom) {
        labelRect.moveBottom(height - 1);
    }

    pMark->m_label.setAreaRect(labelRect);

    // Fill with transparent pixels
    pMark->m_image.fill(QColor(0, 0, 0, 0).rgba());

    painter.begin(&pMark->m_image);
    painter.setRenderHint(QPainter::TextAntialiasing);

    painter.setWorldMatrixEnabled(false);

    // Draw marker lines
    if (m_waveformRenderer->getOrientation() == Qt::Horizontal) {
        int middle = width / 2;
        pMark->m_linePosition = middle;
        if (markAlignH == Qt::AlignHCenter) {
            if (labelRect.top() > 0) {
                painter.setPen(pMark->fillColor());
                painter.drawLine(QLineF(middle, 0, middle, labelRect.top()));

                painter.setPen(pMark->borderColor());
                painter.drawLine(QLineF(middle - 1, 0, middle - 1, labelRect.top()));
                painter.drawLine(QLineF(middle + 1, 0, middle + 1, labelRect.top()));
            }

            if (labelRect.bottom() < height) {
                painter.setPen(pMark->fillColor());
                painter.drawLine(QLineF(middle, labelRect.bottom(), middle, height));

                painter.setPen(pMark->borderColor());
                painter.drawLine(QLineF(middle - 1, labelRect.bottom(), middle - 1, height));
                painter.drawLine(QLineF(middle + 1, labelRect.bottom(), middle + 1, height));
            }
        } else { // AlignLeft || AlignRight
            painter.setPen(pMark->fillColor());
            painter.drawLine(middle, 0, middle, height);

            painter.setPen(pMark->borderColor());
            painter.drawLine(middle - 1, 0, middle - 1, height);
            painter.drawLine(middle + 1, 0, middle + 1, height);
        }
    } else { // Vertical
        int middle = height / 2;
        pMark->m_linePosition = middle;
        if (markAlignV == Qt::AlignVCenter) {
            if (labelRect.left() > 0) {
                painter.setPen(pMark->fillColor());
                painter.drawLine(QLineF(0, middle, labelRect.left(), middle));

                painter.setPen(pMark->borderColor());
                painter.drawLine(QLineF(0, middle - 1, labelRect.left(), middle - 1));
                painter.drawLine(QLineF(0, middle + 1, labelRect.left(), middle + 1));
            }

            if (labelRect.right() < width) {
                painter.setPen(pMark->fillColor());
                painter.drawLine(QLineF(labelRect.right(), middle, width, middle));

                painter.setPen(pMark->borderColor());
                painter.drawLine(QLineF(labelRect.right(), middle - 1, width, middle - 1));
                painter.drawLine(QLineF(labelRect.right(), middle + 1, width, middle + 1));
            }
        } else { // AlignTop || AlignBottom
            painter.setPen(pMark->fillColor());
            painter.drawLine(0, middle, width, middle);

            painter.setPen(pMark->borderColor());
            painter.drawLine(0, middle - 1, width, middle - 1);
            painter.drawLine(0, middle + 1, width, middle + 1);
        }
    }

    // Draw the label rect
    painter.setPen(pMark->borderColor());
    painter.setBrush(QBrush(pMark->fillColor()));
    painter.drawRoundedRect(labelRect, 2.0, 2.0);

    // Draw text
    painter.setBrush(QBrush(QColor(0, 0, 0, 0)));
    painter.setFont(font);
    painter.setPen(pMark->labelColor());
    painter.drawText(labelRect, Qt::AlignCenter, label);
}
