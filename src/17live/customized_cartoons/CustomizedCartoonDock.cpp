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
#include <QMessageBox>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTableWidget>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>
#include <unordered_map>

#include "CustomizedCartoonService.hpp"

using json = nlohmann::json;

namespace {

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

    QRect rect() const { return rect_; }

    void setOnRectChanged(std::function<void(const QRect&)> cb) { onRectChanged_ = std::move(cb); }

   protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        p.fillRect(rect(), QColor(0x12, 0x15, 0x1B));

        const QRectF area = contentRect();
        p.setPen(QPen(QColor(0x2A, 0x2F, 0x38), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(area, 10.0, 10.0);

        drawGrid(p, area);
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
        const int pad = 14;
        QRectF area = rect().adjusted(pad, pad, -pad, -pad);
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
        const double s = scale(area);
        const double w = canvasW_ * s;
        const double h = canvasH_ * s;
        const double ox = area.x() + (area.width() - w) / 2.0;
        const double oy = area.y() + (area.height() - h) / 2.0;
        const QRectF canvasRect(ox, oy, w, h);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x3A, 0x40, 0x4B));
        const int step = 60;
        for (int y = 0; y <= canvasH_; y += step) {
            for (int x = 0; x <= canvasW_; x += step) {
                const QPointF pt = canvasToWidget(QPointF(x, y), area);
                p.drawRoundedRect(QRectF(pt.x() - 2.0, pt.y() - 2.0, 4.0, 4.0), 1.0, 1.0);
            }
        }

        p.setPen(QPen(QColor(0x2A, 0x2F, 0x38), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(canvasRect, 8.0, 8.0);
    }

    void drawSelection(QPainter& p, const QRectF& area) {
        const QRectF r = canvasToWidget(rect_, area);
        p.setPen(QPen(QColor(0x00, 0x7A, 0xFF), 2.0));
        p.setBrush(QColor(0x00, 0x7A, 0xFF, 70));
        p.drawRect(r);

        const double h = 6.0;
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
        if (rect_.width() <= 0) rect_.setWidth(100);
        if (rect_.height() <= 0) rect_.setHeight(100);
        if (rect_.x() < 0) rect_.moveLeft(0);
        if (rect_.y() < 0) rect_.moveTop(0);
        if (rect_.right() > canvasW_) rect_.moveRight(canvasW_);
        if (rect_.bottom() > canvasH_) rect_.moveBottom(canvasH_);
        if (rect_.x() < 0) rect_.moveLeft(0);
        if (rect_.y() < 0) rect_.moveTop(0);
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

CustomizedCartoonDock::CustomizedCartoonDock(QWidget* parent, CustomizedCartoonService* service)
    : QDockWidget(parent), service_(service) {
    setWindowTitle(obs_module_text("CustomizedCartoon.Dock.Title"));
    setupUi();
    if (service_) {
        connect(service_, &CustomizedCartoonService::configChanged, this,
                &CustomizedCartoonDock::refreshUi);
        connect(service_, &CustomizedCartoonService::progressUpdated, this,
                &CustomizedCartoonDock::refreshProgress);
    }
    refreshUi();
}

void CustomizedCartoonDock::setupUi() {
    resize(980, 820);
    setMinimumSize(980, 820);

    auto* root = new QWidget(this);
    root->setObjectName("customizedCartoonRoot");
    root->setMinimumSize(980, 820);
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
        "  border-radius: 4px;"
        "}"
        "QFrame#panel {"
        "  background-color: rgba(255,255,255,0.06);"
        "  border-radius: 10px;"
        "}"
        "QFrame#subPanel {"
        "  background-color: rgba(255,255,255,0.06);"
        "  border-radius: 10px;"
        "}"
        "QLabel#sectionTitle { font-size: 18px; font-weight: 700; }"
        "QLabel#pageTitle { font-size: 24px; font-weight: 800; }"
        "QPushButton#primaryButton {"
        "  background-color: #007AFF; color: white; border: none; border-radius: 6px;"
        "  font-size: 16px; padding: 10px 18px;"
        "}"
        "QPushButton#primaryButton:hover { background-color: #0A84FF; }"
        "QPushButton#primaryButton:pressed { background-color: #0060DF; }"
        "QPushButton#dangerButton {"
        "  background-color: #FF0001; color: white; border: none; border-radius: 6px;"
        "  font-size: 14px; padding: 10px 18px;"
        "}"
        "QPushButton#dangerButton:hover { background-color: #FF3B30; }"
        "QPushButton#ghostButton {"
        "  background-color: rgba(255,255,255,0.08); color: white; border: none; border-radius: 6px;"
        "  font-size: 14px; padding: 10px 18px;"
        "}"
        "QPushButton#ghostButton:hover { background-color: rgba(255,255,255,0.12); }"
        "QPushButton#tabButton {"
        "  background-color: transparent; color: rgba(255,255,255,0.7);"
        "  border: 1px solid rgba(255,255,255,0.35); border-radius: 6px;"
        "  font-size: 16px; padding: 10px 18px;"
        "}"
        "QPushButton#tabButton:checked {"
        "  background-color: #007AFF; color: white; border: none;"
        "}"
        "QLineEdit, QSpinBox, QComboBox {"
        "  background-color: rgba(0,0,0,0.25);"
        "  border: 1px solid rgba(255,255,255,0.12);"
        "  border-radius: 6px;"
        "  color: white;"
        "  font-size: 16px;"
        "  padding: 6px 10px;"
        "}"
        "QCheckBox { color: white; font-size: 16px; }"
        "QComboBox::drop-down { border: none; }"
        "QScrollBar:vertical { background: transparent; width: 10px; margin: 0px; }"
        "QScrollBar::handle:vertical { background: rgba(255,255,255,0.18); border-radius: 5px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }");

    auto* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(24, 24, 24, 24);
    rootLayout->setSpacing(18);

    auto* pageTitle = new QLabel(obs_module_text("CustomizedCartoon.Dock.Title"), root);
    pageTitle->setObjectName("pageTitle");
    rootLayout->addWidget(pageTitle);

    auto* body = new QHBoxLayout();
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(24);
    rootLayout->addLayout(body, 1);

    auto* leftContainer = new QWidget(root);
    leftContainer->setFixedWidth(400);
    auto* left = new QVBoxLayout(leftContainer);
    left->setContentsMargins(0, 0, 0, 0);
    left->setSpacing(18);

    auto* mediaPanel = new QFrame(leftContainer);
    mediaPanel->setObjectName("panel");
    mediaPanel->setFixedSize(400, 269);
    auto* mediaLayout = new QVBoxLayout(mediaPanel);
    mediaLayout->setContentsMargins(18, 18, 18, 18);
    mediaLayout->setSpacing(14);

    auto* mediaTitle =
        new QLabel(obs_module_text("CustomizedCartoon.Media.VideoSetupTitle"), mediaPanel);
    mediaTitle->setObjectName("sectionTitle");

    mediaList_ = new QListWidget(mediaPanel);
    mediaList_->setFrameShape(QFrame::NoFrame);
    mediaList_->setSpacing(12);
    mediaList_->setSelectionMode(QAbstractItemView::SingleSelection);
    mediaList_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    mediaList_->setStyleSheet("QListWidget { background: transparent; }");

    auto* mediaBottom = new QHBoxLayout();
    mediaBottom->setContentsMargins(0, 0, 0, 0);
    mediaBottom->setSpacing(12);

    mediaCountLabel_ = new QLabel(mediaPanel);
    mediaCountLabel_->setStyleSheet("QLabel { color: white; font-size: 16px; }");

    addMediaButton_ =
        new QPushButton(obs_module_text("CustomizedCartoon.Media.ChooseVideo"), mediaPanel);
    addMediaButton_->setObjectName("primaryButton");
    addMediaButton_->setMinimumHeight(48);

    mediaBottom->addWidget(mediaCountLabel_, 1);
    mediaBottom->addWidget(addMediaButton_, 0, Qt::AlignRight);

    mediaLayout->addWidget(mediaTitle);
    mediaLayout->addWidget(mediaList_, 1);
    mediaLayout->addLayout(mediaBottom);

    left->addWidget(mediaPanel);
    connect(addMediaButton_, &QPushButton::clicked, this, &CustomizedCartoonDock::onAddMedia);

    auto* positionPanel = new QFrame(leftContainer);
    positionPanel->setObjectName("panel");
    positionPanel->setFixedSize(400, 425);
    auto* positionLayout = new QVBoxLayout(positionPanel);
    positionLayout->setContentsMargins(18, 18, 18, 18);
    positionLayout->setSpacing(12);

    auto* positionTitle =
        new QLabel(obs_module_text("CustomizedCartoon.Position.PanelTitle"), positionPanel);
    positionTitle->setObjectName("sectionTitle");
    positionLayout->addWidget(positionTitle);

    auto* tabRow = new QHBoxLayout();
    tabRow->setContentsMargins(0, 0, 0, 0);
    tabRow->setSpacing(12);
    portraitTabButton_ =
        new QPushButton(obs_module_text("CustomizedCartoon.Position.Tab.Portrait"), positionPanel);
    landscapeTabButton_ =
        new QPushButton(obs_module_text("CustomizedCartoon.Position.Tab.Landscape"), positionPanel);
    portraitTabButton_->setObjectName("tabButton");
    landscapeTabButton_->setObjectName("tabButton");
    portraitTabButton_->setCheckable(true);
    landscapeTabButton_->setCheckable(true);
    portraitTabButton_->setChecked(true);
    tabRow->addWidget(portraitTabButton_);
    tabRow->addWidget(landscapeTabButton_);
    tabRow->addStretch(1);
    positionLayout->addLayout(tabRow);

    canvasRangeLabel_ = new QLabel(positionPanel);
    canvasRangeLabel_->setStyleSheet("QLabel { font-size: 18px; font-weight: 700; }");
    positionLayout->addWidget(canvasRangeLabel_);

    auto* positionBody = new QHBoxLayout();
    positionBody->setContentsMargins(0, 0, 0, 0);
    positionBody->setSpacing(12);

    auto* canvas = new PositionCanvasWidget(positionPanel);
    positionCanvas_ = canvas;
    canvas->setFixedSize(210, 290);
    canvas->setCanvasSize(720, 1280);
    canvas->setRect(QRect(200, 300, 500, 500));
    canvas->setOnRectChanged([this](const QRect& r) {
        if (!posXSpin_ || !posYSpin_ || !widthSpin_ || !heightSpin_) {
            return;
        }
        posXSpin_->blockSignals(true);
        posYSpin_->blockSignals(true);
        widthSpin_->blockSignals(true);
        heightSpin_->blockSignals(true);
        posXSpin_->setValue(r.x());
        posYSpin_->setValue(r.y());
        widthSpin_->setValue(r.width());
        heightSpin_->setValue(r.height());
        posXSpin_->blockSignals(false);
        posYSpin_->blockSignals(false);
        widthSpin_->blockSignals(false);
        heightSpin_->blockSignals(false);
    });
    positionBody->addWidget(canvas, 0, Qt::AlignTop);

    auto* rightSide = new QVBoxLayout();
    rightSide->setContentsMargins(0, 0, 0, 0);
    rightSide->setSpacing(12);

    auto* sizePanel = new QFrame(positionPanel);
    sizePanel->setObjectName("subPanel");
    auto* sizeLayout = new QVBoxLayout(sizePanel);
    sizeLayout->setContentsMargins(14, 14, 14, 14);
    sizeLayout->setSpacing(10);
    auto* sizeTitle =
        new QLabel(obs_module_text("CustomizedCartoon.Position.Size.Title"), sizePanel);
    sizeTitle->setObjectName("sectionTitle");
    sizeTitle->setStyleSheet("QLabel#sectionTitle { font-size: 18px; font-weight: 700; }");
    sizeLayout->addWidget(sizeTitle);

    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(12);

    auto makePx = [sizePanel]() {
        auto* l = new QLabel("px", sizePanel);
        l->setStyleSheet("QLabel { color: rgba(255,255,255,0.7); font-size: 16px; }");
        return l;
    };

    posXSpin_ = new QSpinBox(sizePanel);
    posXSpin_->setRange(-100000, 100000);
    posYSpin_ = new QSpinBox(sizePanel);
    posYSpin_->setRange(-100000, 100000);
    widthSpin_ = new QSpinBox(sizePanel);
    widthSpin_->setRange(20, 100000);
    heightSpin_ = new QSpinBox(sizePanel);
    heightSpin_->setRange(20, 100000);

    auto* xLabel = new QLabel(obs_module_text("CustomizedCartoon.Position.Field.X"), sizePanel);
    auto* yLabel = new QLabel(obs_module_text("CustomizedCartoon.Position.Field.Y"), sizePanel);
    auto* wLabel = new QLabel(obs_module_text("CustomizedCartoon.Position.Field.Width"), sizePanel);
    auto* hLabel = new QLabel(obs_module_text("CustomizedCartoon.Position.Field.Height"), sizePanel);
    xLabel->setStyleSheet("QLabel { font-size: 16px; }");
    yLabel->setStyleSheet("QLabel { font-size: 16px; }");
    wLabel->setStyleSheet("QLabel { font-size: 16px; }");
    hLabel->setStyleSheet("QLabel { font-size: 16px; }");

    grid->addWidget(xLabel, 0, 0);
    grid->addWidget(posXSpin_, 0, 1);
    grid->addWidget(makePx(), 0, 2);

    grid->addWidget(yLabel, 1, 0);
    grid->addWidget(posYSpin_, 1, 1);
    grid->addWidget(makePx(), 1, 2);

    grid->addWidget(wLabel, 2, 0);
    grid->addWidget(widthSpin_, 2, 1);
    grid->addWidget(makePx(), 2, 2);

    grid->addWidget(hLabel, 3, 0);
    grid->addWidget(heightSpin_, 3, 1);
    grid->addWidget(makePx(), 3, 2);

    sizeLayout->addLayout(grid);
    rightSide->addWidget(sizePanel);

    auto* tipsPanel = new QFrame(positionPanel);
    tipsPanel->setObjectName("subPanel");
    auto* tipsLayout = new QVBoxLayout(tipsPanel);
    tipsLayout->setContentsMargins(14, 14, 14, 14);
    tipsLayout->setSpacing(10);
    auto* tipsLabel = new QLabel(positionPanel);
    tipsLabel->setText(QString("• %1<br/>• %2<br/>• %3<br/>• %4")
                           .arg(obs_module_text("CustomizedCartoon.Position.Tips.1"))
                           .arg(obs_module_text("CustomizedCartoon.Position.Tips.2"))
                           .arg(obs_module_text("CustomizedCartoon.Position.Tips.3"))
                           .arg(obs_module_text("CustomizedCartoon.Position.Tips.4")));
    tipsLabel->setTextFormat(Qt::RichText);
    tipsLabel->setWordWrap(true);
    tipsLabel->setStyleSheet("QLabel { font-size: 16px; color: rgba(255,255,255,0.9); }");
    tipsLayout->addWidget(tipsLabel);
    rightSide->addWidget(tipsPanel, 1);

    positionBody->addLayout(rightSide, 1);
    positionLayout->addLayout(positionBody, 1);
    left->addWidget(positionPanel);

    connect(portraitTabButton_, &QPushButton::clicked, this, [this]() {
        if (portraitTabButton_) portraitTabButton_->setChecked(true);
        if (landscapeTabButton_) landscapeTabButton_->setChecked(false);
        onOrientationChanged(0);
    });
    connect(landscapeTabButton_, &QPushButton::clicked, this, [this]() {
        if (portraitTabButton_) portraitTabButton_->setChecked(false);
        if (landscapeTabButton_) landscapeTabButton_->setChecked(true);
        onOrientationChanged(1);
    });

    auto syncCanvasFromInputs = [this, canvas]() {
        if (!posXSpin_ || !posYSpin_ || !widthSpin_ || !heightSpin_) {
            return;
        }
        canvas->setRect(QRect(posXSpin_->value(), posYSpin_->value(), widthSpin_->value(),
                              heightSpin_->value()));
    };
    connect(posXSpin_, &QSpinBox::valueChanged, this, [syncCanvasFromInputs](int) {
        syncCanvasFromInputs();
    });
    connect(posYSpin_, &QSpinBox::valueChanged, this, [syncCanvasFromInputs](int) {
        syncCanvasFromInputs();
    });
    connect(widthSpin_, &QSpinBox::valueChanged, this, [syncCanvasFromInputs](int) {
        syncCanvasFromInputs();
    });
    connect(heightSpin_, &QSpinBox::valueChanged, this, [syncCanvasFromInputs](int) {
        syncCanvasFromInputs();
    });

    body->addWidget(leftContainer, 0, Qt::AlignTop);

    auto* rightPanel = new QFrame(root);
    rightPanel->setObjectName("panel");
    rightPanel->setFixedSize(550, 704);
    auto* right = new QVBoxLayout(rightPanel);
    right->setContentsMargins(18, 18, 18, 18);
    right->setSpacing(14);

    auto* rulesTitle = new QLabel(obs_module_text("CustomizedCartoon.Rules.PanelTitle"), rightPanel);
    rulesTitle->setObjectName("sectionTitle");
    right->addWidget(rulesTitle);

    rulesScrollArea_ = new QScrollArea(rightPanel);
    rulesScrollArea_->setWidgetResizable(true);
    rulesScrollArea_->setFrameShape(QFrame::NoFrame);
    rulesScrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rulesScrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    rulesListContainer_ = new QWidget(rulesScrollArea_);
    rulesListLayout_ = new QVBoxLayout(rulesListContainer_);
    rulesListLayout_->setContentsMargins(0, 0, 0, 0);
    rulesListLayout_->setSpacing(12);
    rulesListLayout_->addStretch(1);
    rulesScrollArea_->setWidget(rulesListContainer_);
    right->addWidget(rulesScrollArea_, 1);

    addRuleButton_ = new QPushButton(obs_module_text("CustomizedCartoon.Rules.AddCondition"), rightPanel);
    addRuleButton_->setObjectName("ghostButton");
    addRuleButton_->setMinimumHeight(48);
    right->addWidget(addRuleButton_);
    connect(addRuleButton_, &QPushButton::clicked, this, &CustomizedCartoonDock::onAddRule);

    auto* bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 0, 0, 0);
    bottomRow->setSpacing(12);
    bottomRow->addStretch(1);
    cancelButton_ = new QPushButton(obs_module_text("CustomizedCartoon.Action.Cancel"), rightPanel);
    confirmButton_ = new QPushButton(obs_module_text("CustomizedCartoon.Action.Confirm"), rightPanel);
    applyButton_ = new QPushButton(obs_module_text("CustomizedCartoon.Action.Apply"), rightPanel);
    cancelButton_->setObjectName("ghostButton");
    confirmButton_->setObjectName("dangerButton");
    applyButton_->setObjectName("dangerButton");
    cancelButton_->setMinimumHeight(44);
    confirmButton_->setMinimumHeight(44);
    applyButton_->setMinimumHeight(44);
    bottomRow->addWidget(cancelButton_);
    bottomRow->addWidget(confirmButton_);
    bottomRow->addWidget(applyButton_);
    right->addLayout(bottomRow);

    connect(cancelButton_, &QPushButton::clicked, this, [this]() { refreshUi(); });
    connect(confirmButton_, &QPushButton::clicked, this, [this]() { setVisible(false); });
    connect(applyButton_, &QPushButton::clicked, this, &CustomizedCartoonDock::onApplyPosition);

    body->addWidget(rightPanel, 0, Qt::AlignTop);

    setWidget(root);
}

void CustomizedCartoonDock::refreshUi() {
    loadFromConfig();
    refreshPositionUi();
    refreshProgress();
}

void CustomizedCartoonDock::refreshPositionUi() {
    if (!service_ || !portraitTabButton_ || !landscapeTabButton_) {
        return;
    }
    const json cfg = service_->getConfigSnapshot();
    const bool landscape = landscapeTabButton_->isChecked();
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
    if (!cfg.contains("position") || !cfg["position"].is_object() || !cfg["position"].contains(key) ||
        !cfg["position"][key].is_object()) {
        if (posXSpin_) posXSpin_->setValue(200);
        if (posYSpin_) posYSpin_->setValue(300);
        if (widthSpin_) widthSpin_->setValue(500);
        if (heightSpin_) heightSpin_->setValue(500);
        return;
    }

    const auto& t = cfg["position"][key];
    if (posXSpin_) posXSpin_->setValue((int)std::round(t.value("x", 200.0)));
    if (posYSpin_) posYSpin_->setValue((int)std::round(t.value("y", 300.0)));
    if (widthSpin_) widthSpin_->setValue((int)std::round(t.value("boundsW", 500.0)));
    if (heightSpin_) heightSpin_->setValue((int)std::round(t.value("boundsH", 500.0)));
}

void CustomizedCartoonDock::loadFromConfig() {
    if (!service_) {
        return;
    }
    const json cfg = service_->getConfigSnapshot();

    mediaList_->clear();
    int videoCount = 0;
    if (cfg.contains("media") && cfg["media"].is_array()) {
        const QIcon videoIcon(":/resources/video.svg");
        const QIcon trashIcon(":/resources/trash-red.svg");
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
            if (type == "video") {
                videoCount++;
            }

            auto* item = new QListWidgetItem(mediaList_);
            item->setData(Qt::UserRole, id);
            item->setSizeHint(QSize(0, 64));

            auto* row = new QFrame(mediaList_);
            row->setObjectName("mediaRow");
            row->setStyleSheet(
                "QFrame#mediaRow { background-color: rgba(255,255,255,0.06); border-radius: 10px; }"
                "QLabel { color: #DDE1E8; font-size: 16px; }"
                "QPushButton { border: none; background: transparent; }");

            auto* rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(14, 10, 14, 10);
            rowLayout->setSpacing(12);

            auto* iconLabel = new QLabel(row);
            iconLabel->setPixmap(videoIcon.pixmap(30, 24));
            iconLabel->setFixedSize(30, 24);

            auto* nameLabel = new QLabel(name, row);
            nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            nameLabel->setTextInteractionFlags(Qt::NoTextInteraction);

            auto* delButton = new QPushButton(row);
            delButton->setIcon(trashIcon);
            delButton->setIconSize(QSize(24, 24));
            delButton->setFixedSize(40, 40);
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

            rowLayout->addWidget(iconLabel);
            rowLayout->addWidget(nameLabel, 1);
            rowLayout->addWidget(delButton);

            mediaList_->addItem(item);
            mediaList_->setItemWidget(item, row);
        }
    }

    if (mediaCountLabel_) {
        mediaCountLabel_->setText(
            QString(obs_module_text("CustomizedCartoon.Media.SelectedVideoCount")).arg(videoCount));
    }
    if (mediaList_->count() > 0 && mediaList_->currentRow() < 0) {
        mediaList_->setCurrentRow(0);
    }

    rebuildRulesUi();
}

void CustomizedCartoonDock::rebuildRulesUi() {
    if (!service_ || !rulesListLayout_ || !rulesListContainer_) {
        return;
    }

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
            cardLayout->setContentsMargins(14, 14, 14, 14);
            cardLayout->setSpacing(12);

            auto* header = new QHBoxLayout();
            header->setContentsMargins(0, 0, 0, 0);
            header->setSpacing(12);

            auto* title =
                new QLabel(QString(obs_module_text("CustomizedCartoon.Rules.ConditionTitle")).arg(idx + 1),
                           card);
            title->setStyleSheet("QLabel { font-size: 16px; font-weight: 700; }");

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
            grid->setHorizontalSpacing(12);
            grid->setVerticalSpacing(12);

            auto makeLabel = [card](const char* key) {
                auto* l = new QLabel(obs_module_text(key), card);
                l->setStyleSheet("QLabel { font-size: 14px; color: rgba(255,255,255,0.75); }");
                return l;
            };

            auto* typeCombo = new QComboBox(card);
            typeCombo->addItem(obs_module_text("CustomizedCartoon.Rules.Type.LuckyBag"),
                               "GIFT_LUCKYBAG_FIRST_PRIZE_MILESTONE");
            typeCombo->addItem(obs_module_text("CustomizedCartoon.Rules.Type.Gift"), "GIFT_AMOUNT_MILESTONE");
            const int typeIndex =
                engageType == "GIFT_LUCKYBAG_FIRST_PRIZE_MILESTONE" ? 0 : 1;
            typeCombo->setCurrentIndex(typeIndex);

            auto* ruleLabel = new QLabel(card);
            ruleLabel->setWordWrap(true);
            ruleLabel->setStyleSheet("QLabel { font-size: 14px; color: rgba(255,255,255,0.9); }");

            auto* pointsSpin = new QSpinBox(card);
            pointsSpin->setRange(0, 100000000);
            pointsSpin->setValue(points);
            auto* countSpin = new QSpinBox(card);
            countSpin->setRange(1, 1000000);
            countSpin->setValue(std::max(1, count));

            auto* mediaCombo = new QComboBox(card);
            for (const auto& m : mediaOptions) {
                mediaCombo->addItem(m.second, m.first);
            }
            const int mediaIndex = mediaCombo->findData(mediaId);
            if (mediaIndex >= 0) {
                mediaCombo->setCurrentIndex(mediaIndex);
            }

            auto* repeatCheck = new QCheckBox(card);
            repeatCheck->setChecked(repeatable);

            auto* statusCombo = new QComboBox(card);
            statusCombo->addItem(obs_module_text("CustomizedCartoon.Rules.Status.Active"), true);
            statusCombo->addItem(obs_module_text("CustomizedCartoon.Rules.Status.Inactive"), false);
            statusCombo->setCurrentIndex(enabled ? 0 : 1);

            auto updateRuleText = [ruleLabel, typeCombo, pointsSpin, countSpin]() {
                const QString t = typeCombo->currentData().toString();
                if (t == "GIFT_LUCKYBAG_FIRST_PRIZE_MILESTONE") {
                    ruleLabel->setText(
                        QString(obs_module_text("CustomizedCartoon.Rules.Params.LuckyBag"))
                            .arg(countSpin->value()));
                } else {
                    ruleLabel->setText(
                        QString(obs_module_text("CustomizedCartoon.Rules.Params.Gift"))
                            .arg(pointsSpin->value())
                            .arg(countSpin->value()));
                }
            };
            updateRuleText();

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
                    r["engageType"] = typeCombo->currentData().toString().toStdString();
                    r["points"] = pointsSpin->value();
                    r["count"] = countSpin->value();
                    r["mediaId"] = mediaCombo->currentData().toString().toStdString();
                    r["repeatable"] = repeatCheck->isChecked();
                    r["enabled"] = statusCombo->currentData().toBool();
                    break;
                }
                service_->saveConfig(cfg);
            };

            connect(typeCombo, &QComboBox::currentIndexChanged, card, [updateRuleText, saveRule](int) {
                updateRuleText();
                saveRule();
            });
            connect(pointsSpin, &QSpinBox::valueChanged, card, [updateRuleText, saveRule](int) {
                updateRuleText();
                saveRule();
            });
            connect(countSpin, &QSpinBox::valueChanged, card, [updateRuleText, saveRule](int) {
                updateRuleText();
                saveRule();
            });
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

            auto* xyRow = new QHBoxLayout();
            xyRow->setContentsMargins(0, 0, 0, 0);
            xyRow->setSpacing(10);
            auto* xTitle = new QLabel("X", card);
            xTitle->setStyleSheet("QLabel { font-size: 14px; color: rgba(255,255,255,0.75); }");
            auto* yTitle = new QLabel("Y", card);
            yTitle->setStyleSheet("QLabel { font-size: 14px; color: rgba(255,255,255,0.75); }");
            xyRow->addWidget(xTitle);
            xyRow->addWidget(pointsSpin, 1);
            xyRow->addWidget(yTitle);
            xyRow->addWidget(countSpin, 1);
            auto* xyWrap = new QWidget(card);
            xyWrap->setLayout(xyRow);
            grid->addWidget(makeLabel("CustomizedCartoon.Rules.Field.Threshold"), 2, 0);
            grid->addWidget(xyWrap, 2, 1);

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

            grid->addWidget(makeLabel("CustomizedCartoon.Rules.Field.Media"), 3, 0);
            grid->addWidget(mediaCombo, 3, 1);

            auto* repeatRow = new QHBoxLayout();
            repeatRow->setContentsMargins(0, 0, 0, 0);
            repeatRow->addWidget(repeatCheck);
            repeatRow->addStretch(1);
            auto* repeatWrap = new QWidget(card);
            repeatWrap->setLayout(repeatRow);
            grid->addWidget(makeLabel("CustomizedCartoon.Rules.Field.Repeat"), 4, 0);
            grid->addWidget(repeatWrap, 4, 1);

            grid->addWidget(makeLabel("CustomizedCartoon.Rules.Field.Status"), 5, 0);
            grid->addWidget(statusCombo, 5, 1);

            cardLayout->addLayout(grid);

            rulesListLayout_->addWidget(card);
            idx++;
        }
    }

    rulesListLayout_->addStretch(1);
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

    service_->saveConfig(cfg);
    refreshUi();
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
    const bool landscape = landscapeTabButton_ && landscapeTabButton_->isChecked();
    if (positionCanvas_) {
        auto* canvas = static_cast<PositionCanvasWidget*>(positionCanvas_);
        canvas->setCanvasSize(landscape ? 1280 : 720, landscape ? 720 : 1280);
    }
    refreshPositionUi();
    if (service_) {
        service_->applyOverlayTransformForOrientation(landscape);
    }
}

void CustomizedCartoonDock::onApplyPosition() {
    if (!service_) {
        return;
    }
    json cfg = service_->getConfigSnapshot();
    if (!cfg.contains("position") || !cfg["position"].is_object()) {
        cfg["position"] = json::object();
    }
    const bool landscape = landscapeTabButton_ && landscapeTabButton_->isChecked();
    const char* key = landscape ? "landscape" : "portrait";
    json t = cfg["position"].contains(key) && cfg["position"][key].is_object() ? cfg["position"][key]
                                                                               : json::object();
    t["x"] = posXSpin_ ? (double)posXSpin_->value() : 200.0;
    t["y"] = posYSpin_ ? (double)posYSpin_->value() : 300.0;
    t["boundsType"] = (int)OBS_BOUNDS_STRETCH;
    t["boundsAlignment"] = (uint32_t)(OBS_ALIGN_LEFT | OBS_ALIGN_TOP);
    t["alignment"] = (uint32_t)(OBS_ALIGN_LEFT | OBS_ALIGN_TOP);
    t["boundsW"] = widthSpin_ ? (double)widthSpin_->value() : 500.0;
    t["boundsH"] = heightSpin_ ? (double)heightSpin_->value() : 500.0;
    t["cropToBounds"] = true;
    if (!t.contains("scaleX")) t["scaleX"] = 1.0;
    if (!t.contains("scaleY")) t["scaleY"] = 1.0;
    if (!t.contains("rot")) t["rot"] = 0.0;
    cfg["position"][key] = std::move(t);
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
    if (posXSpin_) posXSpin_->setValue((int)std::round(t.value("x", 200.0)));
    if (posYSpin_) posYSpin_->setValue((int)std::round(t.value("y", 300.0)));
    if (widthSpin_) widthSpin_->setValue((int)std::round(t.value("boundsW", 500.0)));
    if (heightSpin_) heightSpin_->setValue((int)std::round(t.value("boundsH", 500.0)));
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
    const bool landscape = landscapeTabButton_ && landscapeTabButton_->isChecked();
    if (!service_->startPositionPreview(id, landscape, error)) {
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
