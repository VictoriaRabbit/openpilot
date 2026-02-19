#include "selfdrive/ui/qt/onroad/onroad_home.h"

#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <algorithm>

#include "selfdrive/ui/qt/util.h"

OnroadWindow::OnroadWindow(QWidget *parent) : QWidget(parent) {
  QVBoxLayout *main_layout  = new QVBoxLayout(this);
  main_layout->setMargin(UI_BORDER_SIZE);
  QStackedLayout *stacked_layout = new QStackedLayout;
  stacked_layout->setStackingMode(QStackedLayout::StackAll);
  main_layout->addLayout(stacked_layout);

  nvg = new AnnotatedCameraWidget(VISION_STREAM_ROAD, this);

  QWidget * split_wrapper = new QWidget;
  split = new QHBoxLayout(split_wrapper);
  split->setContentsMargins(0, 0, 0, 0);
  split->setSpacing(0);
  split->addWidget(nvg);

  if (getenv("DUAL_CAMERA_VIEW")) {
    CameraWidget *arCam = new CameraWidget("camerad", VISION_STREAM_ROAD, this);
    split->insertWidget(0, arCam);
  }

  stacked_layout->addWidget(split_wrapper);

  alerts = new OnroadAlerts(this);
  alerts->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  stacked_layout->addWidget(alerts);

  // setup stacking order
  alerts->raise();

    // ==================== ADD WEIGHT ADJUSTER HERE ====================

  // Create container widget for weight controls
  QWidget *weight_container = new QWidget(this);
  weight_container->setFixedSize(380, 100);
  weight_container->setStyleSheet("background-color: rgba(0, 0, 0, 150); border-radius: 10px;");
  weight_container->move(10, 10);  // Top-left corner

  QHBoxLayout *weight_layout = new QHBoxLayout(weight_container);
  weight_layout->setSpacing(15);
  weight_layout->setContentsMargins(10, 10, 10, 10);

  // Minus button (-50 kg)
  weight_minus_btn = new QPushButton("-50", weight_container);
  weight_minus_btn->setFixedSize(100, 80);
  weight_minus_btn->setStyleSheet(
    "QPushButton { font-size: 32px; font-weight: bold; "
    "background-color: rgba(200, 50, 50, 200); color: white; border-radius: 10px; }"
    "QPushButton:pressed { background-color: rgba(150, 30, 30, 250); }"
  );

  // Weight display label
  weight_display = new QLabel(weight_container);
  weight_display->setAlignment(Qt::AlignCenter);
  weight_display->setStyleSheet("font-size: 38px; font-weight: bold; color: white;");
  QString current = QString::fromStdString(params.get("VehicleWeight"));
  weight_display->setText(current.isEmpty() ? "1500 kg" : current + " kg");

  // Plus button (+50 kg)
  weight_plus_btn = new QPushButton("+50", weight_container);
  weight_plus_btn->setFixedSize(100, 80);
  weight_plus_btn->setStyleSheet(
    "QPushButton { font-size: 32px; font-weight: bold; "
    "background-color: rgba(50, 200, 50, 200); color: white; border-radius: 10px; }"
    "QPushButton:pressed { background-color: rgba(30, 150, 30, 250); }"
  );

  // Connect minus button
  QObject::connect(weight_minus_btn, &QPushButton::clicked, [=]() {
    QString current = QString::fromStdString(params.get("VehicleWeight"));
    int weight = current.isEmpty() ? 1500 : current.toInt();
    int new_weight = std::max(500, weight - 50);
    params.put("VehicleWeight", std::to_string(new_weight));
    weight_display->setText(QString::number(new_weight) + " kg");
  });

  // Connect plus button
  QObject::connect(weight_plus_btn, &QPushButton::clicked, [=]() {
    QString current = QString::fromStdString(params.get("VehicleWeight"));
    int weight = current.isEmpty() ? 1500 : current.toInt();
    int new_weight = std::min(5000, weight + 50);
    params.put("VehicleWeight", std::to_string(new_weight));
    weight_display->setText(QString::number(new_weight) + " kg");
  });

  // Add widgets to layout
  weight_layout->addWidget(weight_minus_btn);
  weight_layout->addWidget(weight_display, 1);
  weight_layout->addWidget(weight_plus_btn);

  weight_container->raise();  // Keep on top of camera view

  // ==================== END WEIGHT ADJUSTER ====================

  setAttribute(Qt::WA_OpaquePaintEvent);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &OnroadWindow::updateState);
  QObject::connect(uiState(), &UIState::offroadTransition, this, &OnroadWindow::offroadTransition);
}

void OnroadWindow::updateState(const UIState &s) {
  if (!s.scene.started) {
    return;
  }

  alerts->updateState(s);
  nvg->updateState(s);

  QColor bgColor = bg_colors[s.status];
  if (bg != bgColor) {
    // repaint border
    bg = bgColor;
    update();
  }
}

void OnroadWindow::offroadTransition(bool offroad) {
  alerts->clear();
}

void OnroadWindow::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.fillRect(rect(), QColor(bg.red(), bg.green(), bg.blue(), 255));
}
