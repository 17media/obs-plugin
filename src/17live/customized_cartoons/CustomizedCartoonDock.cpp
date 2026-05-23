#include "CustomizedCartoonDock.hpp"

#include <obs-module.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QStyleFactory>
#include <QStyle>
#include <QTabWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>
#include <unordered_map>

#include "CustomizedCartoonService.hpp"

using json = nlohmann::json;

namespace {

constexpr int kDockOuterMargin = 10;
constexpr int kPanelInnerMargin = 5;
constexpr int kPanelSpacing = 5;
constexpr int kPositionCanvasMinWidth = 210;
constexpr int kPositionSidePanelMinWidth = 220;
constexpr int kPositionPanelMinWidth =
    (kPanelInnerMargin * 2) + kPositionCanvasMinWidth + kPanelSpacing + kPositionSidePanelMinWidth;
constexpr int kDockMinWidth = kPositionPanelMinWidth + 20;
constexpr int kDockMinHeight = 300;
constexpr int kRuleGiftParamLabelWidth = 100;
constexpr int kRuleLuckyBagParamLabelWidth = 100;
constexpr int kRuleGiftAmountSpinWidth = 100;
constexpr int kRuleGiftCountSpinWidth = 60;
constexpr int kRuleLuckyBagCountSpinWidth = 100;
constexpr int kRuleStatusComboWidth = 88;
constexpr int kRuleTypeComboChars = 10;
constexpr int kRuleMediaComboChars = 12;
constexpr int kRuleStatusComboChars = 4;

class PositionCanvasWidget final : public QWidget {
   public:
    explicit PositionCanvasWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMouseTracking(true);
    }

    void setCanvasSize(int w, int h) {
        canvasW_ = std::max(1, w);
        canvasH_ = std::max(1, h);
        clampRect();
        update();
    }

    void setRect(const QRect& r) {
        rect_ = r;
        clampRect();
        update();
    }

    QRect selectionRect() const { return rect_; }

    void setOnRectChanged(std::function<void(const QRect&)> cb) { onRectChanged_ = std::move(cb); }

   protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        p.fillRect(QWidget::rect(), QColor(0x12, 0x15, 0x1B));

        const QRectF area = contentRect();
        drawGrid(p, area);
        drawRulers(p, area);
        drawSelection(p, area);
    }

    void mousePressEvent(QMouseEvent* e) override {
        const auto hit = hitTest(e->position());
        if (hit == Hit::None) {
            active_ = Hit::None;
            return;
        }
        active_ = hit;
        lastPos_ = e->position();
        startRect_ = rect_;
        e->accept();
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        if (active_ == Hit::None) {
            setCursor(cursorForHit(hitTest(e->position())));
            return;
        }

        const QRectF area = contentRect();
        const double s = scale(area);
        if (s <= 0.0) {
            return;
        }
        const QPointF deltaPx = e->position() - lastPos_;
        const QPoint deltaCanvas((int)std::round(deltaPx.x() / s), (int)std::round(deltaPx.y() / s));

        QRect next = startRect_;
        const int minSize = 20;
        switch (active_) {
            case Hit::Move:
                next.translate(deltaCanvas);
                break;
            case Hit::TL:
                next.setTopLeft(next.topLeft() + deltaCanvas);
                break;
            case Hit::TR:
                next.setTopRight(next.topRight() + deltaCanvas);
                break;
            case Hit::BL:
                next.setBottomLeft(next.bottomLeft() + deltaCanvas);
                break;
            case Hit::BR:
                next.setBottomRight(next.bottomRight() + deltaCanvas);
                break;
            case Hit::L:
                next.setLeft(next.left() + deltaCanvas.x());
                break;
            case Hit::R:
                next.setRight(next.right() + deltaCanvas.x());
                break;
            case Hit::T:
                next.setTop(next.top() + deltaCanvas.y());
                break;
            case Hit::B:
                next.setBottom(next.bottom() + deltaCanvas.y());
                break;
            default:
                break;
        }

        if (next.width() < minSize) {
            next.setWidth(minSize);
        }
        if (next.height() < minSize) {
            next.setHeight(minSize);
        }

        rect_ = next.normalized();
        clampRect();
        update();
        if (onRectChanged_) {
            onRectChanged_(rect_);
        }
        e->accept();
    }

    void mouseReleaseEvent(QMouseEvent*) override {
        active_ = Hit::None;
        setCursor(Qt::ArrowCursor);
    }

   private:
    enum class Hit {
        None,
        Move,
        TL,
        TR,
        BL,
        BR,
        L,
        R,
        T,
        B,
    };

    QRectF contentRect() const {
        QRectF area;
        if (canvasH_ > canvasW_) {
            // Portrait preview shifts slightly right/down while keeping bottom space for the ruler label.
            area = QWidget::rect().adjusted(44, 22, -30, -17);
        } else {
            // Landscape preview moves upward and leaves room for the bottom ruler/label.
            area = QWidget::rect().adjusted(56, 2, -56, -30);
        }
        if (area.width() < 10.0 || area.height() < 10.0) {
            area = QRectF(0, 0, width(), height());
        }
        return area;
    }

    double scale(const QRectF& area) const {
        const double sx = area.width() / (double)canvasW_;
        const double sy = area.height() / (double)canvasH_;
        return std::min(sx, sy);
    }

    QPointF canvasToWidget(const QPointF& ptCanvas, const QRectF& area) const {
        const double s = scale(area);
        const double w = canvasW_ * s;
        const double h = canvasH_ * s;
        const double ox = area.x() + (area.width() - w) / 2.0;
        const double oy = area.y() + (area.height() - h) / 2.0;
        return QPointF(ox + ptCanvas.x() * s, oy + ptCanvas.y() * s);
    }

    QRectF canvasToWidget(const QRect& rCanvas, const QRectF& area) const {
        const QPointF tl = canvasToWidget(QPointF(rCanvas.x(), rCanvas.y()), area);
        const QPointF br =
            canvasToWidget(QPointF(rCanvas.x() + rCanvas.width(), rCanvas.y() + rCanvas.height()),
                           area);
        return QRectF(tl, br).normalized();
    }

    QRectF canvasRectInWidget(const QRectF& area) const {
        const double s = scale(area);
        const double w = canvasW_ * s;
        const double h = canvasH_ * s;
        const double ox = area.x() + (area.width() - w) / 2.0;
        const double oy = area.y() + (area.height() - h) / 2.0;
        return QRectF(ox, oy, w, h);
    }

    Hit hitTest(const QPointF& pt) const {
        const QRectF area = contentRect();
        const QRectF r = canvasToWidget(rect_, area);
        const double h = 8.0;

        const QRectF tl(r.topLeft() - QPointF(h, h), QSizeF(h * 2, h * 2));
        const QRectF tr(QPointF(r.right(), r.top()) - QPointF(h, h), QSizeF(h * 2, h * 2));
        const QRectF bl(QPointF(r.left(), r.bottom()) - QPointF(h, h), QSizeF(h * 2, h * 2));
        const QRectF br(QPointF(r.right(), r.bottom()) - QPointF(h, h), QSizeF(h * 2, h * 2));

        if (tl.contains(pt)) return Hit::TL;
        if (tr.contains(pt)) return Hit::TR;
        if (bl.contains(pt)) return Hit::BL;
        if (br.contains(pt)) return Hit::BR;

        const QRectF l(QPointF(r.left() - h, r.center().y() - h), QSizeF(h * 2, h * 2));
        const QRectF rr(QPointF(r.right() - h, r.center().y() - h), QSizeF(h * 2, h * 2));
        const QRectF t(QPointF(r.center().x() - h, r.top() - h), QSizeF(h * 2, h * 2));
        const QRectF b(QPointF(r.center().x() - h, r.bottom() - h), QSizeF(h * 2, h * 2));
        if (l.contains(pt)) return Hit::L;
        if (rr.contains(pt)) return Hit::R;
        if (t.contains(pt)) return Hit::T;
        if (b.contains(pt)) return Hit::B;

        if (r.contains(pt)) return Hit::Move;
        return Hit::None;
    }

    QCursor cursorForHit(Hit h) const {
        switch (h) {
            case Hit::Move:
                return Qt::SizeAllCursor;
            case Hit::TL:
            case Hit::BR:
                return Qt::SizeFDiagCursor;
            case Hit::TR:
            case Hit::BL:
                return Qt::SizeBDiagCursor;
            case Hit::L:
            case Hit::R:
                return Qt::SizeHorCursor;
            case Hit::T:
            case Hit::B:
                return Qt::SizeVerCursor;
            default:
                return Qt::ArrowCursor;
        }
    }

    void drawGrid(QPainter& p, const QRectF& area) {
        p.setPen(QPen(QColor(0x2A, 0x2F, 0x38), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRect(canvasRectInWidget(area));

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x3A, 0x40, 0x4B));
        const int step = 60;
        for (int y = 0; y <= canvasH_; y += step) {
            for (int x = 0; x <= canvasW_; x += step) {
                const QPointF pt = canvasToWidget(QPointF(x, y), area);
                p.drawRoundedRect(QRectF(pt.x() - 2.0, pt.y() - 2.0, 4.0, 4.0), 1.0, 1.0);
            }
        }

    }

    void drawRulers(QPainter& p, const QRectF& area) {
        const QRectF canvasRect = canvasRectInWidget(area);
        const QColor blue(0x00, 0x7A, 0xFF);
        const QColor textColor(0xFF, 0xFF, 0xFF, 200);

        const double leftX = canvasRect.left() - 14.0;
        const double topY = canvasRect.top();
        const double botY = canvasRect.bottom();
        const double bottomY = canvasRect.bottom() + 10.0;
        const double left2 = canvasRect.left();
        const double right2 = canvasRect.right();

        p.setPen(QPen(blue, 2.0));

        p.drawLine(QPointF(leftX, topY + 6.0), QPointF(leftX, botY - 6.0));
        p.drawLine(QPointF(left2 + 6.0, bottomY), QPointF(right2 - 6.0, bottomY));

        auto drawArrow = [&p, &blue](const QPointF& tip, const QPointF& dir) {
            const double size = 6.0;
            const QPointF n(-dir.y(), dir.x());
            const QPointF p1 = tip + (dir * size) + (n * size * 0.6);
            const QPointF p2 = tip + (dir * size) - (n * size * 0.6);
            QPolygonF tri;
            tri << tip << p1 << p2;
            p.setPen(Qt::NoPen);
            p.setBrush(blue);
            p.drawPolygon(tri);
            p.setPen(QPen(blue, 2.0));
            p.setBrush(Qt::NoBrush);
        };

        drawArrow(QPointF(leftX, topY), QPointF(0.0, 1.0));
        drawArrow(QPointF(leftX, botY), QPointF(0.0, -1.0));
        drawArrow(QPointF(left2, bottomY), QPointF(1.0, 0.0));
        drawArrow(QPointF(right2, bottomY), QPointF(-1.0, 0.0));

        p.setPen(textColor);
        p.setFont(QFont("Inter", 12, QFont::Light));

        const QString vText = QString("%1 px").arg(canvasH_);
        const QString hText = QString("%1 px").arg(canvasW_);

        p.save();
        p.translate(leftX - 8.0, (topY + botY) / 2.0);
        p.rotate(-90.0);
        p.drawText(QRectF(-100, -10, 200, 20), Qt::AlignCenter, vText);
        p.restore();

        p.drawText(QRectF(left2, bottomY + 1.0, canvasRect.width(), 20.0), Qt::AlignCenter, hText);
    }

    void drawSelection(QPainter& p, const QRectF& area) {
        const QRectF r = canvasToWidget(rect_, area);
        p.setPen(QPen(QColor(0x00, 0x7A, 0xFF), 2.0));
        p.setBrush(QColor(0x2A, 0x6A, 0xFF, 77));
        p.drawRect(r);

        const double h = 4.0;
        const QColor handleFill(0x00, 0x7A, 0xFF);
        p.setPen(Qt::NoPen);
        p.setBrush(handleFill);

        const QPointF pts[] = {r.topLeft(),
                               QPointF(r.center().x(), r.top()),
                               r.topRight(),
                               QPointF(r.left(), r.center().y()),
                               QPointF(r.right(), r.center().y()),
                               r.bottomLeft(),
                               QPointF(r.center().x(), r.bottom()),
                               r.bottomRight()};
        for (const auto& pt : pts) {
            p.drawRoundedRect(QRectF(pt.x() - h, pt.y() - h, h * 2, h * 2), 2.0, 2.0);
        }

        p.setPen(QColor(0xFF, 0xFF, 0xFF));
        p.setFont(QFont("Inter", 14, QFont::Bold));
        p.drawText(r, Qt::AlignCenter,
                   QString(obs_module_text("CustomizedCartoon.Position.Canvas.Area")).arg(rect_.width()).arg(
                       rect_.height()));
    }

    void clampRect() {
        rect_ = rect_.normalized();
        const int minSize = 20;
        const int clampedWidth = std::clamp(rect_.width(), minSize, canvasW_);
        const int clampedHeight = std::clamp(rect_.height(), minSize, canvasH_);

        rect_.setWidth(clampedWidth);
        rect_.setHeight(clampedHeight);

        const int maxX = std::max(0, canvasW_ - rect_.width());
        const int maxY = std::max(0, canvasH_ - rect_.height());
        const int clampedX = std::clamp(rect_.x(), 0, maxX);
        const int clampedY = std::clamp(rect_.y(), 0, maxY);
        rect_.moveTo(clampedX, clampedY);
    }

    int canvasW_{720};
    int canvasH_{1280};
    QRect rect_{200, 300, 500, 500};
    QRect startRect_{rect_};
    QPointF lastPos_{};
    Hit active_{Hit::None};
    std::function<void(const QRect&)> onRectChanged_{};
};

}  // namespace

namespace {

struct PositionSizePanelWidgets {
    QFrame* panel{nullptr};
    QSpinBox* posX{nullptr};
    QSpinBox* posY{nullptr};
    QSpinBox* width{nullptr};
    QSpinBox* height{nullptr};
};

QRect normalizePositionRect(const QRect& rect, int canvasW, int canvasH) {
    const int minSize = 20;
    const int normalizedWidth = std::clamp(rect.width(), minSize, canvasW);
    const int normalizedHeight = std::clamp(rect.height(), minSize, canvasH);
    const int maxX = std::max(0, canvasW - normalizedWidth);
    const int maxY = std::max(0, canvasH - normalizedHeight);
    const int normalizedX = std::clamp(rect.x(), 0, maxX);
    const int normalizedY = std::clamp(rect.y(), 0, maxY);
    return QRect(normalizedX, normalizedY, normalizedWidth, normalizedHeight);
}

void applyPositionInputs(const PositionSizePanelWidgets& inputs, const QRect& rect, int canvasW, int canvasH) {
    if (!inputs.posX || !inputs.posY || !inputs.width || !inputs.height) {
        return;
    }

    const QRect normalized = normalizePositionRect(rect, canvasW, canvasH);
    const int maxX = std::max(0, canvasW - normalized.width());
    const int maxY = std::max(0, canvasH - normalized.height());

    for (auto* spin : {inputs.posX, inputs.posY, inputs.width, inputs.height}) {
        spin->blockSignals(true);
    }

    inputs.width->setRange(20, canvasW);
    inputs.height->setRange(20, canvasH);
    inputs.posX->setRange(0, maxX);
    inputs.posY->setRange(0, maxY);

    inputs.posX->setValue(normalized.x());
    inputs.posY->setValue(normalized.y());
    inputs.width->setValue(normalized.width());
    inputs.height->setValue(normalized.height());

    for (auto* spin : {inputs.posX, inputs.posY, inputs.width, inputs.height}) {
        spin->blockSignals(false);
    }
}

QRect applyPositionState(PositionCanvasWidget* canvas, const PositionSizePanelWidgets& inputs, const QRect& rect,
                         int canvasW, int canvasH) {
    const QRect normalized = normalizePositionRect(rect, canvasW, canvasH);
    applyPositionInputs(inputs, normalized, canvasW, canvasH);
    if (canvas) {
        canvas->setRect(normalized);
    }
    return normalized;
}

PositionSizePanelWidgets createPositionSizePanel(QWidget* parent) {
    PositionSizePanelWidgets out;
    out.panel = new QFrame(parent);
    out.panel->setObjectName("subPanel");
    out.panel->setMinimumWidth(kPositionSidePanelMinWidth);

    auto* sizeLayout = new QVBoxLayout(out.panel);
    sizeLayout->setContentsMargins(5, 5, 5, 5);
    sizeLayout->setSpacing(5);
    auto* sizeTitle =
        new QLabel(obs_module_text("CustomizedCartoon.Position.Size.Title"), out.panel);
    sizeTitle->setObjectName("sectionTitle");
    sizeTitle->setStyleSheet("QLabel#sectionTitle { font-size: 16px; font-weight: 600; color: #FFFFFF; }");
    sizeLayout->addWidget(sizeTitle);

    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(5);
    grid->setVerticalSpacing(5);

    auto makePx = [p = out.panel]() {
        auto* l = new QLabel("px", p);
        l->setObjectName("positionUnitLabel");
        l->setFixedHeight(21);
        l->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        return l;
    };

    out.posX = new QSpinBox(out.panel);
    out.posX->setRange(-100000, 100000);
    out.posX->setObjectName("posSpinBox");
    out.posY = new QSpinBox(out.panel);
    out.posY->setRange(-100000, 100000);
    out.posY->setObjectName("posSpinBox");
    out.width = new QSpinBox(out.panel);
    out.width->setRange(20, 100000);
    out.width->setObjectName("posSpinBox");
    out.height = new QSpinBox(out.panel);
    out.height->setRange(20, 100000);
    out.height->setObjectName("posSpinBox");

    for (auto* sb : {out.posX, out.posY, out.width, out.height}) {
        sb->setFixedHeight(32);
        sb->setMinimumWidth(110);
        sb->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        sb->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    }

    auto* xLabel = new QLabel(obs_module_text("CustomizedCartoon.Position.Field.X"), out.panel);
    auto* yLabel = new QLabel(obs_module_text("CustomizedCartoon.Position.Field.Y"), out.panel);
    auto* wLabel = new QLabel(obs_module_text("CustomizedCartoon.Position.Field.Width"), out.panel);
    auto* hLabel = new QLabel(obs_module_text("CustomizedCartoon.Position.Field.Height"), out.panel);
    for (auto* label : {xLabel, yLabel, wLabel, hLabel}) {
        label->setObjectName("positionFieldLabel");
        label->setFixedHeight(21);
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }

    grid->addWidget(xLabel, 0, 0);
    grid->addWidget(out.posX, 0, 1);
    grid->addWidget(makePx(), 0, 2);

    grid->addWidget(yLabel, 1, 0);
    grid->addWidget(out.posY, 1, 1);
    grid->addWidget(makePx(), 1, 2);

    grid->addWidget(wLabel, 2, 0);
    grid->addWidget(out.width, 2, 1);
    grid->addWidget(makePx(), 2, 2);

    grid->addWidget(hLabel, 3, 0);
    grid->addWidget(out.height, 3, 1);
    grid->addWidget(makePx(), 3, 2);

    sizeLayout->addLayout(grid);
    return out;
}

QFrame* createPositionTipsPanel(QWidget* parent) {
    auto* tipsPanel = new QFrame(parent);
    tipsPanel->setObjectName("subPanel");
    tipsPanel->setMinimumWidth(kPositionSidePanelMinWidth);
    auto* tipsLayout = new QVBoxLayout(tipsPanel);
    tipsLayout->setContentsMargins(5, 5, 5, 5);
    tipsLayout->setSpacing(5);
    auto* tipsLabel = new QLabel(tipsPanel);
    tipsLabel->setText(QString("• %1<br/>• %2<br/>• %3<br/>• %4")
                           .arg(obs_module_text("CustomizedCartoon.Position.Tips.1"))
                           .arg(obs_module_text("CustomizedCartoon.Position.Tips.2"))
                           .arg(obs_module_text("CustomizedCartoon.Position.Tips.3"))
                           .arg(obs_module_text("CustomizedCartoon.Position.Tips.4")));
    tipsLabel->setTextFormat(Qt::RichText);
    tipsLabel->setWordWrap(true);
    tipsLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: 400; color: #FFFFFF; }");
    tipsLayout->addWidget(tipsLabel);
    return tipsPanel;
}

bool editMediaSettingsDialog(QWidget* parent, const QString& mediaName, bool isVideo, int currentDisplaySec,
                             bool currentMuted, bool currentPreserveAspectRatio, int& outDisplaySec,
                             bool& outMuted, bool& outPreserveAspectRatio) {
    QDialog dialog(parent);
    dialog.setWindowTitle(obs_module_text("CustomizedCartoon.Media.Settings.Title"));
    dialog.setModal(true);
    dialog.setMinimumWidth(420);
    dialog.setStyleSheet(
        "QDialog { background-color: #272A33; color: #FFFFFF; }"
        "QLabel { color: #FFFFFF; font-size: 14px; }"
        "QSpinBox { background-color: rgba(0,0,0,0.25); border: 1px solid rgba(255,255,255,0.12); "
        "border-radius: 2px; color: #FFFFFF; font-size: 14px; padding: 4px 8px; }"
        "QCheckBox { color: #FFFFFF; font-size: 14px; spacing: 6px; }"
        "QPushButton { min-width: 88px; min-height: 32px; padding: 0px 12px; }");

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* nameLabel = new QLabel(mediaName, &dialog);
    nameLabel->setWordWrap(true);
    nameLabel->setStyleSheet("QLabel { color: #FFFFFF; font-size: 16px; font-weight: 600; }");
    layout->addWidget(nameLabel);

    auto* formLayout = new QFormLayout();
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setHorizontalSpacing(12);
    formLayout->setVerticalSpacing(12);
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto* durationSpin = new QSpinBox(&dialog);
    durationSpin->setRange(1, 600);
    durationSpin->setValue(std::max(1, currentDisplaySec));
    durationSpin->setSuffix(" s");
    formLayout->addRow(obs_module_text("CustomizedCartoon.Media.Settings.Duration"), durationSpin);

    QCheckBox* muteCheck = nullptr;
    if (isVideo) {
        muteCheck = new QCheckBox(&dialog);
        muteCheck->setChecked(currentMuted);
        formLayout->addRow(obs_module_text("CustomizedCartoon.Media.Settings.Mute"), muteCheck);
    }

    auto* preserveAspectCheck = new QCheckBox(&dialog);
    preserveAspectCheck->setChecked(currentPreserveAspectRatio);
    formLayout->addRow(obs_module_text("CustomizedCartoon.Media.Settings.PreserveAspectRatio"),
                       preserveAspectCheck);

    layout->addLayout(formLayout);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    if (auto* okButton = buttonBox->button(QDialogButtonBox::Ok)) {
        okButton->setText(obs_module_text("CustomizedCartoon.Action.Confirm"));
        okButton->setStyleSheet(
            "QPushButton { background-color: #FF0001; color: #FFFFFF; border: none; border-radius: 2px; "
            "font-size: 14px; font-weight: 700; }"
            "QPushButton:hover { background-color: #FF3B30; }");
    }
    if (auto* cancelButton = buttonBox->button(QDialogButtonBox::Cancel)) {
        cancelButton->setText(obs_module_text("CustomizedCartoon.Action.Cancel"));
        cancelButton->setStyleSheet(
            "QPushButton { background-color: #3C404D; color: #FFFFFF; border: 1px solid #757575; "
            "border-radius: 2px; font-size: 14px; font-weight: 700; }"
            "QPushButton:hover { background-color: #4A4F5E; }");
    }
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    outDisplaySec = durationSpin->value();
    outMuted = muteCheck ? muteCheck->isChecked() : false;
    outPreserveAspectRatio = preserveAspectCheck->isChecked();
    return true;
}

}  // namespace

CustomizedCartoonDock::CustomizedCartoonDock(QWidget* parent, CustomizedCartoonService* service)
    : QDockWidget(parent), service_(service) {
    setWindowTitle(obs_module_text("CustomizedCartoon.Dock.Title"));
    setAllowedAreas(Qt::AllDockWidgetAreas);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable |
                QDockWidget::DockWidgetClosable);
    setupUi();
    if (service_) {
        connect(service_, &CustomizedCartoonService::configChanged, this,
                &CustomizedCartoonDock::refreshUi);
        connect(service_, &CustomizedCartoonService::progressUpdated, this,
                &CustomizedCartoonDock::refreshProgress);
        connect(service_, &CustomizedCartoonService::previewStateChanged, this,
                &CustomizedCartoonDock::refreshMediaList);
    }
    refreshUi();
}

void CustomizedCartoonDock::setupUi() {
    resize(500, 820);
    setMinimumSize(kDockMinWidth, kDockMinHeight);

    auto* root = new QWidget(this);
    root->setObjectName("customizedCartoonRoot");
    root->setMinimumWidth(kDockMinWidth);
    root->setMinimumHeight(kDockMinHeight);
    root->setStyleSheet(
        "QWidget#customizedCartoonRoot {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0E1116, stop:1 #0B0D11);"
        "  color: white;"
        "  font-family: 'Inter';"
        "}"
        "QToolTip {"
        "  background-color: #333333;"
        "  color: #FFFFFF;"
        "  font-weight: 400;"
        "  font-size: 12px;"
        "  padding: 5px;"
        "  border: none;"
        "  border-radius: 2px;"
        "}"
        "QFrame#panel {"
        "  background-color: #12141A;"
        "  border: 0.5px solid #3D3D3D;"
        "  border-radius: 2px;"
        "}"
        "QFrame#mediaPanel {"
        "  background-color: #12141A;"
        "  border: 0.5px solid #3D3D3D;"
        "  border-radius: 2px;"
        "}"
        "QFrame#mediaTitleBar {"
        "  background-color: transparent;"
        "  border: none;"
        "  border-bottom: 0.5px solid #3D3D3D;"
        "}"
        "QFrame#subPanel {"
        "  background-color: rgba(255,255,255,0.06);"
        "  border-radius: 2px;"
        "}"
        "QLabel#mediaTitle {"
        "  color: #FFFFFF;"
        "  font-size: 16px;"
        "  font-weight: 600;"
        "  line-height: 20px;"
        "  padding: 0px;"
        "}"
        "QLabel#sectionTitle { font-size: 18px; font-weight: 700; }"
        "QLabel#rulesPanelTitle {"
        "  color: #FFFFFF;"
        "  font-size: 16px;"
        "  font-weight: 600;"
        "  line-height: 20px;"
        "  min-height: 24px;"
        "  max-height: 24px;"
        "}"
        "QLabel#ruleCardTitle {"
        "  color: #FFFFFF;"
        "  font-size: 14px;"
        "  font-weight: 500;"
        "  line-height: 20px;"
        "  min-height: 24px;"
        "  max-height: 24px;"
        "}"
        "QLabel#positionPanelTitle {"
        "  color: #FFFFFF;"
        "  font-size: 16px;"
        "  font-weight: 600;"
        "  padding: 0px;"
        "}"
        "QLabel#positionCommentLabel {"
        "  color: #FFFFFF;"
        "  font-size: 14px;"
        "  font-weight: 400;"
        "  padding: 0px;"
        "}"
        "QLabel#positionFieldLabel, QLabel#positionUnitLabel {"
        "  color: #FFFFFF;"
        "  font-size: 14px;"
        "  font-weight: 400;"
        "  padding: 0px;"
        "}"
        "QLabel#pageTitle { font-size: 24px; font-weight: 800; }"
        "QPushButton#primaryButton {"
        "  background-color: #007AFF; color: white; border: none; border-radius: 2px;"
        "  font-size: 16px; padding: 10px 18px;"
        "}"
        "QPushButton#mediaAddButton {"
        "  background-color: #007AFF; color: #FFFFFF; border: none; border-radius: 2px;"
        "  font-size: 16px; font-weight: 600; padding: 0px 14px;"
        "}"
        "QPushButton#mediaAddButton:hover { background-color: #0A84FF; }"
        "QPushButton#mediaAddButton:pressed { background-color: #0060DF; }"
        "QPushButton#primaryButton:hover { background-color: #0A84FF; }"
        "QPushButton#primaryButton:pressed { background-color: #0060DF; }"
        "QPushButton#dangerButton {"
        "  background-color: #FF0001; color: #FFFFFF; border: none; border-radius: 2px;"
        "  font-size: 14px; font-weight: 700; padding: 0px;"
        "}"
        "QPushButton#dangerButton:hover { background-color: #FF3B30; }"
        "QPushButton#ghostButton {"
        "  background-color: rgba(255,255,255,0.08); color: white; border: none; border-radius: 2px;"
        "  font-size: 14px; padding: 10px 18px;"
        "}"
        "QPushButton#ghostButton:hover { background-color: rgba(255,255,255,0.12); }"
        "QPushButton#cancelActionButton {"
        "  background-color: #3C404D; color: #FFFFFF; border: 1px solid #757575; border-radius: 2px;"
        "  font-size: 14px; font-weight: 700; padding: 0px;"
        "}"
        "QPushButton#cancelActionButton:hover { background-color: #4A4F5E; }"
        "QPushButton#cancelActionButton:pressed { background-color: #343844; }"
        "QPushButton#helpIconButton { border: none; background: transparent; padding: 0px; }"
        "QPushButton#helpIconButton:hover { background-color: rgba(255,255,255,0.08); }"
        "QPushButton#tabButton {"
        "  background-color: transparent; color: rgba(255,255,255,0.7);"
        "  border: 1px solid rgba(255,255,255,0.35); border-radius: 2px;"
        "  font-size: 16px; padding: 10px 18px;"
        "}"
        "QPushButton#tabButton:checked {"
        "  background-color: #007AFF; color: white; border: none;"
        "}"
        "QTabWidget#positionOrientationTabs::pane, QTabWidget#mainSectionTabs::pane {"
        "  border: none;"
        "  border-top: 1px solid #77808F;"
        "  background: transparent;"
        "  top: 0px;"
        "  margin-top: 0px;"
        "}"
        "QTabWidget#positionOrientationTabs QTabBar, QTabWidget#mainSectionTabs QTabBar {"
        "  alignment: left;"
        "}"
        "QTabWidget#positionOrientationTabs QTabBar::tab, QTabWidget#mainSectionTabs QTabBar::tab {"
        "  background-color: #12141A; color: #FFFFFF;"
        "  border: 1px solid #77808F; border-bottom: none;"
        "  border-top-left-radius: 2px; border-top-right-radius: 2px;"
        "  border-bottom-left-radius: 0px; border-bottom-right-radius: 0px;"
        "  font-size: 14px; font-weight: 400; min-width: 56px; min-height: 28px; max-height: 28px;"
        "  padding: 3px 12px; margin-right: 4px; margin-top: 0px; margin-bottom: 0px; text-align: center;"
        "}"
        "QTabWidget#positionOrientationTabs QTabBar::tab:selected, "
        "QTabWidget#mainSectionTabs QTabBar::tab:selected {"
        "  background-color: #007ACC; color: #FFFFFF; border-color: #77808F;"
        "  margin-top: 0px;"
        "  margin-bottom: 0px;"
        "}"
        "QTabWidget#positionOrientationTabs QTabBar::tab:hover, "
        "QTabWidget#mainSectionTabs QTabBar::tab:hover {"
        "  background-color: #505050;"
        "}"
        "QSpinBox#posSpinBox {"
        "  background-color: rgba(0,0,0,0.25);"
        "  border: 1px solid rgba(255,255,255,0.12);"
        "  border-radius: 2px;"
        "  color: #FFFFFF;"
        "  font-size: 14px;"
        "  font-weight: 400;"
        "  padding: 4px 8px;"
        "}"
        "QSpinBox#posSpinBox::up-button, QSpinBox#posSpinBox::down-button {"
        "  width: 18px;"
        "  subcontrol-origin: border;"
        "  background: transparent;"
        "  border: none;"
        "}"
        "QSpinBox#posSpinBox::up-arrow, QSpinBox#posSpinBox::down-arrow {"
        "  width: 8px;"
        "  height: 8px;"
        "}"
        "QLineEdit, QSpinBox, QComboBox {"
        "  background-color: rgba(0,0,0,0.25);"
        "  border: 1px solid rgba(255,255,255,0.12);"
        "  border-radius: 2px;"
        "  color: white;"
        "  font-size: 16px;"
        "  padding: 6px 10px;"
        "}"
        "QCheckBox { color: white; font-size: 16px; }"
        "QComboBox::drop-down { border: none; }"
        "QListWidget#mediaList {"
        "  background: transparent;"
        "  border: none;"
        "  outline: none;"
        "}"
        "QListWidget#mediaList::item {"
        "  background: transparent;"
        "  border: none;"
        "  height: 43px;"
        "  padding: 0px;"
        "  margin: 0px 0px 5px 0px;"
        "}"
        "QListWidget#mediaList::item:selected {"
        "  background: transparent;"
        "  border: none;"
        "  color: inherit;"
        "}"
        "QLabel#mediaCountLabel {"
        "  color: #FFFFFF;"
        "  font-size: 14px;"
        "  font-weight: 400;"
        "  line-height: 21px;"
        "}"
        "QScrollBar:vertical { background: transparent; width: 10px; margin: 0px; }"
        "QScrollBar::handle:vertical { background: rgba(255,255,255,0.18); border-radius: 2px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }");

    auto* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(kDockOuterMargin, kDockOuterMargin, kDockOuterMargin,
                                   kDockOuterMargin);
    rootLayout->setSpacing(10);

    auto* titleBar = new QWidget(this);
    titleBar->setObjectName("customizedCartoonTitleBar");
    titleBar->setStyleSheet(
        "QWidget#customizedCartoonTitleBar {"
        "  background-color: #12141A;"
        "  border: 0.5px solid #3D3D3D;"
        "  border-radius: 2px;"
        "}"
        "QLabel#customizedCartoonTitleLabel {"
        "  color: #FFFFFF;"
        "  font-size: 14px;"
        "  font-weight: 600;"
        "}"
        "QPushButton#customizedCartoonTitleButton {"
        "  border: none;"
        "  background: transparent;"
        "  padding: 0px;"
        "}"
        "QPushButton#customizedCartoonTitleButton:hover { background-color: rgba(255,255,255,0.08); }");
    auto* titleBarLayout = new QHBoxLayout(titleBar);
    titleBarLayout->setContentsMargins(10, 4, 8, 4);
    titleBarLayout->setSpacing(6);

    auto* titleLabel = new QLabel(windowTitle(), titleBar);
    titleLabel->setObjectName("customizedCartoonTitleLabel");
    titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    titleBarLayout->addWidget(titleLabel, 0, Qt::AlignVCenter);
    titleBarLayout->addStretch();

    auto* titleHelpButton = new QPushButton(titleBar);
    titleHelpButton->setObjectName("customizedCartoonTitleButton");
    titleHelpButton->setIcon(QIcon(":/resources/question.svg"));
    titleHelpButton->setIconSize(QSize(16, 16));
    titleHelpButton->setFixedSize(20, 20);
    titleHelpButton->setCursor(Qt::PointingHandCursor);
    titleHelpButton->setToolTip(obs_module_text("CustomizedCartoon.Help.Tooltip"));
    titleBarLayout->addWidget(titleHelpButton, 0, Qt::AlignVCenter);

    auto* floatButton = new QPushButton(titleBar);
    floatButton->setObjectName("customizedCartoonTitleButton");
    floatButton->setFixedSize(20, 20);
    floatButton->setCursor(Qt::PointingHandCursor);
    titleBarLayout->addWidget(floatButton, 0, Qt::AlignVCenter);

    auto* closeButton = new QPushButton(titleBar);
    closeButton->setObjectName("customizedCartoonTitleButton");
    closeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    closeButton->setIconSize(QSize(12, 12));
    closeButton->setFixedSize(20, 20);
    closeButton->setCursor(Qt::PointingHandCursor);
    titleBarLayout->addWidget(closeButton, 0, Qt::AlignVCenter);

    auto updateTitleBarButtons = [this, floatButton, closeButton, titleLabel]() {
        titleLabel->setText(windowTitle());
        const auto dockFeatures = features();
        const bool canFloat = dockFeatures.testFlag(QDockWidget::DockWidgetFloatable);
        const bool canClose = dockFeatures.testFlag(QDockWidget::DockWidgetClosable);
        floatButton->setVisible(canFloat);
        closeButton->setVisible(canClose);
        floatButton->setIcon(
            style()->standardIcon(isFloating() ? QStyle::SP_TitleBarNormalButton
                                               : QStyle::SP_TitleBarMaxButton));
        floatButton->setIconSize(QSize(12, 12));
        floatButton->setToolTip(isFloating()
                                    ? obs_module_text("CustomizedCartoon.Dock.Docked")
                                    : obs_module_text("CustomizedCartoon.Dock.Float"));
        closeButton->setToolTip(obs_module_text("CustomizedCartoon.Dock.Close"));
    };

    connect(titleHelpButton, &QPushButton::clicked, this, &CustomizedCartoonDock::openHelpDialog);
    connect(floatButton, &QPushButton::clicked, this, [this]() { setFloating(!isFloating()); });
    connect(closeButton, &QPushButton::clicked, this, &QDockWidget::close);
    connect(this, &QDockWidget::topLevelChanged, this, [updateTitleBarButtons](bool) {
        updateTitleBarButtons();
    });
    connect(this, &QDockWidget::featuresChanged, this,
            [updateTitleBarButtons](QDockWidget::DockWidgetFeatures) {
                updateTitleBarButtons();
            });
    connect(this, &QWidget::windowTitleChanged, this, [updateTitleBarButtons](const QString&) {
        updateTitleBarButtons();
    });
    updateTitleBarButtons();
    setTitleBarWidget(titleBar);

    auto* contentScrollArea = new QScrollArea(root);
    contentScrollArea->setWidgetResizable(true);
    contentScrollArea->setFrameShape(QFrame::NoFrame);
    contentScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    contentScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    rootLayout->addWidget(contentScrollArea, 1);

    auto* scrollContent = new QWidget(contentScrollArea);
    auto* scrollContentLayout = new QVBoxLayout(scrollContent);
    scrollContentLayout->setContentsMargins(0, 0, 0, 0);
    scrollContentLayout->setSpacing(0);

    auto* mainTabWidget = new QTabWidget(scrollContent);
    mainTabWidget->setObjectName("mainSectionTabs");
    mainTabWidget->setUsesScrollButtons(false);
    mainTabWidget->tabBar()->setExpanding(false);
    mainTabWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
#ifdef Q_OS_MACOS
    mainTabWidget->setUsesScrollButtons(true);
    mainTabWidget->tabBar()->setStyle(QStyleFactory::create("Fusion"));
#endif
    scrollContentLayout->addWidget(mainTabWidget);

    auto* settingsPage = new QWidget(mainTabWidget);
    auto* settingsPageLayout = new QVBoxLayout(settingsPage);
    settingsPageLayout->setContentsMargins(0, 0, 0, 0);
    settingsPageLayout->setSpacing(18);

    auto* leftContainer = new QWidget(settingsPage);
    leftContainer->setMinimumWidth(kPositionPanelMinWidth);
    leftContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* left = new QVBoxLayout(leftContainer);
    left->setContentsMargins(0, 0, 0, 0);
    left->setSpacing(18);

    auto* mediaPanel = new QFrame(leftContainer);
    mediaPanel->setObjectName("mediaPanel");
    mediaPanel->setMinimumSize(kPositionPanelMinWidth, 269);
    mediaPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* mediaLayout = new QVBoxLayout(mediaPanel);
    mediaLayout->setContentsMargins(5, 5, 5, 5);
    mediaLayout->setSpacing(5);

    auto* mediaTitleBar = new QFrame(mediaPanel);
    mediaTitleBar->setObjectName("mediaTitleBar");
    mediaTitleBar->setFixedHeight(29);
    auto* mediaTitleLayout = new QHBoxLayout(mediaTitleBar);
    mediaTitleLayout->setContentsMargins(0, 0, 0, 5);
    mediaTitleLayout->setSpacing(0);

    auto* mediaTitle =
        new QLabel(obs_module_text("CustomizedCartoon.Media.VideoSetupTitle"), mediaTitleBar);
    mediaTitle->setObjectName("mediaTitle");
    mediaTitle->setFixedHeight(24);
    mediaTitleLayout->addWidget(mediaTitle, 0, Qt::AlignLeft | Qt::AlignVCenter);

    mediaList_ = new QListWidget(mediaPanel);
    mediaList_->setObjectName("mediaList");
    mediaList_->setFrameShape(QFrame::NoFrame);
    mediaList_->setSpacing(2);
    mediaList_->setSelectionMode(QAbstractItemView::SingleSelection);
    mediaList_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    mediaList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mediaList_->setStyleSheet("QListWidget#mediaList { background: transparent; }");

    auto* mediaBottom = new QHBoxLayout();
    mediaBottom->setContentsMargins(0, 0, 0, 0);
    mediaBottom->setSpacing(5);

    mediaCountLabel_ = new QLabel(mediaPanel);
    mediaCountLabel_->setObjectName("mediaCountLabel");
    mediaCountLabel_->setFixedHeight(21);

    addMediaButton_ =
        new QPushButton(obs_module_text("CustomizedCartoon.Media.ChooseVideo"), mediaPanel);
    addMediaButton_->setObjectName("mediaAddButton");
    addMediaButton_->setFixedSize(150, 30);

    mediaBottom->addWidget(mediaCountLabel_, 1);
    mediaBottom->addWidget(addMediaButton_, 0, Qt::AlignRight);

    mediaLayout->addWidget(mediaTitleBar);
    mediaLayout->addWidget(mediaList_, 1);
    mediaLayout->addLayout(mediaBottom);

    left->addWidget(mediaPanel);
    connect(addMediaButton_, &QPushButton::clicked, this, &CustomizedCartoonDock::onAddMedia);

    auto* positionPanel = new QFrame(leftContainer);
    positionPanel->setObjectName("panel");
    positionPanel->setMinimumSize(kPositionPanelMinWidth, 425);
    positionPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    positionPanel->setLayoutDirection(Qt::LeftToRight);
    auto* positionLayout = new QVBoxLayout(positionPanel);
    positionLayout->setContentsMargins(5, 5, 5, 5);
    positionLayout->setSpacing(5);

    auto* positionTitle =
        new QLabel(obs_module_text("CustomizedCartoon.Position.PanelTitle"), positionPanel);
    positionTitle->setObjectName("positionPanelTitle");
    positionTitle->setFixedHeight(20);
    positionLayout->addWidget(positionTitle);

    positionTabWidget_ = new QTabWidget(positionPanel);
    positionTabWidget_->setObjectName("positionOrientationTabs");
    positionTabWidget_->setUsesScrollButtons(false);
    positionTabWidget_->tabBar()->setExpanding(false);
#ifdef Q_OS_MACOS
    positionTabWidget_->setUsesScrollButtons(true);
    positionTabWidget_->tabBar()->setStyle(QStyleFactory::create("Fusion"));
#endif

    auto bindCanvasAndInputs = [this](PositionCanvasWidget* canvas, const PositionSizePanelWidgets& inputs,
                                      int canvasW, int canvasH) {
        canvas->setOnRectChanged([this, inputs, canvasW, canvasH](const QRect& r) {
            applyPositionInputs(inputs, r, canvasW, canvasH);
            syncPreviewDraftTransform();
        });

        auto syncCanvasFromInputs = [this, canvas, inputs, canvasW, canvasH]() {
            if (!inputs.posX || !inputs.posY || !inputs.width || !inputs.height) {
                return;
            }
            applyPositionState(canvas, inputs,
                               QRect(inputs.posX->value(), inputs.posY->value(), inputs.width->value(),
                                     inputs.height->value()),
                               canvasW, canvasH);
            syncPreviewDraftTransform();
        };
        connect(inputs.posX, &QSpinBox::valueChanged, this, [syncCanvasFromInputs](int) {
            syncCanvasFromInputs();
        });
        connect(inputs.posY, &QSpinBox::valueChanged, this, [syncCanvasFromInputs](int) {
            syncCanvasFromInputs();
        });
        connect(inputs.width, &QSpinBox::valueChanged, this, [syncCanvasFromInputs](int) {
            syncCanvasFromInputs();
        });
        connect(inputs.height, &QSpinBox::valueChanged, this, [syncCanvasFromInputs](int) {
            syncCanvasFromInputs();
        });
    };

    auto* portraitPage = new QWidget(positionTabWidget_);
    portraitPage->setLayoutDirection(Qt::LeftToRight);
    auto* portraitPageLayout = new QVBoxLayout(portraitPage);
    portraitPageLayout->setContentsMargins(0, 0, 0, 0);
    portraitPageLayout->setSpacing(4);

    auto* portraitRangeLabel = new QLabel(portraitPage);
    portraitRangeLabel->setObjectName("positionCommentLabel");
    portraitRangeLabel->setFixedHeight(21);
    portraitRangeLabel->setAlignment(Qt::AlignLeft);
    portraitPageLayout->addWidget(portraitRangeLabel);

    auto* portraitBody = new QHBoxLayout();
    portraitBody->setContentsMargins(0, 0, 0, 0);
    portraitBody->setSpacing(5);

    auto* portraitCanvas = new PositionCanvasWidget(portraitPage);
    portraitCanvas->setFixedWidth(210);
    portraitCanvas->setMinimumHeight(310);
    portraitCanvas->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    portraitCanvas->setCanvasSize(720, 1280);
    portraitCanvas->setRect(QRect(200, 300, 500, 500));
    portraitBody->addWidget(portraitCanvas);

    auto portraitInputs = createPositionSizePanel(portraitPage);
    auto* portraitTipsPanel = createPositionTipsPanel(portraitPage);

    auto* portraitRightSide = new QVBoxLayout();
    portraitRightSide->setContentsMargins(0, 0, 0, 0);
    portraitRightSide->setSpacing(5);
    portraitRightSide->addWidget(portraitInputs.panel);
    portraitRightSide->addWidget(portraitTipsPanel, 1);
    portraitBody->addLayout(portraitRightSide, 1);
    portraitPageLayout->addLayout(portraitBody, 1);
    positionTabWidget_->addTab(portraitPage,
                               obs_module_text("CustomizedCartoon.Position.Tab.Portrait"));

    auto* landscapePage = new QWidget(positionTabWidget_);
    landscapePage->setLayoutDirection(Qt::LeftToRight);
    auto* landscapePageLayout = new QVBoxLayout(landscapePage);
    landscapePageLayout->setContentsMargins(0, 0, 0, 0);
    landscapePageLayout->setSpacing(2);

    auto* landscapeRangeLabel = new QLabel(landscapePage);
    landscapeRangeLabel->setObjectName("positionCommentLabel");
    landscapeRangeLabel->setFixedHeight(21);
    landscapeRangeLabel->setAlignment(Qt::AlignLeft);
    landscapePageLayout->addWidget(landscapeRangeLabel);

    auto* landscapeBody = new QVBoxLayout();
    landscapeBody->setContentsMargins(0, 0, 0, 0);
    landscapeBody->setSpacing(4);

    auto* landscapeCanvas = new PositionCanvasWidget(landscapePage);
    landscapeCanvas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    landscapeCanvas->setFixedHeight(160);
    landscapeCanvas->setCanvasSize(1280, 720);
    landscapeCanvas->setRect(QRect(200, 100, 500, 500));
    landscapeBody->addWidget(landscapeCanvas, 0, Qt::AlignTop);

    auto landscapeInputs = createPositionSizePanel(landscapePage);
    auto* landscapeTipsPanel = createPositionTipsPanel(landscapePage);

    auto* landscapeBottom = new QHBoxLayout();
    landscapeBottom->setContentsMargins(0, 0, 0, 0);
    landscapeBottom->setSpacing(5);
    landscapeBottom->addWidget(landscapeInputs.panel, 1);
    landscapeBottom->addWidget(landscapeTipsPanel, 1);
    landscapeBody->addLayout(landscapeBottom, 1);
    landscapePageLayout->addLayout(landscapeBody, 1);
    positionTabWidget_->addTab(landscapePage,
                               obs_module_text("CustomizedCartoon.Position.Tab.Landscape"));
    positionTabWidget_->setCurrentIndex(0);

    canvasRangeLabel_ = portraitRangeLabel;
    positionCanvas_ = portraitCanvas;
    posXSpin_ = portraitInputs.posX;
    posYSpin_ = portraitInputs.posY;
    widthSpin_ = portraitInputs.width;
    heightSpin_ = portraitInputs.height;

    bindCanvasAndInputs(portraitCanvas, portraitInputs, 720, 1280);
    bindCanvasAndInputs(landscapeCanvas, landscapeInputs, 1280, 720);

    positionLayout->addWidget(positionTabWidget_, 1);
    left->addWidget(positionPanel);

    connect(positionTabWidget_, &QTabWidget::currentChanged, this,
            [this, portraitCanvas, portraitInputs, portraitRangeLabel, landscapeCanvas,
             landscapeInputs, landscapeRangeLabel](int index) {
        if (index == 0) {
            canvasRangeLabel_ = portraitRangeLabel;
            positionCanvas_ = portraitCanvas;
            posXSpin_ = portraitInputs.posX;
            posYSpin_ = portraitInputs.posY;
            widthSpin_ = portraitInputs.width;
            heightSpin_ = portraitInputs.height;
            onOrientationChanged(0);
        } else {
            canvasRangeLabel_ = landscapeRangeLabel;
            positionCanvas_ = landscapeCanvas;
            posXSpin_ = landscapeInputs.posX;
            posYSpin_ = landscapeInputs.posY;
            widthSpin_ = landscapeInputs.width;
            heightSpin_ = landscapeInputs.height;
            onOrientationChanged(1);
        }
    });

    settingsPageLayout->addWidget(leftContainer, 0);
    mainTabWidget->addTab(settingsPage, obs_module_text("CustomizedCartoon.Tab.Settings"));

    auto* rulesPage = new QWidget(mainTabWidget);
    auto* rulesPageLayout = new QVBoxLayout(rulesPage);
    rulesPageLayout->setContentsMargins(0, 0, 0, 0);
    rulesPageLayout->setSpacing(0);

    auto* rightPanel = new QFrame(rulesPage);
    rightPanel->setObjectName("panel");
    rightPanel->setMinimumWidth(kPositionPanelMinWidth);
    rightPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    rightPanel->setMinimumHeight(712);
    auto* right = new QVBoxLayout(rightPanel);
    right->setContentsMargins(5, 5, 5, 5);
    right->setSpacing(5);

    auto* rulesTitle = new QLabel(obs_module_text("CustomizedCartoon.Rules.PanelTitle"), rightPanel);
    rulesTitle->setObjectName("rulesPanelTitle");
    rulesTitle->setFixedHeight(24);
    right->addWidget(rulesTitle);

    rulesScrollArea_ = new QScrollArea(rightPanel);
    rulesScrollArea_->setWidgetResizable(true);
    rulesScrollArea_->setFrameShape(QFrame::NoFrame);
    rulesScrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rulesScrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    rulesListContainer_ = new QWidget(rulesScrollArea_);
    rulesListLayout_ = new QVBoxLayout(rulesListContainer_);
    rulesListLayout_->setContentsMargins(0, 0, 0, 0);
    rulesListLayout_->setSpacing(5);
    rulesListLayout_->addStretch(1);
    rulesScrollArea_->setWidget(rulesListContainer_);
    right->addWidget(rulesScrollArea_, 1);

    addRuleButton_ = new QPushButton(obs_module_text("CustomizedCartoon.Rules.AddCondition"), rightPanel);
    addRuleButton_->setObjectName("ghostButton");
    addRuleButton_->setMinimumHeight(48);
    right->addWidget(addRuleButton_);
    connect(addRuleButton_, &QPushButton::clicked, this, &CustomizedCartoonDock::onAddRule);
    rulesPageLayout->addWidget(rightPanel, 0);
    mainTabWidget->addTab(rulesPage, obs_module_text("CustomizedCartoon.Rules.PanelTitle"));

    auto* bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 0, 20, 0);
    bottomRow->setSpacing(5);
    bottomRow->addStretch(1);
    cancelButton_ = new QPushButton(obs_module_text("CustomizedCartoon.Action.Cancel"), root);
    confirmButton_ = new QPushButton(obs_module_text("CustomizedCartoon.Action.Confirm"), root);
    applyButton_ = new QPushButton(obs_module_text("CustomizedCartoon.Action.Apply"), root);
    cancelButton_->setObjectName("cancelActionButton");
    confirmButton_->setObjectName("dangerButton");
    applyButton_->setObjectName("dangerButton");
    cancelButton_->setFixedSize(100, 40);
    confirmButton_->setFixedSize(100, 40);
    applyButton_->setFixedSize(100, 40);
    bottomRow->addWidget(cancelButton_);
    bottomRow->addWidget(confirmButton_);
    bottomRow->addWidget(applyButton_);
    scrollContentLayout->addSpacing(10);
    scrollContentLayout->addLayout(bottomRow);

    connect(cancelButton_, &QPushButton::clicked, this, [this]() {
        refreshUi();
        setVisible(false);
    });
    connect(confirmButton_, &QPushButton::clicked, this, [this]() { setVisible(false); });
    connect(applyButton_, &QPushButton::clicked, this, &CustomizedCartoonDock::onApplyPosition);

    contentScrollArea->setWidget(scrollContent);
    setWidget(root);
}

void CustomizedCartoonDock::refreshUi() {
    loadFromConfig();
    refreshPositionUi();
    refreshProgress();
}

void CustomizedCartoonDock::refreshPositionUi() {
    if (!service_ || !positionTabWidget_) {
        return;
    }
    const json cfg = service_->getConfigSnapshot();
    const bool landscape = positionTabWidget_->currentIndex() == 1;
    const char* key = landscape ? "landscape" : "portrait";
    const int canvasW = landscape ? 1280 : 720;
    const int canvasH = landscape ? 720 : 1280;

    if (canvasRangeLabel_) {
        canvasRangeLabel_->setText(
            QString(obs_module_text("CustomizedCartoon.Position.CanvasRange"))
                .arg(landscape ? obs_module_text("CustomizedCartoon.Position.Tab.Landscape")
                               : obs_module_text("CustomizedCartoon.Position.Tab.Portrait"))
                .arg(canvasW)
                .arg(canvasH));
    }

    auto* currentCanvas = positionCanvas_ ? static_cast<PositionCanvasWidget*>(positionCanvas_) : nullptr;
    PositionSizePanelWidgets currentInputs{nullptr, posXSpin_, posYSpin_, widthSpin_, heightSpin_};
    if (!cfg.contains("position") || !cfg["position"].is_object() || !cfg["position"].contains(key) ||
        !cfg["position"][key].is_object()) {
        applyPositionState(currentCanvas, currentInputs, QRect(200, 300, 500, 500), canvasW, canvasH);
        return;
    }

    const auto& t = cfg["position"][key];
    applyPositionState(currentCanvas, currentInputs,
                       QRect((int)std::round(t.value("x", 200.0)), (int)std::round(t.value("y", 300.0)),
                             (int)std::round(t.value("boundsW", 500.0)),
                             (int)std::round(t.value("boundsH", 500.0))),
                       canvasW, canvasH);
}

void CustomizedCartoonDock::loadFromConfig() {
    if (!service_) {
        return;
    }
    refreshMediaList();
    rebuildRulesUi();
}

void CustomizedCartoonDock::refreshMediaList() {
    if (!service_ || !mediaList_) {
        return;
    }
    const json cfg = service_->getConfigSnapshot();

    mediaList_->clear();
    int mediaCount = 0;
    const QIcon videoIcon(":/resources/video.svg");
    const QIcon imageIcon(":/resources/image.svg");
    const QIcon playIcon(":/resources/play.svg");
    const QIcon stopIcon(":/resources/stop.svg");
    const QIcon settingsIcon(":/resources/settings.svg");
    const QIcon trashIcon(":/resources/trash-red.svg");
    const bool previewing = service_->isMediaPreviewing();
    const QString previewingId = service_->previewingMediaId();
    if (cfg.contains("media") && cfg["media"].is_array()) {
        for (const auto& it : cfg["media"]) {
            if (!it.is_object())
                continue;
            const QString id =
                it.contains("id") && it["id"].is_string()
                    ? QString::fromStdString(it["id"].get<std::string>())
                    : QString();
            const QString name =
                it.contains("name") && it["name"].is_string()
                    ? QString::fromStdString(it["name"].get<std::string>())
                    : QString();
            const QString type =
                it.contains("type") && it["type"].is_string()
                    ? QString::fromStdString(it["type"].get<std::string>())
                    : QString();
            if (id.isEmpty()) {
                continue;
            }
            mediaCount++;

            auto* item = new QListWidgetItem(mediaList_);
            item->setData(Qt::UserRole, id);
            item->setSizeHint(QSize(0, 43));

            auto* row = new QFrame(mediaList_);
            row->setObjectName("mediaRow");
            row->setStyleSheet(
                "QFrame#mediaRow { background-color: #272A33; border: none; border-radius: 0px; }"
                "QLabel { color: #A1A9B6; font-size: 14px; font-weight: 400; }"
                "QPushButton { border: none; background: transparent; }");
            row->setMinimumHeight(43);
            row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

            auto* rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(5, 5, 5, 5);
            rowLayout->setSpacing(5);

            auto* iconLabel = new QLabel(row);
            const QIcon& mediaIcon = type == "image" ? imageIcon : videoIcon;
            iconLabel->setPixmap(mediaIcon.pixmap(30, 24));
            iconLabel->setFixedSize(30, 24);

            auto* nameLabel = new QLabel(name, row);
            nameLabel->setFixedHeight(24);
            nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            nameLabel->setTextInteractionFlags(Qt::NoTextInteraction);
            nameLabel->setStyleSheet(
                "QLabel { color: #A1A9B6; font-size: 14px; font-weight: 400; background: transparent; }");

            const bool previewingThis = previewing && previewingId == id;
            auto* settingsButton = new QPushButton(row);
            settingsButton->setIcon(settingsIcon);
            settingsButton->setIconSize(QSize(16, 16));
            settingsButton->setFixedSize(24, 24);
            settingsButton->setCursor(Qt::PointingHandCursor);
            settingsButton->setToolTip(obs_module_text("CustomizedCartoon.Media.Settings.Tooltip"));

            auto* previewButton = new QPushButton(row);
            previewButton->setIcon(previewingThis ? stopIcon : playIcon);
            previewButton->setIconSize(QSize(24, 24));
            previewButton->setFixedSize(24, 24);
            previewButton->setCursor(Qt::PointingHandCursor);

            auto* delButton = new QPushButton(row);
            delButton->setIcon(trashIcon);
            delButton->setIconSize(QSize(24, 24));
            delButton->setFixedSize(24, 24);
            delButton->setCursor(Qt::PointingHandCursor);

            connect(delButton, &QPushButton::clicked, this, [this, id]() {
                if (!service_) {
                    return;
                }
                QString error;
                if (!service_->deleteMedia(id, error)) {
                    QMessageBox::warning(this, obs_module_text("CustomizedCartoon.Dock.Title"), error,
                                         QMessageBox::Ok);
                }
                refreshUi();
            });

            connect(settingsButton, &QPushButton::clicked, this, [this, id]() {
                openMediaSettingsDialog(id);
            });

            connect(previewButton, &QPushButton::clicked, this, [this, item, id]() {
                if (!service_) {
                    return;
                }
                if (mediaList_) {
                    mediaList_->setCurrentItem(item);
                }

                const bool previewing = service_->isMediaPreviewing();
                const QString previewingId = service_->previewingMediaId();
                if (previewing && previewingId == id) {
                    service_->stopMediaPreview();
                    return;
                }
                if (previewing && previewingId != id) {
                    QMessageBox::information(this, obs_module_text("CustomizedCartoon.Dock.Title"),
                                             obs_module_text("CustomizedCartoon.Media.PreviewBusy"),
                                             QMessageBox::Ok);
                    return;
                }

                QString error;
                const bool landscape = positionTabWidget_ && positionTabWidget_->currentIndex() == 1;
                const auto draft = buildCurrentPositionDraft(landscape);
                if (!service_->startMediaPreview(id, landscape, &draft, error)) {
                    QMessageBox::warning(this, obs_module_text("CustomizedCartoon.Dock.Title"), error,
                                         QMessageBox::Ok);
                }
            });

            rowLayout->addWidget(iconLabel);
            rowLayout->addWidget(nameLabel, 1);
            rowLayout->addWidget(settingsButton);
            rowLayout->addWidget(previewButton);
            rowLayout->addWidget(delButton);

            mediaList_->addItem(item);
            mediaList_->setItemWidget(item, row);
        }
    }

    if (mediaCountLabel_) {
        mediaCountLabel_->setText(
            QString(obs_module_text("CustomizedCartoon.Media.SelectedVideoCount")).arg(mediaCount));
    }
    if (mediaList_->count() > 0 && mediaList_->currentRow() < 0) {
        mediaList_->setCurrentRow(0);
    }
}

void CustomizedCartoonDock::openMediaSettingsDialog(const QString& mediaId) {
    if (!service_ || mediaId.isEmpty()) {
        return;
    }

    json cfg = service_->getConfigSnapshot();
    if (!cfg.contains("media") || !cfg["media"].is_array()) {
        return;
    }

    for (auto& media : cfg["media"]) {
        if (!media.is_object() || !media.contains("id") || !media["id"].is_string()) {
            continue;
        }
        if (QString::fromStdString(media["id"].get<std::string>()) != mediaId) {
            continue;
        }

        const QString name =
            media.contains("name") && media["name"].is_string()
                ? QString::fromStdString(media["name"].get<std::string>())
                : mediaId;
        const QString type =
            media.contains("type") && media["type"].is_string()
                ? QString::fromStdString(media["type"].get<std::string>())
                : QString();
        int displaySec =
            media.contains("displaySec") && media["displaySec"].is_number_integer()
                ? media["displaySec"].get<int>()
                : (type == "video" ? 15 : 5);
        bool muted =
            media.contains("muted") && media["muted"].is_boolean() ? media["muted"].get<bool>()
                                                                   : (type == "video");
        bool preserveAspectRatio =
            media.contains("preserveAspectRatio") && media["preserveAspectRatio"].is_boolean()
                ? media["preserveAspectRatio"].get<bool>()
                : false;

        int nextDisplaySec = displaySec;
        bool nextMuted = muted;
        bool nextPreserveAspectRatio = preserveAspectRatio;
        if (!editMediaSettingsDialog(this, name, type == "video", displaySec, muted,
                                     preserveAspectRatio, nextDisplaySec, nextMuted,
                                     nextPreserveAspectRatio)) {
            return;
        }

        media["displaySec"] = nextDisplaySec;
        media["muted"] = (type == "video") ? nextMuted : false;
        media["preserveAspectRatio"] = nextPreserveAspectRatio;

        if (!service_->saveConfig(cfg)) {
            QMessageBox::warning(this, obs_module_text("CustomizedCartoon.Dock.Title"),
                                 obs_module_text("CustomizedCartoon.Error.SaveConfigFailed"),
                                 QMessageBox::Ok);
        }
        return;
    }
}

void CustomizedCartoonDock::openHelpDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle(obs_module_text("CustomizedCartoon.Help.Title"));
    dialog.setModal(true);
    dialog.setMinimumWidth(420);
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog.setStyleSheet(
        "QDialog { background-color: #4A5568; border-radius: 10px; color: #FFFFFF; }"
        "QLabel#helpTitle { color: #FFFFFF; font-size: 16px; font-weight: 700; }"
        "QLabel#helpContent { color: #FFFFFF; font-size: 14px; line-height: 150%; }"
        "QLabel#helpContent a { color: #007AFF; text-decoration: none; }"
        "QPushButton { min-width: 88px; min-height: 32px; padding: 0px 12px; }"
        "QPushButton { background-color: #007AFF; color: #FFFFFF; border: none; border-radius: 2px; "
        "font-size: 14px; font-weight: 600; }"
        "QPushButton:hover { background-color: #0A84FF; }");

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel(obs_module_text("CustomizedCartoon.Help.Title"), &dialog);
    titleLabel->setObjectName("helpTitle");
    layout->addWidget(titleLabel);

    auto* contentLabel = new QLabel(&dialog);
    contentLabel->setObjectName("helpContent");
    contentLabel->setWordWrap(true);
    contentLabel->setTextFormat(Qt::RichText);
    contentLabel->setOpenExternalLinks(true);
    contentLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    contentLabel->setText(obs_module_text("CustomizedCartoon.Help.Content"));
    layout->addWidget(contentLabel);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    if (auto* okButton = buttonBox->button(QDialogButtonBox::Ok)) {
        okButton->setText(obs_module_text("CustomizedCartoon.Help.Button"));
    }
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    layout->addWidget(buttonBox, 0, Qt::AlignRight);

    dialog.exec();
}

void CustomizedCartoonDock::rebuildRulesUi() {
    if (!service_ || !rulesListLayout_ || !rulesListContainer_) {
        return;
    }

    const int previousScrollValue =
        rulesScrollArea_ && rulesScrollArea_->verticalScrollBar()
            ? rulesScrollArea_->verticalScrollBar()->value()
            : 0;
    if (rulesScrollArea_) {
        rulesScrollArea_->setUpdatesEnabled(false);
    }
    rulesListContainer_->setUpdatesEnabled(false);

    while (QLayoutItem* item = rulesListLayout_->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    const json cfg = service_->getConfigSnapshot();

    std::vector<std::pair<QString, QString>> mediaOptions;
    if (cfg.contains("media") && cfg["media"].is_array()) {
        for (const auto& m : cfg["media"]) {
            if (!m.is_object() || !m.contains("id") || !m["id"].is_string()) {
                continue;
            }
            const QString id = QString::fromStdString(m["id"].get<std::string>());
            const QString name = m.contains("name") && m["name"].is_string()
                                     ? QString::fromStdString(m["name"].get<std::string>())
                                     : id;
            mediaOptions.emplace_back(id, name);
        }
    }

    const QIcon trashIcon(":/resources/trash-red.svg");
    int idx = 0;
    QWidget* latestCard = nullptr;
    if (cfg.contains("rules") && cfg["rules"].is_array()) {
        for (const auto& it : cfg["rules"]) {
            if (!it.is_object() || !it.contains("id") || !it["id"].is_string()) {
                continue;
            }
            const QString id = QString::fromStdString(it["id"].get<std::string>());
            const QString engageType =
                it.contains("engageType") && it["engageType"].is_string()
                    ? QString::fromStdString(it["engageType"].get<std::string>())
                    : QString("GIFT_AMOUNT_MILESTONE");
            const int points = it.contains("points") && it["points"].is_number_integer()
                                   ? it["points"].get<int>()
                                   : 0;
            const int count = it.contains("count") && it["count"].is_number_integer() ? it["count"].get<int>()
                                                                                      : 0;
            const QString mediaId =
                it.contains("mediaId") && it["mediaId"].is_string()
                    ? QString::fromStdString(it["mediaId"].get<std::string>())
                    : QString();
            const bool repeatable =
                it.contains("repeatable") && it["repeatable"].is_boolean() ? it["repeatable"].get<bool>()
                                                                           : true;
            const bool enabled =
                it.contains("enabled") && it["enabled"].is_boolean() ? it["enabled"].get<bool>() : true;

            auto* card = new QFrame(rulesListContainer_);
            card->setObjectName("subPanel");
            auto* cardLayout = new QVBoxLayout(card);
            cardLayout->setContentsMargins(5, 5, 5, 5);
            cardLayout->setSpacing(5);

            auto* header = new QHBoxLayout();
            header->setContentsMargins(0, 0, 0, 0);
            header->setSpacing(5);

            auto* title =
                new QLabel(QString(obs_module_text("CustomizedCartoon.Rules.ConditionTitle")).arg(idx + 1),
                           card);
            title->setObjectName("ruleCardTitle");
            title->setFixedHeight(24);

            auto* delButton = new QPushButton(card);
            delButton->setIcon(trashIcon);
            delButton->setIconSize(QSize(24, 24));
            delButton->setFixedSize(32, 32);
            delButton->setCursor(Qt::PointingHandCursor);
            delButton->setStyleSheet("QPushButton { border: none; background: transparent; }");

            header->addWidget(title);
            header->addStretch(1);
            header->addWidget(delButton);
            cardLayout->addLayout(header);

            auto* grid = new QGridLayout();
            grid->setContentsMargins(0, 0, 0, 0);
            grid->setHorizontalSpacing(5);
            grid->setVerticalSpacing(5);
            grid->setColumnStretch(1, 1);

            auto makeLabel = [card](const char* key) {
                auto* l = new QLabel(obs_module_text(key), card);
                l->setStyleSheet("QLabel { font-size: 14px; color: #FFFFFF; }");
                return l;
            };

            auto* typeCombo = new QComboBox(card);
            typeCombo->addItem(obs_module_text("CustomizedCartoon.Rules.Type.LuckyBag"),
                               "GIFT_LUCKYBAG_FIRST_PRIZE_MILESTONE");
            typeCombo->addItem(obs_module_text("CustomizedCartoon.Rules.Type.Gift"), "GIFT_AMOUNT_MILESTONE");
            const int typeIndex =
                engageType == "GIFT_LUCKYBAG_FIRST_PRIZE_MILESTONE" ? 0 : 1;
            typeCombo->setCurrentIndex(typeIndex);
            typeCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            typeCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            typeCombo->setMinimumContentsLength(kRuleTypeComboChars);
            typeCombo->setStyleSheet("QComboBox { font-size: 14px; color: #FFFFFF; }");

            auto* ruleLabel = new QLabel(card);
            ruleLabel->setWordWrap(true);
            ruleLabel->setStyleSheet("QLabel { font-size: 14px; color: #FFFFFF; }");

            auto* pointsSpin = new QSpinBox(card);
            pointsSpin->setRange(1, 100000000);
            pointsSpin->setValue(std::max(1, points));
            pointsSpin->setFixedWidth(kRuleGiftAmountSpinWidth);
            pointsSpin->setStyleSheet(
                "QSpinBox { font-size: 14px; color: #FFFFFF; padding: 4px 4px 4px 6px; }");
            auto* countSpin = new QSpinBox(card);
            countSpin->setRange(1, 1000000);
            countSpin->setValue(std::max(1, count));
            countSpin->setFixedWidth(kRuleGiftCountSpinWidth);
            countSpin->setStyleSheet(
                "QSpinBox { font-size: 14px; color: #FFFFFF; padding: 4px 4px 4px 6px; }");

            auto* mediaCombo = new QComboBox(card);
            for (const auto& m : mediaOptions) {
                mediaCombo->addItem(m.second, m.first);
            }
            const int mediaIndex = mediaCombo->findData(mediaId);
            if (mediaIndex >= 0) {
                mediaCombo->setCurrentIndex(mediaIndex);
            }
            mediaCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            mediaCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            mediaCombo->setMinimumContentsLength(kRuleMediaComboChars);
            mediaCombo->setStyleSheet("QComboBox { font-size: 14px; color: #FFFFFF; }");

            auto* repeatCheck = new QCheckBox(card);
            repeatCheck->setChecked(repeatable);
            repeatCheck->setText(obs_module_text("CustomizedCartoon.Rules.Field.Repeat"));
            repeatCheck->setStyleSheet("QCheckBox { font-size: 14px; color: #FFFFFF; }");

            auto* statusCombo = new QComboBox(card);
            statusCombo->addItem(obs_module_text("CustomizedCartoon.Rules.Status.Active"), true);
            statusCombo->addItem(obs_module_text("CustomizedCartoon.Rules.Status.Inactive"), false);
            statusCombo->setCurrentIndex(enabled ? 0 : 1);
            statusCombo->setFixedWidth(kRuleStatusComboWidth);
            statusCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            statusCombo->setMinimumContentsLength(kRuleStatusComboChars);
            statusCombo->setStyleSheet("QComboBox { font-size: 14px; color: #FFFFFF; }");

            auto* paramsRow = new QHBoxLayout();
            paramsRow->setContentsMargins(0, 0, 0, 0);
            paramsRow->setSpacing(2);
            paramsRow->addStretch(1);
            auto* paramXLabel = new QLabel(card);
            paramXLabel->setFixedWidth(kRuleGiftParamLabelWidth);
            paramXLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            paramXLabel->setStyleSheet(
                "QLabel { font-size: 14px; color: #FFFFFF; padding: 0px; margin: 0px; }");
            auto* paramYLabel = new QLabel(card);
            paramYLabel->setFixedWidth(kRuleGiftParamLabelWidth);
            paramYLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            paramYLabel->setStyleSheet(
                "QLabel { font-size: 14px; color: #FFFFFF; padding: 0px; margin: 0px; }");
            paramsRow->addWidget(paramXLabel);
            paramsRow->addWidget(pointsSpin, 0);
            paramsRow->addWidget(paramYLabel);
            paramsRow->addWidget(countSpin, 0);
            auto* paramsWrap = new QWidget(card);
            paramsWrap->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
            paramsWrap->setLayout(paramsRow);

            auto* statusLabel = new QLabel(obs_module_text("CustomizedCartoon.Rules.Field.Status"), card);
            statusLabel->setStyleSheet("QLabel { font-size: 14px; color: #FFFFFF; }");

            auto updateRuleUi = [ruleLabel, typeCombo, pointsSpin, countSpin, paramXLabel, paramYLabel]() {
                const QString t = typeCombo->currentData().toString();
                if (t == "GIFT_LUCKYBAG_FIRST_PRIZE_MILESTONE") {
                    ruleLabel->setText(obs_module_text("CustomizedCartoon.Rules.Desc.LuckyBag"));
                    paramXLabel->setText(obs_module_text("CustomizedCartoon.Rules.Param.LuckyBagX"));
                    paramXLabel->setFixedWidth(kRuleLuckyBagParamLabelWidth);
                    countSpin->setFixedWidth(kRuleLuckyBagCountSpinWidth);
                    pointsSpin->hide();
                    paramYLabel->hide();
                    countSpin->show();
                } else {
                    ruleLabel->setText(obs_module_text("CustomizedCartoon.Rules.Desc.Gift"));
                    paramXLabel->setText(obs_module_text("CustomizedCartoon.Rules.Param.GiftX"));
                    paramXLabel->setFixedWidth(kRuleGiftParamLabelWidth);
                    pointsSpin->setFixedWidth(kRuleGiftAmountSpinWidth);
                    pointsSpin->show();
                    paramYLabel->setText(obs_module_text("CustomizedCartoon.Rules.Param.GiftY"));
                    paramYLabel->setFixedWidth(kRuleGiftParamLabelWidth);
                    paramYLabel->show();
                    countSpin->setFixedWidth(kRuleGiftCountSpinWidth);
                    countSpin->show();
                }
            };
            updateRuleUi();

            auto saveRule = [this, id, typeCombo, pointsSpin, countSpin, mediaCombo, repeatCheck,
                             statusCombo]() {
                if (!service_) {
                    return;
                }
                json cfg = service_->getConfigSnapshot();
                if (!cfg.contains("rules") || !cfg["rules"].is_array()) {
                    return;
                }
                for (auto& r : cfg["rules"]) {
                    if (!r.is_object() || !r.contains("id") || !r["id"].is_string()) {
                        continue;
                    }
                    if (QString::fromStdString(r["id"].get<std::string>()) != id) {
                        continue;
                    }
                    const QString engageType = typeCombo->currentData().toString();
                    const bool luckyBag = engageType == "GIFT_LUCKYBAG_FIRST_PRIZE_MILESTONE";
                    r["engageType"] = engageType.toStdString();
                    r["points"] = luckyBag ? 0 : pointsSpin->value();
                    r["count"] = countSpin->value();
                    r["mediaId"] = mediaCombo->currentData().toString().toStdString();
                    r["repeatable"] = repeatCheck->isChecked();
                    r["enabled"] = statusCombo->currentData().toBool();
                    break;
                }
                service_->saveConfig(cfg);
            };

            connect(typeCombo, &QComboBox::currentIndexChanged, card, [updateRuleUi, saveRule](int) {
                updateRuleUi();
                saveRule();
            });
            connect(pointsSpin, &QSpinBox::valueChanged, card, [saveRule](int) { saveRule(); });
            connect(countSpin, &QSpinBox::valueChanged, card, [saveRule](int) { saveRule(); });
            connect(mediaCombo, &QComboBox::currentIndexChanged, card, [saveRule](int) { saveRule(); });
            connect(repeatCheck, &QCheckBox::toggled, card, [saveRule](bool) { saveRule(); });
            connect(statusCombo, &QComboBox::currentIndexChanged, card, [saveRule](int) { saveRule(); });

            connect(delButton, &QPushButton::clicked, card, [this, id]() {
                if (!service_) {
                    return;
                }
                json cfg = service_->getConfigSnapshot();
                if (!cfg.contains("rules") || !cfg["rules"].is_array()) {
                    return;
                }
                json newRules = json::array();
                for (const auto& r : cfg["rules"]) {
                    if (!r.is_object() || !r.contains("id") || !r["id"].is_string()) {
                        continue;
                    }
                    if (QString::fromStdString(r["id"].get<std::string>()) == id) {
                        continue;
                    }
                    newRules.push_back(r);
                }
                cfg["rules"] = std::move(newRules);
                service_->saveConfig(cfg);
            });

            grid->addWidget(makeLabel("CustomizedCartoon.Rules.Field.Type"), 0, 0);
            grid->addWidget(typeCombo, 0, 1);

            grid->addWidget(makeLabel("CustomizedCartoon.Rules.Field.Rule"), 1, 0);
            grid->addWidget(ruleLabel, 1, 1);

            const bool isLuckyBag = typeCombo->currentData().toString() == "GIFT_LUCKYBAG_FIRST_PRIZE_MILESTONE";
            pointsSpin->setEnabled(!isLuckyBag);
            if (isLuckyBag) pointsSpin->setValue(0);
            connect(typeCombo, &QComboBox::currentIndexChanged, card,
                    [pointsSpin, typeCombo](int) {
                        const bool lb =
                            typeCombo->currentData().toString() == "GIFT_LUCKYBAG_FIRST_PRIZE_MILESTONE";
                        pointsSpin->setEnabled(!lb);
                        if (lb) pointsSpin->setValue(0);
                    });

            auto* paramSpacer = new QWidget(card);
            paramSpacer->setFixedWidth(1);
            grid->addWidget(paramSpacer, 2, 0);
            grid->addWidget(paramsWrap, 2, 1, Qt::AlignLeft);

            grid->addWidget(makeLabel("CustomizedCartoon.Rules.Field.Media"), 3, 0);
            grid->addWidget(mediaCombo, 3, 1);

            auto* bottomControls = new QHBoxLayout();
            bottomControls->setContentsMargins(0, 0, 0, 0);
            bottomControls->setSpacing(4);
            bottomControls->addWidget(repeatCheck);
            bottomControls->addStretch(1);
            bottomControls->addWidget(statusLabel);
            bottomControls->addWidget(statusCombo);
            auto* bottomWrap = new QWidget(card);
            bottomWrap->setLayout(bottomControls);
            auto* bottomSpacer = new QWidget(card);
            bottomSpacer->setFixedWidth(1);
            grid->addWidget(bottomSpacer, 4, 0);
            grid->addWidget(bottomWrap, 4, 1);

            cardLayout->addLayout(grid);

            rulesListLayout_->addWidget(card);
            latestCard = card;
            idx++;
        }
    }

    rulesListLayout_->addStretch(1);
    rulesListContainer_->adjustSize();
    if (rulesListLayout_) {
        rulesListLayout_->activate();
    }
    rulesListContainer_->setUpdatesEnabled(true);
    if (rulesScrollArea_) {
        rulesScrollArea_->setUpdatesEnabled(true);
        if (pendingScrollToLatestRule_ && latestCard) {
            QPointer<QScrollArea> scrollArea(rulesScrollArea_);
            QPointer<QWidget> latestCardPtr(latestCard);
            QTimer::singleShot(0, rulesScrollArea_, [scrollArea, latestCardPtr]() {
                if (!scrollArea || !latestCardPtr) {
                    return;
                }
                if (auto* scrollBar = scrollArea->verticalScrollBar()) {
                    scrollBar->setValue(scrollBar->maximum());
                }
                scrollArea->ensureWidgetVisible(latestCardPtr, 0, 8);
            });
        } else if (auto* scrollBar = rulesScrollArea_->verticalScrollBar()) {
            scrollBar->setValue(previousScrollValue);
        }
    }
    pendingScrollToLatestRule_ = false;
}

void CustomizedCartoonDock::refreshProgress() {
    if (!service_) {
        return;
    }
    if (!progressTable_) {
        return;
    }
    const auto progress = service_->getProgressSnapshot();
    progressTable_->setRowCount(0);
    int row = 0;
    for (const auto& p : progress) {
        progressTable_->insertRow(row);
        progressTable_->setItem(row, 0, new QTableWidgetItem(p.engageID));
        progressTable_->setItem(row, 1, new QTableWidgetItem(QString::number(p.current)));
        progressTable_->setItem(row, 2, new QTableWidgetItem(QString::number(p.target)));
        progressTable_->setItem(row, 3, new QTableWidgetItem(QString::number(p.round)));
        row++;
    }
}

void CustomizedCartoonDock::onAddMedia() {
    if (!service_) {
        return;
    }

    const QString path =
        QFileDialog::getOpenFileName(this, obs_module_text("CustomizedCartoon.Media.ChooseVideo"),
                                                      QString(), QString());
    if (path.isEmpty()) {
        return;
    }

    QString mediaId;
    QString error;
    if (!service_->importMediaFile(path, mediaId, error)) {
        QMessageBox::warning(this, obs_module_text("CustomizedCartoon.Dock.Title"), error,
                             QMessageBox::Ok);
        return;
    }
    refreshUi();
}

void CustomizedCartoonDock::onRemoveMedia() {
    if (!service_) {
        return;
    }
    auto* item = mediaList_->currentItem();
    if (!item) {
        return;
    }
    const QString id = item->data(Qt::UserRole).toString();
    if (id.isEmpty()) {
        return;
    }
    QString error;
    if (!service_->deleteMedia(id, error)) {
        QMessageBox::warning(this, obs_module_text("CustomizedCartoon.Dock.Title"), error,
                             QMessageBox::Ok);
    }
    refreshUi();
}

void CustomizedCartoonDock::onAddRule() {
    if (!service_) {
        return;
    }

    json cfg = service_->getConfigSnapshot();
    if (!cfg.contains("rules") || !cfg["rules"].is_array()) {
        cfg["rules"] = json::array();
    }
    if (cfg["rules"].size() >= 5) {
        QMessageBox::information(this, obs_module_text("CustomizedCartoon.Dock.Title"),
                                 obs_module_text("CustomizedCartoon.Rules.Limit"), QMessageBox::Ok);
        return;
    }

    QString defaultMediaId;
    if (cfg.contains("media") && cfg["media"].is_array()) {
        for (const auto& m : cfg["media"]) {
            if (!m.is_object() || !m.contains("id") || !m["id"].is_string()) {
                continue;
            }
            defaultMediaId = QString::fromStdString(m["id"].get<std::string>());
            break;
        }
    }
    if (defaultMediaId.isEmpty()) {
        QMessageBox::warning(this, obs_module_text("CustomizedCartoon.Dock.Title"),
                             obs_module_text("CustomizedCartoon.Rules.MissingMedia"), QMessageBox::Ok);
        return;
    }

    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    cfg["rules"].push_back({{"id", id.toStdString()},
                            {"name",
                             QString(obs_module_text("CustomizedCartoon.Rules.ConditionTitle"))
                                 .arg((int)cfg["rules"].size() + 1)
                                 .toStdString()},
                            {"mediaId", defaultMediaId.toStdString()},
                            {"engageType", "GIFT_AMOUNT_MILESTONE"},
                            {"points", 100},
                            {"count", 5},
                            {"repeatable", true},
                            {"enabled", true}});

    pendingScrollToLatestRule_ = true;
    if (!service_->saveConfig(cfg)) {
        pendingScrollToLatestRule_ = false;
    }
}

void CustomizedCartoonDock::onRemoveRule() {
    return;
}

void CustomizedCartoonDock::onPreview() {
    if (!service_) {
        return;
    }
    service_->previewPlayAll();
}

void CustomizedCartoonDock::onOrientationChanged(int) {
    const bool landscape = positionTabWidget_ && positionTabWidget_->currentIndex() == 1;
    if (positionCanvas_) {
        auto* canvas = static_cast<PositionCanvasWidget*>(positionCanvas_);
        canvas->setCanvasSize(landscape ? 1280 : 720, landscape ? 720 : 1280);
    }
    refreshPositionUi();
    syncPreviewDraftTransform();
}

nlohmann::json CustomizedCartoonDock::buildCurrentPositionDraft(bool landscape) const {
    nlohmann::json cfg = service_ ? service_->getConfigSnapshot() : nlohmann::json::object();
    if (!cfg.contains("position") || !cfg["position"].is_object()) {
        cfg["position"] = nlohmann::json::object();
    }

    const char* key = landscape ? "landscape" : "portrait";
    nlohmann::json t = cfg["position"].contains(key) && cfg["position"][key].is_object() ? cfg["position"][key]
                                                                                          : nlohmann::json::object();

    const int canvasW = landscape ? 1280 : 720;
    const int canvasH = landscape ? 720 : 1280;
    const QRect normalized = normalizePositionRect(
        QRect(posXSpin_ ? posXSpin_->value() : 200, posYSpin_ ? posYSpin_->value() : 300,
              widthSpin_ ? widthSpin_->value() : 500, heightSpin_ ? heightSpin_->value() : 500),
        canvasW, canvasH);

    t["x"] = (double)normalized.x();
    t["y"] = (double)normalized.y();
    t["boundsType"] = (int)OBS_BOUNDS_STRETCH;
    t["boundsAlignment"] = (uint32_t)(OBS_ALIGN_LEFT | OBS_ALIGN_TOP);
    t["alignment"] = (uint32_t)(OBS_ALIGN_LEFT | OBS_ALIGN_TOP);
    t["boundsW"] = (double)normalized.width();
    t["boundsH"] = (double)normalized.height();
    t["cropToBounds"] = true;
    if (!t.contains("scaleX")) t["scaleX"] = 1.0;
    if (!t.contains("scaleY")) t["scaleY"] = 1.0;
    if (!t.contains("rot")) t["rot"] = 0.0;
    return t;
}

void CustomizedCartoonDock::syncPreviewDraftTransform() {
    if (!service_ || (!service_->isMediaPreviewing() && !service_->isPositionPreviewing())) {
        return;
    }
    const bool landscape = positionTabWidget_ && positionTabWidget_->currentIndex() == 1;
    const auto draft = buildCurrentPositionDraft(landscape);
    service_->applyOverlayTransformForOrientation(landscape, &draft);
}

void CustomizedCartoonDock::onApplyPosition() {
    if (!service_) {
        return;
    }
    json cfg = service_->getConfigSnapshot();
    if (!cfg.contains("position") || !cfg["position"].is_object()) {
        cfg["position"] = json::object();
    }
    const bool landscape = positionTabWidget_ && positionTabWidget_->currentIndex() == 1;
    const char* key = landscape ? "landscape" : "portrait";
    cfg["position"][key] = buildCurrentPositionDraft(landscape);
    service_->saveConfig(cfg);
    service_->applyOverlayTransformForOrientation(landscape);
}

void CustomizedCartoonDock::onReadPositionFromCanvas() {
    if (!service_) {
        return;
    }
    json t;
    QString error;
    if (!service_->getCurrentOverlayTransform(t, error)) {
        QMessageBox::warning(this, obs_module_text("CustomizedCartoon.Dock.Title"), error,
                             QMessageBox::Ok);
        return;
    }
    const bool landscape = positionTabWidget_ && positionTabWidget_->currentIndex() == 1;
    const int canvasW = landscape ? 1280 : 720;
    const int canvasH = landscape ? 720 : 1280;
    auto* currentCanvas = positionCanvas_ ? static_cast<PositionCanvasWidget*>(positionCanvas_) : nullptr;
    applyPositionState(currentCanvas,
                       PositionSizePanelWidgets{nullptr, posXSpin_, posYSpin_, widthSpin_, heightSpin_},
                       QRect((int)std::round(t.value("x", 200.0)), (int)std::round(t.value("y", 300.0)),
                             (int)std::round(t.value("boundsW", 500.0)),
                             (int)std::round(t.value("boundsH", 500.0))),
                       canvasW, canvasH);
}

void CustomizedCartoonDock::onStartPositionPreview() {
    if (!service_) {
        return;
    }
    auto* item = mediaList_ ? mediaList_->currentItem() : nullptr;
    if (!item) {
        QMessageBox::information(this, obs_module_text("CustomizedCartoon.Dock.Title"),
                                 obs_module_text("CustomizedCartoon.Position.NoMediaForPreview"),
                                 QMessageBox::Ok);
        return;
    }
    const QString id = item->data(Qt::UserRole).toString();
    if (id.isEmpty()) {
        return;
    }
    QString error;
    const bool landscape = positionTabWidget_ && positionTabWidget_->currentIndex() == 1;
    const auto draft = buildCurrentPositionDraft(landscape);
    if (!service_->startPositionPreview(id, landscape, &draft, error)) {
        QMessageBox::warning(this, obs_module_text("CustomizedCartoon.Dock.Title"), error,
                             QMessageBox::Ok);
        return;
    }
}

void CustomizedCartoonDock::onStopPositionPreview() {
    if (service_) {
        service_->stopPositionPreview();
    }
}
